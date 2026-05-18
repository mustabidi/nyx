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

#include "nyx-app-queue-progression-model.h"
#include "nyx-app-queue-progression-item.h"
#include "nyx-app-utils.h"

#define N_PROGRESSION_MODES 5

struct _NyxAppQueueProgressionModel
{
  GObject parent;

  GListStore *store;
};

static GType
nyx_app_queue_progression_model_get_item_type (GListModel *model)
{
  return NYX_APP_TYPE_QUEUE_PROGRESSION_ITEM;
}

static guint
nyx_app_queue_progression_model_get_n_items (GListModel *model)
{
  NyxAppQueueProgressionModel *self = NYX_APP_QUEUE_PROGRESSION_MODEL_CAST (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->store));
}

static gpointer
nyx_app_queue_progression_model_get_item (GListModel *model, guint index)
{
  NyxAppQueueProgressionModel *self = NYX_APP_QUEUE_PROGRESSION_MODEL_CAST (model);

  return g_list_model_get_item (G_LIST_MODEL (self->store), index);
}

static void
_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = nyx_app_queue_progression_model_get_item_type;
  iface->get_n_items = nyx_app_queue_progression_model_get_n_items;
  iface->get_item = nyx_app_queue_progression_model_get_item;
}

#define parent_class nyx_app_queue_progression_model_parent_class
G_DEFINE_TYPE_WITH_CODE (NyxAppQueueProgressionModel, nyx_app_queue_progression_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, _list_model_iface_init));

static void
nyx_app_queue_progression_model_init (NyxAppQueueProgressionModel *self)
{
  self->store = g_list_store_new (NYX_APP_TYPE_QUEUE_PROGRESSION_ITEM);
}

static void
nyx_app_queue_progression_model_constructed (GObject *object)
{
  NyxAppQueueProgressionModel *self = NYX_APP_QUEUE_PROGRESSION_MODEL_CAST (object);
  guint i;

  for (i = 0; i < N_PROGRESSION_MODES; ++i) {
    NyxAppQueueProgressionItem *item;
    const gchar *icon = NULL, *label = NULL;

    nyx_app_utils_parse_progression (i, &icon, &label);

    item = nyx_app_queue_progression_item_new (icon, label);

    g_list_store_append (self->store, item);
    g_object_unref (item);
  }

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
nyx_app_queue_progression_model_finalize (GObject *object)
{
  NyxAppQueueProgressionModel *self = NYX_APP_QUEUE_PROGRESSION_MODEL_CAST (object);

  g_object_unref (self->store);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_app_queue_progression_model_class_init (NyxAppQueueProgressionModelClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = nyx_app_queue_progression_model_constructed;
  gobject_class->finalize = nyx_app_queue_progression_model_finalize;
}
