/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _CGROUP_DRM_H
#define _CGROUP_DRM_H

#include <linux/types.h>
#include <linux/llist.h>

#include <drm/drm_managed.h>

struct drm_device;
struct drm_file;

struct drmcgroup_pool_state;

/*
 * Use 8 as max, because of N^2 lookup when setting things, can be bumped if needed
 * Identical to TTM_NUM_MEM_TYPES to allow simplifying that code.
 */
#define DRMCG_MAX_REGIONS 8

/* Public definition of cgroup device, should not be modified after _register() */
struct drmcgroup_device {
	struct {
		u64 size;
		const char *name;
	} regions[DRMCG_MAX_REGIONS];

	int num_regions;

	/* used by cgroups, do not use */
	void *priv;
};

#if IS_ENABLED(CONFIG_CGROUP_DRM)
int drmcg_register_device(struct drm_device *dev,
			   struct drmcgroup_device *drm_cg);
void drmcg_unregister_device(struct drmcgroup_device *cgdev);
int drmcg_try_charge(struct drmcgroup_pool_state **drmcs,
		     struct drmcgroup_pool_state **limitcs,
		     struct drmcgroup_device *cgdev,
		     u32 index, u64 size);
void drmcg_uncharge(struct drmcgroup_pool_state *drmcs,
		    struct drmcgroup_device *cgdev,
		    u32 index, u64 size);

bool drmcs_evict_valuable(struct drmcgroup_pool_state *limitcs,
			  struct drmcgroup_device *dev,
			  int index,
			  struct drmcgroup_pool_state *testcs,
			  bool ignore_low,
			  bool *hit_low);

void drmcs_pool_put(struct drmcgroup_pool_state *drmcs);
#else
static inline int
drmcg_register_device(struct drm_device *dev,
		      struct drmcgroup_device *drm_cg)
{
	return 0;
}

static inline void drmcg_unregister_device(struct drmcgroup_device *cgdev)
{
}

static inline int drmcg_try_charge(struct drmcgroup_pool_state **drmcs,
				   struct drmcgroup_pool_state **limitcs,
				   struct drmcgroup_device *cgdev,
				   u32 index, u64 size)
{
	*drmcs = NULL;
	if (limitcs)
		*limitcs = NULL;
	return 0;
}

static inline void drmcg_uncharge(struct drmcgroup_pool_state *drmcs,
				  struct drmcgroup_device *cgdev,
				  u32 index, u64 size)
{ }

static inline bool drmcs_evict_valuable(struct drmcgroup_pool_state *limitcs,
					struct drmcgroup_device *dev, int index,
					struct drmcgroup_pool_state *testcs,
					bool ignore_low, bool *hit_low)
{
	return true;
}

static inline void drmcs_pool_put(struct drmcgroup_pool_state *drmcs)
{ }

#endif

static inline void drmmcg_unregister_device(struct drm_device *dev, void *arg)
{
	drmcg_unregister_device(arg);
}

/*
 * This needs to be done as inline, because cgroup lives in the core
 * kernel and it cannot call drm calls directly
 */
static inline int drmmcg_register_device(struct drm_device *dev,
					 struct drmcgroup_device *cgdev)
{
	return drmcg_register_device(dev, cgdev) ?:
		drmm_add_action_or_reset(dev, drmmcg_unregister_device, cgdev);
}

#endif	/* _CGROUP_DRM_H */
