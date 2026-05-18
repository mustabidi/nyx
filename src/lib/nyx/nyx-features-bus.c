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

#include "nyx-bus-private.h"
#include "nyx-features-manager-private.h"
#include "nyx-features-bus-private.h"

#define GST_CAT_DEFAULT nyx_features_bus_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxFeaturesBus
{
  GstBus parent;
};

#define parent_class nyx_features_bus_parent_class
G_DEFINE_TYPE (NyxFeaturesBus, nyx_features_bus, GST_TYPE_BUS);

enum
{
  NYX_FEATURES_BUS_STRUCTURE_UNKNOWN = 0,
  NYX_FEATURES_BUS_STRUCTURE_EVENT
};

static NyxBusQuark _structure_quarks[] = {
  {"unknown", 0},
  {"event", 0},
  {NULL, 0}
};

enum
{
  NYX_FEATURES_BUS_FIELD_UNKNOWN = 0,
  NYX_FEATURES_BUS_FIELD_EVENT,
  NYX_FEATURES_BUS_FIELD_VALUE,
  NYX_FEATURES_BUS_FIELD_EXTRA_VALUE
};

static NyxBusQuark _field_quarks[] = {
  {"unknown", 0},
  {"event", 0},
  {"value", 0},
  {"extra-value", 0},
  {NULL, 0}
};

#define _STRUCTURE_QUARK(q) (_structure_quarks[NYX_FEATURES_BUS_STRUCTURE_##q].quark)
#define _FIELD_QUARK(q) (_field_quarks[NYX_FEATURES_BUS_FIELD_##q].quark)
#define _MESSAGE_SRC_NYX_FEATURES_MANAGER(msg) ((NyxFeaturesManager *) GST_MESSAGE_SRC (msg))

void
nyx_features_bus_initialize (void)
{
  gint i;

  for (i = 0; _structure_quarks[i].name; ++i)
    _structure_quarks[i].quark = g_quark_from_static_string (_structure_quarks[i].name);
  for (i = 0; _field_quarks[i].name; ++i)
    _field_quarks[i].quark = g_quark_from_static_string (_field_quarks[i].name);
}

void
nyx_features_bus_post_event (NyxFeaturesBus *self,
    NyxFeaturesManager *src, NyxFeaturesManagerEvent event,
    GValue *value, GValue *extra_value)
{
  GstStructure *structure = gst_structure_new_id (_STRUCTURE_QUARK (EVENT),
      _FIELD_QUARK (EVENT), G_TYPE_ENUM, event,
      NULL);

  if (value)
    gst_structure_id_take_value (structure, _FIELD_QUARK (VALUE), value);
  if (extra_value)
    gst_structure_id_take_value (structure, _FIELD_QUARK (EXTRA_VALUE), extra_value);

  gst_bus_post (GST_BUS_CAST (self), gst_message_new_application (
      GST_OBJECT_CAST (src), structure));
}

static inline void
_handle_event_msg (GstMessage *msg, const GstStructure *structure,
    NyxFeaturesManager *features_manager)
{
  NyxFeaturesManagerEvent event = NYX_FEATURES_MANAGER_EVENT_UNKNOWN;
  const GValue *value = gst_structure_id_get_value (structure, _FIELD_QUARK (VALUE));
  const GValue *extra_value = gst_structure_id_get_value (structure, _FIELD_QUARK (EXTRA_VALUE));

  gst_structure_id_get (structure,
      _FIELD_QUARK (EVENT), G_TYPE_ENUM, &event,
      NULL);

  nyx_features_manager_handle_event (features_manager, event, value, extra_value);
}

static gboolean
nyx_features_bus_message_func (GstBus *bus, GstMessage *msg, gpointer user_data G_GNUC_UNUSED)
{
  if (G_LIKELY (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_APPLICATION)) {
    NyxFeaturesManager *features_manager = _MESSAGE_SRC_NYX_FEATURES_MANAGER (msg);
    const GstStructure *structure = gst_message_get_structure (msg);
    GQuark quark = gst_structure_get_name_id (structure);

    if (quark == _STRUCTURE_QUARK (EVENT))
      _handle_event_msg (msg, structure, features_manager);
  }

  return G_SOURCE_CONTINUE;
}

/*
 * nyx_features_bus_new:
 *
 * Returns: (transfer full): a new #NyxFeaturesBus instance.
 */
NyxFeaturesBus *
nyx_features_bus_new (void)
{
  GstBus *features_bus;

  features_bus = GST_BUS_CAST (g_object_new (NYX_TYPE_FEATURES_BUS, NULL));
  gst_object_ref_sink (features_bus);

  gst_bus_add_watch (features_bus, (GstBusFunc) nyx_features_bus_message_func, NULL);

  return NYX_FEATURES_BUS_CAST (features_bus);
}

static void
nyx_features_bus_init (NyxFeaturesBus *self)
{
}

static void
nyx_features_bus_finalize (GObject *object)
{
  NyxFeaturesBus *self = NYX_FEATURES_BUS_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_features_bus_class_init (NyxFeaturesBusClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxfeaturesbus", 0,
      "Nyx Features Bus");

  gobject_class->finalize = nyx_features_bus_finalize;
}
