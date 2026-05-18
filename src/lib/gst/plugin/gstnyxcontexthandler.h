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

#include "gst/plugin/nyx-gst-visibility.h"

G_BEGIN_DECLS

#define GST_TYPE_NYX_CONTEXT_HANDLER               (gst_nyx_context_handler_get_type())
#define GST_IS_NYX_CONTEXT_HANDLER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_NYX_CONTEXT_HANDLER))
#define GST_IS_NYX_CONTEXT_HANDLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_NYX_CONTEXT_HANDLER))
#define GST_NYX_CONTEXT_HANDLER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_NYX_CONTEXT_HANDLER, GstNyxContextHandlerClass))
#define GST_NYX_CONTEXT_HANDLER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_NYX_CONTEXT_HANDLER, GstNyxContextHandler))
#define GST_NYX_CONTEXT_HANDLER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_NYX_CONTEXT_HANDLER, GstNyxContextHandlerClass))
#define GST_NYX_CONTEXT_HANDLER_CAST(obj)          ((GstNyxContextHandler *)(obj))

typedef struct _GstNyxContextHandler GstNyxContextHandler;
typedef struct _GstNyxContextHandlerClass GstNyxContextHandlerClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstNyxContextHandler, gst_object_unref)
#endif

struct _GstNyxContextHandler
{
  GstObject parent;
};

struct _GstNyxContextHandlerClass
{
  GstObjectClass parent_class;

  gboolean (* handle_context_query) (GstNyxContextHandler *handler,
                                     GstBaseSink              *bsink,
                                     GstQuery                 *query);
};

NYX_GST_API
GType                      gst_nyx_context_handler_get_type              (void);

NYX_GST_API
gboolean                   gst_nyx_context_handler_handle_context_query  (GstNyxContextHandler *handler, GstBaseSink *bsink, GstQuery *query);

NYX_GST_API
GstNyxContextHandler * gst_nyx_context_handler_obtain_with_type      (GPtrArray *context_handlers, GType type);

G_END_DECLS
