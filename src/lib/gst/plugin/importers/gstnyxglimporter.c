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

#include "gstnyxglimporter.h"

#define GST_CAT_DEFAULT gst_nyx_gl_importer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_nyx_gl_importer_parent_class
GST_NYX_IMPORTER_DEFINE (GstNyxGLImporter, gst_nyx_gl_importer, GST_TYPE_NYX_IMPORTER);

static GstBufferPool *
gst_nyx_gl_importer_create_pool (GstNyxImporter *importer, GstStructure **config)
{
  GstNyxGLImporter *self = GST_NYX_GL_IMPORTER_CAST (importer);
  GstBufferPool *pool;

  GST_DEBUG_OBJECT (self, "Creating new GL buffer pool");

  pool = gst_gl_buffer_pool_new (self->gl_handler->gst_context);
  *config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_add_option (*config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (*config, GST_BUFFER_POOL_OPTION_GL_SYNC_META);

  return pool;
}

static void
gst_nyx_gl_importer_add_allocation_metas (GstNyxImporter *importer, GstQuery *query)
{
  GstNyxGLImporter *self = GST_NYX_GL_IMPORTER_CAST (importer);

  /* We can support GL sync meta */
  if (self->gl_handler->gst_context->gl_vtable->FenceSync)
    gst_query_add_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL);

  /* Also add base importer class supported meta */
  GST_NYX_IMPORTER_CLASS (parent_class)->add_allocation_metas (importer, query);
}

static GdkTexture *
gst_nyx_gl_importer_generate_texture (GstNyxImporter *importer,
    GstBuffer *buffer, GstVideoInfo *v_info)
{
  GstNyxGLImporter *self = GST_NYX_GL_IMPORTER_CAST (importer);

  return gst_nyx_gl_context_handler_make_gl_texture (self->gl_handler, buffer, v_info);
}

static void
gst_nyx_gl_importer_init (GstNyxGLImporter *self)
{
}

static void
gst_nyx_gl_importer_finalize (GObject *object)
{
  GstNyxGLImporter *self = GST_NYX_GL_IMPORTER_CAST (object);

  gst_clear_object (&self->gl_handler);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_nyx_gl_importer_class_init (GstNyxGLImporterClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstNyxImporterClass *importer_class = (GstNyxImporterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxglimporter", 0,
      "Nyx GL Importer");

  gobject_class->finalize = gst_nyx_gl_importer_finalize;

  importer_class->create_pool = gst_nyx_gl_importer_create_pool;
  importer_class->add_allocation_metas = gst_nyx_gl_importer_add_allocation_metas;
  importer_class->generate_texture = gst_nyx_gl_importer_generate_texture;
}

GstNyxImporter *
make_importer (GPtrArray *context_handlers)
{
  GstNyxGLImporter *self;
  GstNyxContextHandler *handler;

  handler = gst_nyx_context_handler_obtain_with_type (context_handlers,
      GST_TYPE_NYX_GL_CONTEXT_HANDLER);

  if (G_UNLIKELY (!handler))
    return NULL;

  self = g_object_new (GST_TYPE_NYX_GL_IMPORTER, NULL);
  self->gl_handler = GST_NYX_GL_CONTEXT_HANDLER_CAST (handler);

  return GST_NYX_IMPORTER_CAST (self);
}

GstCaps *
make_caps (gboolean is_template, GstRank *rank, GPtrArray *context_handlers)
{
  *rank = GST_RANK_SECONDARY;

  if (!is_template && context_handlers)
    gst_nyx_gl_context_handler_add_handler (context_handlers);

  return gst_nyx_gl_context_handler_make_gdk_gl_caps (
      GST_CAPS_FEATURE_MEMORY_GL_MEMORY, TRUE);
}
