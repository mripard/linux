/*
 * Copyright (C) Icenowy Zheng <icenowy@aosc.io>
 *
 * Based on sun4i_layer.h, which is:
 *   Copyright (C) 2015 Free Electrons
 *   Copyright (C) 2015 NextThing Co
 *
 *   Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _SUN8I_UI_H_
#define _SUN8I_UI_H_

struct sunxi_engine;

struct sun8i_ui {
	struct drm_plane	plane;
	struct sun4i_drv	*drv;
	struct sun8i_mixer	*mixer;
	u8			chan;
	int			id;
};

static inline struct sun8i_ui *
plane_to_sun8i_ui(struct drm_plane *plane)
{
	return container_of(plane, struct sun8i_ui, plane);
}

struct drm_plane **sun8i_ui_init(struct drm_device *drm,
				 struct sunxi_engine *engine);
#endif /* _SUN8I_UI_H_ */
