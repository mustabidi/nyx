/*
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

#include "nyx-server-json-private.h"
#include "nyx-server-names-private.h"
#include "nyx-player.h"
#include "nyx-queue.h"

#define NYX_SERVER_JSON_BUILD(dest, ...) {                 \
    GString *_json = g_string_new ("{");                       \
    __VA_ARGS__                                                \
    g_string_append (_json, "}");                              \
    *dest = g_string_free (_json, FALSE); }

#define _JSON_AUTO_COMMA                                       \
    if (_json->str[_json->len - 1] != '{'                      \
        && _json->str[_json->len - 1] != '[')                  \
      g_string_append (_json, ",");

#define _ADD_KEY_VAL_BOOLEAN(key, val)                         \
    _JSON_AUTO_COMMA                                           \
    g_string_append_printf (_json, "\"%s\":%s", key, (val) ? "true" : "false");

#define _ADD_KEY_VAL_UINT(key, val)                            \
    _JSON_AUTO_COMMA                                           \
    g_string_append_printf (_json, "\"%s\":%" G_GUINT64_FORMAT, key, (guint64) val);

#define _ADD_KEY_VAL_DOUBLE(key, val) {                        \
    gchar _buf[G_ASCII_DTOSTR_BUF_SIZE];                       \
    _JSON_AUTO_COMMA                                           \
    g_string_append_printf (_json, "\"%s\":%s", key,           \
        g_ascii_formatd (_buf, sizeof (_buf), "%.2f", val)); }

#define _ADD_KEY_VAL_STRING(key, val)                          \
    _JSON_AUTO_COMMA                                           \
    if (G_UNLIKELY (val == NULL))                              \
      g_string_append_printf (_json, "\"%s\":null", key);      \
    else                                                       \
      g_string_append_printf (_json, "\"%s\":\"%s\"", key, val);

#define _ADD_KEY_VAL_STRING_TAKE(key, val)                     \
    _ADD_KEY_VAL_STRING(key, val)                              \
    g_free (val);

#define _ADD_VAL_STRING(val)                                   \
    _JSON_AUTO_COMMA                                           \
    if (G_UNLIKELY (val == NULL))                              \
      g_string_append (_json, "null");                         \
    else                                                       \
      g_string_append_printf (_json, "\"%s\"", val);

#define _ADD_VAL_STRING_TAKE(val)                              \
    _ADD_VAL_STRING(val)                                       \
    g_free (val);

#define _ADD_OBJECT(...)                                       \
    _JSON_AUTO_COMMA                                           \
    g_string_append (_json, "{");                              \
    __VA_ARGS__                                                \
    g_string_append (_json, "}");

#define _ADD_NAMED_OBJECT(name, ...)                           \
    _JSON_AUTO_COMMA                                           \
    g_string_append_printf (_json, "\"%s\":{", name);          \
    __VA_ARGS__                                                \
    g_string_append (_json, "}");

#define _ADD_NAMED_ARRAY(name, ...)                            \
    _JSON_AUTO_COMMA                                           \
    g_string_append_printf (_json, "\"%s\":[", name);          \
    __VA_ARGS__                                                \
    g_string_append (_json, "]");

static inline void
nyx_server_json_escape_string (gchar **string)
{
  gchar *dest, *src = *string;
  guint i, offset = 0;

  for (i = 0; src[i] != '\0'; ++i) {
    switch (src[i]) {
      case '\"':
        offset++;
      default:
        break;
    }
  }

  /* Nothing to escape, leave string unchaged */
  if (offset == 0)
    return;

  /* Previous length + n_escapes + term */
  dest = g_new (gchar, i + offset + 1);
  offset = 0;

  for (i = 0; src[i] != '\0'; ++i) {
    switch (src[i]) {
      case '\"':
        dest[i + offset] = '\\';
        dest[i + offset + 1] = '\"';
        offset++;
        break;
      default:
        dest[i + offset] = src[i];
        break;
    }
  }
  dest[i + offset] = '\0';

  g_free (*string);
  *string = dest;
}

gchar *
nyx_server_json_build_complete (NyxServer *server, NyxMediaItem *played_item,
    guint played_index, GPtrArray *items)
{
  NyxPlayer *player;
  gchar *data = NULL;

  player = NYX_PLAYER_CAST (gst_object_get_parent (GST_OBJECT_CAST (server)));

  if (G_UNLIKELY (player == NULL))
    return NULL;

  NYX_SERVER_JSON_BUILD (&data, {
    switch (nyx_player_get_state (player)) {
      case NYX_PLAYER_STATE_PLAYING:
        _ADD_KEY_VAL_STRING ("state", NYX_SERVER_PLAYER_STATE_PLAYING);
        break;
      case NYX_PLAYER_STATE_PAUSED:
        _ADD_KEY_VAL_STRING ("state", NYX_SERVER_PLAYER_STATE_PAUSED);
        break;
      case NYX_PLAYER_STATE_BUFFERING:
        _ADD_KEY_VAL_STRING ("state", NYX_SERVER_PLAYER_STATE_BUFFERING);
        break;
      case NYX_PLAYER_STATE_STOPPED:
        _ADD_KEY_VAL_STRING ("state", NYX_SERVER_PLAYER_STATE_STOPPED);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    _ADD_KEY_VAL_UINT ("position", nyx_player_get_position (player));
    _ADD_KEY_VAL_DOUBLE ("speed", nyx_player_get_speed (player));
    _ADD_KEY_VAL_DOUBLE ("volume", nyx_player_get_volume (player));
    _ADD_KEY_VAL_BOOLEAN ("mute", nyx_player_get_mute (player));

    _ADD_NAMED_OBJECT ("queue", {
      NyxQueue *queue = nyx_player_get_queue (player);

      _ADD_KEY_VAL_BOOLEAN ("controllable", nyx_server_get_queue_controllable (server));
      _ADD_KEY_VAL_UINT ("played_index", played_index);
      _ADD_KEY_VAL_UINT ("n_items", items->len);

      switch (nyx_queue_get_progression_mode (queue)) {
        case NYX_QUEUE_PROGRESSION_NONE:
          _ADD_KEY_VAL_STRING ("progression", NYX_SERVER_QUEUE_PROGRESSION_NONE);
          break;
        case NYX_QUEUE_PROGRESSION_CONSECUTIVE:
          _ADD_KEY_VAL_STRING ("progression", NYX_SERVER_QUEUE_PROGRESSION_CONSECUTIVE);
          break;
        case NYX_QUEUE_PROGRESSION_REPEAT_ITEM:
          _ADD_KEY_VAL_STRING ("progression", NYX_SERVER_QUEUE_PROGRESSION_REPEAT_ITEM);
          break;
        case NYX_QUEUE_PROGRESSION_CAROUSEL:
          _ADD_KEY_VAL_STRING ("progression", NYX_SERVER_QUEUE_PROGRESSION_CAROUSEL);
          break;
        case NYX_QUEUE_PROGRESSION_SHUFFLE:
          _ADD_KEY_VAL_STRING ("progression", NYX_SERVER_QUEUE_PROGRESSION_SHUFFLE);
          break;
        default:
          g_assert_not_reached ();
          break;
      }

      _ADD_NAMED_ARRAY ("items", {
        guint i;

        for (i = 0; i < items->len; ++i) {
          _ADD_OBJECT ({
            NyxMediaItem *item = (NyxMediaItem *) g_ptr_array_index (items, i);
            gchar *title = nyx_media_item_get_title (item);

            if (title)
              nyx_server_json_escape_string (&title);

            _ADD_KEY_VAL_UINT ("id", nyx_media_item_get_id (item));
            _ADD_KEY_VAL_STRING_TAKE ("title", title);
            _ADD_KEY_VAL_UINT ("duration", nyx_media_item_get_duration (item));

            /* TODO: Add more info per item (including timeline markers) */
          });
        }
      });
    });
  });

  gst_object_unref (player);

  return data;
}
