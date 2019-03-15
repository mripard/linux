#ifndef _IMAGE_FORMATS_H_
#define _IMAGE_FORMATS_H_

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
		 * @char_per_block, image_format_block_width() and
		 * image_format_block_height() which allows handling both
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
	 * image_format_block_width()
	 */
	u8 block_w[3];

	/**
	 * @block_h:
	 *
	 * Block height in pixels, this is intended to be accessed through
	 * image_format_block_height()
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

const struct image_format_info *__image_format_drm_lookup(u32 drm);
const struct image_format_info *image_format_drm_lookup(u32 drm);
const struct image_format_info *__image_format_v4l2_lookup(u32 v4l2);
const struct image_format_info *image_format_v4l2_lookup(u32 v4l2);
unsigned int image_format_plane_cpp(const struct image_format_info *format,
				    int plane);
unsigned int image_format_plane_width(int width,
				      const struct image_format_info *format,
				      int plane);
unsigned int image_format_plane_height(int height,
				       const struct image_format_info *format,
				       int plane);
unsigned int image_format_block_width(const struct image_format_info *format,
				      int plane);
unsigned int image_format_block_height(const struct image_format_info *format,
				       int plane);
uint64_t image_format_min_pitch(const struct image_format_info *info,
				int plane, unsigned int buffer_width);

#endif /* _IMAGE_FORMATS_H_ */
