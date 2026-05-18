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
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define NYX_TYPE_EXTRACTABLE_SRC (nyx_extractable_src_get_type())
#define NYX_EXTRACTABLE_SRC_CAST(obj) ((NyxExtractableSrc *)(obj))

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (NyxExtractableSrc, nyx_extractable_src, NYX, EXTRACTABLE_SRC, GstPushSrc)

GST_ELEMENT_REGISTER_DECLARE (nyxextractablesrc)

G_END_DECLS
