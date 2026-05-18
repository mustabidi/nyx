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

#include "nyx-enums-private.h"
#include "nyx-threaded-object.h"
#include "nyx-feature.h"

G_BEGIN_DECLS

#define NYX_TYPE_FEATURES_MANAGER (nyx_features_manager_get_type())
#define NYX_FEATURES_MANAGER_CAST(obj) ((NyxFeaturesManager *)(obj))

G_DECLARE_FINAL_TYPE (NyxFeaturesManager, nyx_features_manager, NYX, FEATURES_MANAGER, NyxThreadedObject)

G_GNUC_INTERNAL
NyxFeaturesManager * nyx_features_manager_new (void);

G_GNUC_INTERNAL
void nyx_features_manager_add_feature (NyxFeaturesManager *features, NyxFeature *feature, GstObject *parent);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_property_changed (NyxFeaturesManager *self, NyxFeature *feature, GParamSpec *pspec);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_state_changed (NyxFeaturesManager *features, NyxPlayerState state);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_position_changed (NyxFeaturesManager *features, gdouble position);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_speed_changed (NyxFeaturesManager *features, gdouble speed);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_volume_changed (NyxFeaturesManager *features, gdouble volume);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_mute_changed (NyxFeaturesManager *features, gboolean mute);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_played_item_changed (NyxFeaturesManager *features, NyxMediaItem *item);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_item_updated (NyxFeaturesManager *features, NyxMediaItem *item);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_queue_item_added (NyxFeaturesManager *features, NyxMediaItem *item, guint index);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_queue_item_removed (NyxFeaturesManager *features, NyxMediaItem *item, guint index);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_queue_item_repositioned (NyxFeaturesManager *features, guint before, guint after);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_queue_cleared (NyxFeaturesManager *features);

G_GNUC_INTERNAL
void nyx_features_manager_trigger_queue_progression_changed (NyxFeaturesManager *features, NyxQueueProgressionMode mode);

G_GNUC_INTERNAL
void nyx_features_manager_handle_event (NyxFeaturesManager *features, NyxFeaturesManagerEvent event, const GValue *value, const GValue *extra_value);

G_END_DECLS
