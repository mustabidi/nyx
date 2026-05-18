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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnyxrawimporter.h"
#include "gst/plugin/gstgtkutils.h"
#include "gst/plugin/gstgdkformats.h"

#define GST_CAT_DEFAULT gst_nyx_raw_importer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_nyx_raw_importer_parent_class
GST_NYX_IMPORTER_DEFINE (GstNyxRawImporter, gst_nyx_raw_importer, GST_TYPE_NYX_IMPORTER);

static GstBufferPool *
gst_nyx_raw_importer_create_pool (GstNyxImporter *importer, GstStructure **config)
{
  GstNyxRawImporter *self = GST_NYX_RAW_IMPORTER_CAST (importer);
  GstBufferPool *pool;

  GST_DEBUG_OBJECT (self, "Creating new buffer pool");

  pool = gst_video_buffer_pool_new ();
  *config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_add_option (*config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  return pool;
}

static GdkTexture *
gst_nyx_raw_importer_generate_texture (GstNyxImporter *importer,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GdkTexture *texture;
  GstVideoFrame frame;

  if (G_UNLIKELY (!gst_video_frame_map (&frame, v_info, buffer, GST_MAP_READ))) {
    GST_ERROR_OBJECT (importer, "Could not map input buffer for reading");
    return NULL;
  }

  texture = gst_video_frame_into_gdk_texture (&frame);
  gst_video_frame_unmap (&frame);

  return texture;
}

static void
gst_nyx_raw_importer_init (GstNyxRawImporter *self)
{
}

static void
gst_nyx_raw_importer_class_init (GstNyxRawImporterClass *klass)
{
  GstNyxImporterClass *importer_class = (GstNyxImporterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxrawimporter", 0,
      "Nyx RAW Importer");

  importer_class->create_pool = gst_nyx_raw_importer_create_pool;
  importer_class->generate_texture = gst_nyx_raw_importer_generate_texture;
}

GstNyxImporter *
make_importer (GPtrArray *context_handlers)
{
  return g_object_new (GST_TYPE_NYX_RAW_IMPORTER, NULL);
}

GstCaps *
make_caps (gboolean is_template, GstRank *rank, GPtrArray *context_handlers)
{
  *rank = GST_RANK_MARGINAL;

  return gst_caps_from_string (
      GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ", "
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
          "{ " GST_GDK_MEMORY_FORMATS " }")
      "; "
      GST_VIDEO_CAPS_MAKE (
          "{ " GST_GDK_MEMORY_FORMATS " }"));
}
