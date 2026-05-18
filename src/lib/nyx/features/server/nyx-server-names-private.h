/*
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

G_BEGIN_DECLS

#define NYX_SERVER_WS_EVENT_STATE "state"
#define NYX_SERVER_WS_EVENT_POSITION "position"
#define NYX_SERVER_WS_EVENT_SPEED "speed"
#define NYX_SERVER_WS_EVENT_VOLUME "volume"
#define NYX_SERVER_WS_EVENT_MUTED "muted"
#define NYX_SERVER_WS_EVENT_UNMUTED "unmuted"
#define NYX_SERVER_WS_EVENT_PLAYED_INDEX "played_index"
#define NYX_SERVER_WS_EVENT_QUEUE_CHANGED "queue_changed"
#define NYX_SERVER_WS_EVENT_QUEUE_PROGRESSION "queue_progression"

#define NYX_SERVER_PLAYER_STATE_STOPPED "stopped"
#define NYX_SERVER_PLAYER_STATE_BUFFERING "buffering"
#define NYX_SERVER_PLAYER_STATE_PAUSED "paused"
#define NYX_SERVER_PLAYER_STATE_PLAYING "playing"

#define NYX_SERVER_QUEUE_PROGRESSION_NONE "none"
#define NYX_SERVER_QUEUE_PROGRESSION_CONSECUTIVE "consecutive"
#define NYX_SERVER_QUEUE_PROGRESSION_REPEAT_ITEM "repeat_item"
#define NYX_SERVER_QUEUE_PROGRESSION_CAROUSEL "carousel"
#define NYX_SERVER_QUEUE_PROGRESSION_SHUFFLE "shuffle"

G_END_DECLS
