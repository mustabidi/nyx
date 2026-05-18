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
 * NyxStreamList:
 *
 * A list of media streams.
 */

#include <gio/gio.h>

#include "nyx-stream-list-private.h"
#include "nyx-stream-private.h"
#include "nyx-player-private.h"
#include "nyx-playbin-bus-private.h"

#define GST_CAT_DEFAULT nyx_stream_list_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxStreamList
{
  GstObject parent;

  GPtrArray *streams;

  NyxStream *current_stream;
  guint current_index;

  gboolean in_refresh;
};

enum
{
  PROP_0,
  PROP_CURRENT_STREAM,
  PROP_CURRENT_INDEX,
  PROP_N_STREAMS,
  PROP_LAST
};

static void nyx_stream_list_model_iface_init (GListModelInterface *iface);

#define parent_class nyx_stream_list_parent_class
G_DEFINE_TYPE_WITH_CODE (NyxStreamList, nyx_stream_list, GST_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, nyx_stream_list_model_iface_init));

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GType
nyx_stream_list_model_get_item_type (GListModel *model)
{
  return NYX_TYPE_STREAM;
}

static guint
nyx_stream_list_model_get_n_items (GListModel *model)
{
  NyxStreamList *self = NYX_STREAM_LIST_CAST (model);
  guint n_streams;

  GST_OBJECT_LOCK (self);
  n_streams = self->streams->len;
  GST_OBJECT_UNLOCK (self);

  return n_streams;
}

static gpointer
nyx_stream_list_model_get_item (GListModel *model, guint index)
{
  NyxStreamList *self = NYX_STREAM_LIST_CAST (model);
  NyxStream *stream = NULL;

  GST_OBJECT_LOCK (self);
  if (G_LIKELY (index < self->streams->len))
    stream = gst_object_ref (g_ptr_array_index (self->streams, index));
  GST_OBJECT_UNLOCK (self);

  return stream;
}

static void
nyx_stream_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = nyx_stream_list_model_get_item_type;
  iface->get_n_items = nyx_stream_list_model_get_n_items;
  iface->get_item = nyx_stream_list_model_get_item;
}

static void
_post_stream_change (NyxStreamList *self)
{
  NyxPlayer *player;

  GST_OBJECT_LOCK (self);
  /* We will do a single initial selection ourselves
   * after all lists are refreshed, so do nothing here yet */
  if (G_UNLIKELY (self->in_refresh)) {
    GST_WARNING_OBJECT (self, "Trying to select/autoselect stream before"
        " initial selection. This is not supported, please fix your app.");
    GST_OBJECT_UNLOCK (self);
    return;
  }
  GST_OBJECT_UNLOCK (self);

  player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

  if (G_LIKELY (player != NULL)) {
    nyx_playbin_bus_post_stream_change (player->bus);
    gst_object_unref (player);
  }
}

static void
_announce_current_stream_and_index_change (NyxStreamList *self)
{
  NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));
  gboolean is_main_thread;

  if (G_UNLIKELY (player == NULL))
    return;

  is_main_thread = g_main_context_is_owner (g_main_context_default ());

  GST_DEBUG_OBJECT (self, "Announcing current stream change from %smain thread,"
      " now: %" GST_PTR_FORMAT " (index: %u)",
      (is_main_thread) ? "" : "non-", self->current_stream, self->current_index);

  if (is_main_thread) {
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_STREAM]);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_CURRENT_INDEX]);
  } else {
    nyx_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_STREAM]);
    nyx_app_bus_post_prop_notify (player->app_bus,
        GST_OBJECT_CAST (self), param_specs[PROP_CURRENT_INDEX]);
  }

  gst_object_unref (player);
}

static gboolean
nyx_stream_list_select_index_unlocked (NyxStreamList *self, guint index)
{
  NyxStream *stream = NULL;

  if (index != NYX_STREAM_LIST_INVALID_POSITION)
    stream = g_ptr_array_index (self->streams, index);

  if (gst_object_replace ((GstObject **) &self->current_stream, GST_OBJECT_CAST (stream))) {
    self->current_index = index;
    return TRUE;
  }

  return FALSE;
}

/*
 * nyx_stream_list_new:
 *
 * Returns: (transfer full): a new #NyxStreamList instance
 */
NyxStreamList *
nyx_stream_list_new (void)
{
  NyxStreamList *list;

  list = g_object_new (NYX_TYPE_STREAM_LIST, NULL);
  gst_object_ref_sink (list);

  return list;
}

/**
 * nyx_stream_list_select_stream:
 * @list: a #NyxStreamList
 * @stream: a #NyxStream
 *
 * Selects #NyxStream from @list to be activated.
 *
 * Returns: %TRUE if stream was in the @list, %FALSE otherwise.
 */
gboolean
nyx_stream_list_select_stream (NyxStreamList *self, NyxStream *stream)
{
  gboolean found, changed = FALSE;
  guint index = 0;

  g_return_val_if_fail (NYX_IS_STREAM_LIST (self), FALSE);
  g_return_val_if_fail (NYX_IS_STREAM (stream), FALSE);

  GST_OBJECT_LOCK (self);
  if ((found = g_ptr_array_find (self->streams, stream, &index)))
    changed = nyx_stream_list_select_index_unlocked (self, index);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    _post_stream_change (self);
    _announce_current_stream_and_index_change (self);
  }

  return found;
}

/**
 * nyx_stream_list_select_index:
 * @list: a #NyxStreamList
 * @index: a stream index
 *
 * Selects #NyxStream at @index from @list as current one.
 *
 * Returns: %TRUE if stream could be selected, %FALSE otherwise.
 */
gboolean
nyx_stream_list_select_index (NyxStreamList *self, guint index)
{
  gboolean found, changed = FALSE;

  g_return_val_if_fail (NYX_IS_STREAM_LIST (self), FALSE);
  g_return_val_if_fail (index != NYX_STREAM_LIST_INVALID_POSITION, FALSE);

  GST_OBJECT_LOCK (self);
  if ((found = index < self->streams->len))
    changed = nyx_stream_list_select_index_unlocked (self, index);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    _post_stream_change (self);
    _announce_current_stream_and_index_change (self);
  }

  return found;
}

/**
 * nyx_stream_list_get_stream:
 * @list: a #NyxStreamList
 * @index: a stream index
 *
 * Get the #NyxStream at index.
 *
 * This behaves the same as [method@Gio.ListModel.get_item], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: (transfer full) (nullable): The #NyxStream at @index.
 */
NyxStream *
nyx_stream_list_get_stream (NyxStreamList *self, guint index)
{
  g_return_val_if_fail (NYX_IS_STREAM_LIST (self), NULL);

  return g_list_model_get_item (G_LIST_MODEL (self), index);
}

/**
 * nyx_stream_list_get_current_stream:
 * @list: a #NyxStreamList
 *
 * Get the currently selected #NyxStream.
 *
 * Returns: (transfer full) (nullable): The current #NyxStream.
 */
NyxStream *
nyx_stream_list_get_current_stream (NyxStreamList *self)
{
  NyxStream *stream = NULL;

  g_return_val_if_fail (NYX_IS_STREAM_LIST (self), NULL);

  GST_OBJECT_LOCK (self);
  if (self->current_stream)
    stream = gst_object_ref (self->current_stream);
  GST_OBJECT_UNLOCK (self);

  return stream;
}

/**
 * nyx_stream_list_get_current_index:
 * @list: a #NyxStreamList
 *
 * Get index of the currently selected #NyxStream.
 *
 * Returns: Current stream index or [const@Nyx.STREAM_LIST_INVALID_POSITION]
 *   when nothing is selected.
 */
guint
nyx_stream_list_get_current_index (NyxStreamList *self)
{
  guint index;

  g_return_val_if_fail (NYX_IS_STREAM_LIST (self), NYX_STREAM_LIST_INVALID_POSITION);

  GST_OBJECT_LOCK (self);
  index = self->current_index;
  GST_OBJECT_UNLOCK (self);

  return index;
}

/**
 * nyx_stream_list_get_n_streams:
 * @list: a #NyxStreamList
 *
 * Get the number of streams in #NyxStreamList.
 *
 * This behaves the same as [method@Gio.ListModel.get_n_items], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: The number of streams in #NyxStreamList.
 */
guint
nyx_stream_list_get_n_streams (NyxStreamList *self)
{
  g_return_val_if_fail (NYX_IS_STREAM_LIST (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self));
}

void
nyx_stream_list_replace_streams (NyxStreamList *self, GList *streams)
{
  GList *st;
  guint prev_n_streams, n_streams;
  guint index = 0, selected_index = 0;
  gboolean changed, selected = FALSE;

  GST_OBJECT_LOCK (self);

  self->in_refresh = TRUE;
  prev_n_streams = self->streams->len;

  if (prev_n_streams > 0)
    g_ptr_array_remove_range (self->streams, 0, prev_n_streams);

  for (st = streams; st != NULL; st = st->next) {
    NyxStream *stream = NYX_STREAM_CAST (st->data);

    /* Try to select first "default" stream, while avoiding
     * streams that should not be selected by default.
     * NOTE: This works only with playbin3 */
    if (!selected) {
      GstStream *gst_stream = nyx_stream_get_gst_stream (stream);
      GstStreamFlags flags = gst_stream_get_stream_flags (gst_stream);

      GST_LOG_OBJECT (self, "Stream flags: %i", flags);

      if ((flags & GST_STREAM_FLAG_SELECT) == GST_STREAM_FLAG_SELECT) {
        GST_DEBUG_OBJECT (self, "Stream has \"select\" stream flag");
        selected = TRUE;
        selected_index = index;
      } else if ((flags & GST_STREAM_FLAG_UNSELECT) == GST_STREAM_FLAG_UNSELECT) {
        GST_DEBUG_OBJECT (self, "Stream has \"unselect\" stream flag");
        if (selected_index == index)
          selected_index++;
      }
    }

    g_ptr_array_add (self->streams, stream);
    gst_object_set_parent (GST_OBJECT_CAST (stream), GST_OBJECT_CAST (self));

    index++;
  }

  n_streams = self->streams->len;

  GST_OBJECT_UNLOCK (self);

  if (prev_n_streams > 0 || n_streams > 0) {
    g_list_model_items_changed (G_LIST_MODEL (self), 0, prev_n_streams, n_streams);

    if (prev_n_streams != n_streams)
      g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_N_STREAMS]);
  }

  /* This can happen when ALL streams had "unselect" flag.
   * In this case just select the first one. */
  if (n_streams > 0) {
    if (G_UNLIKELY (selected_index > n_streams - 1))
      selected_index = 0;
  } else {
    selected_index = NYX_STREAM_LIST_INVALID_POSITION;
  }

  /* TODO: Consider adding an API (or signal that returns index)
   * to select preferred initial stream (by e.g. language) */

  GST_OBJECT_LOCK (self);
  changed = nyx_stream_list_select_index_unlocked (self, selected_index);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_INFO_OBJECT (self, "Initially selecting stream index: %u", selected_index);
    _announce_current_stream_and_index_change (self);
  }

  GST_OBJECT_LOCK (self);
  self->in_refresh = FALSE;
  GST_OBJECT_UNLOCK (self);
}

NyxStream *
nyx_stream_list_get_stream_for_gst_stream (NyxStreamList *self, GstStream *gst_stream)
{
  NyxStream *found_stream = NULL;
  guint i;

  GST_OBJECT_LOCK (self);

  for (i = 0; i < self->streams->len; ++i) {
    NyxStream *stream = g_ptr_array_index (self->streams, i);
    GstStream *list_gst_stream = nyx_stream_get_gst_stream (stream);

    if (gst_stream == list_gst_stream) {
      found_stream = gst_object_ref (stream);
      break;
    }
  }

  GST_OBJECT_UNLOCK (self);

  return found_stream;
}

static void
_stream_remove_func (NyxStream *stream)
{
  gst_object_unparent (GST_OBJECT_CAST (stream));
  gst_object_unref (stream);
}

static void
nyx_stream_list_init (NyxStreamList *self)
{
  self->streams = g_ptr_array_new_with_free_func ((GDestroyNotify) _stream_remove_func);
  self->current_index = NYX_STREAM_LIST_INVALID_POSITION;
}

static void
nyx_stream_list_finalize (GObject *object)
{
  NyxStreamList *self = NYX_STREAM_LIST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_object (&self->current_stream);
  g_ptr_array_unref (self->streams);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_stream_list_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxStreamList *self = NYX_STREAM_LIST_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_STREAM:
      g_value_take_object (value, nyx_stream_list_get_current_stream (self));
      break;
    case PROP_CURRENT_INDEX:
      g_value_set_uint (value, nyx_stream_list_get_current_index (self));
      break;
    case PROP_N_STREAMS:
      g_value_set_uint (value, nyx_stream_list_get_n_streams (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_stream_list_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxStreamList *self = NYX_STREAM_LIST_CAST (object);

  switch (prop_id) {
    case PROP_CURRENT_INDEX:
      nyx_stream_list_select_index (self, g_value_get_uint (value));
      break;
  default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_stream_list_class_init (NyxStreamListClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxstreamlist", 0,
      "Nyx Stream List");

  gobject_class->get_property = nyx_stream_list_get_property;
  gobject_class->set_property = nyx_stream_list_set_property;
  gobject_class->finalize = nyx_stream_list_finalize;

  /**
   * NyxStreamList:current-stream:
   *
   * Currently selected stream.
   */
  param_specs[PROP_CURRENT_STREAM] = g_param_spec_object ("current-stream",
      NULL, NULL, NYX_TYPE_STREAM,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxStreamList:current-index:
   *
   * Index of currently selected stream.
   */
  param_specs[PROP_CURRENT_INDEX] = g_param_spec_uint ("current-index",
      NULL, NULL, 0, G_MAXUINT, NYX_STREAM_LIST_INVALID_POSITION,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxStreamList:n-streams:
   *
   * Number of streams in the list.
   */
  param_specs[PROP_N_STREAMS] = g_param_spec_uint ("n-streams",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
