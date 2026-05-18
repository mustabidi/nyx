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
 * NyxEnhancerProxyList:
 *
 * A list of enhancer proxies.
 *
 * Since: 0.10
 */

#include <gio/gio.h>

#include "nyx-basic-functions.h"
#include "nyx-enhancer-proxy-list-private.h"
#include "nyx-enhancer-proxy-private.h"

#define GST_CAT_DEFAULT nyx_enhancer_proxy_list_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxEnhancerProxyList
{
  GstObject parent;

  GPtrArray *proxies;
};

enum
{
  PROP_0,
  PROP_N_PROXIES,
  PROP_LAST
};

static void nyx_enhancer_proxy_list_model_iface_init (GListModelInterface *iface);

#define parent_class nyx_enhancer_proxy_list_parent_class
G_DEFINE_TYPE_WITH_CODE (NyxEnhancerProxyList, nyx_enhancer_proxy_list, GST_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, nyx_enhancer_proxy_list_model_iface_init));

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GType
nyx_enhancer_proxy_list_model_get_item_type (GListModel *model)
{
  return NYX_TYPE_ENHANCER_PROXY;
}

static guint
nyx_enhancer_proxy_list_model_get_n_items (GListModel *model)
{
  return NYX_ENHANCER_PROXY_LIST_CAST (model)->proxies->len;
}

static gpointer
nyx_enhancer_proxy_list_model_get_item (GListModel *model, guint index)
{
  NyxEnhancerProxyList *self = NYX_ENHANCER_PROXY_LIST_CAST (model);
  NyxEnhancerProxy *proxy = NULL;

  if (G_LIKELY (index < self->proxies->len))
    proxy = gst_object_ref (g_ptr_array_index (self->proxies, index));

  return proxy;
}

static void
nyx_enhancer_proxy_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = nyx_enhancer_proxy_list_model_get_item_type;
  iface->get_n_items = nyx_enhancer_proxy_list_model_get_n_items;
  iface->get_item = nyx_enhancer_proxy_list_model_get_item;
}

/*
 * nyx_enhancer_proxy_list_new_named:
 * @name: (nullable): name of the #GstObject
 *
 * Returns: (transfer full): a new #NyxEnhancerProxyList instance
 */
NyxEnhancerProxyList *
nyx_enhancer_proxy_list_new_named (const gchar *name)
{
  NyxEnhancerProxyList *list;

  list = g_object_new (NYX_TYPE_ENHANCER_PROXY_LIST,
      "name", name, NULL);
  gst_object_ref_sink (list);

  return list;
}

void
nyx_enhancer_proxy_list_take_proxy (NyxEnhancerProxyList *self, NyxEnhancerProxy *proxy)
{
  g_ptr_array_add (self->proxies, proxy);
  gst_object_set_parent (GST_OBJECT_CAST (proxy), GST_OBJECT_CAST (self));
}

/*
 * nyx_enhancer_proxy_list_fill_from_global_proxies:
 *
 * Fill list with unconfigured proxies from global proxies list.
 */
void
nyx_enhancer_proxy_list_fill_from_global_proxies (NyxEnhancerProxyList *self)
{
  NyxEnhancerProxyList *global_list = nyx_get_global_enhancer_proxies ();
  static guint _list_id = 0;
  guint i;

  for (i = 0; i < global_list->proxies->len; ++i) {
    NyxEnhancerProxy *proxy, *proxy_copy;
    gchar obj_name[64];

    proxy = nyx_enhancer_proxy_list_peek_proxy (global_list, i);

    /* Name newly created proxy, very useful for debugging. Keep index per
     * list, so it will be the same as the player that proxy belongs to. */
    g_snprintf (obj_name, sizeof (obj_name), "%s-proxy%u",
        nyx_enhancer_proxy_get_friendly_name (proxy), _list_id);
    proxy_copy = nyx_enhancer_proxy_copy (proxy, obj_name);

    nyx_enhancer_proxy_list_take_proxy (self, proxy_copy);
  }
  _list_id++;
}

static gint
_sort_values_by_name (NyxEnhancerProxy *proxy_a, NyxEnhancerProxy *proxy_b)
{
  return g_ascii_strcasecmp (
      nyx_enhancer_proxy_get_friendly_name (proxy_a),
      nyx_enhancer_proxy_get_friendly_name (proxy_b));
}

/*
 * nyx_enhancer_proxy_list_sort:
 *
 * Sort all list elements by enhancer friendly name.
 */
void
nyx_enhancer_proxy_list_sort (NyxEnhancerProxyList *self)
{
  g_ptr_array_sort_values (self->proxies, (GCompareFunc) _sort_values_by_name);
}

/*
 * nyx_enhancer_proxy_list_has_proxy_with_interface:
 * @iface_type: an interface #GType
 *
 * Check if any enhancer implementing given interface type is available.
 *
 * Returns: whether any enhancer proxy was found.
 */
gboolean
nyx_enhancer_proxy_list_has_proxy_with_interface (NyxEnhancerProxyList *self, GType iface_type)
{
  guint i;

  for (i = 0; i < self->proxies->len; ++i) {
    NyxEnhancerProxy *proxy = nyx_enhancer_proxy_list_peek_proxy (self, i);

    if (nyx_enhancer_proxy_target_has_interface (proxy, iface_type))
      return TRUE;
  }

  return FALSE;
}

/**
 * nyx_enhancer_proxy_list_get_proxy:
 * @list: a #NyxEnhancerProxyList
 * @index: an enhancer proxy index
 *
 * Get the #NyxEnhancerProxy at index.
 *
 * This behaves the same as [method@Gio.ListModel.get_item], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: (transfer full) (nullable): The #NyxEnhancerProxy at @index.
 *
 * Since: 0.10
 */
NyxEnhancerProxy *
nyx_enhancer_proxy_list_get_proxy (NyxEnhancerProxyList *self, guint index)
{
  g_return_val_if_fail (NYX_IS_ENHANCER_PROXY_LIST (self), NULL);

  return g_list_model_get_item (G_LIST_MODEL (self), index);
}

/**
 * nyx_enhancer_proxy_list_peek_proxy: (skip)
 * @list: a #NyxEnhancerProxyList
 * @index: an enhancer proxy index
 *
 * Get the #NyxEnhancerProxy at index.
 *
 * Similar to [method@Nyx.EnhancerProxyList.get_proxy], but does not take
 * a new reference on proxy.
 *
 * Proxies in a list are only removed when a [class@Nyx.Player] instance
 * they originate from is destroyed, so do not use returned object afterwards
 * unless you take an additional reference on it.
 *
 * Returns: (transfer none) (nullable): The #NyxEnhancerProxy at @index.
 *
 * Since: 0.10
 */
NyxEnhancerProxy *
nyx_enhancer_proxy_list_peek_proxy (NyxEnhancerProxyList *self, guint index)
{
  g_return_val_if_fail (NYX_IS_ENHANCER_PROXY_LIST (self), NULL);

  return g_ptr_array_index (self->proxies, index);
}

/**
 * nyx_enhancer_proxy_list_get_proxy_by_module:
 * @list: a #NyxEnhancerProxyList
 * @module_name: an enhancer module name
 *
 * Get the #NyxEnhancerProxy by module name as defined in its plugin file.
 *
 * A convenience function to find a #NyxEnhancerProxy by its unique
 * module name in the list.
 *
 * Returns: (transfer full) (nullable): The #NyxEnhancerProxy with requested module name.
 *
 * Since: 0.10
 */
NyxEnhancerProxy *
nyx_enhancer_proxy_list_get_proxy_by_module (NyxEnhancerProxyList *self, const gchar *module_name)
{
  guint i;

  g_return_val_if_fail (NYX_IS_ENHANCER_PROXY_LIST (self), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  for (i = 0; i < self->proxies->len; ++i) {
    NyxEnhancerProxy *proxy = g_ptr_array_index (self->proxies, i);

    if (strcmp (nyx_enhancer_proxy_get_module_name (proxy), module_name) == 0)
      return gst_object_ref (proxy);
  }

  return NULL;
}

/**
 * nyx_enhancer_proxy_list_get_n_proxies:
 * @list: a #NyxEnhancerProxyList
 *
 * Get the number of proxies in #NyxEnhancerProxyList.
 *
 * This behaves the same as [method@Gio.ListModel.get_n_items], and is here
 * for code uniformity and convenience to avoid type casting by user.
 *
 * Returns: The number of proxies in #NyxEnhancerProxyList.
 *
 * Since: 0.10
 */
guint
nyx_enhancer_proxy_list_get_n_proxies (NyxEnhancerProxyList *self)
{
  g_return_val_if_fail (NYX_IS_ENHANCER_PROXY_LIST (self), 0);

  return g_list_model_get_n_items (G_LIST_MODEL (self));
}

static void
_proxy_remove_func (NyxEnhancerProxy *proxy)
{
  gst_object_unparent (GST_OBJECT_CAST (proxy));
  gst_object_unref (proxy);
}

static void
nyx_enhancer_proxy_list_init (NyxEnhancerProxyList *self)
{
  self->proxies = g_ptr_array_new_with_free_func ((GDestroyNotify) _proxy_remove_func);
}

static void
nyx_enhancer_proxy_list_finalize (GObject *object)
{
  NyxEnhancerProxyList *self = NYX_ENHANCER_PROXY_LIST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_ptr_array_unref (self->proxies);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_enhancer_proxy_list_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxEnhancerProxyList *self = NYX_ENHANCER_PROXY_LIST_CAST (object);

  switch (prop_id) {
    case PROP_N_PROXIES:
      g_value_set_uint (value, nyx_enhancer_proxy_list_get_n_proxies (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_enhancer_proxy_list_class_init (NyxEnhancerProxyListClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxenhancerproxylist", 0,
      "Nyx Enhancer Proxy List");

  gobject_class->get_property = nyx_enhancer_proxy_list_get_property;
  gobject_class->finalize = nyx_enhancer_proxy_list_finalize;

  /**
   * NyxEnhancerProxyList:n-proxies:
   *
   * Number of proxies in the list.
   *
   * Since: 0.10
   */
  param_specs[PROP_N_PROXIES] = g_param_spec_uint ("n-proxies",
      NULL, NULL, 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
