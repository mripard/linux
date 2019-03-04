/*
 * Copyright (c) 2016 Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * DRM core format related functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <linux/bug.h>
#include <linux/ctype.h>
#include <linux/export.h>
#include <linux/kernel.h>

#include <drm/drmP.h>
#include <drm/drm_fourcc.h>

static char printable_char(int c)
{
	return isascii(c) && isprint(c) ? c : '?';
}

/**
 * drm_mode_legacy_fb_format - compute drm fourcc code from legacy description
 * @bpp: bits per pixels
 * @depth: bit depth per pixel
 *
 * Computes a drm fourcc pixel format code for the given @bpp/@depth values.
 * Useful in fbdev emulation code, since that deals in those values.
 */
uint32_t drm_mode_legacy_fb_format(uint32_t bpp, uint32_t depth)
{
	uint32_t fmt = DRM_FORMAT_INVALID;

	switch (bpp) {
	case 8:
		if (depth == 8)
			fmt = DRM_FORMAT_C8;
		break;

	case 16:
		switch (depth) {
		case 15:
			fmt = DRM_FORMAT_XRGB1555;
			break;
		case 16:
			fmt = DRM_FORMAT_RGB565;
			break;
		default:
			break;
		}
		break;

	case 24:
		if (depth == 24)
			fmt = DRM_FORMAT_RGB888;
		break;

	case 32:
		switch (depth) {
		case 24:
			fmt = DRM_FORMAT_XRGB8888;
			break;
		case 30:
			fmt = DRM_FORMAT_XRGB2101010;
			break;
		case 32:
			fmt = DRM_FORMAT_ARGB8888;
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

	return fmt;
}
EXPORT_SYMBOL(drm_mode_legacy_fb_format);

/**
 * drm_driver_legacy_fb_format - compute drm fourcc code from legacy description
 * @dev: DRM device
 * @bpp: bits per pixels
 * @depth: bit depth per pixel
 *
 * Computes a drm fourcc pixel format code for the given @bpp/@depth values.
 * Unlike drm_mode_legacy_fb_format() this looks at the drivers mode_config,
 * and depending on the &drm_mode_config.quirk_addfb_prefer_host_byte_order flag
 * it returns little endian byte order or host byte order framebuffer formats.
 */
uint32_t drm_driver_legacy_fb_format(struct drm_device *dev,
				     uint32_t bpp, uint32_t depth)
{
	uint32_t fmt = drm_mode_legacy_fb_format(bpp, depth);

	if (dev->mode_config.quirk_addfb_prefer_host_byte_order) {
		if (fmt == DRM_FORMAT_XRGB8888)
			fmt = DRM_FORMAT_HOST_XRGB8888;
		if (fmt == DRM_FORMAT_ARGB8888)
			fmt = DRM_FORMAT_HOST_ARGB8888;
		if (fmt == DRM_FORMAT_RGB565)
			fmt = DRM_FORMAT_HOST_RGB565;
		if (fmt == DRM_FORMAT_XRGB1555)
			fmt = DRM_FORMAT_HOST_XRGB1555;
	}

	if (dev->mode_config.quirk_addfb_prefer_xbgr_30bpp &&
	    fmt == DRM_FORMAT_XRGB2101010)
		fmt = DRM_FORMAT_XBGR2101010;

	return fmt;
}
EXPORT_SYMBOL(drm_driver_legacy_fb_format);

/**
 * drm_get_format_name - fill a string with a drm fourcc format's name
 * @format: format to compute name of
 * @buf: caller-supplied buffer
 */
const char *drm_get_format_name(uint32_t format, struct drm_format_name_buf *buf)
{
	snprintf(buf->str, sizeof(buf->str),
		 "%c%c%c%c %s-endian (0x%08x)",
		 printable_char(format & 0xff),
		 printable_char((format >> 8) & 0xff),
		 printable_char((format >> 16) & 0xff),
		 printable_char((format >> 24) & 0x7f),
		 format & DRM_FORMAT_BIG_ENDIAN ? "big" : "little",
		 format);

	return buf->str;
}
EXPORT_SYMBOL(drm_get_format_name);

/**
 * drm_get_format_info - query information for a given framebuffer configuration
 * @dev: DRM device
 * @mode_cmd: metadata from the userspace fb creation request
 *
 * Returns:
 * The instance of struct image_format_info that describes the pixel format, or
 * NULL if the format is unsupported.
 */
const struct image_format_info *
drm_get_format_info(struct drm_device *dev,
		    const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct image_format_info *info = NULL;

	if (dev->mode_config.funcs->get_format_info)
		info = dev->mode_config.funcs->get_format_info(mode_cmd);

	if (!info)
		info = image_format_drm_lookup(mode_cmd->pixel_format);

	return info;
}
EXPORT_SYMBOL(drm_get_format_info);
