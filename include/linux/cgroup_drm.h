/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _CGROUP_DRM_H
#define _CGROUP_DRM_H

#include <linux/types.h>
#include <linux/llist.h>

struct drm_device;

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
int drmcg_register_device(struct drmcgroup_device *drm_cg,
			  struct drm_device *dev);
void drmcg_unregister_device(struct drmcgroup_device *cgdev);
int drmcg_try_charge(struct drmcgroup_device *cgdev,
		     u32 index, u64 size,
		     struct drmcgroup_pool_state **drmcs,
		     struct drmcgroup_pool_state **limitcs);
void drmcg_uncharge(struct drmcgroup_pool_state *drmcs,
		    u32 index, u64 size);
bool drmcs_evict_valuable(struct drmcgroup_device *dev,
			  int index,
			  struct drmcgroup_pool_state *limitcs,
			  struct drmcgroup_pool_state *testcs,
			  bool ignore_low,
			  bool *hit_low);

void drmcs_pool_put(struct drmcgroup_pool_state *drmcs);
#else
static inline int
drmcg_register_device(struct drmcgroup_device *drm_cg,
		      struct drm_device *dev)
{
	return 0;
}

static inline void drmcg_unregister_device(struct drmcgroup_device *cgdev)
{
}

static int int drmcg_try_charge(struct drmcgroup_device *cgdev,
				u32 index, u64 size,
				struct drmcgroup_pool_state **drmcs,
				struct drmcgroup_pool_state **limitcs);
{
	*drmcs = NULL;
	if (limitcs)
		*limitcs = NULL;
	return 0;
}

static inline void drmcg_uncharge(struct drmcgroup_pool_state *drmcs,
				  u32 index, u64 size)
{ }

static inline bool drmcs_evict_valuable(struct drmcgroup_device *dev,
					int index,
					struct drmcgroup_pool_state *limitcs,
					struct drmcgroup_pool_state *testcs,
					bool ignore_low, bool *hit_low)
{
	return true;
}

static inline void drmcs_pool_put(struct drmcgroup_pool_state *drmcs)
{ }

#endif
#endif	/* _CGROUP_DRM_H */
