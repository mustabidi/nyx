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
#include <gst/video/video.h>
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#include <gtk/gtk.h>

#include "gst/plugin/gstnyxcontexthandler.h"
#include "gst/plugin/nyx-gst-visibility.h"

G_BEGIN_DECLS

#define GST_TYPE_NYX_GL_CONTEXT_HANDLER (gst_nyx_gl_context_handler_get_type())

NYX_GST_API
G_DECLARE_FINAL_TYPE (GstNyxGLContextHandler, gst_nyx_gl_context_handler, GST, NYX_GL_CONTEXT_HANDLER, GstNyxContextHandler)

#define GST_NYX_GL_CONTEXT_HANDLER_CAST(obj)        ((GstNyxGLContextHandler *)(obj))

#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_WAYLAND     (GST_GL_HAVE_WINDOW_WAYLAND && defined (GDK_WINDOWING_WAYLAND))
#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_X11         (GST_GL_HAVE_WINDOW_X11 && defined (GDK_WINDOWING_X11))
#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_X11_GLX     (GST_NYX_GL_CONTEXT_HANDLER_HAVE_X11 && GST_GL_HAVE_PLATFORM_GLX)
#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_X11_EGL     (GST_NYX_GL_CONTEXT_HANDLER_HAVE_X11 && GST_GL_HAVE_PLATFORM_EGL)
#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_WIN32       (GST_GL_HAVE_WINDOW_WIN32 && defined (GDK_WINDOWING_WIN32))
#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_WIN32_WGL   (GST_NYX_GL_CONTEXT_HANDLER_HAVE_WIN32 && GST_GL_HAVE_PLATFORM_WGL)
#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_WIN32_EGL   (GST_NYX_GL_CONTEXT_HANDLER_HAVE_WIN32 && GST_GL_HAVE_PLATFORM_EGL)
#define GST_NYX_GL_CONTEXT_HANDLER_HAVE_MACOS       (GST_GL_HAVE_WINDOW_COCOA && defined (GDK_WINDOWING_MACOS) && GST_GL_HAVE_PLATFORM_CGL)

struct _GstNyxGLContextHandler
{
  GstNyxContextHandler parent;

  GdkGLContext *gdk_context;

  GstGLDisplay *gst_display;
  GstGLContext *wrapped_context;
  GstGLContext *gst_context;
};

NYX_GST_API
void         gst_nyx_gl_context_handler_add_handler            (GPtrArray *context_handlers);

NYX_GST_API
GstCaps *    gst_nyx_gl_context_handler_make_gdk_gl_caps       (const gchar *features, gboolean only_2d);

NYX_GST_API
GdkTexture * gst_nyx_gl_context_handler_make_gl_texture        (GstNyxGLContextHandler *handler, GstBuffer *buffer, GstVideoInfo *v_info);

G_END_DECLS
