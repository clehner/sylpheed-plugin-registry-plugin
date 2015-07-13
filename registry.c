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
#include <sys/stat.h>

#include "sylmain.h"
#include "plugin.h"
#include "utils.h"
#include "spawn_curl.h"

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
	GtkWidget *registry_page;
	GtkListStore *store;
} pman = {0};

static const guint expire_time = 12 * 60 * 60;

static struct {
	gboolean loaded;
	gchar *tmp_file;
} registry = {0};

enum {
	COL_INFO,
	COL_ACTION,
	N_COLS
};

typedef struct _RegistryPluginInfo {
	SylPluginInfo syl;
} RegistryPluginInfo;

static void init_done_cb(GObject *obj, gpointer data);
static void plugin_manager_open_cb(GObject *obj, GtkWidget *window,
		gpointer data);
static void plugin_manager_foreach_cb(GtkWidget *widget, gpointer data);
static void registry_fetch_cb(GPid pid, gint status, gpointer data);
/*
static void notebook_page_switch_cb (GtkNotebook *notebook, GtkWidget *page,
		guint page_num, gpointer data);
		*/

static void wrap_plugin_manager_window(void);
static void unwrap_plugin_manager_window(void);
static GtkWidget *registry_page_create(void);
static void registry_set_list_row(GtkTreeIter *, RegistryPluginInfo *);
static void registry_load(void);
static void registry_fetch(void);
static gboolean registry_file_exists(void);

void plugin_load(void)
{
	GList *list, *cur;
	const gchar *ver;
	gpointer mainwin;

	g_print("registry plug-in loaded!\n");

	registry.tmp_file = g_strdup_printf("%s%cregistry.ini",
			get_tmp_dir(), G_DIR_SEPARATOR);

	g_signal_connect(syl_app_get(), "init-done",
			G_CALLBACK(init_done_cb), NULL);
	syl_plugin_signal_connect("plugin-manager-open",
			G_CALLBACK(plugin_manager_open_cb), NULL);

	g_print("registry plug-in loading done\n");
}

void plugin_unload(void)
{
	if (pman.window)
		unwrap_plugin_manager_window();
	g_free(registry.tmp_file);
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
	if (!pman.window) {
		pman.window = window;
		wrap_plugin_manager_window();
	}

	if (!registry.loaded) {
		if (registry_file_exists()) {
			registry_load();
		} else {
			registry_fetch();
		}
	}
}

static gboolean registry_file_exists(void)
{
	GStatBuf s;

	return g_stat(registry.tmp_file, &s) == 0 &&
		S_ISREG(s.st_mode) &&
		s.st_mtime + expire_time > time(NULL);
}

static void plugin_manager_foreach_cb(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_SCROLLED_WINDOW(widget))
		pman.original_child = widget;
}

static void wrap_plugin_manager_window(void)
{
	GtkWidget *label;
	GtkWidget *vbox = GTK_BIN(pman.window)->child;

	/* Find the scrolledwin */
	gtk_container_foreach(GTK_CONTAINER(vbox), plugin_manager_foreach_cb,
			NULL);

	if (!pman.original_child) {
		g_warning("Couldn't find plugin manager scrolled window");
		return;
	}

	/* Wrap the scrolledwin in a notebook */
	g_object_ref(pman.original_child);
	gtk_container_remove(GTK_CONTAINER(vbox), pman.original_child);
	pman.notebook = gtk_notebook_new();
	label = gtk_label_new(_("Your plug-ins"));
	gtk_notebook_append_page(GTK_NOTEBOOK(pman.notebook),
			pman.original_child, label);
	g_object_unref(pman.original_child);

	/* Add registry page */
	label = gtk_label_new(_("Plug-in Registry"));
	pman.registry_page = registry_page_create();
	gtk_notebook_append_page(GTK_NOTEBOOK(pman.notebook),
			pman.registry_page, label);

	gtk_widget_show_all(pman.notebook);
	gtk_box_pack_start(GTK_BOX(vbox), pman.notebook, TRUE, TRUE, 0);

	/*
	g_signal_connect(G_OBJECT(pman.notebook), "switch_page",
			 G_CALLBACK(notebook_page_switch_cb), NULL);
			 */
}

static void unwrap_plugin_manager_window(void)
{
	GtkWidget *vbox = GTK_BIN(pman.window)->child;
	gtk_container_remove(GTK_CONTAINER(vbox), pman.notebook);
	gtk_box_pack_start(GTK_BOX(vbox), pman.original_child,
			TRUE, TRUE, 0);
}

static GtkWidget *registry_page_create(void)
{
	GtkWidget *scrolledwin;
	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *info_renderer, *action_renderer;

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_widget_set_size_request(scrolledwin, -1, -1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
			GTK_SHADOW_IN);

	pman.store = gtk_list_store_new(N_COLS,
			G_TYPE_STRING, G_TYPE_STRING);

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pman.store));
	g_object_unref(G_OBJECT(pman.store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), COL_INFO);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview),
			GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
#endif

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	info_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Plug-in information"), info_renderer, "text", COL_INFO, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	action_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(NULL, action_renderer, "text", COL_ACTION, NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	gtk_widget_show(treeview);
	gtk_container_add(GTK_CONTAINER(scrolledwin), treeview);

	return scrolledwin;
}

static void registry_set_list_row(GtkTreeIter *_iter, RegistryPluginInfo *info)
{
	GtkListStore *store = pman.store;
	GtkTreeIter iter;
	gchar *plugin_info;

	g_return_if_fail(info != NULL);

	if (_iter)
		iter = *_iter;
	else
		gtk_list_store_append(store, &iter);

	gtk_list_store_set(store, &iter,
			COL_INFO, info->syl.name,
			COL_ACTION, "action!",
			-1);
}

/*
static void notebook_page_switch_cb (GtkNotebook *notebook, GtkWidget *page,
		guint page_num, gpointer data)
{
	if (page == pman.registry_page) {
		registry_set_list_row(NULL, &info);
	}
}
*/

/* read the plugins registry key file from the temp file */
static void registry_load(void)
{
	GKeyFile *key_file = g_key_file_new();
	GError *error;
	gchar **groups, **group;

	if (!g_key_file_load_from_file(key_file, registry.tmp_file,
			G_KEY_FILE_NONE, &error)) {
		g_warning("g_key_file_load_from_file: %s",
				error->message);
		return;
	}

	groups = g_key_file_get_groups(key_file, NULL);
	gtk_list_store_clear(pman.store);
	for (group = groups; *group; group++) {
		RegistryPluginInfo info = {
			.syl.name = g_key_file_get_locale_string(key_file,
					*group, "name", "jp", &error),
		};
		registry_set_list_row(NULL, &info);
	}
	g_strfreev(groups);
}

static void registry_fetch(void)
{
	/* download the plugins registry key file */
	if (spawn_curl(url.plugins, registry_fetch_cb, registry.tmp_file,
				NULL) < 0) {
		/* TODO: show failure notification */
	}
}

static void registry_fetch_cb(GPid pid, gint status, gpointer data)
{
	debug_print("registry_fetch_cb\n");
	registry_load();

	g_spawn_close_pid(pid);
}
