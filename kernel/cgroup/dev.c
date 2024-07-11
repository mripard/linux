// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023-2024 Intel
 * Partially based on the rdma and misc controllers, which bear the following copyrights:
 *
 * Copyright 2020 Google LLC
 * Copyright (C) 2016 Parav Pandit <pandit.parav@gmail.com>
 */

#include <linux/cgroup.h>
#include <linux/cgroup_dev.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/page_counter.h>
#include <linux/parser.h>
#include <linux/slab.h>

struct devcg_device {
	spinlock_t lock;
	struct kref ref;
	struct rcu_head rcu;

	/* Protected by RCU and global spinlock */
	struct list_head dev_node;

	/* Protected by global spinlock only */
	struct list_head pools;

	/* Copy of the struct passed by device, to prevent lifetime issues */
	struct dev_cgroup_device base;

	/* Name describing the card, set by dev_cgroup_register_device */
	const char *name;

	/* Whether the device is unregistered by its caller.
	 * No new pools should be added to the device afterwards.
	 */
	bool unregistered;
};

struct devcg_state {
	struct cgroup_subsys_state css;

	struct list_head pools;
};

struct dev_cgroup_pool_state {
	struct devcg_device *device;
	struct devcg_state *cs;

	/* css node, RCU protected against device teardown */
	struct list_head	css_node;

	/* dev node, no RCU protection required */
	struct list_head	dev_node;

	int num_res, inited;
	struct rcu_head rcu;

	struct devcg_pool_res {
		struct page_counter cnt;
	} resources[];
};

/*
 * 3 operations require locking protection:
 * - Registering and unregistering device to/from list, requires global lock.
 * - Adding a dev_cgroup_pool_state to a CSS, removing when CSS is freed.
 * - Adding a dev_cgroup_pool_state to a device list.
 *
 * Since for the most common operations RCU provides enough protection, I
 * do not think more granular locking makes sense. Most protection is offered
 * by RCU and the lockless operating page_counter.
 */
static DEFINE_SPINLOCK(devcg_lock);
static LIST_HEAD(devcg_devices);

static inline struct devcg_state *
css_to_devcs(struct cgroup_subsys_state *css)
{
	return container_of(css, struct devcg_state, css);
}

static inline struct devcg_state *get_current_devcs(void)
{
	return css_to_devcs(task_get_css(current, dev_cgrp_id));
}

static struct devcg_state *parent_devcs(struct devcg_state *cg)
{
	return cg->css.parent ? css_to_devcs(cg->css.parent) : NULL;
}

static void free_cg_pool(struct dev_cgroup_pool_state *pool)
{
	list_del(&pool->dev_node);
	kfree(pool);
}

static void
set_resource_min(struct dev_cgroup_pool_state *pool, int i, u64 val)
{
	page_counter_set_min(&pool->resources[i].cnt, val);
}

static void
set_resource_low(struct dev_cgroup_pool_state *pool, int i, u64 val)
{
	page_counter_set_low(&pool->resources[i].cnt, val);
}

static void
set_resource_max(struct dev_cgroup_pool_state *pool, int i, u64 val)
{
	page_counter_set_max(&pool->resources[i].cnt, val);
}

static u64 get_resource_low(struct dev_cgroup_pool_state *pool, int idx)
{
	return pool ? READ_ONCE(pool->resources[idx].cnt.low) : 0;
}

static u64 get_resource_min(struct dev_cgroup_pool_state *pool, int idx)
{
	return pool ? READ_ONCE(pool->resources[idx].cnt.min) : 0;
}

static u64 get_resource_max(struct dev_cgroup_pool_state *pool, int idx)
{
	return pool ? READ_ONCE(pool->resources[idx].cnt.max) : PAGE_COUNTER_MAX;
}

static u64 get_resource_current(struct dev_cgroup_pool_state *pool, int idx)
{
	return pool ? page_counter_read(&pool->resources[idx].cnt) : 0;
}

static void reset_all_resource_limits(struct dev_cgroup_pool_state *rpool)
{
	int i;

	for (i = 0; i < rpool->num_res; i++) {
		set_resource_min(rpool, i, 0);
		set_resource_low(rpool, i, 0);
		set_resource_max(rpool, i, PAGE_COUNTER_MAX);
	}
}

static void devcs_offline(struct cgroup_subsys_state *css)
{
	struct devcg_state *devcs = css_to_devcs(css);
	struct dev_cgroup_pool_state *pool;

	rcu_read_lock();
	list_for_each_entry_rcu(pool, &devcs->pools, css_node)
		reset_all_resource_limits(pool);
	rcu_read_unlock();
}

static void devcs_free(struct cgroup_subsys_state *css)
{
	struct devcg_state *devcs = css_to_devcs(css);
	struct dev_cgroup_pool_state *pool, *next;

	spin_lock(&devcg_lock);
	list_for_each_entry_safe(pool, next, &devcs->pools, css_node) {
		/*
		 *The pool is dead and all references are 0,
		 * no need for RCU protection with list_del_rcu or freeing.
		 */
		list_del(&pool->css_node);
		free_cg_pool(pool);
	}
	spin_unlock(&devcg_lock);

	kfree(devcs);
}

static struct cgroup_subsys_state *
devcs_alloc(struct cgroup_subsys_state *parent_css)
{
	struct devcg_state *devcs = kzalloc(sizeof(*devcs), GFP_KERNEL);
	if (!devcs)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&devcs->pools);
	return &devcs->css;
}

static struct dev_cgroup_pool_state *
find_cg_pool_locked(struct devcg_state *devcs, struct devcg_device *dev)
{
	struct dev_cgroup_pool_state *pool;

	list_for_each_entry_rcu(pool, &devcs->pools, css_node, spin_locked(&devcg_lock))
		if (pool->device == dev)
			return pool;

	return NULL;
}

static struct dev_cgroup_pool_state *pool_parent(struct dev_cgroup_pool_state *pool)
{
	if (!pool->resources[0].cnt.parent)
		return NULL;

	return container_of(pool->resources[0].cnt.parent, typeof(*pool), resources[0].cnt);
}

bool dev_cgroup_state_evict_valuable(struct dev_cgroup_device *dev, int index,
				     struct dev_cgroup_pool_state *limit,
				     struct dev_cgroup_pool_state *test,
				     bool ignore_low,
				     bool *hit_low)
{
	struct dev_cgroup_pool_state *pool = test;
	struct page_counter *climit, *ctest;
	u64 used, min, low;

	/* Can always evict from current pool, despite limits */
	if (limit == test)
		return true;

	if (limit) {
		if (!parent_devcs(limit->cs))
			return true;

		for (pool = test; pool && limit != pool; pool = pool_parent(pool))
			{}

		if (!pool)
			return false;
	} else {
		/*
		 * If there is no cgroup limiting memory usage, use the root
		 * cgroup instead for limit calculations.
		 */
		for (limit = test; pool_parent(limit); limit = pool_parent(limit))
			{}
	}

	climit = &limit->resources[index].cnt;
	ctest = &test->resources[index].cnt;

	page_counter_calculate_protection(climit, ctest, true);

	used = page_counter_read(ctest);
	min = READ_ONCE(ctest->emin);

	if (used <= min)
		return false;

	if (!ignore_low) {
		low = READ_ONCE(ctest->elow);
		if (used > low)
			return true;

		*hit_low = true;
		return false;
	}
	return true;
}
EXPORT_SYMBOL_GPL(dev_cgroup_state_evict_valuable);

static struct dev_cgroup_pool_state *
alloc_pool_single(struct devcg_state *devcs, struct devcg_device *dev,
		  struct dev_cgroup_pool_state **allocpool)
{
	struct devcg_state *parent = parent_devcs(devcs);
	struct dev_cgroup_pool_state *pool, *ppool = NULL;
	int i;

	if (!*allocpool) {
		pool = kzalloc(offsetof(struct dev_cgroup_pool_state, resources[dev->base.num_regions]), GFP_NOWAIT);
		if (!pool)
			return ERR_PTR(-ENOMEM);
	} else {
		pool = *allocpool;
		*allocpool = NULL;
	}

	pool->device = dev;
	pool->num_res = dev->base.num_regions;
	pool->cs = devcs;

	if (parent)
		ppool = find_cg_pool_locked(parent, dev);

	for (i = 0; i < pool->num_res; i++)
		page_counter_init(&pool->resources[i].cnt, ppool ? &ppool->resources[i].cnt : NULL);
	reset_all_resource_limits(pool);

	list_add_tail_rcu(&pool->css_node, &devcs->pools);
	list_add_tail(&pool->dev_node, &dev->pools);

	if (!parent)
		pool->inited = true;
	else
		pool->inited = ppool ? ppool->inited : false;
	return pool;
}

static struct dev_cgroup_pool_state *
get_cg_pool_locked(struct devcg_state *devcs, struct devcg_device *dev,
		   struct dev_cgroup_pool_state **allocpool)
{
	struct dev_cgroup_pool_state *pool, *ppool, *retpool;
	struct devcg_state *p, *pp;
	int i;

	/*
	 * Recursively create pool, we may not initialize yet on
	 * recursion, this is done as a separate step.
	 */
	for (p = devcs; p; p = parent_devcs(p)) {
		pool = find_cg_pool_locked(p, dev);
		if (!pool)
			pool = alloc_pool_single(p, dev, allocpool);

		if (IS_ERR(pool))
			return pool;

		if (p == devcs && pool->inited)
			return pool;

		if (pool->inited)
			break;
	}

	retpool = pool = find_cg_pool_locked(devcs, dev);
	for (p = devcs, pp = parent_devcs(devcs); pp; p = pp, pp = parent_devcs(p)) {
		if (pool->inited)
			break;

		/* ppool was created if it didn't exist by above loop. */
		ppool = find_cg_pool_locked(pp, dev);

		/* Fix up parent links, mark as inited. */
		for (i = 0; i < pool->num_res; i++)
			pool->resources[i].cnt.parent = &ppool->resources[i].cnt;
		pool->inited = true;

		pool = ppool;
	}

	return retpool;
}

static void devcg_free_rcu(struct rcu_head *rcu)
{
	struct devcg_device *dev = container_of(rcu, typeof(*dev), rcu);
	struct dev_cgroup_pool_state *pool, *next;

	list_for_each_entry_safe(pool, next, &dev->pools, dev_node)
		free_cg_pool(pool);
	kfree(dev->name);
	kfree(dev);
}

static void devcg_free_device(struct kref *ref)
{
	struct devcg_device *cgdev = container_of(ref, typeof(*cgdev), ref);

	call_rcu(&cgdev->rcu, devcg_free_rcu);
}

void dev_cgroup_unregister_device(struct dev_cgroup_device *cgdev)
{
	struct devcg_device *dev;
	struct list_head *entry;

	if (!cgdev || !cgdev->priv)
		return;

	dev = cgdev->priv;
	cgdev->priv = NULL;

	spin_lock(&devcg_lock);

	/* Remove from global device list */
	list_del_rcu(&dev->dev_node);

	list_for_each_rcu(entry, &dev->pools) {
		struct dev_cgroup_pool_state *pool =
			container_of(entry, typeof(*pool), dev_node);

		list_del_rcu(&pool->css_node);
	}

	/*
	 * Ensure any RCU based lookups fail. Additionally,
	 * no new pools should be added to the dead device
	 * by get_cg_pool_unlocked.
	 */
	dev->unregistered = true;
	spin_unlock(&devcg_lock);

	kref_put(&dev->ref, devcg_free_device);
}

EXPORT_SYMBOL_GPL(dev_cgroup_unregister_device);

int dev_cgroup_register_device(struct dev_cgroup_device *cgdev,
			       const char *name)
{
	struct devcg_device *dev;
	char *dev_name;

	cgdev->priv = NULL;
	if (!cgdev->num_regions)
		return 0;

	cgdev->priv = dev = kzalloc(sizeof (*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev_name = kstrdup(name, GFP_KERNEL);
	if (!dev_name) {
		kfree(dev);
		cgdev->priv = NULL;
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&dev->pools);
	dev->name = dev_name;
	dev->base = *cgdev;
	kref_init(&dev->ref);

	spin_lock(&devcg_lock);
	list_add_tail_rcu(&dev->dev_node, &devcg_devices);
	spin_unlock(&devcg_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(dev_cgroup_register_device);

static struct devcg_device *devcg_get_device(const char *name)
{
	struct devcg_device *dev;

	list_for_each_entry_rcu(dev, &devcg_devices, dev_node, spin_locked(&devcg_lock))
		if (!strcmp(name, dev->name) &&
		    kref_get_unless_zero(&dev->ref))
			return dev;

	return NULL;
}

void dev_cgroup_pool_state_put(struct dev_cgroup_pool_state *pool)
{
	if (pool)
		css_put(&pool->cs->css);
}
EXPORT_SYMBOL_GPL(dev_cgroup_pool_state_put);

static struct dev_cgroup_pool_state *
get_cg_pool_unlocked(struct devcg_state *cg, struct devcg_device *dev)
{
	struct dev_cgroup_pool_state *pool, *allocpool = NULL;

	/* fastpath lookup? */
	rcu_read_lock();
	pool = find_cg_pool_locked(cg, dev);
	if (pool && !READ_ONCE(pool->inited))
		pool = NULL;
	rcu_read_unlock();

	while (!pool) {
		spin_lock(&devcg_lock);
		if (!dev->unregistered)
			pool = get_cg_pool_locked(cg, dev, &allocpool);
		else
			pool = ERR_PTR(-ENODEV);
		spin_unlock(&devcg_lock);

		if (pool == ERR_PTR(-ENOMEM)) {
			pool = NULL;
			if (WARN_ON(allocpool))
				continue;

			allocpool = kzalloc(offsetof(struct dev_cgroup_pool_state, resources[dev->base.num_regions]), GFP_KERNEL);
			if (allocpool) {
				pool = NULL;
				continue;
			}
		}
	}

	kfree(allocpool);
	return pool;
}

void dev_cgroup_uncharge(struct dev_cgroup_pool_state *pool,
			 u32 index, u64 size)
{
	struct dev_cgroup_device *cgdev;

	if (!pool)
		return;

	cgdev = &pool->device->base;
	if (index >= cgdev->num_regions)
		return;

	page_counter_uncharge(&pool->resources[index].cnt, size);
	css_put(&pool->cs->css);
}
EXPORT_SYMBOL_GPL(dev_cgroup_uncharge);

int dev_cgroup_try_charge(struct dev_cgroup_device *dev,
			  u32 index, u64 size,
			  struct dev_cgroup_pool_state **ret_pool,
			  struct dev_cgroup_pool_state **ret_limit_pool)
{
	struct devcg_device *cgdev = dev->priv;
	struct devcg_state *cg;
	struct dev_cgroup_pool_state *pool;
	struct page_counter *fail;
	int ret;

	*ret_pool = NULL;
	if (ret_limit_pool)
		*ret_limit_pool = NULL;

	/* Early init or device unregistered */
	if (!cgdev)
		return 0;

	if (index >= cgdev->base.num_regions)
		return -EINVAL;

	/*
	 * hold on to css, as cgroup can be removed but resource
	 * accounting happens on css.
	 */
	cg = get_current_devcs();

	pool = get_cg_pool_unlocked(cg, cgdev);
	if (IS_ERR(pool)) {
		ret = PTR_ERR(pool);
		goto err;
	}

	if (!page_counter_try_charge(&pool->resources[index].cnt, size, &fail)) {
		if (ret_limit_pool) {
			*ret_limit_pool = container_of(fail, struct dev_cgroup_pool_state, resources[index].cnt);
			css_get(&(*ret_limit_pool)->cs->css);
		}
		ret = -EAGAIN;
		goto err;
	}

	/* On success, reference is transferred to *ret_pool */
	*ret_pool = pool;
	return 0;

err:
	css_put(&cg->css);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_cgroup_try_charge);

static int devcg_capacity_show(struct seq_file *sf, void *v)
{
	struct devcg_device *dev;
	int i;

	rcu_read_lock();
	list_for_each_entry_rcu(dev, &devcg_devices, dev_node) {
		seq_puts(sf, dev->name);
		for (i = 0; i < dev->base.num_regions; i++)
			seq_printf(sf, " region.%s=%llu",
				   dev->base.regions[i].name,
				   dev->base.regions[i].size);
		seq_putc(sf, '\n');
	}
	rcu_read_unlock();
	return 0;
}

static s64 parse_resource(char *c, char **retname)
{
	substring_t argstr;
	char *name, *value = c;
	size_t len;
	int ret;
	u64 retval;

	name = strsep(&value, "=");
	if (!name || !value)
		return -EINVAL;

	/* Only support region setting for now */
	if (strncmp(name, "region.", 7))
		return -EINVAL;
	else
		name += 7;

	*retname = name;
	len = strlen(value);

	argstr.from = value;
	argstr.to = value + len;

	ret = match_u64(&argstr, &retval);
	if (ret >= 0) {
		if (retval > S64_MAX)
			return -EINVAL;
		if (retval > PAGE_COUNTER_MAX)
			return PAGE_COUNTER_MAX;
		return retval;
	}
	if (!strncmp(value, "max", len))
		return PAGE_COUNTER_MAX;

	/* Not u64 or max, error */
	return -EINVAL;
}

static int devcg_parse_limits(char *options, struct devcg_device *dev,
			      u64 *new_limits, unsigned long *set_mask)
{
	char *c, *region;

	/* parse resource options */
	while ((c = strsep(&options, " \t")) != NULL) {
		s64 limit;
		int i;

		limit = parse_resource(c, &region);
		if (limit < 0)
			return limit;


		for (i = 0; i < dev->base.num_regions; i++)
			if (!strcmp(dev->base.regions[i].name, region))
				break;

		if (i == dev->base.num_regions)
			return -EINVAL;

		new_limits[i] = limit;
		*set_mask |= BIT(i);
	}
	return 0;
}

static ssize_t devcg_limit_write(struct kernfs_open_file *of,
				 char *buf, size_t nbytes, loff_t off,
				 void (*apply)(struct dev_cgroup_pool_state *, int, u64))
{
	struct devcg_state *devcs = css_to_devcs(of_css(of));
	int err = 0;

	while (buf && !err) {
		struct dev_cgroup_pool_state *pool = NULL;
		char *options, *dev_name;
		unsigned long set_mask = 0;
		struct devcg_device *dev;
		u64 new_limits[DEVICE_CGROUP_MAX_REGIONS];
		int i;

		options = buf;
		buf = strchr(buf, '\n');
		if (buf)
			*buf++ = '\0';

		options = strstrip(options);

		/* eat empty lines */
		if (!options[0])
			continue;

		dev_name = strsep(&options, " \t");
		if (!dev_name[0])
			continue;

		rcu_read_lock();
		dev = devcg_get_device(dev_name);
		rcu_read_unlock();

		if (!dev)
			return -EINVAL;

		err = devcg_parse_limits(options, dev, new_limits, &set_mask);
		if (err < 0)
			goto out_put;

		pool = get_cg_pool_unlocked(devcs, dev);
		if (IS_ERR(pool)) {
			err = PTR_ERR(pool);
			goto out_put;
		}

		/* And commit */
		for_each_set_bit(i, &set_mask, DEVICE_CGROUP_MAX_REGIONS)
			apply(pool, i, new_limits[i]);

out_put:
		kref_put(&dev->ref, devcg_free_device);
	}


	return err ?: nbytes;
}

static int devcg_limit_show(struct seq_file *sf, void *v,
			    u64 (*fn)(struct dev_cgroup_pool_state *, int))
{
	struct devcg_state *devcs = css_to_devcs(seq_css(sf));
	struct devcg_device *dev;

	rcu_read_lock();
	list_for_each_entry_rcu(dev, &devcg_devices, dev_node) {
		struct dev_cgroup_pool_state *pool = find_cg_pool_locked(devcs, dev);

		seq_puts(sf, dev->name);

		for (int i = 0; i < dev->base.num_regions; i++) {
			u64 val = fn(pool, i);

			if (val < PAGE_COUNTER_MAX)
				seq_printf(sf, " region.%s=%lld",
					   dev->base.regions[i].name, val);
			else
				seq_printf(sf, " region.%s=max", dev->base.regions[i].name);
		}

		seq_putc(sf, '\n');
	}
	rcu_read_unlock();

	css_put(&devcs->css);

	return 0;
}

static int devcg_current_show(struct seq_file *sf, void *v)
{
	return devcg_limit_show(sf, v, get_resource_current);
}

static int devcg_min_show(struct seq_file *sf, void *v)
{
	return devcg_limit_show(sf, v, get_resource_min);
}

static ssize_t devcg_min_write(struct kernfs_open_file *of,
			       char *buf, size_t nbytes, loff_t off)
{
	return devcg_limit_write(of, buf, nbytes, off, set_resource_min);
}

static int devcg_low_show(struct seq_file *sf, void *v)
{
	return devcg_limit_show(sf, v, get_resource_low);
}

static ssize_t devcg_low_write(struct kernfs_open_file *of,
			       char *buf, size_t nbytes, loff_t off)
{
	return devcg_limit_write(of, buf, nbytes, off, set_resource_low);
}

static int devcg_max_show(struct seq_file *sf, void *v)
{
	return devcg_limit_show(sf, v, get_resource_max);
}

static ssize_t devcg_max_write(struct kernfs_open_file *of,
			       char *buf, size_t nbytes, loff_t off)
{
	return devcg_limit_write(of, buf, nbytes, off, set_resource_max);
}

static struct cftype files[] = {
	{
		.name = "capacity",
		.seq_show = devcg_capacity_show,
		.flags = CFTYPE_ONLY_ON_ROOT,
	},
	{
		.name = "current",
		.seq_show = devcg_current_show,
	},
	{
		.name = "min",
		.write = devcg_min_write,
		.seq_show = devcg_min_show,
		.flags = CFTYPE_NOT_ON_ROOT,
	},
	{
		.name = "low",
		.write = devcg_low_write,
		.seq_show = devcg_low_show,
		.flags = CFTYPE_NOT_ON_ROOT,
	},
	{
		.name = "max",
		.write = devcg_max_write,
		.seq_show = devcg_max_show,
		.flags = CFTYPE_NOT_ON_ROOT,
	},
	{ } /* Zero entry terminates. */
};

struct cgroup_subsys dev_cgrp_subsys = {
	.css_alloc	= devcs_alloc,
	.css_free	= devcs_free,
	.css_offline	= devcs_offline,
	.legacy_cftypes	= files,
	.dfl_cftypes	= files,
};
