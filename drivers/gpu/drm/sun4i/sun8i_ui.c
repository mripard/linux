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

#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drmP.h>

#include "sun8i_ui.h"
#include "sun8i_mixer.h"

struct sun8i_plane_desc {
	       enum drm_plane_type     type;
	       const uint32_t          *formats;
	       uint32_t                nformats;
};

static void sun8i_mixer_ui_atomic_disable(struct drm_plane *plane,
					  struct drm_plane_state *old_state)
{
	struct sun8i_ui *ui = plane_to_sun8i_ui(plane);
	struct sun8i_mixer *mixer = ui->mixer;

	sun8i_mixer_layer_enable(mixer, ui, false);
}

static void sun8i_mixer_ui_atomic_update(struct drm_plane *plane,
					 struct drm_plane_state *old_state)
{
	struct sun8i_ui *ui = plane_to_sun8i_ui(plane);
	struct sun8i_mixer *mixer = ui->mixer;

	sun8i_mixer_update_layer_coord(mixer, ui);
	sun8i_mixer_update_layer_formats(mixer, ui);
	sun8i_mixer_update_layer_buffer(mixer, ui);
	sun8i_mixer_layer_enable(mixer, ui, true);
}

static struct drm_plane_helper_funcs sun8i_mixer_ui_helper_funcs = {
	.atomic_disable	= sun8i_mixer_ui_atomic_disable,
	.atomic_update	= sun8i_mixer_ui_atomic_update,
};

static const struct drm_plane_funcs sun8i_mixer_ui_funcs = {
	.atomic_destroy_state	= drm_atomic_helper_plane_destroy_state,
	.atomic_duplicate_state	= drm_atomic_helper_plane_duplicate_state,
	.destroy		= drm_plane_cleanup,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.reset			= drm_atomic_helper_plane_reset,
	.update_plane		= drm_atomic_helper_update_plane,
};

static const uint32_t sun8i_mixer_ui_formats[] = {
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XRGB8888,
};

static const struct sun8i_plane_desc sun8i_mixer_planes[] = {
	{
		.type = DRM_PLANE_TYPE_PRIMARY,
		.formats = sun8i_mixer_ui_formats,
		.nformats = ARRAY_SIZE(sun8i_mixer_ui_formats),
	},
};

static struct sun8i_ui *sun8i_ui_init_one(struct drm_device *drm,
					  struct sun8i_mixer *mixer,
					  const struct sun8i_plane_desc *plane)
{
	struct sun8i_ui *ui;
	int ret;

	ui = devm_kzalloc(drm->dev, sizeof(*ui), GFP_KERNEL);
	if (!ui)
		return ERR_PTR(-ENOMEM);

	/* possible crtcs are set later */
	ret = drm_universal_plane_init(drm, &ui->plane, 0,
				       &sun8i_mixer_ui_funcs,
				       plane->formats, plane->nformats,
				       NULL, plane->type, NULL);
	if (ret) {
		dev_err(drm->dev, "Couldn't initialize ui\n");
		return ERR_PTR(ret);
	}

	drm_plane_helper_add(&ui->plane,
			     &sun8i_mixer_ui_helper_funcs);
	ui->mixer = mixer;

	return ui;
}

struct drm_plane **sun8i_ui_init(struct drm_device *drm,
				 struct sunxi_engine *engine)
{
	struct drm_plane **planes;
	struct sun8i_mixer *mixer = engine_to_sun8i_mixer(engine);
	int i;

	planes = devm_kcalloc(drm->dev, ARRAY_SIZE(sun8i_mixer_planes) + 1,
			      sizeof(*planes), GFP_KERNEL);
	if (!planes)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < ARRAY_SIZE(sun8i_mixer_planes); i++) {
		const struct sun8i_plane_desc *plane = &sun8i_mixer_planes[i];
		struct sun8i_ui *ui;

		ui = sun8i_ui_init_one(drm, mixer, plane);
		if (IS_ERR(ui)) {
			dev_err(drm->dev, "Couldn't initialize %s plane\n",
				i ? "overlay" : "primary");
			return ERR_CAST(ui);
		};

		/* TODO: Support several UI channels */
		ui->chan = 0;
		ui->id = i;
		planes[i] = &ui->plane;
	};

	return planes;
}
