/* Nyx Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include "nyx-app-types.h"

#include "nyx-app-headerbar.h"
#include "nyx-app-media-item-box.h"
#include "nyx-app-property-row.h"
#include "nyx-app-queue-list.h"
#include "nyx-app-queue-progression-model.h"
#include "nyx-app-window-state-buttons.h"

/*
 * nyx_app_types_init:
 *
 * Ensure private types that appear in UI files in order for
 * GtkBuilder to be able to find them when building templates.
 */
inline void
nyx_app_types_init (void)
{
  g_type_ensure (NYX_APP_TYPE_HEADERBAR);
  g_type_ensure (NYX_APP_TYPE_MEDIA_ITEM_BOX);
  g_type_ensure (NYX_APP_TYPE_PROPERTY_ROW);
  g_type_ensure (NYX_APP_TYPE_QUEUE_LIST);
  g_type_ensure (NYX_APP_TYPE_QUEUE_PROGRESSION_MODEL);
  g_type_ensure (NYX_APP_TYPE_WINDOW_STATE_BUTTONS);
}
