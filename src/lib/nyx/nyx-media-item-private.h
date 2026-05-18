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
#include <gst/pbutils/pbutils.h>

#include "nyx-media-item.h"
#include "nyx-player.h"
#include "nyx-app-bus-private.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void nyx_media_item_update_from_tag_list (NyxMediaItem *item, const GstTagList *tags, gboolean allow_overwrite, NyxPlayer *player);

G_GNUC_INTERNAL
void nyx_media_item_update_from_discoverer_info (NyxMediaItem *self, GstDiscovererInfo *info);

G_GNUC_INTERNAL
gboolean nyx_media_item_update_from_parsed_playlist (NyxMediaItem *item, GListStore *playlist, GstObject *playlist_src, NyxPlayer *player);

G_GNUC_INTERNAL
gboolean nyx_media_item_set_duration (NyxMediaItem *item, gdouble duration, NyxAppBus *app_bus);

G_GNUC_INTERNAL
void nyx_media_item_set_cache_location (NyxMediaItem *item, const gchar *location);

G_GNUC_INTERNAL
const gchar * nyx_media_item_get_playback_uri (NyxMediaItem *item);

G_GNUC_INTERNAL
void nyx_media_item_set_used (NyxMediaItem *item, gboolean used);

G_GNUC_INTERNAL
gboolean nyx_media_item_get_used (NyxMediaItem *item);

G_END_DECLS
