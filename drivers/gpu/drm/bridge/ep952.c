// SPDX-License-Identifier: GPL-2.0+
// Copyright 2018 Maxime Ripard <maxime.ripard@bootlin.com>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>

#include <linux/gpio/consumer.h>

#define EP952_CTL1_REG		0x08
#define EP952_CTL1_EDGE			BIT(1)

#define EP952_CS_CTL_REG	0x0c
#define EP952_CS_CTL_VMUTE		BIT(3)

#define EP952_CTL4_REG		0x0e
#define EP952_CTL4_HDMI			BIT(0)

#define EP952_IIS_CTL_REG	0x3f
#define EP952_IIS_CTL_AVI_EN		BIT(6)

struct ep952 {
	struct drm_bridge	bridge;
	struct drm_connector	connector;
	struct i2c_client	*client;
	struct i2c_adapter	*ddc;
	bool			hdmi_mode;
	struct gpio_desc	*reset;
};

static inline struct ep952 *bridge_to_ep952(struct drm_bridge *bridge)
{
	return container_of(bridge, struct ep952, bridge);
}

static inline struct ep952 *connector_to_ep952(struct drm_connector *connector)
{
	return container_of(connector, struct ep952, connector);
}

static u8 ep952_read_reg(struct ep952 *ep952, u8 reg)
{
	return i2c_smbus_read_byte_data(ep952->client, reg);
}

static void ep952_write_reg(struct ep952 *ep952, u8 reg, u8 val)
{
	i2c_smbus_write_byte_data(ep952->client, reg, val);
}

static void ep952_clr_bit(struct ep952 *ep952, u8 reg, u8 bit)
{
	u8 val = ep952_read_reg(ep952, reg);
	val &= ~bit;
	ep952_write_reg(ep952, reg, val);
}

static void ep952_set_bit(struct ep952 *ep952, u8 reg, u8 bit)
{
	u8 val = ep952_read_reg(ep952, reg);
	val |= bit;
	ep952_write_reg(ep952, reg, val);
}

static int ep952_get_modes(struct drm_connector *connector)
{
	struct ep952 *ep952 = connector_to_ep952(connector);
	struct edid *edid;
	int count;

	edid = drm_get_edid(connector, ep952->ddc);
	if (!edid) {
		DRM_ERROR("EDID readout failed\n");
		return 0;
	}

	drm_mode_connector_update_edid_property(connector, edid);
	count = drm_add_edid_modes(connector, edid);

	ep952->hdmi_mode = drm_detect_hdmi_monitor(edid);

	kfree(edid);
	return count;
}

static const struct drm_connector_helper_funcs ep952_connector_helper_funcs = {
	.get_modes	= ep952_get_modes,
};

static enum drm_connector_status ep952_detect(struct drm_connector *connector,
					      bool force)
{
	return connector_status_disconnected;
}

static const struct drm_connector_funcs ep952_connector_funcs = {
	.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
	.detect			= ep952_detect,
	.destroy		= drm_connector_cleanup,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.reset			= drm_atomic_helper_connector_reset,
};

static void ep952_hw_reset(struct ep952 *ep952)
{
	if (!ep952->reset)
		return;

	gpiod_set_value(ep952->reset, 0);
	msleep(10);
	gpiod_set_value(ep952->reset, 1);
	msleep(10);
}

static void ep952_write_infoframes(struct ep952 *ep952,
				   const struct drm_display_mode *mode)
{
	struct hdmi_avi_infoframe frame;
	u8 avi_buf[HDMI_INFOFRAME_SIZE(AVI)];
	int i, ret;

	ret = drm_hdmi_avi_infoframe_from_display_mode(&frame, mode, false);
	if (ret < 0) {
		DRM_ERROR("Couldn't fill AVI info frames\n");
		return;
	}

	ret = hdmi_avi_infoframe_pack(&frame, avi_buf, sizeof(avi_buf));
	if (ret < 0) {
		DRM_ERROR("Couldn't pack AVI info frames\n");
		return;
	}

	for (i = HDMI_INFOFRAME_HEADER_SIZE; i < HDMI_INFOFRAME_SIZE(AVI); i++)
		ep952_write_reg(ep952,
				EP952_AVI_REG(i - HDMI_INFOFRAME_HEADER_SIZE),
				avi_buf[i]);
}

static void ep952_enable(struct drm_bridge *bridge)
{
	struct ep952 *ep952 = bridge_to_ep952(bridge);

	ep952_hw_reset(ep952);

	ep952_set_bit(ep952, EP952_TXPHY_CTL0_REG, EP952_TXPHY_CTL0_TERM_EN);
	ep952_set_bit(ep952, EP952_CS_CTL_REG, EP952_CS_CTL_VMUTE);

	if (ep952->hdmi_mode) {
		ep952_write_infoframes(ep952);
		ep952_write_reg(ep952, EP952_CTL4_REG, EP952_CTL4_HDMI);
	}

	if (display->flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
		ep952_set_bit(ep952, EP952_CTL1_REG, EP952_CTL1_EDGE);
	else
		ep952_clr_bit(ep952, EP952_CTL1_REG, EP952_CTL1_EDGE);

	ep952_set_bit(ep952, EP952_IIS_CTL_REG, EP952_IIS_CTL_AVI_EN);
	ep952_clr_bit(ep952, EP952_CS_CTL_REG, EP952_CS_CTL_VMUTE);
}

static void ep952_disable(struct drm_bridge *bridge)
{
	struct ep952 *ep952 = bridge_to_ep952(bridge);

	ep952_set_bit(ep952, EP952_CS_CTL_REG, EP952_CS_CTL_VMUTE);
}

static int ep952_attach(struct drm_bridge *bridge)
{
	struct ep952 *ep952 = bridge_to_ep952(bridge);
	struct drm_connector *connector = &ep952->connector;
	struct drm_device *drm = bridge->dev;
	int ret;

	drm_connector_helper_add(connector, &ep952_connector_helper_funcs);
	ret = drm_connector_init(drm, connector, &ep952_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret)
		return ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	drm_mode_connector_attach_encoder(connector, bridge->encoder);

	return 0;
}

static const struct drm_bridge_funcs ep952_bridge_funcs = {
	.attach		= ep952_attach,
	.disable	= ep952_disable,
	.enable		= ep952_enable,
};

static struct i2c_adapter *ep952_retrieve_ddc(struct device *dev)
{
	struct device_node *phandle, *remote;
	struct i2c_adapter *ddc;

	remote = of_graph_get_remote_node(dev->of_node, 1, -1);
	if (!remote)
		return ERR_PTR(-EINVAL);

	phandle = of_parse_phandle(remote, "ddc-i2c-bus", 0);
	of_node_put(remote);
	if (!phandle)
		return ERR_PTR(-ENODEV);

	ddc = of_get_i2c_adapter_by_node(phandle);
	of_node_put(phandle);
	if (!ddc)
		return ERR_PTR(-EPROBE_DEFER);

	return ddc;
}

static int ep952_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct ep952 *ep952;

	ep952 = devm_kzalloc(&client->dev, sizeof(*ep952), GFP_KERNEL);
	if (!ep952)
		return -ENOMEM;

	ep952->reset = gpiod_get_optional(&client->dev, "reset",
					  GPIOD_OUT_HIGH);
	if (IS_ERR(ep952->reset)) {
		dev_err(&client->dev, "Couldn't retrieve our reset GPIO\n");
		return PTR_ERR(ep952->reset);
	}

	ep952->ddc = ep952_retrieve_ddc(&client->dev);
	if (IS_ERR(ep952->ddc)) {
		dev_err(&client->dev, "Couldn't retrieve i2c bus\n");
		return PTR_ERR(ep952->ddc);
	}

	ep952->bridge.funcs = &ep952_bridge_funcs;
	ep952->bridge.of_node = client->dev.of_node;
	drm_bridge_add(&ep952->bridge);

	i2c_set_clientdata(client, ep952);

	return 0;
}

static int ep952_remove(struct i2c_client *client)
{
	struct ep952 *ep952 = i2c_get_clientdata(client);

	drm_bridge_remove(&ep952->bridge);

	return 0;
}

static const struct of_device_id ep952_dt_ids[] = {
	{ .compatible = "explore,ep952", },
	{ },
};
MODULE_DEVICE_TABLE(of, ep952_dt_ids);

static const struct i2c_device_id ep952_i2c_ids[] = {
	{ "ep952", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ep952_i2c_ids);

static struct i2c_driver ep952_driver = {
	.driver	= {
		.name		= "ep952",
		.of_match_table	= ep952_dt_ids,
	},
	.id_table	= ep952_i2c_ids,
	.probe		= ep952_probe,
	.remove		= ep952_remove,
};
module_i2c_driver(ep952_driver);

MODULE_AUTHOR("Maxime Ripard <maxime.ripard@bootlin.com>");
MODULE_DESCRIPTION("Explore Semiconductor EP952 RGB to HDMI bridge driver");
MODULE_LICENSE("GPL");
