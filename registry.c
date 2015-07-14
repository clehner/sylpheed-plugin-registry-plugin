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
#include <ctype.h>
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
	GtkWidget *plugins_vbox;
} pman = {0};

static const guint expire_time = 12 * 60 * 60;

static gchar install_url_key[32];
static gchar install_sha1sum_key[32];

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
	gchar *license;
	gchar *url;
	gchar *install_sha1sum;
	gchar *install_url;
} RegistryPluginInfo;

typedef struct _PluginBox {
	RegistryPluginInfo *plugin_info;
	GtkWidget *widget;
	GtkWidget *title_link_btn;
	GtkWidget *install_btn;
	GtkWidget *remove_btn;
	GtkWidget *update_btn;
	GtkWidget *description_label;
	GtkWidget *author_label;
	GtkWidget *license_label;
} PluginBox;

struct version {
	gint major;
	gint minor;
	gint micro;
	const gchar *extra;
};

static void init_done_cb(GObject *obj, gpointer data);
static void plugin_manager_open_cb(GObject *obj, GtkWidget *window,
		gpointer data);
static void plugin_manager_foreach_cb(GtkWidget *widget, gpointer data);
static void registry_fetch_cb(GPid pid, gint status, gpointer data);

static void wrap_plugin_manager_window(void);
static void unwrap_plugin_manager_window(void);
static GtkWidget *registry_page_create(void);
static void registry_list_add_plugin(RegistryPluginInfo *);
static void registry_list_clear(void);
static void registry_load(void);
static void registry_fetch(void);
static gboolean registry_file_exists(void);
static SylPluginInfo *registry_get_installed_plugin(RegistryPluginInfo *info);
static gint compare_syl_plugin_versions(SylPluginInfo *a, SylPluginInfo *b);

static PluginBox *plugin_box_new(RegistryPluginInfo *info);
static void plugin_box_update_buttons(PluginBox *plugin_box);

void plugin_load(void)
{
	GList *list, *cur;
	const gchar *ver;
	gpointer mainwin;

	g_print("registry plug-in loaded!\n");

	registry.tmp_file = g_strdup_printf("%s%cregistry.ini",
			get_tmp_dir(), G_DIR_SEPARATOR);

	g_snprintf(install_url_key, sizeof install_url_key,
			"%s_url", PLATFORM);
	g_snprintf(install_sha1sum_key, sizeof install_sha1sum_key,
			"%s_sha1sum", PLATFORM);

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
	gtk_notebook_append_page(GTK_NOTEBOOK(pman.notebook),
			registry_page_create(), label);

	gtk_widget_show_all(pman.notebook);
	gtk_box_pack_start(GTK_BOX(vbox), pman.notebook, TRUE, TRUE, 0);
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
	GtkWidget *plugins_vbox;

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_widget_set_size_request(scrolledwin, -1, -1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
			GTK_SHADOW_IN);

	plugins_vbox = gtk_vbox_new(TRUE, 6);

	gtk_container_add(GTK_CONTAINER(scrolledwin), plugins_vbox);

	pman.plugins_vbox = plugins_vbox;

	return scrolledwin;
}

PluginBox *plugin_box_new(RegistryPluginInfo *info)
{
	PluginBox *plugin_box;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *title_link_btn;
	GtkWidget *install_btn;
	GtkWidget *remove_btn;
	GtkWidget *update_btn;
	GtkWidget *description_label;
	GtkWidget *author_label;
	GtkWidget *license_label;

	g_return_val_if_fail(info != NULL, NULL);

	plugin_box = g_new0(PluginBox, 1);

	vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	title_link_btn = gtk_link_button_new_with_label(info->url,
			info->syl.name);
	gtk_box_pack_start(GTK_BOX(hbox), title_link_btn, FALSE, FALSE, 0);
	gtk_widget_show(title_link_btn);

	install_btn = gtk_button_new_with_label(_("Install"));
	gtk_box_pack_end(GTK_BOX(hbox), install_btn, FALSE, FALSE, 0);

	update_btn = gtk_button_new_with_label(_("Update"));
	gtk_box_pack_end(GTK_BOX(hbox), update_btn, FALSE, FALSE, 0);

	remove_btn = gtk_button_new_with_label(_("Remove"));
	gtk_box_pack_end(GTK_BOX(hbox), remove_btn, FALSE, FALSE, 0);

	description_label = gtk_label_new(info->syl.description);
	gtk_box_pack_start(GTK_BOX(vbox), description_label,
			FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(description_label), 0, 0);
	gtk_widget_show(description_label);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	author_label = gtk_label_new(info->syl.author);
	gtk_box_pack_start(GTK_BOX(hbox), author_label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(author_label), 0, 0);
	gtk_widget_show(author_label);

	license_label = gtk_label_new(info->license);
	gtk_box_pack_start(GTK_BOX(hbox), license_label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(license_label), 0, 0);
	gtk_widget_show(license_label);

	plugin_box->plugin_info = info;
	plugin_box->widget = vbox;
	plugin_box->title_link_btn = title_link_btn;
	plugin_box->remove_btn = remove_btn;
	plugin_box->update_btn = update_btn;
	plugin_box->install_btn = install_btn;
	plugin_box->description_label = description_label;
	plugin_box->author_label = author_label;
	plugin_box->license_label = license_label;

	plugin_box_update_buttons(plugin_box);

	return plugin_box;
}

static void plugin_box_update_buttons(PluginBox *pbox)
{
	RegistryPluginInfo *info = pbox->plugin_info;
	SylPluginInfo *installed_info = registry_get_installed_plugin(info);
	gboolean can_install = !installed_info && info->install_url;
	gboolean can_remove = installed_info != NULL;
	gboolean can_update = can_install &&
		compare_syl_plugin_versions(&info->syl, installed_info) > 0;

	gtk_widget_set_visible(pbox->install_btn, can_install && !can_update);
	gtk_widget_set_visible(pbox->update_btn, can_update);
	gtk_widget_set_visible(pbox->remove_btn, can_remove);
}

static void registry_list_add_plugin(RegistryPluginInfo *info)
{
	PluginBox *pbox = plugin_box_new(info);
	gtk_box_pack_start(GTK_BOX(pman.plugins_vbox), pbox->widget,
			TRUE, TRUE, 0);
}

static void registry_list_clear(void)
{
	pman.plugins_vbox;
}

static gint registry_load_plugin_info(GKeyFile *key_file, const gchar *name,
		const gchar *locale, RegistryPluginInfo *info)
{
	info->syl.name = g_key_file_get_locale_string(key_file, name,
			"name", locale, NULL);
	info->syl.version = g_key_file_get_string(key_file, name,
			"version", NULL);
	info->syl.description = g_key_file_get_locale_string(key_file,
			name, "description", locale, NULL);
	info->syl.author = g_key_file_get_string(key_file, name, "author",
			NULL);
	info->url = g_key_file_get_string(key_file, name, "url", NULL);
	info->install_url = g_key_file_get_string(key_file, name,
			install_url_key, NULL);
	info->license = g_key_file_get_string(key_file, name,
			"license", NULL);
	info->install_sha1sum = g_key_file_get_string(key_file, name,
			install_sha1sum_key, NULL);

	return 0;
}

/* read the plugins registry key file from the temp file */
static void registry_load(void)
{
	GKeyFile *key_file = g_key_file_new();
	GError *error;
	gchar **groups, **group;
	const gchar *locale = NULL;

	if (!g_key_file_load_from_file(key_file, registry.tmp_file,
			G_KEY_FILE_NONE, &error)) {
		g_warning("g_key_file_load_from_file: %s",
				error->message);
		g_error_free(error);
		return;
	}

	groups = g_key_file_get_groups(key_file, NULL);
	registry_list_clear();
	for (group = groups; *group; group++) {
		RegistryPluginInfo info = {0};
		if (registry_load_plugin_info(key_file, *group, locale,
					&info) >= 0)
			registry_list_add_plugin(&info);
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

/* Get the installed version of a plugin */
static SylPluginInfo *registry_get_installed_plugin(RegistryPluginInfo *info)
{
	GSList *cur;
	const gchar *name;

	g_return_val_if_fail(info != NULL, NULL);

	name = info->syl.name;

	for (cur = syl_plugin_get_module_list(); cur; cur = cur->next) {
		SylPluginInfo *plugin_info = syl_plugin_get_info(cur->data);
		if (plugin_info && plugin_info->name == name)
			return plugin_info;
	}
	return NULL;
}

static gint compare_versions(struct version a, struct version b)
{
	debug_print("comparing %d.%d.%d.%s <> %d.%d.%d.%s\n",
		    a.major, a.minor, a.micro, a.extra,
		    b.major, b.minor, b.micro, b.extra);

	return
		a.major > b.major ? 1 :
		a.major < b.major ? -1 :
		a.minor > b.minor ? 1 :
		a.minor < b.minor ? -1 :
		a.micro > b.micro ? 1 :
		a.micro < b.micro ? -1 :
		g_strcmp0(a.extra, b.extra);
}

static struct version version_from_string(const gchar *str)
{
	struct version ver = {0};

	ver.major = atoi(str);
	if ((str = strchr(str, '.'))) {
		ver.minor = atoi(++str);
		if ((str = strchr(str, '.'))) {
			ver.micro = atoi(++str);
			while (isdigit(*str))
				str++;
			ver.extra = str;
		}
	}
	return ver;
}

static gint compare_syl_plugin_versions(SylPluginInfo *a, SylPluginInfo *b)
{
	return !a && !b ? 0 :
		a && !b ? 1 :
		!a && b ? -1 :
		compare_versions(version_from_string(a->version),
				version_from_string(b->version));
}
