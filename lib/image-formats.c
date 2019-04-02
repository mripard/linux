#include <linux/bug.h>
#include <linux/image-formats.h>
#include <linux/kernel.h>
#include <linux/math64.h>

#include <uapi/drm/drm_fourcc.h>

static const struct image_format_info formats[] = {
	{
		.drm_fmt = DRM_FORMAT_C8,
		.v4l2_fmt = V4L2_PIX_FMT_GREY,
		.depth = 8,
		.num_planes = 1,
		.cpp = { 1, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_RGB332,
		.v4l2_fmt = V4L2_PIX_FMT_RGB332,
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
		.v4l2_fmt = V4L2_PIX_FMT_RGB565X,
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
		.v4l2_fmt = V4L2_PIX_FMT_BGR24,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 3, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_BGR888,
		.v4l2_fmt = V4L2_PIX_FMT_RGB24,
		.depth = 24,
		.num_planes = 1,
		.cpp = { 3, 0, 0 },
		.hsub = 1,
		.vsub = 1,
	}, {
		.drm_fmt = DRM_FORMAT_XRGB8888,
		.v4l2_fmt = V4L2_PIX_FMT_XBGR32,
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
		.v4l2_fmt = V4L2_PIX_FMT_ABGR32,
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
		.v4l2_fmt = V4L2_PIX_FMT_YUV410,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 4,
		.vsub = 4,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU410,
		.v4l2_fmt = V4L2_PIX_FMT_YVU410,
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
		.v4l2_fmt = V4L2_PIX_FMT_YUV420,
		.v4l2_mp_fmt = V4L2_PIX_FMT_YUV420M,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU420,
		.v4l2_fmt = V4L2_PIX_FMT_YVU420,
		.v4l2_mp_fmt = V4L2_PIX_FMT_YVU420M,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUV422,
		.v4l2_fmt = V4L2_PIX_FMT_YUV422,
		.v4l2_mp_fmt = V4L2_PIX_FMT_YUV422M,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU422,
		.v4l2_fmt = V4L2_PIX_FMT_YVU422,
		.v4l2_mp_fmt = V4L2_PIX_FMT_YVU422M,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUV444,
		.v4l2_fmt = V4L2_PIX_FMT_YUV444,
		.v4l2_mp_fmt = V4L2_PIX_FMT_YUV444M,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVU444,
		.v4l2_fmt = V4L2_PIX_FMT_YVU444,
		.v4l2_mp_fmt = V4L2_PIX_FMT_YVU444M,
		.depth = 0,
		.num_planes = 3,
		.cpp = { 1, 1, 1 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV12,
		.v4l2_fmt = V4L2_PIX_FMT_NV12,
		.v4l2_mp_fmt = V4L2_PIX_FMT_NV12M,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV21,
		.v4l2_fmt = V4L2_PIX_FMT_NV21,
		.v4l2_mp_fmt = V4L2_PIX_FMT_NV21M,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 2,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV16,
		.v4l2_fmt = V4L2_PIX_FMT_NV16,
		.v4l2_mp_fmt = V4L2_PIX_FMT_NV16M,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV61,
		.v4l2_fmt = V4L2_PIX_FMT_NV61,
		.v4l2_mp_fmt = V4L2_PIX_FMT_NV61M,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV24,
		.v4l2_fmt = V4L2_PIX_FMT_NV24,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_NV42,
		.v4l2_fmt = V4L2_PIX_FMT_NV42,
		.depth = 0,
		.num_planes = 2,
		.cpp = { 1, 2, 0 },
		.hsub = 1,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YUYV,
		.v4l2_fmt = V4L2_PIX_FMT_YUYV,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_YVYU,
		.v4l2_fmt = V4L2_PIX_FMT_YVYU,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_UYVY,
		.v4l2_fmt = V4L2_PIX_FMT_UYVY,
		.depth = 0,
		.num_planes = 1,
		.cpp = { 2, 0, 0 },
		.hsub = 2,
		.vsub = 1,
		.is_yuv = true,
	}, {
		.drm_fmt = DRM_FORMAT_VYUY,
		.v4l2_fmt = V4L2_PIX_FMT_VYUY,
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
 * __image_format_drm_lookup - query information for a given format
 * @drm: DRM fourcc pixel format (DRM_FORMAT_*)
 *
 * The caller should only pass a supported pixel format to this function.
 *
 * Returns:
 * The instance of struct image_format_info that describes the pixel format, or
 * NULL if the format is unsupported.
 */
const struct image_format_info *__image_format_drm_lookup(u32 drm)
{
	return __image_format_lookup(drm_fmt, drm);
}
EXPORT_SYMBOL(__image_format_drm_lookup);

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

	format = __image_format_drm_lookup(drm);

	WARN_ON(!format);
	return format;
}
EXPORT_SYMBOL(image_format_drm_lookup);

/**
 * __image_format_v4l2_lookup - query information for a given format
 * @v4l2: V4L2 fourcc pixel format (V4L2_PIX_FMT_*)
 *
 * The caller should only pass a supported pixel format to this function.
 *
 * Returns:
 * The instance of struct image_format_info that describes the pixel format, or
 * NULL if the format is unsupported.
 */
const struct image_format_info *__image_format_v4l2_lookup(u32 v4l2)
{
	return __image_format_lookup(v4l2_fmt, v4l2);
}
EXPORT_SYMBOL(__image_format_v4l2_lookup);

/**
 * image_format_v4l2_lookup - query information for a given format
 * @v4l2: V4L2 fourcc pixel format (V4L2_PIX_FMT_*)
 *
 * The caller should only pass a supported pixel format to this function.
 * Unsupported pixel formats will generate a warning in the kernel log.
 *
 * Returns:
 * The instance of struct image_format_info that describes the pixel format, or
 * NULL if the format is unsupported.
 */
const struct image_format_info *image_format_v4l2_lookup(u32 v4l2)
{
	const struct image_format_info *format;

	format = __image_format_v4l2_lookup(v4l2);

	WARN_ON(!format);
	return format;
}
EXPORT_SYMBOL(image_format_v4l2_lookup);
