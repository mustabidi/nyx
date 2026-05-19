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

#include <nyx/nyx-enum-types.h>

G_BEGIN_DECLS

/**
 * NyxPlayerState:
 * @NYX_PLAYER_STATE_STOPPED: Player is stopped.
 * @NYX_PLAYER_STATE_BUFFERING: Player is buffering.
 * @NYX_PLAYER_STATE_PAUSED: Player is paused.
 * @NYX_PLAYER_STATE_PLAYING: Player is playing.
 */
typedef enum
{
  NYX_PLAYER_STATE_STOPPED = 0,
  NYX_PLAYER_STATE_BUFFERING,
  NYX_PLAYER_STATE_PAUSED,
  NYX_PLAYER_STATE_PLAYING,
} NyxPlayerState;

/**
 * NyxPlayerSeekMethod:
 * @NYX_PLAYER_SEEK_METHOD_ACCURATE: Seek to exact position (slow).
 * @NYX_PLAYER_SEEK_METHOD_NORMAL: Seek to approximated position.
 * @NYX_PLAYER_SEEK_METHOD_FAST: Seek to position of nearest keyframe (fast).
 */
typedef enum
{
  NYX_PLAYER_SEEK_METHOD_ACCURATE = 0,
  NYX_PLAYER_SEEK_METHOD_NORMAL,
  NYX_PLAYER_SEEK_METHOD_FAST,
} NyxPlayerSeekMethod;

/**
 * NyxPlayerMessageDestination:
 * @NYX_PLAYER_MESSAGE_DESTINATION_PLAYER: Messaging from application or reactable enhancers to the player itself.
 * @NYX_PLAYER_MESSAGE_DESTINATION_REACTABLES: Messaging from application to the reactable enhancers.
 * @NYX_PLAYER_MESSAGE_DESTINATION_APPLICATION: Messaging from reactable enhancers to the application.
 *
 * Since: 0.10
 */
typedef enum
{
  NYX_PLAYER_MESSAGE_DESTINATION_PLAYER = 0,
  NYX_PLAYER_MESSAGE_DESTINATION_REACTABLES,
  NYX_PLAYER_MESSAGE_DESTINATION_APPLICATION,
} NyxPlayerMessageDestination;

/**
 * NyxQueueProgressionMode:
 * @NYX_QUEUE_PROGRESSION_NONE: Queue will not change current item after playback finishes.
 * @NYX_QUEUE_PROGRESSION_CONSECUTIVE: Queue selects items one after another until the end.
 *   When end of queue is reached, this mode will continue one another item is added to the queue,
 *   playing it if player autoplay property is set, otherwise current player state is kept.
 * @NYX_QUEUE_PROGRESSION_REPEAT_ITEM: Queue keeps repeating current media item.
 * @NYX_QUEUE_PROGRESSION_CAROUSEL: Queue starts from beginning after last media item.
 * @NYX_QUEUE_PROGRESSION_SHUFFLE: Queue selects a random media item after current one.
 *   Shuffle mode will avoid reselecting previously shuffled items as long as possible.
 *   After it runs out of unused items, shuffling begins anew.
 */
typedef enum
{
  NYX_QUEUE_PROGRESSION_NONE = 0,
  NYX_QUEUE_PROGRESSION_CONSECUTIVE,
  NYX_QUEUE_PROGRESSION_REPEAT_ITEM,
  NYX_QUEUE_PROGRESSION_CAROUSEL,
  NYX_QUEUE_PROGRESSION_SHUFFLE,
} NyxQueueProgressionMode;

/**
 * NyxMarkerType:
 * @NYX_MARKER_TYPE_UNKNOWN: Unknown marker type.
 * @NYX_MARKER_TYPE_TITLE: A title marker in timeline.
 * @NYX_MARKER_TYPE_CHAPTER: A chapter marker in timeline.
 * @NYX_MARKER_TYPE_TRACK: A track marker in timeline.
 * @NYX_STREAM_TYPE_CUSTOM_1: A custom marker 1 for free usage by application.
 * @NYX_STREAM_TYPE_CUSTOM_2: A custom marker 2 for free usage by application.
 * @NYX_STREAM_TYPE_CUSTOM_3: A custom marker 3 for free usage by application.
 */
typedef enum
{
  NYX_MARKER_TYPE_UNKNOWN = 0,
  NYX_MARKER_TYPE_TITLE,
  NYX_MARKER_TYPE_CHAPTER,
  NYX_MARKER_TYPE_TRACK,
  NYX_MARKER_TYPE_CUSTOM_1 = 101,
  NYX_MARKER_TYPE_CUSTOM_2 = 102,
  NYX_MARKER_TYPE_CUSTOM_3 = 103,
} NyxMarkerType;

/**
 * NyxStreamType:
 * @NYX_STREAM_TYPE_UNKNOWN: Unknown stream type.
 * @NYX_STREAM_TYPE_VIDEO: Stream is a #NyxVideoStream.
 * @NYX_STREAM_TYPE_AUDIO: Stream is a #NyxAudioStream.
 * @NYX_STREAM_TYPE_SUBTITLE: Stream is a #NyxSubtitleStream.
 */
typedef enum
{
  NYX_STREAM_TYPE_UNKNOWN = 0,
  NYX_STREAM_TYPE_VIDEO,
  NYX_STREAM_TYPE_AUDIO,
  NYX_STREAM_TYPE_SUBTITLE,
} NyxStreamType;

/**
 * NyxDiscovererDiscoveryMode:
 * @NYX_DISCOVERER_DISCOVERY_ALWAYS: Run discovery for every single media item added to [class@Nyx.Queue].
 *   This mode is useful when application presents a list of items to select from to the user before playback.
 *   It will scan every single item in queue, so user can have an updated list of items when selecting what to play.
 * @NYX_DISCOVERER_DISCOVERY_NONCURRENT: Only run discovery on an item if it is not a currently selected item in [class@Nyx.Queue].
 *   This mode is optimal when application always plays (or at least goes into paused) after selecting item from queue.
 *   It will skip discovery of such items since they will be discovered by [class@Nyx.Player] anyway.
 *
 * Deprecated: 0.10: Use Media Scanner from `nyx-enhancers` repo instead.
 */
typedef enum
{
  NYX_DISCOVERER_DISCOVERY_ALWAYS = 0,
  NYX_DISCOVERER_DISCOVERY_NONCURRENT,
} NyxDiscovererDiscoveryMode;

/* NOTE: GStreamer uses param flags 8-16, so start with 17. */
/**
 * NyxEnhancerParamFlags:
 * @NYX_ENHANCER_PARAM_GLOBAL: Use this flag for enhancer properties that should have global access scope.
 *   Such are meant for application `USER` to configure.
 * @NYX_ENHANCER_PARAM_LOCAL: Use this flag for enhancer properties that should have local access scope.
 *   Such are meant for `APPLICATION` to configure.
 * @NYX_ENHANCER_PARAM_FILEPATH: Use this flag for enhancer properties that store string with a file path.
 *   Applications can use this as a hint to show file selection instead of a text entry.
 * @NYX_ENHANCER_PARAM_DIRPATH: Use this flag for enhancer properties that store string with a directory path.
 *   Applications can use this as a hint to show directory selection instead of a text entry.
 *
 * Additional [flags@GObject.ParamFlags] to be set in enhancer plugins implementations.
 *
 * Since: 0.10
 */
typedef enum
{
  NYX_ENHANCER_PARAM_GLOBAL = 1 << 17,
  NYX_ENHANCER_PARAM_LOCAL = 1 << 18,
  NYX_ENHANCER_PARAM_FILEPATH = 1 << 19,
  NYX_ENHANCER_PARAM_DIRPATH = 1 << 20,
} NyxEnhancerParamFlags;

/**
 * NyxReactableItemUpdatedFlags:
 * @NYX_REACTABLE_ITEM_UPDATED_TITLE: Media item title was updated.
 * @NYX_REACTABLE_ITEM_UPDATED_DURATION: Media item duration was updated.
 * @NYX_REACTABLE_ITEM_UPDATED_TIMELINE: Media item timeline was updated.
 * @NYX_REACTABLE_ITEM_UPDATED_TAGS: Media item tags were updated.
 * @NYX_REACTABLE_ITEM_UPDATED_REDIRECT_URI: Media item redirect URI was updated.
 * @NYX_REACTABLE_ITEM_UPDATED_CACHE_LOCATION: Media item cache location was updated.
 *
 * Flags informing which properties were updated within [class@Nyx.MediaItem].
 *
 * Since: 0.10
 */
typedef enum
{
  NYX_REACTABLE_ITEM_UPDATED_TITLE = 1 << 0,
  NYX_REACTABLE_ITEM_UPDATED_DURATION = 1 << 1,
  NYX_REACTABLE_ITEM_UPDATED_TIMELINE = 1 << 2,
  NYX_REACTABLE_ITEM_UPDATED_TAGS = 1 << 3,
  NYX_REACTABLE_ITEM_UPDATED_REDIRECT_URI = 1 << 4,
  NYX_REACTABLE_ITEM_UPDATED_CACHE_LOCATION = 1 << 5,
} NyxReactableItemUpdatedFlags;

G_END_DECLS
