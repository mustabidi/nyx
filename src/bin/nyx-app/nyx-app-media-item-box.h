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
#include <nyx/nyx.h>

G_BEGIN_DECLS

#define NYX_APP_TYPE_MEDIA_ITEM_BOX (nyx_app_media_item_box_get_type())
#define NYX_APP_MEDIA_ITEM_BOX_CAST(obj) ((NyxAppMediaItemBox *)(obj))

G_DECLARE_FINAL_TYPE (NyxAppMediaItemBox, nyx_app_media_item_box, NYX_APP, MEDIA_ITEM_BOX, GtkBox)

G_GNUC_INTERNAL
NyxMediaItem * nyx_app_media_item_box_get_media_item (NyxAppMediaItemBox *box);

G_END_DECLS
