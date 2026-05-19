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
#include <gst/gst.h>

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-enums.h>
#include <nyx/nyx-media-item.h>

G_BEGIN_DECLS

#define NYX_TYPE_QUEUE (nyx_queue_get_type())
#define NYX_QUEUE_CAST(obj) ((NyxQueue *)(obj))

NYX_API
G_DECLARE_FINAL_TYPE (NyxQueue, nyx_queue, NYX, QUEUE, GstObject)

/**
 * NYX_QUEUE_INVALID_POSITION:
 *
 * The value used to refer to an invalid position in a #NyxQueue
 */
#define NYX_QUEUE_INVALID_POSITION ((guint) 0xffffffff)

NYX_API
void nyx_queue_add_item (NyxQueue *queue, NyxMediaItem *item);

NYX_API
void nyx_queue_insert_item (NyxQueue *queue, NyxMediaItem *item, gint index);

NYX_API
void nyx_queue_insert_item_after (NyxQueue *queue, NyxMediaItem *item, NyxMediaItem *after_item);

NYX_API
void nyx_queue_reposition_item (NyxQueue *queue, NyxMediaItem *item, gint index);

NYX_API
void nyx_queue_remove_item (NyxQueue *queue, NyxMediaItem *item);

NYX_API
void nyx_queue_remove_index (NyxQueue *queue, guint index);

NYX_API
NyxMediaItem * nyx_queue_steal_index (NyxQueue *queue, guint index);

NYX_API
void nyx_queue_clear (NyxQueue *queue);

NYX_API
gboolean nyx_queue_select_item (NyxQueue *queue, NyxMediaItem *item);

NYX_API
gboolean nyx_queue_select_index (NyxQueue *queue, guint index);

NYX_API
gboolean nyx_queue_select_next_item (NyxQueue *queue);

NYX_API
gboolean nyx_queue_select_previous_item (NyxQueue *queue);

NYX_API
NyxMediaItem * nyx_queue_get_item (NyxQueue *queue, guint index);

NYX_API
NyxMediaItem * nyx_queue_get_current_item (NyxQueue *queue);

NYX_API
guint nyx_queue_get_current_index (NyxQueue *queue);

NYX_API
gboolean nyx_queue_item_is_current (NyxQueue *queue, NyxMediaItem *item);

NYX_API
gboolean nyx_queue_find_item (NyxQueue *queue, NyxMediaItem *item, guint *index);

NYX_API
guint nyx_queue_get_n_items (NyxQueue *queue);

NYX_API
void nyx_queue_set_progression_mode (NyxQueue *queue, NyxQueueProgressionMode mode);

NYX_API
NyxQueueProgressionMode nyx_queue_get_progression_mode (NyxQueue *queue);

NYX_API
void nyx_queue_set_gapless (NyxQueue *queue, gboolean gapless);

NYX_API
gboolean nyx_queue_get_gapless (NyxQueue *queue);

NYX_API
void nyx_queue_set_instant (NyxQueue *queue, gboolean instant);

NYX_API
gboolean nyx_queue_get_instant (NyxQueue *queue);

G_END_DECLS
