/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <gst/gst.h>
#include <gtk/gtk.h>

#include "nyx-app-queue-selection.h"

#define GST_CAT_DEFAULT nyx_app_queue_selection_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxAppQueueSelection
{
  GObject parent;

  NyxQueue *queue;

  NyxMediaItem *current_item;
  guint current_position;
};

enum
{
  PROP_0,
  PROP_QUEUE,
  PROP_LAST
};

enum
{
  SIGNAL_ITEM_SELECTED,
  SIGNAL_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

static GType
nyx_app_queue_selection_get_item_type (GListModel *model)
{
  return NYX_TYPE_MEDIA_ITEM;
}

static guint
nyx_app_queue_selection_get_n_items (GListModel *model)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (model);

  return (self->queue) ? nyx_queue_get_n_items (self->queue) : 0;
}

static gpointer
nyx_app_queue_selection_get_item (GListModel *model, guint index)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (model);

  return (self->queue) ? nyx_queue_get_item (self->queue, index) : NULL;
}

static void
_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = nyx_app_queue_selection_get_item_type;
  iface->get_n_items = nyx_app_queue_selection_get_n_items;
  iface->get_item = nyx_app_queue_selection_get_item;
}

static inline void
_refresh_current_selection (NyxAppQueueSelection *self)
{
  guint position, old_position, index, n_changed;

  position = nyx_queue_get_current_index (self->queue);

  /* Nyx -> GTK expected value change.
   * Should be the same, but better be safe. */
  if (position == NYX_QUEUE_INVALID_POSITION)
    position = GTK_INVALID_LIST_POSITION;

  /* No change */
  if (position == self->current_position)
    return;

  old_position = self->current_position;
  self->current_position = position;

  if (old_position == GTK_INVALID_LIST_POSITION) {
    index = position;
    n_changed = 1;
  } else if (position == GTK_INVALID_LIST_POSITION) {
    index = old_position;
    n_changed = 1;
  } else if (position < old_position) {
    index = position;
    n_changed = old_position - position + 1;
  } else {
    index = old_position;
    n_changed = position - old_position + 1;
  }

  GST_DEBUG ("Selection changed, index: %u, n_changed: %u", index, n_changed);
  gtk_selection_model_selection_changed (GTK_SELECTION_MODEL (self), index, n_changed);
}

static gboolean
nyx_app_queue_selection_is_selected (GtkSelectionModel *model, guint position)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (model);

  return (position == self->current_position);
}

static GtkBitset *
nyx_app_queue_selection_get_selection_in_range (GtkSelectionModel *model, guint position, guint n_items)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (model);
  GtkBitset *bitset = gtk_bitset_new_empty ();

  if (self->current_position != GTK_INVALID_LIST_POSITION
      && position <= self->current_position
      && position + n_items > self->current_position)
    gtk_bitset_add (bitset, self->current_position);

  return bitset;
}

static gboolean
nyx_app_queue_selection_select_item (GtkSelectionModel *model, guint position, gboolean exclusive)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (model);
  gboolean res = TRUE;

  if (G_UNLIKELY (self->queue == NULL))
    return FALSE;

  /* Disallow reselecting of the same item */
  if (self->current_position != position)
    res = nyx_queue_select_index (self->queue, position);

  /* Need to always emit this signal when select item succeeds */
  if (G_LIKELY (res))
    g_signal_emit (self, signals[SIGNAL_ITEM_SELECTED], 0, position);

  return res;
}

static gboolean
nyx_app_queue_selection_unselect_item (GtkSelectionModel *model, guint position)
{
  return FALSE;
}

static void
_selection_model_iface_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = nyx_app_queue_selection_is_selected;
  iface->get_selection_in_range = nyx_app_queue_selection_get_selection_in_range;
  iface->select_item = nyx_app_queue_selection_select_item;
  iface->unselect_item = nyx_app_queue_selection_unselect_item;
}

#define parent_class nyx_app_queue_selection_parent_class
G_DEFINE_TYPE_WITH_CODE (NyxAppQueueSelection, nyx_app_queue_selection, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, _list_model_iface_init)
    G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL, _selection_model_iface_init))

static void
_queue_model_items_changed_cb (GListModel *model, guint position, guint removed, guint added,
    NyxAppQueueSelection *self)
{
  /* Forward event from internal model */
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
_queue_current_index_changed_cb (NyxQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, NyxAppQueueSelection *self)
{
  _refresh_current_selection (self);
}

/*
 * nyx_app_queue_selection_new:
 * @queue: (nullable): a #NyxQueue
 *
 * Creates a new #NyxAppQueueSelection instance.
 *
 * Returns: (transfer full): a new #NyxAppQueueSelection.
 */
NyxAppQueueSelection *
nyx_app_queue_selection_new (NyxQueue *queue)
{
  return g_object_new (NYX_APP_TYPE_QUEUE_SELECTION, "queue", queue, NULL);
}

/*
 * nyx_app_queue_selection_set_queue:
 * @selection: a #NyxAppQueueSelection
 * @queue: a #NyxQueue
 *
 * Set #NyxQueue to be managed by this selection model.
 */
void
nyx_app_queue_selection_set_queue (NyxAppQueueSelection *self, NyxQueue *queue)
{
  guint n_before = 0, n_after = 0;

  g_return_if_fail (NYX_APP_IS_QUEUE_SELECTION (self));
  g_return_if_fail (NYX_IS_QUEUE (queue));

  if (self->queue) {
    g_signal_handlers_disconnect_by_func (G_LIST_MODEL (self->queue), _queue_model_items_changed_cb, self);
    g_signal_handlers_disconnect_by_func (self->queue, _queue_current_index_changed_cb, self);

    n_before = nyx_queue_get_n_items (self->queue);
  }

  gst_object_replace ((GstObject **) &self->queue, GST_OBJECT_CAST (queue));

  g_signal_connect (G_LIST_MODEL (self->queue), "items-changed",
      G_CALLBACK (_queue_model_items_changed_cb), self);
  g_signal_connect (self->queue, "notify::current-index",
      G_CALLBACK (_queue_current_index_changed_cb), self);

  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_QUEUE]);

  n_after = nyx_queue_get_n_items (self->queue);

  /* Refresh selected item after queue change */
  self->current_position = GTK_INVALID_LIST_POSITION;
  _queue_model_items_changed_cb (G_LIST_MODEL (self->queue), 0, n_before, n_after, self);
  _refresh_current_selection (self);
}

/*
 * nyx_app_queue_selection_get_queue:
 * @selection: a #NyxAppQueueSelection
 *
 * Get #NyxQueue managed by this selection model.
 *
 * Returns: (transfer none): #NyxQueue being managed.
 */
NyxQueue *
nyx_app_queue_selection_get_queue (NyxAppQueueSelection *self)
{
  g_return_val_if_fail (NYX_APP_IS_QUEUE_SELECTION (self), NULL);

  return self->queue;
}

static void
nyx_app_queue_selection_init (NyxAppQueueSelection *self)
{
  self->current_position = GTK_INVALID_LIST_POSITION;
}

static void
nyx_app_queue_selection_finalize (GObject *object)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (object);

  if (self->queue) {
    g_signal_handlers_disconnect_by_func (G_LIST_MODEL (self->queue), _queue_model_items_changed_cb, self);
    g_signal_handlers_disconnect_by_func (self->queue, _queue_current_index_changed_cb, self);

    g_object_unref (self->queue);
  }
  g_clear_object (&self->current_item);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_app_queue_selection_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (object);

  switch (prop_id) {
    case PROP_QUEUE:
      g_value_set_object (value, nyx_app_queue_selection_get_queue (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_app_queue_selection_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxAppQueueSelection *self = NYX_APP_QUEUE_SELECTION_CAST (object);

  switch (prop_id) {
    case PROP_QUEUE:
      nyx_app_queue_selection_set_queue (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_app_queue_selection_class_init (NyxAppQueueSelectionClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxappqueueselection", 0,
      "Nyx App Queue Selection");

  gobject_class->get_property = nyx_app_queue_selection_get_property;
  gobject_class->set_property = nyx_app_queue_selection_set_property;
  gobject_class->finalize = nyx_app_queue_selection_finalize;

  /*
   * NyxAppQueueSelection:queue:
   *
   * The queue being managed.
   */
  param_specs[PROP_QUEUE] = g_param_spec_object ("queue",
      NULL, NULL, NYX_TYPE_QUEUE,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /*
   * NyxAppQueueSelection::item-selected:
   * @selection: a #NyxAppQueueSelection
   * @index: an index of selected item
   *
   * Signals when user selected item within the [iface@Gtk.SelectionModel].
   *
   * Note that this signal is emitted only when item gets selected from
   * the GTK side (also when the same item is reselected). If item was
   * changed internally by e.g. progression of [class@Nyx.Queue],
   * this signal will not be emitted.
   *
   * #NyxAppQueueSelection automatically takes care of having its
   * selection in sync with passed [class@Nyx.Queue], so you do not
   * have to listen for the changes. This signal is useful if you need
   * to differentiate what caused item selection, otherwise use either
   * [signal@Gtk.SelectionModel::selection-changed] signal or listen for
   * changes of [property@Nyx.Queue:current-item] property.
   */
  signals[SIGNAL_ITEM_SELECTED] = g_signal_new ("item-selected",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
