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
#include <gst/gst.h>

#include "nyx-enums-private.h"
#include "nyx-player.h"
#include "nyx-media-item.h"

G_BEGIN_DECLS

void nyx_playbin_bus_initialize (void);

gboolean nyx_playbin_bus_message_func (GstBus *bus, GstMessage *msg, NyxPlayer *player);

void nyx_playbin_bus_post_set_volume (GstBus *bus, GstElement *playbin, gdouble volume);

void nyx_playbin_bus_post_set_prop (GstBus *bus, GstObject *src, const gchar *name, GValue *value);

void nyx_playbin_bus_post_set_play_flag (GstBus *bus, NyxPlayerPlayFlags flag, gboolean enabled);

void nyx_playbin_bus_post_request_state (GstBus *bus, NyxPlayer *player, GstState state);

void nyx_playbin_bus_post_seek (GstBus *bus, gdouble position, NyxPlayerSeekMethod flags);

void nyx_playbin_bus_post_rate_change (GstBus *bus, gdouble rate);

void nyx_playbin_bus_post_advance_frame (GstBus *bus);

void nyx_playbin_bus_post_stream_change (GstBus *bus);

void nyx_playbin_bus_post_current_item_change (GstBus *bus, NyxMediaItem *current_item, NyxQueueItemChangeMode mode);

void nyx_playbin_bus_post_item_suburi_change (GstBus *bus, NyxMediaItem *item);

void nyx_playbin_bus_post_user_message (GstBus *bus, GstMessage *msg);

G_END_DECLS
