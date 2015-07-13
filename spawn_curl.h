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

#ifndef __SPAWN_CURL_H__
#define __SPAWN_CURL_H__

gint spawn_curl(const gchar *url, GChildWatchFunc func, const gchar *outfile,
        gpointer data);
gchar *read_child_stdout_and_close(gint fd);
void close_child_stdout(gint fd);

#endif /* __SPAWN_CURL_H__ */