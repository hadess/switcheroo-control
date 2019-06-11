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

static gboolean
parse_kernel_cmdline (gboolean *force_igpu)
{
	gboolean ret = TRUE;
	GRegex *regex;
	GMatchInfo *match;
	char *contents;
	char *word;
	const char *arg;

	if (!g_file_get_contents ("/proc/cmdline", &contents, NULL, NULL))
		return FALSE;

	regex = g_regex_new ("xdg.force_integrated=(\\S+)", 0, G_REGEX_MATCH_NOTEMPTY, NULL);
	if (!g_regex_match (regex, contents, G_REGEX_MATCH_NOTEMPTY, &match)) {
		ret = FALSE;
		goto out;
	}

	word = g_match_info_fetch (match, 0);
	g_debug ("Found command-line match '%s'", word);
	arg = word + strlen ("xdg.force_integrated=");
	if (*arg == '0' || *arg == '1') {
		*force_igpu = atoi (arg);
	} else if (g_ascii_strcasecmp (arg, "true") == 0 ||
		   g_ascii_strcasecmp (arg, "on") == 0) {
		*force_igpu = TRUE;
	} else if (g_ascii_strcasecmp (arg, "false") == 0 ||
		   g_ascii_strcasecmp (arg, "off") == 0) {
		*force_igpu = FALSE;
	} else {
		g_warning ("Invalid value '%s' for xdg.force_integrated passed in kernel command line.\n", arg);
		ret = FALSE;
	}

	g_free (word);

out:
	g_match_info_free (match);
	g_regex_unref (regex);
	g_free (contents);

	if (ret)
		g_debug ("Kernel command-line parsed to %d", *force_igpu);
	else
		g_debug ("Could not parse kernel command-line");

	return ret;
}

static void
force_integrate_card (int fd)
{
	int ret;
	gboolean force_igpu = FALSE;

	if (!parse_kernel_cmdline (&force_igpu))
		force_igpu = TRUE;
	if (!force_igpu)
		return;

	g_debug ("Forcing the integrated card as the default");

	ret = write (fd, FORCE_INTEGRATED_CMD, FORCE_INTEGRATED_CMD_LEN);
	if (ret < 0) {
		g_warning ("could not force the integrated card on: %s",
			   g_strerror (errno));
	}
}

static gboolean
selinux_enforcing (void)
{
	GError *error = NULL;
	char *out;
	gboolean ret = FALSE;

	if (!g_spawn_command_line_sync ("getenforce", &out, NULL, NULL, &error)) {
		g_debug ("Could not execute 'getenforce': %s", error->message);
		g_clear_error (&error);
		return ret;
	}

	ret = (g_strcmp0 (out, "Enforcing") == 0);
	g_debug ("getenforce status '%s' means SELinux is %s",
		 out, ret ? "enforcing" : "not enforcing");
	g_free (out);

	return ret;
}

int main (int argc, char **argv)
{
	ControlData *data;
	int fd;

	/* g_setenv ("G_MESSAGES_DEBUG", "all", TRUE); */

	if (selinux_enforcing ())
		return 0;

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
	force_integrate_card (fd);
	close (fd);

	data = g_new0 (ControlData, 1);
	data->available = TRUE;
	setup_dbus (data);
	data->init_done = TRUE;
	if (data->connection)
		send_dbus_event (data);

	data->loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (data->loop);

	free_control_data (data);

	return 0;
}
