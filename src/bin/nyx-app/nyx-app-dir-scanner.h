/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 * Copyright (C) 2025 Moon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <nyx/nyx.h>

G_BEGIN_DECLS

#define NYX_APP_DIR_SCANNER_ERROR \
    (nyx_app_dir_scanner_error_quark ())

typedef enum {
  NYX_APP_DIR_SCANNER_ERROR_NOT_DIRECTORY = 0,
  NYX_APP_DIR_SCANNER_ERROR_PERMISSION,
  NYX_APP_DIR_SCANNER_ERROR_EMPTY,
} NyxAppDirScannerError;

GQuark nyx_app_dir_scanner_error_quark (void);

typedef void (*NyxAppDirScanCallback) (GPtrArray *items,
    GError *error, gpointer user_data);

G_GNUC_INTERNAL
void nyx_app_dir_scanner_scan_async (GFile *directory,
    gboolean recursive, GCancellable *cancellable,
    NyxAppDirScanCallback callback, gpointer user_data);

G_END_DECLS
