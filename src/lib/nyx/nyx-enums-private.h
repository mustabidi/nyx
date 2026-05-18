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

G_BEGIN_DECLS

typedef enum
{
  NYX_PLAYER_PLAY_FLAG_VIDEO             = (1 << 0),
  NYX_PLAYER_PLAY_FLAG_AUDIO             = (1 << 1),
  NYX_PLAYER_PLAY_FLAG_TEXT              = (1 << 2),
  NYX_PLAYER_PLAY_FLAG_VIS               = (1 << 3),
  NYX_PLAYER_PLAY_FLAG_SOFT_VOLUME       = (1 << 4),
  NYX_PLAYER_PLAY_FLAG_NATIVE_AUDIO      = (1 << 5),
  NYX_PLAYER_PLAY_FLAG_NATIVE_VIDEO      = (1 << 6),
  NYX_PLAYER_PLAY_FLAG_DOWNLOAD          = (1 << 7),
  NYX_PLAYER_PLAY_FLAG_BUFFERING         = (1 << 8),
  NYX_PLAYER_PLAY_FLAG_DEINTERLACE       = (1 << 9),
  NYX_PLAYER_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
  NYX_PLAYER_PLAY_FLAG_FORCE_FILTERS     = (1 << 11),
  NYX_PLAYER_PLAY_FLAG_FORCE_SW_DECODERS = (1 << 12)
} NyxPlayerPlayFlags;

typedef enum
{
  NYX_FEATURES_MANAGER_EVENT_UNKNOWN = 0,
  NYX_FEATURES_MANAGER_EVENT_FEATURE_ADDED,
  NYX_FEATURES_MANAGER_EVENT_FEATURE_PROPERTY_CHANGED,
  NYX_FEATURES_MANAGER_EVENT_STATE_CHANGED,
  NYX_FEATURES_MANAGER_EVENT_POSITION_CHANGED,
  NYX_FEATURES_MANAGER_EVENT_SPEED_CHANGED,
  NYX_FEATURES_MANAGER_EVENT_VOLUME_CHANGED,
  NYX_FEATURES_MANAGER_EVENT_MUTE_CHANGED,
  NYX_FEATURES_MANAGER_EVENT_PLAYED_ITEM_CHANGED,
  NYX_FEATURES_MANAGER_EVENT_ITEM_UPDATED,
  NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_ADDED,
  NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REMOVED,
  NYX_FEATURES_MANAGER_EVENT_QUEUE_ITEM_REPOSITIONED,
  NYX_FEATURES_MANAGER_EVENT_QUEUE_CLEARED,
  NYX_FEATURES_MANAGER_EVENT_QUEUE_PROGRESSION_CHANGED
} NyxFeaturesManagerEvent;

typedef enum
{
  NYX_QUEUE_ITEM_CHANGE_NORMAL = 1,
  NYX_QUEUE_ITEM_CHANGE_INSTANT = 2,
  NYX_QUEUE_ITEM_CHANGE_GAPLESS = 3,
} NyxQueueItemChangeMode;

G_END_DECLS
