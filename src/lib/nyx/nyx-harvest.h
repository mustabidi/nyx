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

#if !defined(__NYX_INSIDE__) && !defined(NYX_COMPILATION)
#error "Only <nyx/nyx.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>

#include <nyx/nyx-visibility.h>

G_BEGIN_DECLS

#define NYX_TYPE_HARVEST (nyx_harvest_get_type())
#define NYX_HARVEST_CAST(obj) ((NyxHarvest *)(obj))

G_DECLARE_FINAL_TYPE (NyxHarvest, nyx_harvest, NYX, HARVEST, GstObject)

NYX_API
gboolean nyx_harvest_fill (NyxHarvest *harvest, const gchar *media_type, gpointer data, gsize size);

NYX_API
gboolean nyx_harvest_fill_with_text (NyxHarvest *harvest, const gchar *media_type, gchar *text);

NYX_API
gboolean nyx_harvest_fill_with_bytes (NyxHarvest *harvest, const gchar *media_type, GBytes *bytes);

NYX_API
void nyx_harvest_tags_add (NyxHarvest *harvest, const gchar *tag, ...) G_GNUC_NULL_TERMINATED;

NYX_API
void nyx_harvest_tags_add_value (NyxHarvest *harvest, const gchar *tag, const GValue *value);

NYX_API
void nyx_harvest_toc_add (NyxHarvest *harvest, GstTocEntryType type, const gchar *title, gdouble start, gdouble end);

NYX_API
void nyx_harvest_headers_set (NyxHarvest *harvest, const gchar *key, ...) G_GNUC_NULL_TERMINATED;

NYX_API
void nyx_harvest_headers_set_value (NyxHarvest *harvest, const gchar *key, const GValue *value);

NYX_API
void nyx_harvest_set_expiration_date_utc (NyxHarvest *harvest, GDateTime *date_utc);

NYX_API
void nyx_harvest_set_expiration_seconds (NyxHarvest *harvest, gdouble seconds);

G_END_DECLS
