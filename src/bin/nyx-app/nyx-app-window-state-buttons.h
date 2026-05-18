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

G_BEGIN_DECLS

#define NYX_APP_TYPE_WINDOW_STATE_BUTTONS (nyx_app_window_state_buttons_get_type())
#define NYX_APP_WINDOW_STATE_BUTTONS_CAST(obj) ((NyxAppWindowStateButtons *)(obj))

G_DECLARE_FINAL_TYPE (NyxAppWindowStateButtons, nyx_app_window_state_buttons, NYX_APP, WINDOW_STATE_BUTTONS, GtkBox)

G_END_DECLS
