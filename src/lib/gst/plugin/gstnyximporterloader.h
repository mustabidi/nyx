/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include "gstnyximporter.h"

G_BEGIN_DECLS

#define GST_TYPE_NYX_IMPORTER_LOADER (gst_nyx_importer_loader_get_type())
G_DECLARE_FINAL_TYPE (GstNyxImporterLoader, gst_nyx_importer_loader, GST, NYX_IMPORTER_LOADER, GstObject)

#define GST_NYX_IMPORTER_LOADER_CAST(obj)        ((GstNyxImporterLoader *)(obj))

struct _GstNyxImporterLoader
{
  GstObject parent;

  GModule *last_module;

  GPtrArray *importers;
  GPtrArray *context_handlers;
};

GstNyxImporterLoader * gst_nyx_importer_loader_new                             (void);

GstPadTemplate *           gst_nyx_importer_loader_make_sink_pad_template          (void);

GstCaps *                  gst_nyx_importer_loader_make_actual_caps                (GstNyxImporterLoader *loader);

gboolean                   gst_nyx_importer_loader_handle_context_query            (GstNyxImporterLoader *loader, GstBaseSink *bsink, GstQuery *query);

gboolean                   gst_nyx_importer_loader_find_importer_for_caps          (GstNyxImporterLoader *loader, GstCaps *caps, GstNyxImporter **importer);

G_END_DECLS
