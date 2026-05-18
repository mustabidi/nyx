/* Nyx Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include <gst/gst.h>

#include "nyx-enums.h"
#include "nyx-threaded-object.h"
#include "nyx-enhancer-proxy.h"
#include "nyx-media-item.h"

G_BEGIN_DECLS

#define NYX_TYPE_REACTABLES_MANAGER (nyx_reactables_manager_get_type())
#define NYX_REACTABLES_MANAGER_CAST(obj) ((NyxReactablesManager *)(obj))

G_DECLARE_FINAL_TYPE (NyxReactablesManager, nyx_reactables_manager, NYX, REACTABLES_MANAGER, NyxThreadedObject)

G_GNUC_INTERNAL
void nyx_reactables_manager_initialize (void);

G_GNUC_INTERNAL
NyxReactablesManager * nyx_reactables_manager_new (void);

G_GNUC_INTERNAL
void nyx_reactables_manager_post_message (NyxReactablesManager *manager, GstMessage *msg);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_configure_take_config (NyxReactablesManager *manager, NyxEnhancerProxy *proxy, GstStructure *config);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_state_changed (NyxReactablesManager *manager, NyxPlayerState state);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_position_changed (NyxReactablesManager *manager, gdouble position);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_speed_changed (NyxReactablesManager *manager, gdouble speed);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_volume_changed (NyxReactablesManager *manager, gdouble volume);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_mute_changed (NyxReactablesManager *manager, gboolean mute);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_played_item_changed (NyxReactablesManager *manager, NyxMediaItem *item);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_item_updated (NyxReactablesManager *manager, NyxMediaItem *item, NyxReactableItemUpdatedFlags flags);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_queue_item_added (NyxReactablesManager *manager, NyxMediaItem *item, guint index);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_queue_item_removed (NyxReactablesManager *manager, NyxMediaItem *item, guint index);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_queue_item_repositioned (NyxReactablesManager *manager, guint before, guint after);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_queue_cleared (NyxReactablesManager *manager);

G_GNUC_INTERNAL
void nyx_reactables_manager_trigger_queue_progression_changed (NyxReactablesManager *manager, NyxQueueProgressionMode mode);

G_END_DECLS
