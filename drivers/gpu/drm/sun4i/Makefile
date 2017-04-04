sun4i-drm-y += sun4i_drv.o
sun4i-drm-y += sun4i_framebuffer.o

sun4i-tcon-y += sun4i_tcon.o
sun4i-tcon-y += sun4i_rgb.o
sun4i-tcon-y += sun4i_dotclock.o
sun4i-tcon-y += sun4i_crtc.o
sun4i-tcon-y += sun4i_layer.o

obj-$(CONFIG_DRM_SUN4I)		+= sun4i-drm.o sun4i-tcon.o
obj-$(CONFIG_DRM_SUN4I)		+= sun4i_backend.o
obj-$(CONFIG_DRM_SUN4I)		+= sun6i_drc.o
obj-$(CONFIG_DRM_SUN4I)		+= sun4i_tv.o

sun4i-drm-hdmi-y += sun4i_hdmi_enc.o
sun4i-drm-hdmi-y += sun4i_hdmi_ddc_clk.o
sun4i-drm-hdmi-y += sun4i_hdmi_tmds_clk.o

obj-$(CONFIG_DRM_SUN4I_HDMI)	+= sun4i-drm-hdmi.o
