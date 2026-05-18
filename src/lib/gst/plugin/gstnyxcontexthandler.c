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

#include "gstnyxcontexthandler.h"

#define parent_class gst_nyx_context_handler_parent_class
G_DEFINE_TYPE (GstNyxContextHandler, gst_nyx_context_handler, GST_TYPE_OBJECT);

static gboolean
_default_handle_context_query (GstNyxContextHandler *self,
    GstBaseSink *bsink, GstQuery *query)
{
  GST_FIXME_OBJECT (self, "Need to handle context query");

  return FALSE;
}

static void
gst_nyx_context_handler_init (GstNyxContextHandler *self)
{
}

static void
gst_nyx_context_handler_finalize (GObject *object)
{
  GstNyxContextHandler *self = GST_NYX_CONTEXT_HANDLER_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_nyx_context_handler_class_init (GstNyxContextHandlerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstNyxContextHandlerClass *handler_class = (GstNyxContextHandlerClass *) klass;

  gobject_class->finalize = gst_nyx_context_handler_finalize;

  handler_class->handle_context_query = _default_handle_context_query;
}

gboolean
gst_nyx_context_handler_handle_context_query (GstNyxContextHandler *self,
    GstBaseSink *bsink, GstQuery *query)
{
  GstNyxContextHandlerClass *handler_class = GST_NYX_CONTEXT_HANDLER_GET_CLASS (self);

  return handler_class->handle_context_query (self, bsink, query);
}

GstNyxContextHandler *
gst_nyx_context_handler_obtain_with_type (GPtrArray *context_handlers, GType type)
{
  guint i;

  for (i = 0; i < context_handlers->len; i++) {
    GstNyxContextHandler *handler = g_ptr_array_index (context_handlers, i);

    if (G_TYPE_CHECK_INSTANCE_TYPE (handler, type))
      return gst_object_ref (handler);
  }

  return NULL;
}
