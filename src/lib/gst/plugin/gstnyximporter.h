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

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gst/plugin/nyx-gst-visibility.h"

G_BEGIN_DECLS

#define GST_TYPE_NYX_IMPORTER               (gst_nyx_importer_get_type())
#define GST_IS_NYX_IMPORTER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_NYX_IMPORTER))
#define GST_IS_NYX_IMPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_NYX_IMPORTER))
#define GST_NYX_IMPORTER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_NYX_IMPORTER, GstNyxImporterClass))
#define GST_NYX_IMPORTER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_NYX_IMPORTER, GstNyxImporter))
#define GST_NYX_IMPORTER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_NYX_IMPORTER, GstNyxImporterClass))
#define GST_NYX_IMPORTER_CAST(obj)          ((GstNyxImporter *)(obj))

#define GST_NYX_IMPORTER_DEFINE(camel,lower,type)                            \
G_DEFINE_TYPE (camel, lower, type)                                               \
G_MODULE_EXPORT GstNyxImporter *make_importer (GPtrArray *context_handlers); \
G_MODULE_EXPORT GstCaps *make_caps (gboolean is_template,                        \
    GstRank *rank, GPtrArray *context_handlers);

typedef struct _GstNyxImporter GstNyxImporter;
typedef struct _GstNyxImporterClass GstNyxImporterClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstNyxImporter, gst_object_unref)
#endif

struct _GstNyxImporter
{
  GstObject parent;

  GstCaps *pending_caps;
  GstBuffer *pending_buffer, *buffer;
  GPtrArray *pending_overlays, *overlays;
  GstVideoInfo pending_v_info, v_info;
  gboolean has_pending_v_info;

  GdkTexture *texture;

  GdkRGBA bg;
};

struct _GstNyxImporterClass
{
  GstObjectClass parent_class;

  void (* set_caps) (GstNyxImporter *importer,
                     GstCaps            *caps);

  GstBufferPool * (* create_pool) (GstNyxImporter *importer,
                                   GstStructure      **config);

  void (* add_allocation_metas) (GstNyxImporter *importer,
                                 GstQuery           *query);

  GdkTexture * (* generate_texture) (GstNyxImporter *importer,
                                     GstBuffer          *buffer,
                                     GstVideoInfo       *v_info);
};

NYX_GST_API
GType           gst_nyx_importer_get_type                (void);

GstBufferPool * gst_nyx_importer_create_pool             (GstNyxImporter *importer, GstStructure **config);
void            gst_nyx_importer_add_allocation_metas    (GstNyxImporter *importer, GstQuery *query);

void            gst_nyx_importer_set_caps                (GstNyxImporter *importer, GstCaps *caps);
void            gst_nyx_importer_set_buffer              (GstNyxImporter *importer, GstBuffer *buffer);

void            gst_nyx_importer_snapshot                (GstNyxImporter *importer, GdkSnapshot *snapshot, gdouble width, gdouble height);

G_END_DECLS
