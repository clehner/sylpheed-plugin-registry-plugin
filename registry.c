/*
 * Sylpheed Plugin Registry Plugin
 * Copyright (C) 2015 Charles Lehner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "sylmain.h"
#include "plugin.h"

static SylPluginInfo info = {
        "Sylpheed Plugin Registry Plugin",
        "0.0.1",
        "Charles Lehner",
        "Registry plug-in for Sylpheed plug-in system"
};

static struct {
       const gchar *versions, *site, *plugins;
} url = {
       .versions = "http://localhost/sylpheed-plugin-registry/plugin_version.txt",
       .site     = "http://localhost/sylpheed-plugin-registry/plugins.html",
       .plugins  = "http://localhost/sylpheed-plugin-registry/plugins.ini",
};

static struct {
       GtkWidget *window;
       GtkWidget *original_child;
       GtkWidget *notebook;
} plugin_manager = {0};

static void init_done_cb(GObject *obj, gpointer data);
static void plugin_manager_open_cb(GObject *obj, GtkWidget *window,
              gpointer data);
static void plugin_manager_foreach_cb(GtkWidget *widget, gpointer data);

static void wrap_plugin_manager_window(void);
static void unwrap_plugin_manager_window(void);
static GtkWidget *registry_page_create(void);

void plugin_load(void)
{
       GList *list, *cur;
       const gchar *ver;
       gpointer mainwin;

       g_print("registry plug-in loaded!\n");

       g_signal_connect(syl_app_get(), "init-done", G_CALLBACK(init_done_cb),
                        NULL);
       syl_plugin_signal_connect("plugin-manager-open",
                                 G_CALLBACK(plugin_manager_open_cb), NULL);

       g_print("registry plug-in loading done\n");
}

void plugin_unload(void)
{
       if (plugin_manager.window)
              unwrap_plugin_manager_window();
       g_print("registry plug-in unloaded!\n");
}

SylPluginInfo *plugin_info(void)
{
       return &info;
}

gint plugin_interface_version(void)
{
       return SYL_PLUGIN_INTERFACE_VERSION;
}

static void init_done_cb(GObject *obj, gpointer data)
{
       syl_plugin_update_check_set_check_plugin_url(url.versions);
       syl_plugin_update_check_set_jump_plugin_url(url.site);

       g_print("registry: %p: app init done\n", obj);
}

static void plugin_manager_open_cb(GObject *obj, GtkWidget *window,
              gpointer data)
{
       plugin_manager.window = window;
       wrap_plugin_manager_window();
       syl_plugin_signal_disconnect(plugin_manager_open_cb, NULL);
}

static void plugin_manager_foreach_cb(GtkWidget *widget, gpointer data)
{
       if (GTK_IS_SCROLLED_WINDOW(widget))
              plugin_manager.original_child = widget;
}

static void wrap_plugin_manager_window(void)
{
       GtkWidget *label;
       GtkWidget *vbox = GTK_BIN(plugin_manager.window)->child;
       GtkNotebook *notebook;

       /* Find the scrolledwin */
       gtk_container_foreach(GTK_CONTAINER(vbox), plugin_manager_foreach_cb,
                     NULL);

       if (!plugin_manager.original_child) {
              g_warning("Couldn't find plugin manager scrolled window");
              return;
       }

       /* Wrap the scrolledwin in a notebook */
       g_object_ref(plugin_manager.original_child);
       gtk_container_remove(GTK_CONTAINER(vbox),
                     plugin_manager.original_child);
       plugin_manager.notebook = gtk_notebook_new();
       label = gtk_label_new(_("Your plug-ins"));
       gtk_notebook_append_page(GTK_NOTEBOOK(plugin_manager.notebook),
                     plugin_manager.original_child, label);
       g_object_unref(plugin_manager.original_child);

       /* Add registry page */
       label = gtk_label_new(_("Plug-in Registry"));
       gtk_notebook_append_page(GTK_NOTEBOOK(plugin_manager.notebook),
                     registry_page_create(), label);

       gtk_widget_show_all(plugin_manager.notebook);
       gtk_box_pack_start(GTK_BOX(vbox), plugin_manager.notebook,
                     TRUE, TRUE, 0);
}

static void unwrap_plugin_manager_window(void)
{
       GtkWidget *vbox = GTK_BIN(plugin_manager.window)->child;
       gtk_container_remove(GTK_CONTAINER(vbox), plugin_manager.notebook);
       gtk_box_pack_start(GTK_BOX(vbox), plugin_manager.original_child,
                     TRUE, TRUE, 0);
}

static GtkWidget *registry_page_create(void)
{
       GtkWidget *label = gtk_label_new("foo");
       return label;
}
