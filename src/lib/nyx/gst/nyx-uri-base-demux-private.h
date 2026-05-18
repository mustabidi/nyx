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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/gstbin.h>

G_BEGIN_DECLS

#define NYX_TYPE_URI_BASE_DEMUX (nyx_uri_base_demux_get_type())
#define NYX_URI_BASE_DEMUX_CAST(obj) ((NyxUriBaseDemux *)(obj))

G_GNUC_INTERNAL
G_DECLARE_DERIVABLE_TYPE (NyxUriBaseDemux, nyx_uri_base_demux, NYX, URI_BASE_DEMUX, GstBin)

struct _NyxUriBaseDemuxClass
{
  GstBinClass parent_class;

  gboolean (* process_buffer) (NyxUriBaseDemux *uri_bd, GstBuffer *buffer, GCancellable *cancellable);

  void (* handle_caps) (NyxUriBaseDemux *uri_bd, GstCaps *caps);

  void (* handle_custom_event) (NyxUriBaseDemux *uri_bd, GstEvent *event);

  gboolean (* handle_custom_query) (NyxUriBaseDemux *uri_bd, GstQuery *query);
};

gboolean nyx_uri_base_demux_set_uri (NyxUriBaseDemux *uri_bd, const gchar *uri, const gchar *blacklisted_el);

G_END_DECLS
