/* Nyx Playback Library
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gio/gio.h>

#include "nyx-queue.h"
#include "nyx-media-item.h"
#include "nyx-player.h"
#include "nyx-app-bus-private.h"

G_BEGIN_DECLS

NyxQueue * nyx_queue_new (void);

void nyx_queue_handle_played_item_changed (NyxQueue *queue, NyxMediaItem *played_item, NyxAppBus *app_bus);

void nyx_queue_handle_playlist (NyxQueue *queue, NyxMediaItem *playlist_item, GListStore *playlist);

void nyx_queue_handle_about_to_finish (NyxQueue *queue, NyxPlayer *player);

gboolean nyx_queue_handle_eos (NyxQueue *queue, NyxPlayer *player);

G_END_DECLS
