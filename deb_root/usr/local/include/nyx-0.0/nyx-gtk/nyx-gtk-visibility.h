/* Nyx API Visibility
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

#if !defined(__NYX_GTK_INSIDE__) && !defined(NYX_GTK_COMPILATION)
#error "This header file can not be included directly."
#endif

#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(NYX_GTK_STATIC_COMPILATION)
#define _NYX_GTK_EXPORT __declspec(dllexport) extern
#define _NYX_GTK_IMPORT __declspec(dllimport) extern
#elif __GNUC__ >= 4
#define _NYX_GTK_EXPORT __attribute__((visibility("default"))) extern
#define _NYX_GTK_IMPORT extern
#else
#define _NYX_GTK_EXPORT extern
#define _NYX_GTK_IMPORT extern
#endif

#if defined(NYX_GTK_COMPILATION)
#define _NYX_GTK_VISIBILITY _NYX_GTK_EXPORT
#else
#define _NYX_GTK_VISIBILITY _NYX_GTK_IMPORT
#endif

#define NYX_GTK_API                _NYX_GTK_VISIBILITY

#if !defined(NYX_DISABLE_DEPRECATION_WARNINGS)
#define NYX_GTK_DEPRECATED         G_DEPRECATED _NYX_GTK_VISIBILITY
#define NYX_GTK_DEPRECATED_FOR(f)  G_DEPRECATED_FOR(f) _NYX_GTK_VISIBILITY
#else
#define NYX_GTK_DEPRECATED         _NYX_GTK_VISIBILITY
#define NYX_GTK_DEPRECATED_FOR(f)  _NYX_GTK_VISIBILITY
#endif
