/*
 * Copyright (c) 2016 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published by
 * the Free Software Foundation.
 *
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gio/gio.h>

#include "switcheroo-control-resources.h"

#define CONTROL_PROXY_DBUS_NAME          "net.hadess.SwitcherooControl"
#define CONTROL_PROXY_DBUS_PATH          "/net/hadess/SwitcherooControl"
#define CONTROL_PROXY_IFACE_NAME         CONTROL_PROXY_DBUS_NAME

#define SWITCHEROO_SYSFS_PATH            "/sys/kernel/debug/vgaswitcheroo/switch"

#define FORCE_INTEGRATED_CMD             "DIGD"
#define FORCE_INTEGRATED_CMD_LEN         (strlen(FORCE_INTEGRATED_CMD))

typedef struct {
	GMainLoop *loop;
	GDBusNodeInfo *introspection_data;
	GDBusConnection *connection;
	guint name_id;
	gboolean init_done;

	/* Whether switcheroo is available */
	gboolean available;
} ControlData;

static void
free_control_data (ControlData *data)
{
	if (data == NULL)
		return;

	if (data->name_id != 0) {
		g_bus_unown_name (data->name_id);
		data->name_id = 0;
	}

	g_clear_pointer (&data->introspection_data, g_dbus_node_info_unref);
	g_clear_object (&data->connection);
	g_clear_pointer (&data->loop, g_main_loop_unref);
	g_free (data);
}


static void
send_dbus_event (ControlData     *data)
{
	GVariantBuilder props_builder;
	GVariant *props_changed = NULL;

	g_assert (data->connection);

	g_variant_builder_init (&props_builder, G_VARIANT_TYPE ("a{sv}"));

	g_variant_builder_add (&props_builder, "{sv}", "HasDualGpu",
			       g_variant_new_boolean (data->available));

	props_changed = g_variant_new ("(s@a{sv}@as)", CONTROL_PROXY_IFACE_NAME,
				       g_variant_builder_end (&props_builder),
				       g_variant_new_strv (NULL, 0));

	g_dbus_connection_emit_signal (data->connection,
				       NULL,
				       CONTROL_PROXY_DBUS_PATH,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       props_changed, NULL);
}

static GVariant *
handle_get_property (GDBusConnection *connection,
		     const gchar     *sender,
		     const gchar     *object_path,
		     const gchar     *interface_name,
		     const gchar     *property_name,
		     GError         **error,
		     gpointer         user_data)
{
	ControlData *data = user_data;

	g_assert (data->connection);

	if (g_strcmp0 (property_name, "HasDualGpu") == 0)
		return g_variant_new_boolean (data->available);

	return NULL;
}

static const GDBusInterfaceVTable interface_vtable =
{
	NULL,
	handle_get_property,
	NULL
};

static void
name_lost_handler (GDBusConnection *connection,
		   const gchar     *name,
		   gpointer         user_data)
{
	g_debug ("switcheroo-control is already running, or it cannot own its D-Bus name. Verify installation.");
	exit (0);
}

static void
bus_acquired_handler (GDBusConnection *connection,
		      const gchar     *name,
		      gpointer         user_data)
{
	ControlData *data = user_data;

	g_dbus_connection_register_object (connection,
					   CONTROL_PROXY_DBUS_PATH,
					   data->introspection_data->interfaces[0],
					   &interface_vtable,
					   data,
					   NULL,
					   NULL);

	data->connection = g_object_ref (connection);
}

static void
name_acquired_handler (GDBusConnection *connection,
		       const gchar     *name,
		       gpointer         user_data)
{
	ControlData *data = user_data;

	if (data->init_done)
		send_dbus_event (data);
}

static gboolean
setup_dbus (ControlData *data)
{
	GBytes *bytes;

	bytes = g_resources_lookup_data ("/net/hadess/SwitcherooControl/net.hadess.SwitcherooControl.xml",
					 G_RESOURCE_LOOKUP_FLAGS_NONE,
					 NULL);
	data->introspection_data = g_dbus_node_info_new_for_xml (g_bytes_get_data (bytes, NULL), NULL);
	g_bytes_unref (bytes);
	g_assert (data->introspection_data != NULL);

	data->name_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
					CONTROL_PROXY_DBUS_NAME,
					G_BUS_NAME_OWNER_FLAGS_NONE,
					bus_acquired_handler,
					name_acquired_handler,
					name_lost_handler,
					data,
					NULL);

	return TRUE;
}

int main (int argc, char **argv)
{
	ControlData *data;
	int fd, ret;

	/* g_setenv ("G_MESSAGES_DEBUG", "all", TRUE); */

	/* Check for VGA switcheroo availability */
	fd = open (SWITCHEROO_SYSFS_PATH, O_WRONLY);
	if (fd < 0) {
		int err = errno;

		switch (err) {
		case EACCES:
			g_warning ("switcheroo-control needs to run as root");
			break;
		case ENOENT:
			g_debug ("No switcheroo support available");
			/* not an error */
			return 0;
		default:
			g_warning ("switcheroo-control could not query vga_switcheroo status: %s",
				   g_strerror (err));
		}
		return 1;
	}

	/* And force the integrated card to be the default card */
	ret = write (fd, FORCE_INTEGRATED_CMD, FORCE_INTEGRATED_CMD_LEN);
	if (ret < 0) {
		g_warning ("could not force the integrated card on: %s",
			   g_strerror (errno));
	}
	close (fd);

	data = g_new0 (ControlData, 1);
	data->available = (ret == FORCE_INTEGRATED_CMD_LEN);
	setup_dbus (data);
	data->init_done = TRUE;
	if (data->connection)
		send_dbus_event (data);

	data->loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (data->loop);

	free_control_data (data);

	return 0;
}
