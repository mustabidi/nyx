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

/**
 * NyxReactable:
 *
 * An interface for creating enhancers that react to the
 * playback and/or events that should influence it.
 *
 * Since: 0.10
 */

#include "nyx-reactable.h"
#include "nyx-utils-private.h"

#define NYX_REACTABLE_DO_WITH_QUEUE(reactable, _queue_dst, ...) {  \
    NyxPlayer *_player = nyx_reactable_get_player (reactable); \
    if (G_LIKELY (_player != NULL)) {                                  \
      *_queue_dst = nyx_player_get_queue (_player);                \
      __VA_ARGS__                                                      \
      gst_object_unref (_player); }}

G_DEFINE_INTERFACE (NyxReactable, nyx_reactable, GST_TYPE_OBJECT);

static void
nyx_reactable_default_init (NyxReactableInterface *iface)
{
}

/**
 * nyx_reactable_get_player:
 * @reactable: a #NyxReactable
 *
 * Get the [class@Nyx.Player] that this reactable is reacting to.
 *
 * This is meant to be used in implementations where reaction goes the
 * other way around (from enhancer plugin to the player). For example
 * some external event needs to influence parent player object like
 * changing its state, seeking, etc.
 *
 * Note that enhancers are working in a non-main application thread, thus
 * if you need to do operations on a [class@Nyx.Queue] such as adding/removing
 * items, you need to switch thread first. Otherwise this will not be thread safe
 * for applications that use single threaded toolkits such as #GTK. You can do this
 * manually or use provided reactable convenience functions.
 *
 * Due to the threaded nature, you should also avoid comparisons to the current
 * properties values in the player or its queue. While these are thread safe, there
 * is no guarantee that values/objects between threads are still the same in both
 * (or still exist). For example, instead of using [property@Nyx.Queue:current_item],
 * monitor it with implemented [vfunc@Nyx.Reactable.played_item_changed] instead,
 * as these functions are all serialized into your implementation thread.
 *
 * Returns: (transfer full) (nullable): A reference to the parent #NyxPlayer.
 *
 * Since: 0.10
 */
NyxPlayer *
nyx_reactable_get_player (NyxReactable *self)
{
  g_return_val_if_fail (NYX_IS_REACTABLE (self), NULL);

  return NYX_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (self)));
}

/**
 * nyx_reactable_queue_append_sync:
 * @reactable: a #NyxReactable
 * @item: a #NyxMediaItem
 *
 * A convenience function that within application main thread synchronously appends
 * an @item to the playback queue of the player that @reactable belongs to.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
nyx_reactable_queue_append_sync (NyxReactable *self, NyxMediaItem *item)
{
  NyxQueue *queue;

  g_return_if_fail (NYX_IS_REACTABLE (self));
  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));

  NYX_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    nyx_utils_queue_append_on_main_sync (queue, item);
  });
}

/**
 * nyx_reactable_queue_insert_sync:
 * @reactable: a #NyxReactable
 * @item: a #NyxMediaItem
 * @after_item: a #NyxMediaItem after which to insert or %NULL to prepend
 *
 * A convenience function that within application main thread synchronously inserts
 * an @item to the playback queue position after @after_item of the player that
 * @reactable belongs to.
 *
 * This function uses @after_item instead of position index in order to ensure
 * desired position does not change during thread switching.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
nyx_reactable_queue_insert_sync (NyxReactable *self,
    NyxMediaItem *item, NyxMediaItem *after_item)
{
  NyxQueue *queue;

  g_return_if_fail (NYX_IS_REACTABLE (self));
  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));
  g_return_if_fail (after_item == NULL || NYX_IS_MEDIA_ITEM (after_item));

  NYX_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    nyx_utils_queue_insert_on_main_sync (queue, item, after_item);
  });
}

/**
 * nyx_reactable_queue_remove_sync:
 * @reactable: a #NyxReactable
 * @item: a #NyxMediaItem
 *
 * A convenience function that within application main thread synchronously removes
 * an @item from the playback queue of the player that @reactable belongs to.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
nyx_reactable_queue_remove_sync (NyxReactable *self, NyxMediaItem *item)
{
  NyxQueue *queue;

  g_return_if_fail (NYX_IS_REACTABLE (self));
  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));

  NYX_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    nyx_utils_queue_remove_on_main_sync (queue, item);
  });
}

/**
 * nyx_reactable_queue_clear_sync:
 * @reactable: a #NyxReactable
 *
 * A convenience function that within application main thread synchronously clears
 * the playback queue of the player that @reactable belongs to.
 *
 * Reactable enhancers should only modify the queue from the application
 * main thread, switching thread either themselves or using this convenience
 * function that does so.
 *
 * Note that this function will do no operation if called when there is no player
 * set yet (e.g. inside enhancer construction) or if enhancer outlived the parent
 * instance somehow. Both cases are considered to be implementation bug.
 *
 * Since: 0.10
 */
void
nyx_reactable_queue_clear_sync (NyxReactable *self)
{
  NyxQueue *queue;

  g_return_if_fail (NYX_IS_REACTABLE (self));

  NYX_REACTABLE_DO_WITH_QUEUE (self, &queue, {
    nyx_utils_queue_clear_on_main_sync (queue);
  });
}

/**
 * nyx_reactable_timeline_insert_sync:
 * @reactable: a #NyxReactable
 * @timeline: a #NyxTimeline
 * @marker: a #NyxMarker
 *
 * A convenience function that within application main thread synchronously
 * inserts @marker into @timeline.
 *
 * Reactable enhancers should only modify timeline of an item that is already
 * in queue from the application main thread, switching thread either themselves
 * or using this convenience function that does so.
 *
 * Since: 0.10
 */
void
nyx_reactable_timeline_insert_sync (NyxReactable *self,
    NyxTimeline *timeline, NyxMarker *marker)
{
  g_return_if_fail (NYX_IS_REACTABLE (self));
  g_return_if_fail (NYX_IS_TIMELINE (timeline));
  g_return_if_fail (NYX_IS_MARKER (marker));

  nyx_utils_timeline_insert_on_main_sync (timeline, marker);
}

/**
 * nyx_reactable_timeline_remove_sync:
 * @reactable: a #NyxReactable
 * @timeline: a #NyxTimeline
 * @marker: a #NyxMarker
 *
 * A convenience function that within application main thread synchronously
 * removes @marker from @timeline.
 *
 * Reactable enhancers should only modify timeline of an item that is already
 * in queue from the application main thread, switching thread either themselves
 * or using this convenience function that does so.
 *
 * Since: 0.10
 */
void
nyx_reactable_timeline_remove_sync (NyxReactable *self,
    NyxTimeline *timeline, NyxMarker *marker)
{
  g_return_if_fail (NYX_IS_REACTABLE (self));
  g_return_if_fail (NYX_IS_TIMELINE (timeline));
  g_return_if_fail (NYX_IS_MARKER (marker));

  nyx_utils_timeline_remove_on_main_sync (timeline, marker);
}
