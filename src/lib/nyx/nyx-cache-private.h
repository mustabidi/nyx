/* Nyx Playback Library
 * Copyright (C) 2025 Rafał Dzięgiel <rafostar.github@gmail.com>
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

G_BEGIN_DECLS

G_GNUC_INTERNAL
void nyx_cache_initialize (void);

G_GNUC_INTERNAL
gboolean nyx_cache_is_disabled (void);

G_GNUC_INTERNAL
GMappedFile * nyx_cache_open (const gchar *filename, const gchar **data, GError **error);

G_GNUC_INTERNAL
gboolean nyx_cache_read_boolean (const gchar **data);

G_GNUC_INTERNAL
gint nyx_cache_read_int (const gchar **data);

G_GNUC_INTERNAL
guint nyx_cache_read_uint (const gchar **data);

G_GNUC_INTERNAL
gint64 nyx_cache_read_int64 (const gchar **data);

G_GNUC_INTERNAL
gdouble nyx_cache_read_double (const gchar **data);

G_GNUC_INTERNAL
const gchar * nyx_cache_read_string (const gchar **data);

G_GNUC_INTERNAL
const guint8 * nyx_cache_read_data (const gchar **data, gsize *size);

G_GNUC_INTERNAL
GType nyx_cache_read_enum (const gchar **data);

G_GNUC_INTERNAL
GType nyx_cache_read_flags (const gchar **data);

G_GNUC_INTERNAL
GType nyx_cache_read_iface (const gchar **data);

G_GNUC_INTERNAL
GParamSpec * nyx_cache_read_pspec (const gchar **data);

G_GNUC_INTERNAL
GByteArray * nyx_cache_create (void);

G_GNUC_INTERNAL
void nyx_cache_store_boolean (GByteArray *bytes, gboolean val);

G_GNUC_INTERNAL
void nyx_cache_store_int (GByteArray *bytes, gint val);

G_GNUC_INTERNAL
void nyx_cache_store_uint (GByteArray *bytes, guint val);

G_GNUC_INTERNAL
void nyx_cache_store_int64 (GByteArray *bytes, gint64 val);

G_GNUC_INTERNAL
void nyx_cache_store_double (GByteArray *bytes, gdouble val);

G_GNUC_INTERNAL
void nyx_cache_store_string (GByteArray *bytes, const gchar *val);

G_GNUC_INTERNAL
void nyx_cache_store_data (GByteArray *bytes, const guint8 *val, gsize val_size);

G_GNUC_INTERNAL
void nyx_cache_store_enum (GByteArray *bytes, GType enum_type);

G_GNUC_INTERNAL
void nyx_cache_store_flags (GByteArray *bytes, GType flags_type);

G_GNUC_INTERNAL
gboolean nyx_cache_store_iface (GByteArray *bytes, GType iface);

G_GNUC_INTERNAL
gboolean nyx_cache_store_pspec (GByteArray *bytes, GParamSpec *pspec);

G_GNUC_INTERNAL
gboolean nyx_cache_write (const gchar *filename, GByteArray *bytes, GError **error);

G_END_DECLS
