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
#include <nyx/nyx.h>

G_BEGIN_DECLS

#define NYX_APP_TYPE_QUEUE_SELECTION (nyx_app_queue_selection_get_type())
#define NYX_APP_QUEUE_SELECTION_CAST(obj) ((NyxAppQueueSelection *)(obj))

G_DECLARE_FINAL_TYPE (NyxAppQueueSelection, nyx_app_queue_selection, NYX_APP, QUEUE_SELECTION, GObject)

NyxAppQueueSelection * nyx_app_queue_selection_new (NyxQueue *queue);

void nyx_app_queue_selection_set_queue (NyxAppQueueSelection *selection, NyxQueue *queue);

NyxQueue * nyx_app_queue_selection_get_queue (NyxAppQueueSelection *selection);

G_END_DECLS
