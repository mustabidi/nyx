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

#include <nyx/nyx-feature.h>
#include <nyx/nyx-enums.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
void nyx_feature_call_prepare (NyxFeature *feature);

G_GNUC_INTERNAL
void nyx_feature_call_unprepare (NyxFeature *feature);

G_GNUC_INTERNAL
void nyx_feature_call_property_changed (NyxFeature *feature, GParamSpec *pspec);

G_GNUC_INTERNAL
void nyx_feature_call_state_changed (NyxFeature *feature, NyxPlayerState state);

G_GNUC_INTERNAL
void nyx_feature_call_position_changed (NyxFeature *feature, gdouble position);

G_GNUC_INTERNAL
void nyx_feature_call_speed_changed (NyxFeature *feature, gdouble speed);

G_GNUC_INTERNAL
void nyx_feature_call_volume_changed (NyxFeature *feature, gdouble volume);

G_GNUC_INTERNAL
void nyx_feature_call_mute_changed (NyxFeature *feature, gboolean mute);

G_GNUC_INTERNAL
void nyx_feature_call_played_item_changed (NyxFeature *feature, NyxMediaItem *item);

G_GNUC_INTERNAL
void nyx_feature_call_item_updated (NyxFeature *feature, NyxMediaItem *item);

G_GNUC_INTERNAL
void nyx_feature_call_queue_item_added (NyxFeature *feature, NyxMediaItem *item, guint index);

G_GNUC_INTERNAL
void nyx_feature_call_queue_item_removed (NyxFeature *feature, NyxMediaItem *item, guint index);

G_GNUC_INTERNAL
void nyx_feature_call_queue_item_repositioned (NyxFeature *self, guint before, guint after);

G_GNUC_INTERNAL
void nyx_feature_call_queue_cleared (NyxFeature *feature);

G_GNUC_INTERNAL
void nyx_feature_call_queue_progression_changed (NyxFeature *feature, NyxQueueProgressionMode mode);

G_END_DECLS
