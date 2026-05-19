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

#if !defined(__NYX_INSIDE__) && !defined(NYX_COMPILATION)
#error "Only <nyx/nyx.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

#include <nyx/nyx-visibility.h>
#include <nyx/nyx-player.h>
#include <nyx/nyx-media-item.h>
#include <nyx/nyx-timeline.h>
#include <nyx/nyx-marker.h>
#include <nyx/nyx-enums.h>

G_BEGIN_DECLS

#define NYX_TYPE_REACTABLE (nyx_reactable_get_type())
#define NYX_REACTABLE_CAST(obj) ((NyxReactable *)(obj))

NYX_API
G_DECLARE_INTERFACE (NyxReactable, nyx_reactable, NYX, REACTABLE, GstObject)

/**
 * NyxReactableInterface:
 * @parent_iface: The parent interface structure.
 * @state_changed: Player state changed.
 * @position_changed: Player position changed.
 * @speed_changed: Player speed changed.
 * @volume_changed: Player volume changed.
 * @mute_changed: Player mute state changed.
 * @played_item_changed: New media item started playing.
 * @item_updated: An item in queue got updated.
 * @queue_item_added: An item was added to the queue.
 * @queue_item_removed: An item was removed from queue.
 * @queue_item_repositioned: An item changed position within queue.
 * @queue_cleared: All items were removed from queue.
 * @queue_progression_changed: Progression mode of the queue was changed.
 * @message_received: Custom message from user was received on reactables bus.
 */
struct _NyxReactableInterface
{
  GTypeInterface parent_iface;

  /**
   * NyxReactableInterface::state_changed:
   * @reactable: a #NyxReactable
   * @state: a #NyxPlayerState
   *
   * Player state changed.
   *
   * Since: 0.10
   */
  void (* state_changed) (NyxReactable *reactable, NyxPlayerState state);

  /**
   * NyxReactableInterface::position_changed:
   * @reactable: a #NyxReactable
   * @position: a decimal number with current position in seconds
   *
   * Player position changed.
   *
   * Since: 0.10
   */
  void (* position_changed) (NyxReactable *reactable, gdouble position);

  /**
   * NyxReactableInterface::speed_changed:
   * @reactable: a #NyxReactable
   * @speed: the playback speed multiplier
   *
   * Player speed changed.
   *
   * Since: 0.10
   */
  void (* speed_changed) (NyxReactable *reactable, gdouble speed);

  /**
   * NyxReactableInterface::volume_changed:
   * @reactable: a #NyxReactable
   * @volume: the volume level
   *
   * Player volume changed.
   *
   * Since: 0.10
   */
  void (* volume_changed) (NyxReactable *reactable, gdouble volume);

  /**
   * NyxReactableInterface::mute_changed:
   * @reactable: a #NyxReactable
   * @mute: %TRUE if player is muted, %FALSE otherwise
   *
   * Player mute state changed.
   *
   * Since: 0.10
   */
  void (* mute_changed) (NyxReactable *reactable, gboolean mute);

  /**
   * NyxReactableInterface::played_item_changed:
   * @reactable: a #NyxReactable
   * @item: a #NyxMediaItem that is now playing
   *
   * New media item started playing. All following events (such as position changes)
   * will be related to this @item from now on.
   *
   * Since: 0.10
   */
  void (* played_item_changed) (NyxReactable *reactable, NyxMediaItem *item);

  /**
   * NyxReactableInterface::item_updated:
   * @reactable: a #NyxReactable
   * @item: a #NyxMediaItem that was updated
   * @flags: flags informing which properties were updated
   *
   * An item in queue got updated.
   *
   * This might be (or not) currently played item.
   * Implementations can compare it against the last item from
   * [vfunc@Nyx.Reactable.played_item_changed] if they
   * need to know that.
   *
   * Since: 0.10
   */
  void (* item_updated) (NyxReactable *reactable, NyxMediaItem *item, NyxReactableItemUpdatedFlags flags);

  /**
   * NyxReactableInterface::queue_item_added:
   * @reactable: a #NyxReactable
   * @item: a #NyxMediaItem that was added
   * @index: position at which @item was placed in queue
   *
   * An item was added to the queue.
   *
   * Since: 0.10
   */
  void (* queue_item_added) (NyxReactable *reactable, NyxMediaItem *item, guint index);

  /**
   * NyxReactableInterface::queue_item_removed:
   * @reactable: a #NyxReactable
   * @item: a #NyxMediaItem that was removed
   * @index: position from which @item was removed in queue
   *
   * An item was removed from queue.
   *
   * Implementations that are interested in queue items removal
   * should also implement [vfunc@Nyx.Reactable.queue_cleared].
   *
   * Since: 0.10
   */
  void (* queue_item_removed) (NyxReactable *reactable, NyxMediaItem *item, guint index);

  /**
   * NyxReactableInterface::queue_item_repositioned:
   * @reactable: a #NyxReactable
   * @before: position from which #NyxMediaItem was removed
   * @after: position at which #NyxMediaItem was inserted after removal
   *
   * An item changed position within queue.
   *
   * Since: 0.10
   */
  void (* queue_item_repositioned) (NyxReactable *reactable, guint before, guint after);

  /**
   * NyxReactableInterface::queue_cleared:
   * @reactable: a #NyxReactable
   *
   * All items were removed from queue.
   *
   * Note that in such event [vfunc@Nyx.Reactable.queue_item_removed]
   * will NOT be called for each item for performance reasons. You probably
   * want to implement this function if you also implemented item removal.
   *
   * Since: 0.10
   */
  void (* queue_cleared) (NyxReactable *reactable);

  /**
   * NyxReactableInterface::queue_progression_changed:
   * @reactable: a #NyxReactable
   * @mode: a #NyxQueueProgressionMode
   *
   * Progression mode of the queue was changed.
   *
   * Since: 0.10
   */
  void (* queue_progression_changed) (NyxReactable *reactable, NyxQueueProgressionMode mode);

  /**
   * NyxReactableInterface::message_received:
   * @reactable: a #NyxReactable
   * @msg: a #GstMessage
   *
   * Custom message from user was received on reactables bus.
   *
   * Since: 0.10
   */
  void (* message_received) (NyxReactable *reactable, GstMessage *msg);

  /*< private >*/
  gpointer padding[8];
};

NYX_API
NyxPlayer * nyx_reactable_get_player (NyxReactable *reactable);

NYX_API
void nyx_reactable_queue_append_sync (NyxReactable *reactable, NyxMediaItem *item);

NYX_API
void nyx_reactable_queue_insert_sync (NyxReactable *reactable, NyxMediaItem *item, NyxMediaItem *after_item);

NYX_API
void nyx_reactable_queue_remove_sync (NyxReactable *reactable, NyxMediaItem *item);

NYX_API
void nyx_reactable_queue_clear_sync (NyxReactable *reactable);

NYX_API
void nyx_reactable_timeline_insert_sync (NyxReactable *reactable, NyxTimeline *timeline, NyxMarker *marker);

NYX_API
void nyx_reactable_timeline_remove_sync (NyxReactable *reactable, NyxTimeline *timeline, NyxMarker *marker);

G_END_DECLS
