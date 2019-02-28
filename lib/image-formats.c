#include <linux/bug.h>
#include <linux/image-formats.h>
#include <linux/kernel.h>
#include <linux/math64.h>

#include <uapi/drm/drm_fourcc.h>

static const struct image_format_info formats[] = {
	{
		.drm_fmt = DRM_FORMAT_C8,
		.depth = 8,
		.num_planes = 1,
		.cpp = { 1, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGB332,
		.depth = 8,
		.num_planes = 1,
		.cpp = { 1, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGR233,
		.depth = 8,
		.num_planes = 1,
		.cpp = { 1, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_XRGB4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_XBGR4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGBX4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGRX4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_ARGB4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_ABGR4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_RGBA4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_BGRA4444,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_XRGB1555,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_XBGR1555,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGBX5551,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGRX5551,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_ARGB1555,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_ABGR1555,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_RGBA5551,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_BGRA5551,
		.depth = 15,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_RGB565,
		.depth = 16,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGR565,
		.depth = 16,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGB888,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 3, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGR888,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 3, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_XRGB8888,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_XBGR8888,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGBX8888,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGRX8888,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGB565_A8,
		.depth = 24,
		.num_planes = 2,
		.cpp = { 2, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_BGR565_A8,
		.depth = 24,
		.num_planes = 2,
		.cpp = { 2, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_XRGB2101010,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_XBGR2101010,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGBX1010102,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGRX1010102,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_ARGB2101010,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_ABGR2101010,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_RGBA1010102,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_BGRA1010102,
		.depth = 30,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_ARGB8888,
		.depth = 32,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_ABGR8888,
		.depth = 32,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_RGBA8888,
		.depth = 32,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_BGRA8888,
		.depth = 32,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_RGB888_A8,
		.depth = 32,
		.num_planes = 2,
		.cpp = { 3, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_BGR888_A8,
		.depth = 32,
		.num_planes = 2,
		.cpp = { 3, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_XRGB8888_A8,
		.depth = 32,
		.num_planes = 2,
		.cpp = { 4, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_XBGR8888_A8,
		.depth = 32,
		.num_planes = 2,
		.cpp = { 4, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_RGBX8888_A8,
		.depth = 32,
		.num_planes = 2,
		.cpp = { 4, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_BGRX8888_A8,
		.depth = 32,
		.num_planes = 2,
		.cpp = { 4, 1, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUV410,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 4,
		.vsub = 4,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU410,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 4,
		.vsub = 4,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUV411,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 4,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU411,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 4,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUV420,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU420,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUV422,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU422,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUV444,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU444,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV12,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV21,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV16,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV61,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV24,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV42,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUYV,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVYU,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_UYVY,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_VYUY,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_XYUV8888,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_AYUV,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 4, 0, 0 },
		.hsub = 1,
		.vsub = 1,
		.has_alpha = true,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_Y0L0,
		.depth = 0,
		.num_planes = 1,
		.char_per_block = { 8, 0, 0 },
		.block_w = { 2, 0, 0 },
		.block_h = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 2,
		.has_alpha = true,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_X0L0,
		.depth = 0,
		.num_planes = 1,
		.char_per_block = { 8, 0, 0 },
		.block_w = { 2, 0, 0 },
		.block_h = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_Y0L2,
		.depth = 0,
		.num_planes = 1,
		.char_per_block = { 8, 0, 0 },
		.block_w = { 2, 0, 0 },
		.block_h = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 2,
		.has_alpha = true,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_X0L2,
		.depth = 0,
		.num_planes = 1,
		.char_per_block = { 8, 0, 0 },
		.block_w = { 2, 0, 0 },
		.block_h = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	},
};

#define __image_format_lookup(_field, _fmt)			\
	({							\
		const struct image_format_info *format = NULL;	\
		unsigned i;					\
								\
		for (i = 0; i < ARRAY_SIZE(formats); i++)	\
			if (formats[i]._field == _fmt)		\
				format = &formats[i];		\
								\
		format;						\
	})

/**
 * image_format_drm_lookup - query information for a given format
 * @drm: DRM fourcc pixel format (DRM_FORMAT_*)
 *
 * The caller should only pass a supported pixel format to this function.
 * Unsupported pixel formats will generate a warning in the kernel log.
 *
 * Returns:
 * The instance of struct image_format_info that describes the pixel format, or
 * NULL if the format is unsupported.
 */
const struct image_format_info *image_format_drm_lookup(u32 drm)
{
	const struct image_format_info *format;

	format = __image_format_lookup(drm_fmt, drm);

	WARN_ON(!format);
	return format;
}
EXPORT_SYMBOL(image_format_drm_lookup);

/**
 * image_format_plane_cpp - determine the bytes per pixel value
 * @format: pointer to the image_format
 * @plane: plane index
 *
 * Returns:
 * The bytes per pixel value for the specified plane.
 */
unsigned int image_format_plane_cpp(const struct image_format_info *format,
				    int plane)
{
	if (!format || plane >= format->num_planes)
		return 0;

	return format->cpp[plane];
}
EXPORT_SYMBOL(image_format_plane_cpp);

/**
 * image_format_plane_width - width of the plane given the first plane
 * @format: pointer to the image_format
 * @width: width of the first plane
 * @plane: plane index
 *
 * Returns:
 * The width of @plane, given that the width of the first plane is @width.
 */
unsigned int image_format_plane_width(int width,
				      const struct image_format_info *format,
				      int plane)
{
	if (!format || plane >= format->num_planes)
		return 0;

	if (plane == 0)
		return width;

	return width / format->hsub;
}
EXPORT_SYMBOL(image_format_plane_width);

/**
 * image_format_plane_height - height of the plane given the first plane
 * @format: pointer to the image_format
 * @height: height of the first plane
 * @plane: plane index
 *
 * Returns:
 * The height of @plane, given that the height of the first plane is @height.
 */
unsigned int image_format_plane_height(int height,
				       const struct image_format_info *format,
				       int plane)
{
	if (!format || plane >= format->num_planes)
		return 0;

	if (plane == 0)
		return height;

	return height / format->vsub;
}
EXPORT_SYMBOL(image_format_plane_height);

/**
 * image_format_block_width - width in pixels of block.
 * @format: pointer to the image_format
 * @plane: plane index
 *
 * Returns:
 * The width in pixels of a block, depending on the plane index.
 */
unsigned int image_format_block_width(const struct image_format_info *format,
				      int plane)
{
	if (!format || plane < 0 || plane >= format->num_planes)
		return 0;

	if (!format->block_w[plane])
		return 1;

	return format->block_w[plane];
}
EXPORT_SYMBOL(image_format_block_width);

/**
 * image_format_block_height - height in pixels of a block
 * @info: pointer to the image_format
 * @plane: plane index
 *
 * Returns:
 * The height in pixels of a block, depending on the plane index.
 */
unsigned int image_format_block_height(const struct image_format_info *format,
				       int plane)
{
	if (!format || plane < 0 || plane >= format->num_planes)
		return 0;

	if (!format->block_h[plane])
		return 1;

	return format->block_h[plane];
}
EXPORT_SYMBOL(image_format_block_height);

/**
 * image_format_min_pitch - computes the minimum required pitch in bytes
 * @info: pixel format info
 * @plane: plane index
 * @buffer_width: buffer width in pixels
 *
 * Returns:
 * The minimum required pitch in bytes for a buffer by taking into consideration
 * the pixel format information and the buffer width.
 */
uint64_t image_format_min_pitch(const struct image_format_info *info,
				int plane, unsigned int buffer_width)
{
	if (!info || plane < 0 || plane >= info->num_planes)
		return 0;

	return DIV_ROUND_UP_ULL((u64)buffer_width * info->char_per_block[plane],
			    image_format_block_width(info, plane) *
			    image_format_block_height(info, plane));
}
EXPORT_SYMBOL(image_format_min_pitch);
