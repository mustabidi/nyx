/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <nyx/nyx.h>

G_BEGIN_DECLS

#define NYX_APP_TYPE_INFO_WINDOW (nyx_app_info_window_get_type())
#define NYX_APP_INFO_WINDOW_CAST(obj) ((NyxAppInfoWindow *)(obj))

G_DECLARE_FINAL_TYPE (NyxAppInfoWindow, nyx_app_info_window, NYX_APP, INFO_WINDOW, AdwWindow)

G_GNUC_INTERNAL
GtkWidget * nyx_app_info_window_new (GtkApplication *gtk_app, NyxPlayer *player);

G_END_DECLS
