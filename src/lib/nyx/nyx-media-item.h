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

#if !defined(__NYX_INSIDE__) && !defined(NYX_COMPILATION)
#error "Only <nyx/nyx.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-timeline.h>

G_BEGIN_DECLS

#define NYX_TYPE_MEDIA_ITEM (nyx_media_item_get_type())
#define NYX_MEDIA_ITEM_CAST(obj) ((NyxMediaItem *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxMediaItem, nyx_media_item, NYX, MEDIA_ITEM, GstObject)

NYX_API
NyxMediaItem * nyx_media_item_new (const gchar *uri);

NYX_API
NyxMediaItem * nyx_media_item_new_from_file (GFile *file);

NYX_API
NyxMediaItem * nyx_media_item_new_cached (const gchar *uri, const gchar *location);

NYX_API
guint nyx_media_item_get_id (NyxMediaItem *item);

NYX_API
const gchar * nyx_media_item_get_uri (NyxMediaItem *item);

NYX_API
void nyx_media_item_set_suburi (NyxMediaItem *item, const gchar *suburi);

NYX_API
gchar * nyx_media_item_get_suburi (NyxMediaItem *item);

NYX_API
gchar * nyx_media_item_get_redirect_uri (NyxMediaItem *item);

NYX_API
gchar * nyx_media_item_get_cache_location (NyxMediaItem *item);

NYX_API
gchar * nyx_media_item_get_title (NyxMediaItem *item);

NYX_DEPRECATED_FOR(nyx_media_item_get_tags)
gchar * nyx_media_item_get_container_format (NyxMediaItem *item);

NYX_API
gdouble nyx_media_item_get_duration (NyxMediaItem *item);

NYX_API
GstTagList * nyx_media_item_get_tags (NyxMediaItem *item);

NYX_API
gboolean nyx_media_item_populate_tags (NyxMediaItem *item, const GstTagList *tags);

NYX_API
NyxTimeline * nyx_media_item_get_timeline (NyxMediaItem *item);

G_END_DECLS
