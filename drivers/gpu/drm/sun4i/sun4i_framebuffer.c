/*
 * Copyright (C) 2015 Free Electrons
 * Copyright (C) 2015 NextThing Co
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drmP.h>

#include "sun4i_drv.h"
#include "sun4i_backend.h"
#include "sun4i_framebuffer.h"
#include "sun4i_layer.h"

static void sun4i_de_output_poll_changed(struct drm_device *drm)
{
	struct sun4i_drv *drv = drm->dev_private;

	drm_fbdev_cma_hotplug_event(drv->fbdev);
}

static int sun4i_de_atomic_check(struct drm_device *drm,
				 struct drm_atomic_state *state)
{
	struct drm_plane_state *plane_state;
	struct drm_plane *plane_array[SUN4I_BACKEND_NUM_LAYERS];
	struct drm_plane *plane;
	unsigned int num_alpha_planes = 0;
	unsigned int num_yuv_planes = 0;
	unsigned int num_planes = 0;
	unsigned int current_pipe = 0;
	int i = 0;
	int ret;

	ret = drm_atomic_helper_check(drm, state);
	if (ret)
		return ret;

	DRM_DEBUG_DRIVER("Starting checking our planes\n");

	drm_for_each_plane(plane, drm) {
		struct drm_plane_state *plane_state;
		struct drm_framebuffer *fb;
		struct drm_format_name_buf format_name;

		DRM_DEBUG_DRIVER("Testing plane %d in pending state\n", i++);

		plane_state = drm_atomic_get_plane_state(state, plane);
		fb = plane_state->fb;

		if (!fb) {
			DRM_DEBUG_DRIVER("Plane has no FB.. skipping\n");
			continue;
		}

		DRM_DEBUG_DRIVER("Plane FB format is %s\n",
				 drm_get_format_name(fb->format->format,
						     &format_name));

		if (sun4i_backend_format_has_alpha(fb->format->format))
			num_alpha_planes++;

		if (sun4i_backend_format_is_yuv(fb->format->format)) {
			DRM_DEBUG_DRIVER("Plane FB format is YUV\n");
			num_yuv_planes++;
		}

		DRM_DEBUG_DRIVER("Plane zpos is %d\n",
				 plane_state->normalized_zpos);

		/* Sort our planes by Zpos */
                plane_array[plane_state->normalized_zpos] = plane;

		num_planes++;
	}

	/* We can only have a single YUV plane at a time */
	if (num_yuv_planes > SUN4I_BACKEND_NUM_YUV_PLANES) {
		DRM_DEBUG_DRIVER("Too many planes with YUV, rejecting...\n");
		return -EINVAL;
	}

	/*
	 * The hardware is a bit unusual here.
	 *
	 * Even though it supports 4 layers, it does the composition
	 * in two separate steps.
	 *
	 * The first one is assigning a layer to one of its two
	 * pipes. If more that 1 layer is assigned to the same pipe,
	 * and if pixels overlaps, the pipe will take the pixel from
	 * the layer with the highest priority.
	 *
	 * The second step is the actual alpha blending, that takes
	 * the two pipes as input, and uses the eventual alpha
	 * component to do the transparency between the two.
	 *
	 * This two steps scenario makes us unable to guarantee a
	 * robust alpha blending between the 4 layers in all
	 * situations, since this means that we need to have one layer
	 * with alpha at the lowest position of our two pipes.
	 *
	 * However, we cannot even do that, since the hardware has a
	 * bug where the lowest plane of the lowest pipe (pipe 0,
	 * priority 0), if it has any alpha, will discard the pixel
	 * entirely and just display the pixels in the background
	 * color (black by default).
	 *
	 * Since means that we effectively have only three valid
	 * configurations with alpha, all of them with the alpha being
	 * on pipe1 with the lowest position, which can be 1, 2 or 3
	 * depending on the number of planes and their zpos.
	 */
	if (num_alpha_planes > SUN4I_BACKEND_NUM_ALPHA_LAYERS) {
		DRM_DEBUG_DRIVER("Too many planes with alpha, rejecting...\n");
		return -EINVAL;
	}

	/* We can't have an alpha plane at the lowest position */
	if (sun4i_backend_format_has_alpha(plane_array[0]->state->fb->format->format))
		return -EINVAL;

	for (i = 1; i < num_planes; i++) {
		struct drm_plane *plane = plane_array[i];
		struct drm_plane_state *p_state = plane->state;
		struct drm_framebuffer *fb = p_state->fb;
		struct sun4i_layer_state *s_state = state_to_sun4i_layer_state(p_state);

		/*
		 * The only alpha position is the lowest plane of the
		 * second pipe.
		 */
		if (sun4i_backend_format_has_alpha(fb->format->format))
			current_pipe++;

		s_state->pipe = current_pipe;
	}

	DRM_DEBUG_DRIVER("State valid with %u planes, %u alpha\n",
			 num_planes, num_alpha_planes);

	return 0;
}

static const struct drm_mode_config_funcs sun4i_de_mode_config_funcs = {
	.output_poll_changed	= sun4i_de_output_poll_changed,
	.atomic_check		= sun4i_de_atomic_check,
	.atomic_commit		= drm_atomic_helper_commit,
	.fb_create		= drm_fb_cma_create,
};

struct drm_fbdev_cma *sun4i_framebuffer_init(struct drm_device *drm)
{
	drm_mode_config_reset(drm);

	drm->mode_config.max_width = 8192;
	drm->mode_config.max_height = 8192;

	drm->mode_config.funcs = &sun4i_de_mode_config_funcs;

	return drm_fbdev_cma_init(drm, 32, drm->mode_config.num_connector);
}

void sun4i_framebuffer_free(struct drm_device *drm)
{
	struct sun4i_drv *drv = drm->dev_private;

	drm_fbdev_cma_fini(drv->fbdev);
}
