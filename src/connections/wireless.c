/*
 * ConnMan GTK GUI
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 * Author: Jaakko Hannikainen <jaakko.hannikainen@intel.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "style.h"
#include "technology.h"
#include "wireless.h"

struct wireless_technology {
	struct technology parent;

	GtkWidget *scan_button;
};

struct wireless_service {
	struct service parent;

	GtkWidget *ssid;
	GtkWidget *security;
	GtkWidget *signal;
};

static void wireless_scan_cb(GObject *source, GAsyncResult *res,
                             gpointer user_data)
{
	GVariant *ret;
	GError *error = NULL;
	struct wireless_technology *tech = user_data;

	ret = g_dbus_proxy_call_finish(tech->parent.settings->proxy, res, &error);
	gtk_button_set_label(GTK_BUTTON(tech->scan_button), _("_Scan"));
	gtk_widget_set_sensitive(tech->scan_button, TRUE);
	gtk_widget_set_can_focus(tech->scan_button, TRUE);
	if(error) {
		g_warning("failed to scan wifi: %s", error->message);
		g_error_free(error);
		return;
	}
	g_variant_unref(ret);
}

static void scan_button_cb(GtkButton *widget, gpointer user_data)
{
	struct wireless_technology *tech = user_data;
	gtk_button_set_label(GTK_BUTTON(tech->scan_button), _("Scanning"));
	gtk_widget_set_sensitive(tech->scan_button, FALSE);
	gtk_widget_set_can_focus(tech->scan_button, FALSE);
	g_dbus_proxy_call(tech->parent.settings->proxy, "Scan",
	                  NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
	                  wireless_scan_cb, tech);
}

struct technology *technology_wireless_create(void)
{
	struct wireless_technology *tech = g_malloc(sizeof(*tech));
	return (struct technology *)tech;
}

void technology_wireless_free(struct technology *tech)
{
	struct wireless_technology *item = (struct wireless_technology *)tech;

	g_object_unref(item->scan_button);
	g_free(item);
}

void technology_wireless_init(struct technology *tech, GVariant *properties,
                              GDBusProxy *proxy)
{
	struct wireless_technology *item = (struct wireless_technology *)tech;
	gtk_image_set_from_icon_name(GTK_IMAGE(tech->list_item->icon),
	                             "network-wireless-symbolic",
	                             GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_image_set_from_icon_name(GTK_IMAGE(tech->settings->icon),
	                             "network-wireless", GTK_ICON_SIZE_DIALOG);
	item->scan_button = gtk_button_new_with_mnemonic(_("_Scan"));
	g_object_ref(item->scan_button);
	g_signal_connect(item->scan_button, "clicked",
	                 G_CALLBACK(scan_button_cb), item);
	gtk_widget_set_valign(item->scan_button, GTK_ALIGN_START);
	gtk_grid_attach_next_to(GTK_GRID(tech->settings->buttons),
	                        item->scan_button, tech->settings->filler,
	                        GTK_POS_LEFT, 1, 1);
	gtk_widget_show_all(item->scan_button);
}

struct service *service_wireless_create(void)
{
	struct wireless_service *serv = g_malloc(sizeof(*serv));
	return (struct service *)serv;
}

void service_wireless_free(struct service *serv)
{
	struct wireless_service *item = (struct wireless_service *)serv;

	g_object_unref(item->ssid);
	g_object_unref(item->security);
	g_object_unref(item->signal);
	g_free(item);
}

void service_wireless_init(struct service *serv, GDBusProxy *proxy,
                           const gchar *path, GVariant *properties)
{
	struct wireless_service *item = (struct wireless_service *)serv;

	item->ssid = gtk_label_new(NULL);
	item->security = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);
	item->signal = gtk_image_new_from_icon_name("", GTK_ICON_SIZE_MENU);

	g_object_ref(item->ssid);
	g_object_ref(item->security);
	g_object_ref(item->signal);

	STYLE_ADD_MARGIN(item->security, MARGIN_SMALL);
	STYLE_ADD_MARGIN(item->signal, MARGIN_SMALL);

	gtk_widget_set_halign(item->ssid, GTK_ALIGN_START);
	gtk_widget_set_hexpand(item->ssid, TRUE);
	gtk_widget_set_halign(item->signal, GTK_ALIGN_END);
	gtk_widget_set_halign(item->security, GTK_ALIGN_END);
	gtk_widget_set_valign(item->signal, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(item->security, GTK_ALIGN_CENTER);

	gtk_grid_attach(GTK_GRID(serv->contents), item->ssid, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(serv->contents), item->security, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(serv->contents), item->signal, 2, 0, 1, 1);

	gtk_widget_show_all(serv->contents);

	service_wireless_update(serv);
}

void service_wireless_update(struct service *serv)
{
	struct wireless_service *item = (struct wireless_service *)serv;

	GVariant *variant;

	variant = service_get_property(serv, "Name", NULL);
	if(variant) {
		const gchar *value;
		value = g_variant_get_string(variant, NULL);
		gtk_label_set_text(GTK_LABEL(item->ssid), value);
		g_variant_unref(variant);
		gtk_widget_show(serv->item);
	} else
		gtk_widget_hide(serv->item);

	variant = service_get_property(serv, "Security", NULL);
	if(variant) {
		const gchar **value;
		const gchar **cur;
		int security = 0;
		const gchar *icon_name;
		value = g_variant_get_strv(variant, NULL);
		for(cur = value; *cur; cur++) {
			if(!strcmp("ieee8021x", *cur)) {
				security = 3;
				break;
			}
			if(!strcmp("psk", *cur))
				security = 2;
			else if(security < 2 && !strcmp("wps", *cur))
				security = 1;
		}
		g_free(value);
		icon_name = (security == 3 ? "security-high-symbolic" :
		             security == 2 ? "security-medium-symbolic" :
		             security == 1 ? "security-low-symbolic" :
		             NULL);
		gtk_image_set_from_icon_name(GTK_IMAGE(item->security),
		                             icon_name, GTK_ICON_SIZE_MENU);
		if(!security)
			gtk_widget_hide(item->security);
		else
			gtk_widget_show(item->security);
		g_variant_unref(variant);
	}


	variant = service_get_property(serv, "Strength", NULL);
	if(variant) {
		unsigned char value;
		value = g_variant_get_byte(variant);
		gtk_image_set_from_icon_name(GTK_IMAGE(item->signal),
		                             SIGNAL_TO_ICON("wireless", value),
		                             GTK_ICON_SIZE_MENU);
		g_variant_unref(variant);
	}
}