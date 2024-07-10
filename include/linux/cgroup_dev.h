/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _CGROUP_DEV_H
#define _CGROUP_DEV_H

#include <linux/types.h>
#include <linux/llist.h>

struct dev_cgroup_pool_state;

/*
 * Use 8 as max, because of N^2 lookup when setting things, can be bumped if needed
 * Identical to TTM_NUM_MEM_TYPES to allow simplifying that code.
 */
#define DEVICE_CGROUP_MAX_REGIONS 8

/* Public definition of cgroup device, should not be modified after _register() */
struct dev_cgroup_device {
	struct {
		u64 size;
		const char *name;
	} regions[DEVICE_CGROUP_MAX_REGIONS];

	int num_regions;

	/* used by cgroups, do not use */
	void *priv;
};

#if IS_ENABLED(CONFIG_CGROUP_DEV)
int dev_cgroup_register_device(struct dev_cgroup_device *drm_cg,
			       const char *name);
void dev_cgroup_unregister_device(struct dev_cgroup_device *cgdev);
int dev_cgroup_try_charge(struct dev_cgroup_device *cgdev,
			  u32 index, u64 size,
			  struct dev_cgroup_pool_state **drmcs,
			  struct dev_cgroup_pool_state **limitcs);
void dev_cgroup_uncharge(struct dev_cgroup_pool_state *drmcs,
			 u32 index, u64 size);
bool dev_cgroup_state_evict_valuable(struct dev_cgroup_device *dev, int index,
				     struct dev_cgroup_pool_state *limitcs,
				     struct dev_cgroup_pool_state *testcs,
				     bool ignore_low, bool *hit_low);

void dev_cgroup_pool_state_put(struct dev_cgroup_pool_state *drmcs);
#else
static inline int
dev_cgroup_register_device(struct dev_cgroup_device *drm_cg,
			   const char *name)
{
	return 0;
}

static inline void dev_cgroup_unregister_device(struct dev_cgroup_device *cgdev)
{
}

static int int dev_cgroup_try_charge(struct dev_cgroup_device *cgdev,
				     u32 index, u64 size,
				     struct dev_cgroup_pool_state **drmcs,
				     struct dev_cgroup_pool_state **limitcs);
{
	*drmcs = NULL;
	if (limitcs)
		*limitcs = NULL;
	return 0;
}

static inline void dev_cgroup_uncharge(struct dev_cgroup_pool_state *drmcs,
				       u32 index, u64 size)
{ }

static inline
bool dev_cgroup_state_evict_valuable(struct dev_cgroup_device *dev, int index,
				     struct dev_cgroup_pool_state *limitcs,
				     struct dev_cgroup_pool_state *testcs,
				     bool ignore_low, bool *hit_low)
{
	return true;
}

static inline void dev_cgroup_pool_state_put(struct dev_cgroup_pool_state *drmcs)
{ }

#endif
#endif	/* _CGROUP_DEV_H */
