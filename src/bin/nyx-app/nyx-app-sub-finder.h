/* Nyx Application
 * Copyright (C) 2026 Moon
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

#ifndef NYX_APP_SUB_FINDER_H
#define NYX_APP_SUB_FINDER_H

#include <nyx/nyx.h>

// Asynchronously search and set the best matching subtitle for a media item
void nyx_app_sub_finder_find_for_item_async (NyxMediaItem *item, GCancellable *cancellable);

#endif // NYX_APP_SUB_FINDER_H
