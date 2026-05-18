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

#include "gst/plugin/gstnyximporter.h"
#include "gst/plugin/handlers/gl/gstnyxglcontexthandler.h"

G_BEGIN_DECLS

#define GST_TYPE_NYX_GL_IMPORTER (gst_nyx_gl_importer_get_type())
G_DECLARE_FINAL_TYPE (GstNyxGLImporter, gst_nyx_gl_importer, GST, NYX_GL_IMPORTER, GstNyxImporter)

#define GST_NYX_GL_IMPORTER_CAST(obj)        ((GstNyxGLImporter *)(obj))

struct _GstNyxGLImporter
{
  GstNyxImporter parent;

  GstNyxGLContextHandler *gl_handler;
};

G_END_DECLS
