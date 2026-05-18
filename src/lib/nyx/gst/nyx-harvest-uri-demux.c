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

#include "nyx-harvest-uri-demux-private.h"

#define GST_CAT_DEFAULT nyx_harvest_uri_demux_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxHarvestUriDemux
{
  NyxUriBaseDemux parent;

  GMutex lock;
  GstStructure *http_headers;
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-uri, source=(string)nyx-harvest"));

#define parent_class nyx_harvest_uri_demux_parent_class
G_DEFINE_TYPE (NyxHarvestUriDemux, nyx_harvest_uri_demux, NYX_TYPE_URI_BASE_DEMUX);
GST_ELEMENT_REGISTER_DEFINE (nyxharvesturidemux, "nyxharvesturidemux",
    512, NYX_TYPE_HARVEST_URI_DEMUX);

static void
_set_property (GstObject *obj, const gchar *prop_name, gpointer value)
{
  g_object_set (G_OBJECT (obj), prop_name, value, NULL);

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gchar *el_name;

    el_name = gst_object_get_name (obj);
    GST_DEBUG ("Set %s %s", el_name, prop_name);

    g_free (el_name);
  }
}

static gboolean
configure_deep_element (GQuark field_id, const GValue *value, GstElement *child)
{
  GObjectClass *gobject_class;
  const GstStructure *substructure;

  if (!GST_VALUE_HOLDS_STRUCTURE (value))
    return TRUE;

  substructure = gst_value_get_structure (value);

  if (!gst_structure_has_name (substructure, "request-headers"))
    return TRUE;

  gobject_class = G_OBJECT_GET_CLASS (child);

  if (g_object_class_find_property (gobject_class, "user-agent")) {
    const gchar *ua;

    if ((ua = gst_structure_get_string (substructure, "User-Agent")))
      _set_property (GST_OBJECT_CAST (child), "user-agent", (gchar *) ua);
  }

  if (g_object_class_find_property (gobject_class, "extra-headers")) {
    GstStructure *extra_headers;

    extra_headers = gst_structure_copy (substructure);
    gst_structure_set_name (extra_headers, "extra-headers");
    gst_structure_remove_field (extra_headers, "User-Agent");

    _set_property (GST_OBJECT_CAST (child), "extra-headers", extra_headers);

    gst_structure_free (extra_headers);
  }

  return TRUE;
}

static void
nyx_harvest_uri_demux_deep_element_added (GstBin *bin, GstBin *sub_bin, GstElement *child)
{
  if (GST_OBJECT_FLAG_IS_SET (child, GST_ELEMENT_FLAG_SOURCE)) {
    NyxHarvestUriDemux *self = NYX_HARVEST_URI_DEMUX_CAST (bin);

    g_mutex_lock (&self->lock);

    if (self->http_headers) {
      gst_structure_foreach (self->http_headers,
          (GstStructureForeachFunc) configure_deep_element, child);
    }

    g_mutex_unlock (&self->lock);
  }
}

static gboolean
nyx_harvest_uri_demux_process_buffer (NyxUriBaseDemux *uri_bd,
    GstBuffer *buffer, GCancellable *cancellable)
{
  GstMemory *mem = gst_buffer_peek_memory (buffer, 0);
  GstMapInfo info;
  gboolean success = FALSE;

  if (mem && gst_memory_map (mem, &info, GST_MAP_READ)) {
    success = nyx_uri_base_demux_set_uri (uri_bd,
        (const gchar *) info.data, "nyxextractablesrc");
    gst_memory_unmap (mem, &info);
  }

  return success;
}

static void
nyx_harvest_uri_demux_handle_custom_event (NyxUriBaseDemux *uri_bd, GstEvent *event)
{
  const GstStructure *structure = gst_event_get_structure (event);

  if (structure && gst_structure_has_name (structure, "http-headers")) {
    NyxHarvestUriDemux *self = NYX_HARVEST_URI_DEMUX_CAST (uri_bd);

    GST_DEBUG_OBJECT (self, "Received \"http-headers\" custom event");

    g_mutex_lock (&self->lock);

    gst_clear_structure (&self->http_headers);
    self->http_headers = gst_structure_copy (structure);

    g_mutex_unlock (&self->lock);
  }
}

static void
nyx_harvest_uri_demux_init (NyxHarvestUriDemux *self)
{
  g_mutex_init (&self->lock);
}

static void
nyx_harvest_uri_demux_finalize (GObject *object)
{
  NyxHarvestUriDemux *self = NYX_HARVEST_URI_DEMUX_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  gst_clear_structure (&self->http_headers);
  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_harvest_uri_demux_class_init (NyxHarvestUriDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;
  NyxUriBaseDemuxClass *nyxuribd_class = (NyxUriBaseDemuxClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxharvesturidemux", 0,
      "Nyx Harvest URI Demux");

  gobject_class->finalize = nyx_harvest_uri_demux_finalize;

  gstbin_class->deep_element_added = nyx_harvest_uri_demux_deep_element_added;

  nyxuribd_class->process_buffer = nyx_harvest_uri_demux_process_buffer;
  nyxuribd_class->handle_custom_event = nyx_harvest_uri_demux_handle_custom_event;

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gst_element_class_set_static_metadata (gstelement_class, "Nyx Harvest URI Demux",
      "Demuxer", "A custom demuxer for harvested URI",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
