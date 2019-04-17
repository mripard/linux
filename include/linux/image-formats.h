/*
 * Copyright (c) 2016 Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 * Copyright (c) 2019 Maxime Ripard <maxime.ripard@bootlin.com>
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

#ifndef _IMAGE_FORMATS_H_
#define _IMAGE_FORMATS_H_

#include <linux/kernel.h>
#include <linux/types.h>

/**
 * struct image_format_info - information about a image format
 */
struct image_format_info {
	union {
		/**
		 * @drm_fmt:
		 *
		 * DRM 4CC format identifier (DRM_FORMAT_*)
		 */
		u32 drm_fmt;

		/**
		 * @format:
		 *
		 * DRM 4CC format identifier (DRM_FORMAT_*). Kept
		 * around for compatibility reasons with the current
		 * DRM drivers.
		 */
		u32 format;
	};

	/**
	 * @v4l2_fmt:
	 *
	 * V4L2 4CC format identifier (V4L2_PIX_FMT_*)
	 */
	u32 v4l2_fmt;

	/**
	 * @depth:
	 *
	 * Color depth (number of bits per pixel excluding padding bits),
	 * valid for a subset of RGB formats only. This is a legacy field, do
	 * not use in new code and set to 0 for new formats.
	 */
	u8 depth;

	/** @num_planes: Number of color planes (1 to 3) */
	u8 num_planes;

	union {
		/**
		 * @cpp:
		 *
		 * Number of bytes per pixel (per plane), this is aliased with
		 * @char_per_block. It is deprecated in favour of using the
		 * triplet @char_per_block, @block_w, @block_h for better
		 * describing the pixel format.
		 */
		u8 cpp[3];

		/**
		 * @char_per_block:
		 *
		 * Number of bytes per block (per plane), where blocks are
		 * defined as a rectangle of pixels which are stored next to
		 * each other in a byte aligned memory region. Together with
		 * @block_w and @block_h this is used to properly describe tiles
		 * in tiled formats or to describe groups of pixels in packed
		 * formats for which the memory needed for a single pixel is not
		 * byte aligned.
		 *
		 * @cpp has been kept for historical reasons because there are
		 * a lot of places in drivers where it's used. In drm core for
		 * generic code paths the preferred way is to use
		 * @char_per_block, image_format_info_block_width() and
		 * image_format_info_block_height() which allows handling both
		 * block and non-block formats in the same way.
		 *
		 * For formats that are intended to be used only with non-linear
		 * modifiers both @cpp and @char_per_block must be 0 in the
		 * generic format table. Drivers could supply accurate
		 * information from their drm_mode_config.get_format_info hook
		 * if they want the core to be validating the pitch.
		 */
		u8 char_per_block[3];
	};

	/**
	 * @block_w:
	 *
	 * Block width in pixels, this is intended to be accessed through
	 * image_format_info_block_width()
	 */
	u8 block_w[3];

	/**
	 * @block_h:
	 *
	 * Block height in pixels, this is intended to be accessed through
	 * image_format_info_block_height()
	 */
	u8 block_h[3];

	/** @hsub: Horizontal chroma subsampling factor */
	u8 hsub;
	/** @vsub: Vertical chroma subsampling factor */
	u8 vsub;

	/** @has_alpha: Does the format embeds an alpha component? */
	bool has_alpha;

	/** @is_yuv: Is it a YUV format? */
	bool is_yuv;
};

/**
 * image_format_info_is_yuv_packed - check that the format info matches a YUV
 * format with data laid in a single plane
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a packed YUV format.
 */
static inline bool
image_format_info_is_yuv_packed(const struct image_format_info *info)
{
	return info->is_yuv && info->num_planes == 1;
}

/**
 * image_format_info_is_yuv_semiplanar - check that the format info matches a YUV
 * format with data laid in two planes (luminance and chrominance)
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a semiplanar YUV format.
 */
static inline bool
image_format_info_is_yuv_semiplanar(const struct image_format_info *info)
{
	return info->is_yuv && info->num_planes == 2;
}

/**
 * image_format_info_is_yuv_planar - check that the format info matches a YUV
 * format with data laid in three planes (one for each YUV component)
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a planar YUV format.
 */
static inline bool
image_format_info_is_yuv_planar(const struct image_format_info *info)
{
	return info->is_yuv && info->num_planes == 3;
}

/**
 * image_format_info_is_yuv_sampling_410 - check that the format info matches a
 * YUV format with 4:1:0 sub-sampling
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a YUV format with 4:1:0
 * sub-sampling.
 */
static inline bool
image_format_info_is_yuv_sampling_410(const struct image_format_info *info)
{
	return info->is_yuv && info->hsub == 4 && info->vsub == 4;
}

/**
 * image_format_info_is_yuv_sampling_411 - check that the format info matches a
 * YUV format with 4:1:1 sub-sampling
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a YUV format with 4:1:1
 * sub-sampling.
 */
static inline bool
image_format_info_is_yuv_sampling_411(const struct image_format_info *info)
{
	return info->is_yuv && info->hsub == 4 && info->vsub == 1;
}

/**
 * image_format_info_is_yuv_sampling_420 - check that the format info matches a
 * YUV format with 4:2:0 sub-sampling
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a YUV format with 4:2:0
 * sub-sampling.
 */
static inline bool
image_format_info_is_yuv_sampling_420(const struct image_format_info *info)
{
	return info->is_yuv && info->hsub == 2 && info->vsub == 2;
}

/**
 * image_format_info_is_yuv_sampling_422 - check that the format info matches a
 * YUV format with 4:2:2 sub-sampling
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a YUV format with 4:2:2
 * sub-sampling.
 */
static inline bool
image_format_info_is_yuv_sampling_422(const struct image_format_info *info)
{
	return info->is_yuv && info->hsub == 2 && info->vsub == 1;
}

/**
 * image_format_info_is_yuv_sampling_444 - check that the format info matches a
 * YUV format with 4:4:4 sub-sampling
 * @info: format info
 *
 * Returns:
 * A boolean indicating whether the format info matches a YUV format with 4:4:4
 * sub-sampling.
 */
static inline bool
image_format_info_is_yuv_sampling_444(const struct image_format_info *info)
{
	return info->is_yuv && info->hsub == 1 && info->vsub == 1;
}

/**
 * image_format_info_plane_cpp - determine the bytes per pixel value
 * @format: pixel format info
 * @plane: plane index
 *
 * Returns:
 * The bytes per pixel value for the specified plane.
 */
static inline
int image_format_info_plane_cpp(const struct image_format_info *info, int plane)
{
	if (!info || plane >= info->num_planes)
		return 0;

	return info->cpp[plane];
}

/**
 * image_format_info_plane_width - width of the plane given the first plane
 * @format: pixel format info
 * @width: width of the first plane
 * @plane: plane index
 *
 * Returns:
 * The width of @plane, given that the width of the first plane is @width.
 */
static inline
int image_format_info_plane_width(const struct image_format_info *info, int width,
				  int plane)
{
	if (!info || plane >= info->num_planes)
		return 0;

	if (plane == 0)
		return width;

	return width / info->hsub;
}

/**
 * image_format_info_plane_height - height of the plane given the first plane
 * @format: pixel format info
 * @height: height of the first plane
 * @plane: plane index
 *
 * Returns:
 * The height of @plane, given that the height of the first plane is @height.
 */
static inline
int image_format_info_plane_height(const struct image_format_info *info, int height,
				   int plane)
{
	if (!info || plane >= info->num_planes)
		return 0;

	if (plane == 0)
		return height;

	return height / info->vsub;
}

/**
 * image_format_info_block_width - width in pixels of block.
 * @format: pointer to the image_format_info
 * @plane: plane index
 *
 * Returns:
 * The width in pixels of a block, depending on the plane index.
 */
static inline
unsigned int image_format_info_block_width(const struct image_format_info *format,
					   int plane)
{
	if (!format)
		return 0;

	if (plane < 0 || plane >= ARRAY_SIZE(format->block_w))
		return 0;

	if (plane >= format->num_planes)
		return 0;

	if (!format->block_w[plane])
		return 1;

	return format->block_w[plane];
}

/**
 * image_format_info_block_height - height in pixels of a block
 * @info: pointer to the image_format_info
 * @plane: plane index
 *
 * Returns:
 * The height in pixels of a block, depending on the plane index.
 */
static inline
unsigned int image_format_info_block_height(const struct image_format_info *format,
					    int plane)
{
	if (!format)
		return 0;

	if (plane < 0 || plane >= ARRAY_SIZE(format->block_w))
		return 0;

	if (plane >= format->num_planes)
		return 0;

	if (!format->block_h[plane])
		return 1;

	return format->block_h[plane];
}

/**
 * image_format_info_min_pitch - computes the minimum required pitch in bytes
 * @info: pixel format info
 * @plane: plane index
 * @buffer_width: buffer width in pixels
 *
 * Returns:
 * The minimum required pitch in bytes for a buffer by taking into consideration
 * the pixel format information and the buffer width.
 */
static inline
uint64_t image_format_info_min_pitch(const struct image_format_info *info,
				     int plane, unsigned int buffer_width)
{
	if (!info || plane < 0 || plane >= info->num_planes)
		return 0;

	return DIV_ROUND_UP_ULL((u64)buffer_width * info->char_per_block[plane],
			    image_format_info_block_width(info, plane) *
			    image_format_info_block_height(info, plane));
}

/**
 * image_format_info_plane_size - determine the size value
 * @format: pointer to the image_format_info
 * @width: plane width
 * @height: plane width
 * @plane: plane index
 *
 * Returns:
 * The size of the plane buffer.
 */
static inline
unsigned int image_format_info_plane_size(const struct image_format_info *info,
					  unsigned int width, unsigned int height,
					  int plane)
{
	if (!info || plane >= info->num_planes)
		return 0;

	return image_format_info_plane_stride(info, width, plane) *
		image_format_info_plane_height(info, height, plane);
}

const struct image_format_info *__image_format_drm_lookup(u32 drm);
const struct image_format_info *__image_format_v4l2_lookup(u32 v4l2);
const struct image_format_info *image_format_drm_lookup(u32 drm);
const struct image_format_info *image_format_v4l2_lookup(u32 v4l2);

#endif /* _IMAGE_FORMATS_H_ */
