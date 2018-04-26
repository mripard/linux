// SPDX-License-Identifier: GPL-2.0+
// Copyright 2018 Maxime Ripard <maxime.ripard@bootlin.com>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc_helper.h>

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

struct ep952 {
	struct drm_bridge	bridge;
	struct drm_connector	connector;
};

static inline struct ep952 *bridge_to_ep952(struct drm_bridge *bridge)
{
	return container_of(bridge, struct ep952, bridge);
}

static inline struct ep952 *connector_to_ep952(struct drm_connector *connector)
{
	return container_of(connector, struct ep952, connector);
}

static int ep952_get_modes(struct drm_connector *connector)
{
	return 0;
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

static void ep952_enable(struct drm_bridge *bridge)
{
}

static void ep952_disable(struct drm_bridge *bridge)
{
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

static int ep952_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct ep952 *ep952;

	ep952 = devm_kzalloc(&client->dev, sizeof(*ep952), GFP_KERNEL);
	if (!ep952)
		return -ENOMEM;

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
