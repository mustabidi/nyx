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

/**
 * NyxQueue:
 *
 * A queue of media to be played.
 */

#include <gio/gio.h>

#include "nyx-queue-private.h"
#include "nyx-media-item-private.h"
#include "nyx-player-private.h"
#include "nyx-playbin-bus-private.h"
#include "nyx-reactables-manager-private.h"
#include "nyx-features-manager-private.h"

#define NYX_QUEUE_GET_REC_LOCK(obj) (&NYX_QUEUE_CAST(obj)->rec_lock)
#define NYX_QUEUE_REC_LOCK(obj) g_rec_mutex_lock (NYX_QUEUE_GET_REC_LOCK(obj))
#define NYX_QUEUE_REC_UNLOCK(obj) g_rec_mutex_unlock (NYX_QUEUE_GET_REC_LOCK(obj))

#define DEFAULT_PROGRESSION_MODE NYX_QUEUE_PROGRESSION_NONE
#define DEFAULT_GAPLESS FALSE
#define DEFAULT_INSTANT FALSE

#define GST_CAT_DEFAULT nyx_queue_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxQueue
{
  GstObject parent;

  GRecMutex rec_lock;

  GPtrArray *items;
  NyxMediaItem *current_item;
  guint current_index;

  NyxQueueProgressionMode progression_mode;
  gboolean gapless;
  gboolean instant;

  /* Avoid scenario when "gapless" prop is changed
   * between "about-to-finish" and "EOS" */
  gboolean handled_gapless;
};

enum
{
  PROP_0,
  PROP_CURRENT_ITEM,
  PROP_CURRENT_INDEX,
  PROP_N_ITEMS,
  PROP_PROGRESSION_MODE,
  PROP_GAPLESS,
  PROP_INSTANT,
  PROP_LAST
};

static GType
nyx_queue_list_model_get_item_type (GListModel *model)
{
  return NYX_TYPE_MEDIA_ITEM;
}

static guint
nyx_queue_list_model_get_n_items (GListModel *model)
{
  NyxQueue *self = NYX_QUEUE_CAST (model);
  guint n_items;

  NYX_QUEUE_REC_LOCK (self);
  n_items = self->items->len;
  NYX_QUEUE_REC_UNLOCK (self);

  return n_items;
}

static gpointer
nyx_queue_list_model_get_item (GListModel *model, guint index)
{
  NyxQueue *self = NYX_QUEUE_CAST (model);
  NyxMediaItem *item = NULL;

  NYX_QUEUE_REC_LOCK (self);
  if (G_LIKELY (index < self->items->len)) {
    GST_LOG_OBJECT (self, "Reading queue item: %u", index);
    item = g_object_ref (g_ptr_array_index (self->items, index));
  }
  NYX_QUEUE_REC_UNLOCK (self);

  return item;
}

static void
nyx_queue_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = nyx_queue_list_model_get_item_type;
  iface->get_n_items = nyx_queue_list_model_get_n_items;
  iface->get_item = nyx_queue_list_model_get_item;
}

#define parent_class nyx_queue_parent_class
G_DEFINE_TYPE_WITH_CODE (NyxQueue, nyx_queue, GST_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, nyx_queue_list_model_iface_init));

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void
_announce_model_update (NyxQueue *self, guint index, guint removed, guint added,
    NyxMediaItem *changed_item)
{
  GST_DEBUG_OBJECT (self, "Announcing model update, index: %u, removed: %u, added: %u",
      index, removed, added);

  /* We handle reposition separately */
  if (removed != added) {
    NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

    if (player) {
      gboolean have_features = nyx_player_get_have_features (player);

      if (added == 1) { // addition
        if (player->reactables_manager)
          nyx_reactables_manager_trigger_queue_item_added (player->reactables_manager, changed_item, index);
        if (have_features)
          nyx_features_manager_trigger_queue_item_added (player->features_manager, changed_item, index);
      } else if (removed == 1) { // removal
        if (player->reactables_manager)
          nyx_reactables_manager_trigger_queue_item_removed (player->reactables_manager, changed_item, index);
        if (have_features)
          nyx_features_manager_trigger_queue_item_removed (player->features_manager, changed_item, index);
      } else if (removed > 1 && added == 0) { // queue cleared
        if (player->reactables_manager)
          nyx_reactables_manager_trigger_queue_cleared (player->reactables_manager);
        if (have_features)
          nyx_features_manager_trigger_queue_cleared (player->features_manager);
      } else {
        g_assert_not_reached ();
      }
    }

    gst_clear_object (&player);
  }

  g_list_model_items_changed (G_LIST_MODEL (self), index, removed, added);

  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_N_ITEMS]);
}

static void
_announce_reposition (NyxQueue *self, guint before, guint after)
{
  NyxPlayer *player;

  GST_DEBUG_OBJECT (self, "Announcing item reposition: %u -> %u", before, after);

  if ((player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self)))) {
    if (player->reactables_manager)
      nyx_reactables_manager_trigger_queue_item_repositioned (player->reactables_manager, before, after);
    if (nyx_player_get_have_features (player))
      nyx_features_manager_trigger_queue_item_repositioned (player->features_manager, before, after);

    gst_object_unref (player);
  }
}

/*
 * Notify about current index change. This is needed only if some items
 * are added/removed before current selection, otherwise if selection
 * also changes use _announce_current_item_and_index_change() instead.
 */
static void
_announce_current_index_change (NyxQueue *self)
{
  gboolean is_main_thread = g_main_context_is_owner (g_main_context_default ());

  GST_DEBUG_OBJECT (self, "Announcing current index change from %smain thread, now: %u",
      (is_main_thread) ? "" : "non-", self->current_index);

  if (is_main_thread) {
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_INDEX]);
  } else {
    NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

    if (G_LIKELY (player != NULL)) {
      nyx_app_bus_post_prop_notify (player->app_bus,
          GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);

      gst_object_unref (player);
    }
  }
}

/*
 * Notify about both current item and its index changes.
 * Needs to be called while holding NYX_QUEUE_REC_LOCK.
 */
static void
_announce_current_item_and_index_change (NyxQueue *self)
{
  NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));
  gboolean instant, is_main_thread;

  if (G_UNLIKELY (player == NULL))
    return;

  is_main_thread = g_main_context_is_owner (g_main_context_default ());

  GST_DEBUG_OBJECT (self, "Announcing current item change from %smain thread,"
      " now: %" GST_PTR_FORMAT " (index: %u)",
      (is_main_thread) ? "" : "non-", self->current_item, self->current_index);

  GST_OBJECT_LOCK (self);
  instant = self->instant;
  GST_OBJECT_UNLOCK (self);

  nyx_playbin_bus_post_current_item_change (player->bus, self->current_item,
      (instant) ? NYX_QUEUE_ITEM_CHANGE_INSTANT : NYX_QUEUE_ITEM_CHANGE_NORMAL);

  if (is_main_thread) {
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_ITEM]);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_INDEX]);
  } else {
    nyx_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_ITEM]);
    nyx_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);
  }

  gst_object_unref (player);
}

static inline gboolean
_replace_current_item_unlocked (NyxQueue *self, NyxMediaItem *item, guint index)
{
  if (gst_object_replace ((GstObject **) &self->current_item, GST_OBJECT_CAST (item))) {
    self->current_index = index;

    if (self->current_item)
      nyx_media_item_set_used (self->current_item, TRUE);

    GST_TRACE_OBJECT (self, "Current item replaced, now: %" GST_PTR_FORMAT, self->current_item);

    return TRUE;
  }

  return FALSE;
}

static void
_reset_shuffle_unlocked (NyxQueue *self)
{
  guint i;

  for (i = 0; i < self->items->len; ++i) {
    NyxMediaItem *item = g_ptr_array_index (self->items, i);
    nyx_media_item_set_used (item, FALSE);
  }
}

static NyxMediaItem *
_get_next_item_unlocked (NyxQueue *self, NyxQueueProgressionMode mode)
{
  NyxMediaItem *next_item = NULL;

  GST_DEBUG_OBJECT (self, "Handling progression mode: %u", mode);

  if (self->current_index == NYX_QUEUE_INVALID_POSITION) {
    GST_DEBUG_OBJECT (self, "No current item, can not advance");
    return NULL;
  }

  switch (mode) {
    case NYX_QUEUE_PROGRESSION_NONE:
      break;
    case NYX_QUEUE_PROGRESSION_CAROUSEL:
      next_item = g_ptr_array_index (self->items, 0);
      G_GNUC_FALLTHROUGH;
    case NYX_QUEUE_PROGRESSION_CONSECUTIVE:
      if (self->current_index + 1 < self->items->len)
        next_item = g_ptr_array_index (self->items, self->current_index + 1);
      break;
    case NYX_QUEUE_PROGRESSION_REPEAT_ITEM:
      next_item = self->current_item;
      break;
    case NYX_QUEUE_PROGRESSION_SHUFFLE:{
      GList *unused = NULL;
      GRand *rand = g_rand_new ();
      guint i;

      for (i = 0; i < self->items->len; ++i) {
        NyxMediaItem *item = g_ptr_array_index (self->items, i);

        if (!nyx_media_item_get_used (item))
          unused = g_list_append (unused, item);
      }

      if (unused) {
        next_item = g_list_nth_data (unused,
            g_rand_int_range (rand, 0, g_list_length (unused)));
        g_list_free (unused);
      } else {
        _reset_shuffle_unlocked (self);
        next_item = g_ptr_array_index (self->items,
            g_rand_int_range (rand, 0, self->items->len));
      }

      g_rand_free (rand);
      break;
    }
    default:
      g_assert_not_reached ();
      break;
  }

  if (next_item)
    gst_object_ref (next_item);

  return next_item;
}

static void
_take_item_unlocked (NyxQueue *self, NyxMediaItem *item, gint index)
{
  guint prev_length = self->items->len;

  g_ptr_array_insert (self->items, index, item);
  gst_object_set_parent (GST_OBJECT_CAST (item), GST_OBJECT_CAST (self));

  /* In append we inserted at array length */
  if (index < 0)
    index = prev_length;

  _announce_model_update (self, index, 0, 1, item);

  /* If has selection and inserting before it */
  if (self->current_index != NYX_QUEUE_INVALID_POSITION
      && (guint) index <= self->current_index) {
    self->current_index++;
    _announce_current_index_change (self);
  } else if (prev_length == 0 && _replace_current_item_unlocked (self, item, 0)) {
    /* If queue was empty, auto select first item and announce it */
    _announce_current_item_and_index_change (self);
  } else if (self->current_index == prev_length - 1
      && nyx_queue_get_progression_mode (self) == NYX_QUEUE_PROGRESSION_CONSECUTIVE) {
    NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));
    gboolean after_eos = (gboolean) g_atomic_int_get (&player->eos);

    /* In consecutive progression automatically select next item
     * if we were after EOS of last queue item */
    if (after_eos && _replace_current_item_unlocked (self, item, index))
      _announce_current_item_and_index_change (self);

    gst_object_unref (player);
  }
}

/*
 * For gapless we need to manually replace current item in queue when it starts
 * playing and emit notify about change, this function will do that if necessary
 */
void
nyx_queue_handle_played_item_changed (NyxQueue *self, NyxMediaItem *played_item,
    NyxAppBus *app_bus)
{
  guint index = 0;
  gboolean changed = FALSE;

  NYX_QUEUE_REC_LOCK (self);

  /* Item is often the same here (when selected from queue),
   * so compare pointers first to avoid iterating queue */
  if (played_item != self->current_item
      && g_ptr_array_find (self->items, played_item, &index))
    changed = _replace_current_item_unlocked (self, played_item, index);

  NYX_QUEUE_REC_UNLOCK (self);

  if (changed) {
    nyx_app_bus_post_prop_notify (app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_ITEM]);
    nyx_app_bus_post_prop_notify (app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);
  }
}

/* Must be called from main thread */
void
nyx_queue_handle_playlist (NyxQueue *self, NyxMediaItem *playlist_item,
    GListStore *playlist)
{
  GListModel *playlist_model = G_LIST_MODEL (playlist);
  guint i, index, n_items = g_list_model_get_n_items (playlist_model);

  NYX_QUEUE_REC_LOCK (self);

  /* If playlist item is still in the queue, insert
   * remaining items after it, otherwise append */
  if (G_LIKELY (g_ptr_array_find (self->items, playlist_item, &index)))
    index++;
  else
    index = self->items->len;

  for (i = 1; i < n_items; ++i) {
    NyxMediaItem *item = g_list_model_get_item (playlist_model, i);
    _take_item_unlocked (self, item, index++);
  }

  NYX_QUEUE_REC_UNLOCK (self);
}

void
nyx_queue_handle_about_to_finish (NyxQueue *self, NyxPlayer *player)
{
  NyxMediaItem *next_item;
  NyxQueueProgressionMode progression_mode;

  GST_INFO_OBJECT (self, "Handling \"about-to-finish\"");

  GST_OBJECT_LOCK (self);
  if (!(self->handled_gapless = self->gapless)) {
    GST_OBJECT_UNLOCK (self);
    return;
  }
  progression_mode = self->progression_mode;
  GST_OBJECT_UNLOCK (self);

  NYX_QUEUE_REC_LOCK (self);
  next_item = _get_next_item_unlocked (self, progression_mode);
  NYX_QUEUE_REC_UNLOCK (self);

  if (next_item) {
    nyx_player_set_pending_item (player, next_item, NYX_QUEUE_ITEM_CHANGE_GAPLESS);
    gst_object_unref (next_item);
  }
}

gboolean
nyx_queue_handle_eos (NyxQueue *self, NyxPlayer *player)
{
  NyxMediaItem *next_item = NULL;
  NyxQueueProgressionMode progression_mode;
  gboolean handled_eos = FALSE;

  /* On gapless "about-to-finish" selects next item instead and
   * we can reach EOS only if there was either nothing to select or
   * some playback error ocurred */

  GST_INFO_OBJECT (self, "Handling EOS");

  GST_OBJECT_LOCK (self);
  if (self->handled_gapless) {
    self->handled_gapless = FALSE; // reset
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }
  progression_mode = self->progression_mode;
  GST_OBJECT_UNLOCK (self);

  NYX_QUEUE_REC_LOCK (self);
  if ((next_item = _get_next_item_unlocked (self, progression_mode))) {
    if (next_item == self->current_item)
      nyx_player_seek (player, 0);
    else
      nyx_queue_select_item (self, next_item);

    handled_eos = TRUE;
    gst_object_unref (next_item);
  }
  NYX_QUEUE_REC_UNLOCK (self);

  return handled_eos;
}

/*
 * nyx_queue_new:
 *
 * Returns: (transfer full): a new #NyxQueue instance
 */
NyxQueue *
nyx_queue_new (void)
{
  NyxQueue *queue;

  queue = g_object_new (NYX_TYPE_QUEUE, NULL);
  gst_object_ref_sink (queue);

  return queue;
}

/**
 * nyx_queue_add_item:
 * @queue: a #NyxQueue
 * @item: a #NyxMediaItem
 *
 * Add another #NyxMediaItem to the end of queue.
 *
 * If item is already in queue, this function will do nothing,
 * so it is safe to call multiple times if unsure.
 */
void
nyx_queue_add_item (NyxQueue *self, NyxMediaItem *item)
{
  nyx_queue_insert_item (self, item, -1);
}

/**
 * nyx_queue_insert_item:
 * @queue: a #NyxQueue
 * @item: a #NyxMediaItem
 * @index: the index to place @item in queue, -1 to append
 *
 * Insert another #NyxMediaItem at @index position to the queue.
 *
 * If item is already in queue, this function will do nothing,
 * so it is safe to call multiple times if unsure.
 */
void
nyx_queue_insert_item (NyxQueue *self, NyxMediaItem *item, gint index)
{
  g_return_if_fail (NYX_IS_QUEUE (self));
  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));
  g_return_if_fail (index >= -1);

  NYX_QUEUE_REC_LOCK (self);

  if (!g_ptr_array_find (self->items, item, NULL))
    _take_item_unlocked (self, gst_object_ref (item), index);

  NYX_QUEUE_REC_UNLOCK (self);
}

/**
 * nyx_queue_insert_item_after:
 * @queue: a #NyxQueue
 * @item: a #NyxMediaItem
 * @after_item: (nullable): a #NyxMediaItem after which to
 *   insert @item or %NULL to prepend
 *
 * Insert another #NyxMediaItem after some other item position.
 *
 * If @after_item is %NULL, item will be prepended. When set but
 * not found however, item will be appended at the end of queue.
 *
 * If item is already in queue, this function will do nothing,
 * so it is safe to call multiple times if unsure.
 *
 * Since: 0.10
 */
void
nyx_queue_insert_item_after (NyxQueue *self, NyxMediaItem *item,
    NyxMediaItem *after_item)
{
  g_return_if_fail (NYX_IS_QUEUE (self));
  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));
  g_return_if_fail (after_item == NULL || NYX_IS_MEDIA_ITEM (after_item));

  NYX_QUEUE_REC_LOCK (self);

  if (!g_ptr_array_find (self->items, item, NULL)) {
    guint index;

    if (after_item) {
      if (g_ptr_array_find (self->items, after_item, &index))
        index++;
      else
        index = self->items->len; // Append if not found
    } else {
      index = 0;
    }

    _take_item_unlocked (self, gst_object_ref (item), index);
  }

  NYX_QUEUE_REC_UNLOCK (self);
}

/**
 * nyx_queue_reposition_item:
 * @queue: a #NyxQueue
 * @item: a #NyxMediaItem
 * @index: the index to place @item in queue, -1 to place at the end
 *
 * Change position of one #NyxMediaItem within the queue.
 *
 * Note that the @index is the new position you expect item to be
 * after whole reposition operation is finished.
 *
 * If item is not in the queue, this function will do nothing.
 */
void
nyx_queue_reposition_item (NyxQueue *self, NyxMediaItem *item, gint index)
{
  guint index_old = 0;

  g_return_if_fail (NYX_IS_QUEUE (self));
  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));
  g_return_if_fail (index >= -1);

  NYX_QUEUE_REC_LOCK (self);

  if (g_ptr_array_find (self->items, item, &index_old)) {
    NyxMediaItem *removed_item;
    guint index_new, start_index, end_index, n_changed;

    index_new = (index < 0)
        ? self->items->len - 1
        : (guint) index;

    GST_DEBUG_OBJECT (self, "Reposition item %u -> %u, is_current: %s",
        index_old, index_new, (item == self->current_item) ? "yes" : "no");

    removed_item = g_ptr_array_steal_index (self->items, index_old);
    g_ptr_array_insert (self->items, index_new, removed_item);

    _announce_reposition (self, index_old, index_new);

    if (self->current_index != NYX_QUEUE_INVALID_POSITION) {
      guint before = self->current_index;

      if (index_old > self->current_index && index_new <= self->current_index)
        self->current_index++; // Moved before current item
      else if (index_old < self->current_index && index_new >= self->current_index)
        self->current_index--; // Moved after current item
      else if (index_old == self->current_index)
        self->current_index = index_new; // Moved current item

      if (self->current_index != before)
        _announce_current_index_change (self);
    }

    start_index = MIN (index_old, index_new);
    end_index = MAX (index_old, index_new);
    n_changed = end_index - start_index + 1;

    _announce_model_update (self, start_index, n_changed, n_changed, item);
  }

  NYX_QUEUE_REC_UNLOCK (self);
}

/**
 * nyx_queue_remove_item:
 * @queue: a #NyxQueue
 * @item: a #NyxMediaItem
 *
 * Removes #NyxMediaItem from the queue.
 *
 * If item either was never in the queue or was removed from
 * it earlier, this function will do nothing, so it is safe
 * to call multiple times if unsure.
 */
void
nyx_queue_remove_item (NyxQueue *self, NyxMediaItem *item)
{
  guint index = 0;

  g_return_if_fail (NYX_IS_QUEUE (self));
  g_return_if_fail (NYX_IS_MEDIA_ITEM (item));

  NYX_QUEUE_REC_LOCK (self);

  if (g_ptr_array_find (self->items, item, &index))
    nyx_queue_remove_index (self, index);

  NYX_QUEUE_REC_UNLOCK (self);
}

/**
 * nyx_queue_remove_index:
 * @queue: a #NyxQueue
 * @index: an item index
 *
 * Removes #NyxMediaItem at @index from the queue.
 */
void
nyx_queue_remove_index (NyxQueue *self, guint index)
{
  NyxMediaItem *item = nyx_queue_steal_index (self, index);
  gst_clear_object (&item);
}

/**
 * nyx_queue_steal_index:
 * @queue: a #NyxQueue
 * @index: an item index
 *
 * Removes #NyxMediaItem at @index from the queue.
 *
 * Returns: (transfer full) (nullable): The removed #NyxMediaItem at @index.
 */
NyxMediaItem *
nyx_queue_steal_index (NyxQueue *self, guint index)
{
  NyxMediaItem *removed_item = NULL;

  g_return_val_if_fail (NYX_IS_QUEUE (self), NULL);
  g_return_val_if_fail (index != NYX_QUEUE_INVALID_POSITION, NULL);

  NYX_QUEUE_REC_LOCK (self);

  if (index < self->items->len) {
    if (index == self->current_index
        && _replace_current_item_unlocked (self, NULL, NYX_QUEUE_INVALID_POSITION)) {
      _announce_current_item_and_index_change (self);
    } else if (self->current_index != NYX_QUEUE_INVALID_POSITION
        && index < self->current_index) {
      /* If has selection and removed before it */
      self->current_index--;
      _announce_current_index_change (self);
    }

    removed_item = g_ptr_array_steal_index (self->items, index);
    gst_object_unparent (GST_OBJECT_CAST (removed_item));

    _announce_model_update (self, index, 1, 0, removed_item);
  }

  NYX_QUEUE_REC_UNLOCK (self);

  return removed_item;
}

/**
 * nyx_queue_clear:
 * @queue: a #NyxQueue
 *
 * Removes all media items from the queue.
 *
 * If queue is empty, this function will do nothing,
 * so it is safe to call multiple times if unsure.
 */
void
nyx_queue_clear (NyxQueue *self)
{
  guint n_items;

  g_return_if_fail (NYX_IS_QUEUE (self));

  NYX_QUEUE_REC_LOCK (self);

  n_items = self->items->len;

  if (n_items > 0) {
    if (_replace_current_item_unlocked (self, NULL, NYX_QUEUE_INVALID_POSITION))
      _announce_current_item_and_index_change (self);

    g_ptr_array_remove_range (self->items, 0, n_items);
    _announce_model_update (self, 0, n_items, 0, NULL);
  }

  NYX_QUEUE_REC_UNLOCK (self);
}

/**
 * nyx_queue_select_item:
 * @queue: a #NyxQueue
 * @item: (nullable): a #NyxMediaItem or %NULL to unselect
 *
 * Selects #NyxMediaItem from @queue as current one or
 * unselects currently selected item when @item is %NULL.
 *
 * Returns: %TRUE if item could be selected/unselected,
 *   %FALSE if it was not in the queue.
 */
gboolean
nyx_queue_select_item (NyxQueue *self, NyxMediaItem *item)
{
  gboolean success = FALSE;
  guint index = 0;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);
  g_return_val_if_fail (item == NULL || NYX_IS_MEDIA_ITEM (item), FALSE);

  NYX_QUEUE_REC_LOCK (self);
  if (!item)
    success = nyx_queue_select_index (self, NYX_QUEUE_INVALID_POSITION);
  else if (g_ptr_array_find (self->items, item, &index))
    success = nyx_queue_select_index (self, index);
  NYX_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * nyx_queue_select_index:
 * @queue: a #NyxQueue
 * @index: an item index or [const@Nyx.QUEUE_INVALID_POSITION] to unselect
 *
 * Selects #NyxMediaItem at @index from @queue as current one or
 * unselects currently selected index when @index is [const@Nyx.QUEUE_INVALID_POSITION].
 *
 * Returns: %TRUE if item at @index could be selected/unselected,
 *   %FALSE if index was out of queue range.
 */
gboolean
nyx_queue_select_index (NyxQueue *self, guint index)
{
  NyxMediaItem *item = NULL;
  gboolean success;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);

  NYX_QUEUE_REC_LOCK (self);
  if (index != NYX_QUEUE_INVALID_POSITION && index < self->items->len)
    item = g_ptr_array_index (self->items, index);
  if ((success = (index == NYX_QUEUE_INVALID_POSITION
      || index < self->items->len))) {
    if (_replace_current_item_unlocked (self, item, index))
      _announce_current_item_and_index_change (self);
  }
  NYX_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * nyx_queue_select_next_item:
 * @queue: a #NyxQueue
 *
 * Selects next #NyxMediaItem from @queue for playback.
 *
 * Note that this will try to select next item in the order
 * of the queue, regardless of [enum@Nyx.QueueProgressionMode] set.
 *
 * Returns: %TRUE if there was another media item in queue, %FALSE otherwise.
 */
gboolean
nyx_queue_select_next_item (NyxQueue *self)
{
  gboolean success = FALSE;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);

  NYX_QUEUE_REC_LOCK (self);

  if (self->current_index != NYX_QUEUE_INVALID_POSITION
      && self->current_index < self->items->len - 1) {
    GST_DEBUG_OBJECT (self, "Selecting next queue item");
    success = nyx_queue_select_index (self, self->current_index + 1);
  }

  NYX_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * nyx_queue_select_previous_item:
 * @queue: a #NyxQueue
 *
 * Selects previous #NyxMediaItem from @queue for playback.
 *
 * Note that this will try to select previous item in the order
 * of the queue, regardless of [enum@Nyx.QueueProgressionMode] set.
 *
 * Returns: %TRUE if there was previous media item in queue, %FALSE otherwise.
 */
gboolean
nyx_queue_select_previous_item (NyxQueue *self)
{
  gboolean success = FALSE;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);

  NYX_QUEUE_REC_LOCK (self);

  if (self->current_index != NYX_QUEUE_INVALID_POSITION
      && self->current_index > 0) {
    GST_DEBUG_OBJECT (self, "Selecting previous queue item");
    success = nyx_queue_select_index (self, self->current_index - 1);
  }

  NYX_QUEUE_REC_UNLOCK (self);

  return success;
}

/**
 * nyx_queue_get_item: (skip)
 * @queue: a #NyxQueue
 * @index: an item index
 *
 * Get the #NyxMediaItem at index.
 *
 * This behaves the same as [method@Gio.ListModel.get_item], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * This function is not available in bindings as they already
 * inherit `get_item()` method from [iface@Gio.ListModel] interface.
 *
 * Returns: (transfer full) (nullable): The #NyxMediaItem at @index.
 */
NyxMediaItem *
nyx_queue_get_item (NyxQueue *self, guint index)
{
  g_return_val_if_fail (NYX_IS_QUEUE (self), NULL);

  return g_list_model_get_item (G_LIST_MODEL (self), index);
}

/**
 * nyx_queue_get_current_item:
 * @queue: a #NyxQueue
 *
 * Get the currently selected #NyxMediaItem.
 *
 * Returns: (transfer full) (nullable): The current #NyxMediaItem.
 */
NyxMediaItem *
nyx_queue_get_current_item (NyxQueue *self)
{
  NyxMediaItem *item = NULL;

  /* XXX: For updating media item during playback we should
   * use `player->played_item` instead to not be racy when
   * changing and updating current item at the same time */

  g_return_val_if_fail (NYX_IS_QUEUE (self), NULL);

  NYX_QUEUE_REC_LOCK (self);
  if (self->current_item)
    item = gst_object_ref (self->current_item);
  NYX_QUEUE_REC_UNLOCK (self);

  return item;
}

/**
 * nyx_queue_get_current_index:
 * @queue: a #NyxQueue
 *
 * Get index of the currently selected #NyxMediaItem.
 *
 * Returns: Current item index or [const@Nyx.QUEUE_INVALID_POSITION]
 *   when nothing is selected.
 */
guint
nyx_queue_get_current_index (NyxQueue *self)
{
  guint index;

  g_return_val_if_fail (NYX_IS_QUEUE (self), NYX_QUEUE_INVALID_POSITION);

  NYX_QUEUE_REC_LOCK (self);
  index = self->current_index;
  NYX_QUEUE_REC_UNLOCK (self);

  return index;
}

/**
 * nyx_queue_item_is_current:
 * @queue: a #NyxQueue
 * @item: a #NyxMediaItem to check
 *
 * Checks if given #NyxMediaItem is currently selected.
 *
 * Returns: %TRUE if @item is a current media item, %FALSE otherwise.
 */
gboolean
nyx_queue_item_is_current (NyxQueue *self, NyxMediaItem *item)
{
  gboolean is_current;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);
  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (item), FALSE);

  NYX_QUEUE_REC_LOCK (self);
  is_current = (item == self->current_item);
  NYX_QUEUE_REC_UNLOCK (self);

  return is_current;
}

/**
 * nyx_queue_find_item:
 * @queue: a #NyxQueue
 * @item: a #NyxMediaItem to search for
 * @index: (optional) (out): return location for the index of
 *   the element, if found
 *
 * Get the index of #NyxMediaItem within #NyxQueue.
 *
 * Returns: %TRUE if @item is one of the elements of queue.
 */
gboolean
nyx_queue_find_item (NyxQueue *self, NyxMediaItem *item, guint *index)
{
  gboolean found;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);
  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (item), FALSE);

  NYX_QUEUE_REC_LOCK (self);
  found = g_ptr_array_find (self->items, item, index);
  NYX_QUEUE_REC_UNLOCK (self);

  return found;
}

/**
 * nyx_queue_get_n_items: (skip)
 * @queue: a #NyxQueue
 *
 * Get the number of items in #NyxQueue.
 *
 * This behaves the same as [method@Gio.ListModel.get_n_items], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * This function is not available in bindings as they already
 * inherit get_n_items() method from #GListModel interface.
 *
 * Returns: The number of items in #NyxQueue.
 */
guint
nyx_queue_get_n_items (NyxQueue *self)
{
  g_return_val_if_fail (NYX_IS_QUEUE (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self));
}

/**
 * nyx_queue_set_progression_mode:
 * @queue: a #NyxQueue
 * @mode: a #NyxQueueProgressionMode
 *
 * Set the #NyxQueueProgressionMode of the #NyxQueue.
 *
 * Changing the mode set will alter next item selection at the
 * end of playback. For possible values and their descriptions,
 * see #NyxQueueProgressionMode documentation.
 */
void
nyx_queue_set_progression_mode (NyxQueue *self, NyxQueueProgressionMode mode)
{
  gboolean changed;

  g_return_if_fail (NYX_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->progression_mode != mode))
    self->progression_mode = mode;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

    /* Start shuffle from the current item, allowing
     * reselecting past items already used without it */
    if (mode == NYX_QUEUE_PROGRESSION_SHUFFLE) {
      NYX_QUEUE_REC_LOCK (self);

      _reset_shuffle_unlocked (self);
      if (self->current_item)
        nyx_media_item_set_used (self->current_item, TRUE);

      NYX_QUEUE_REC_UNLOCK (self);
    }

    nyx_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_PROGRESSION_MODE]);
    if (player->reactables_manager)
      nyx_reactables_manager_trigger_queue_progression_changed (player->reactables_manager, mode);
    if (nyx_player_get_have_features (player))
      nyx_features_manager_trigger_queue_progression_changed (player->features_manager, mode);

    gst_object_unref (player);
  }
}

/**
 * nyx_queue_get_progression_mode:
 * @queue: a #NyxQueue
 *
 * Get the #NyxQueueProgressionMode of the #NyxQueue.
 *
 * Returns: a currently set #NyxQueueProgressionMode.
 */
NyxQueueProgressionMode
nyx_queue_get_progression_mode (NyxQueue *self)
{
  NyxQueueProgressionMode mode;

  g_return_val_if_fail (NYX_IS_QUEUE (self), DEFAULT_PROGRESSION_MODE);

  GST_OBJECT_LOCK (self);
  mode = self->progression_mode;
  GST_OBJECT_UNLOCK (self);

  return mode;
}

/**
 * nyx_queue_set_gapless:
 * @queue: a #NyxQueue
 * @gapless: %TRUE to enable, %FALSE otherwise.
 *
 * Set #NyxQueue progression to be gapless.
 *
 * Gapless playback will try to re-use as much as possible of underlying
 * GStreamer elements when #NyxQueue progresses, removing any
 * potential gap in the data.
 *
 * Enabling this option mostly makes sense when used together with
 * [property@Nyx.Queue:progression-mode] property set to
 * [enum@Nyx.QueueProgressionMode.CONSECUTIVE].
 *
 * NOTE: This feature within GStreamer is rather new and
 * might still cause playback issues. Disabled by default.
 */
void
nyx_queue_set_gapless (NyxQueue *self, gboolean gapless)
{
  gboolean changed;

  g_return_if_fail (NYX_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->gapless != gapless))
    self->gapless = gapless;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

    nyx_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_GAPLESS]);

    gst_object_unref (player);
  }
}

/**
 * nyx_queue_get_gapless:
 * @queue: a #NyxQueue
 *
 * Get if #NyxQueue is set to use gapless progression.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
nyx_queue_get_gapless (NyxQueue *self)
{
  gboolean gapless;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);

  GST_OBJECT_LOCK (self);
  gapless = self->gapless;
  GST_OBJECT_UNLOCK (self);

  return gapless;
}

/**
 * nyx_queue_set_instant:
 * @queue: a #NyxQueue
 * @instant: %TRUE to enable, %FALSE otherwise.
 *
 * Set #NyxQueue media item changes to be instant.
 *
 * Instant will try to re-use as much as possible of underlying
 * GStreamer elements when #NyxMediaItem is selected, allowing
 * media item change requests to be faster.
 *
 * NOTE: This feature within GStreamer is rather new and
 * might still cause playback issues. Disabled by default.
 */
void
nyx_queue_set_instant (NyxQueue *self, gboolean instant)
{
  gboolean changed;

  g_return_if_fail (NYX_IS_QUEUE (self));

  GST_OBJECT_LOCK (self);
  if ((changed = self->instant != instant))
    self->instant = instant;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

    nyx_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_INSTANT]);

    gst_object_unref (player);
  }
}

/**
 * nyx_queue_get_instant:
 * @queue: a #NyxQueue
 *
 * Get if #NyxQueue is set to use instant media item changes.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 */
gboolean
nyx_queue_get_instant (NyxQueue *self)
{
  gboolean instant;

  g_return_val_if_fail (NYX_IS_QUEUE (self), FALSE);

  GST_OBJECT_LOCK (self);
  instant = self->instant;
  GST_OBJECT_UNLOCK (self);

  return instant;
}

static void
_item_remove_func (NyxMediaItem *item)
{
  gst_object_unparent (GST_OBJECT_CAST (item));
  gst_object_unref (item);
}

static void
nyx_queue_init (NyxQueue *self)
{
  g_rec_mutex_init (&self->rec_lock);

  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify) _item_remove_func);

  self->current_index = NYX_QUEUE_INVALID_POSITION;
  self->progression_mode = DEFAULT_PROGRESSION_MODE;
  self->gapless = DEFAULT_GAPLESS;
  self->instant = DEFAULT_INSTANT;
}

static void
nyx_queue_finalize (GObject *object)
{
  NyxQueue *self = NYX_QUEUE_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_rec_mutex_clear (&self->rec_lock);

  gst_clear_object (&self->current_item);
  g_ptr_array_unref (self->items);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_queue_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxQueue *self = NYX_QUEUE_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_ITEM:
      g_value_take_object (value, nyx_queue_get_current_item (self));
      break;
    case PROP_CURRENT_INDEX:
      g_value_set_uint (value, nyx_queue_get_current_index (self));
      break;
    case PROP_N_ITEMS:
      g_value_set_uint (value, nyx_queue_get_n_items (self));
      break;
    case PROP_PROGRESSION_MODE:
      g_value_set_enum (value, nyx_queue_get_progression_mode (self));
      break;
    case PROP_GAPLESS:
      g_value_set_boolean (value, nyx_queue_get_gapless (self));
      break;
    case PROP_INSTANT:
      g_value_set_boolean (value, nyx_queue_get_instant (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_queue_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxQueue *self = NYX_QUEUE_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_INDEX:
      nyx_queue_select_index (self, g_value_get_uint (value));
      break;
    case PROP_PROGRESSION_MODE:
      nyx_queue_set_progression_mode (self, g_value_get_enum (value));
      break;
    case PROP_GAPLESS:
      nyx_queue_set_gapless (self, g_value_get_boolean (value));
      break;
    case PROP_INSTANT:
      nyx_queue_set_instant (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_queue_class_init (NyxQueueClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxqueue", 0,
      "Nyx Queue");

  gobject_class->get_property = nyx_queue_get_property;
  gobject_class->set_property = nyx_queue_set_property;
  gobject_class->finalize = nyx_queue_finalize;

  /**
   * NyxQueue:current-item:
   *
   * Currently selected media item for playback.
   */
  param_specs[PROP_CURRENT_ITEM] = g_param_spec_object ("current-item",
      NULL, NULL, NYX_TYPE_MEDIA_ITEM,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxQueue:current-index:
   *
   * Index of currently selected media item for playback.
   */
  param_specs[PROP_CURRENT_INDEX] = g_param_spec_uint ("current-index",
      NULL, NULL, 0, G_MAXUINT, NYX_QUEUE_INVALID_POSITION,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxQueue:n-items:
   *
   * Number of media items in the queue.
   */
  param_specs[PROP_N_ITEMS] = g_param_spec_uint ("n-items",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxQueue:progression-mode:
   *
   * Queue progression mode.
   */
  param_specs[PROP_PROGRESSION_MODE] = g_param_spec_enum ("progression-mode",
      NULL, NULL, NYX_TYPE_QUEUE_PROGRESSION_MODE, DEFAULT_PROGRESSION_MODE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxQueue:gapless:
   *
   * Use gapless progression.
   */
  param_specs[PROP_GAPLESS] = g_param_spec_boolean ("gapless",
      NULL, NULL, DEFAULT_GAPLESS,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxQueue:instant:
   *
   * Use instant media item changes.
   */
  param_specs[PROP_INSTANT] = g_param_spec_boolean ("instant",
      NULL, NULL, DEFAULT_INSTANT,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
