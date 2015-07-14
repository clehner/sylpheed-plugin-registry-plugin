/*
 * Sylpheed Plugin Registry Plugin
 * Copyright (C) 2015 Charles Lehner
 *
 * This file is based on code in update_check.c from Sylpheed, which is
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>

#include "prefs_common.h"
#include "socket.h"
#include "utils.h"
#include "spawn_curl.h"

void close_child_stdout(gint fd)
{
#ifdef G_OS_WIN32
	GIOChannel *ch;

	ch = g_io_channel_win32_new_fd(fd);
	g_io_channel_shutdown(ch, TRUE, NULL);
	g_io_channel_unref(ch);
#else
	fd_close(fd);
#endif
}

gint spawn_curl(const gchar *url, GChildWatchFunc func, const gchar *outfile,
        gpointer data)
{
	const gchar *cmdline[11] = {"curl", "--location", "--silent",
		"--max-time", "10"};
	gint argc = 5;
	gint child_stdout = 0;
	GPid pid;
	GError *error = NULL;

	debug_print("spawn_curl: getting from %s\n", url);

	cmdline[argc++] = url;
	if (prefs_common.use_http_proxy && prefs_common.http_proxy_host &&
	    prefs_common.http_proxy_host[0] != '\0') {
		cmdline[argc++] = "--proxy";
		cmdline[argc++] = prefs_common.http_proxy_host;
	}
	if (outfile) {
		cmdline[argc++] = "--output";
		cmdline[argc++] = outfile;
	}
	cmdline[argc++] = NULL;

	if (g_spawn_async_with_pipes
		(NULL, (gchar **)cmdline, NULL,
		 G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
		 NULL, NULL, &pid,
		 NULL, outfile ? NULL : &child_stdout, NULL, &error) == FALSE) {
		g_warning("Couldn't execute curl");
		if (error) {
			g_warning("g_spawn_async_with_pipes: %s",
				  error->message);
			g_error_free(error);
		}
		return -1;
	}
	if (pid == 0) {
		g_warning("Couldn't get PID of child process");
		if (child_stdout)
			close_child_stdout(child_stdout);
		return -1;
	}

	g_child_watch_add(pid, func, data);

	return child_stdout;
}
