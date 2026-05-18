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

/**
 * NyxMediaItem:
 *
 * Represents a media item.
 *
 * A newly created media item must be added to player [class@Nyx.Queue]
 * first in order to be played.
 */

#include "nyx-media-item-private.h"
#include "nyx-timeline-private.h"
#include "nyx-player-private.h"
#include "nyx-playbin-bus-private.h"
#include "nyx-reactables-manager-private.h"
#include "nyx-features-manager-private.h"
#include "nyx-utils-private.h"

#define GST_CAT_DEFAULT nyx_media_item_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxMediaItem
{
  GstObject parent;

  gchar *uri;
  gchar *suburi;

  GstTagList *tags;
  NyxTimeline *timeline;

  guint id;
  gchar *title;
  gchar *container_format;
  gdouble duration;

  /* Whether using title from URI */
  gboolean title_is_parsed;

  GType redirects_src_type;
  gchar *redirect_uri;
  gchar *cache_uri;

  /* For shuffle */
  gboolean used;
};

typedef struct
{
  NyxMediaItem *item;
  gboolean changed;
  gboolean allow_overwrite;
} NyxMediaItemTagIterData;

enum
{
  PROP_0,
  PROP_ID,
  PROP_URI,
  PROP_SUBURI,
  PROP_REDIRECT_URI,
  PROP_CACHE_LOCATION,
  PROP_TAGS,
  PROP_TITLE,
  PROP_CONTAINER_FORMAT,
  PROP_DURATION,
  PROP_TIMELINE,
  PROP_LAST
};

#define parent_class nyx_media_item_parent_class
G_DEFINE_TYPE (NyxMediaItem, nyx_media_item, GST_TYPE_OBJECT);

static gint _item_id = 0;
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/**
 * nyx_media_item_new:
 * @uri: a media URI
 *
 * Creates new #NyxMediaItem from URI.
 *
 * Use one of the URI protocols supported by plugins in #GStreamer
 * installation. For local files you can use either "file" protocol
 * or [ctor@Nyx.MediaItem.new_from_file] method.
 *
 * It is considered a programmer error trying to create new media item from
 * invalid URI. If URI is valid, but unsupported by installed plugins on user
 * system, [class@Nyx.Player] will emit a [signal@Nyx.Player::missing-plugin]
 * signal upon playback.
 *
 * Returns: (transfer full): a new #NyxMediaItem.
 */
NyxMediaItem *
nyx_media_item_new (const gchar *uri)
{
  NyxMediaItem *item;

  g_return_val_if_fail (uri != NULL, NULL);

  item = g_object_new (NYX_TYPE_MEDIA_ITEM, "uri", uri, NULL);

  return gst_object_ref_sink (item);
}

/**
 * nyx_media_item_new_from_file:
 * @file: a #GFile
 *
 * Creates new #NyxMediaItem from #GFile.
 *
 * Same as [ctor@Nyx.MediaItem.new], but uses a [iface@Gio.File]
 * for convenience in some situations instead of an URI.
 *
 * Returns: (transfer full): a new #NyxMediaItem.
 */
NyxMediaItem *
nyx_media_item_new_from_file (GFile *file)
{
  NyxMediaItem *item;
  gchar *uri;
  gsize length;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  uri = g_file_get_uri (file);
  length = strlen (uri);

  /* GFile might incorrectly append "/" at the end of an URI,
   * remove it to make it work with GStreamer URI handling */
  if (uri[length - 1] == '/')
    uri[length - 1] = '\0';

  item = g_object_new (NYX_TYPE_MEDIA_ITEM, "uri", uri, NULL);
  g_free (uri);

  return gst_object_ref_sink (item);
}

/**
 * nyx_media_item_new_cached:
 * @uri: a media URI
 * @location: (type filename) (nullable): a path to downloaded file
 *
 * Same as [ctor@Nyx.MediaItem.new], but allows to provide
 * a location of a cache file where particular media at @uri
 * is supposed to be found.
 *
 * File at @location existence will be checked upon starting playback
 * of created item. If cache file is not found, media item @uri will be
 * used as fallback. In this case when [property@Nyx.Player:download-enabled]
 * is set to %TRUE, item will be downloaded and cached again if possible.
 *
 * Returns: (transfer full): a new #NyxMediaItem.
 *
 * Since: 0.8
 */
NyxMediaItem *
nyx_media_item_new_cached (const gchar *uri, const gchar *location)
{
  NyxMediaItem *item;

  g_return_val_if_fail (uri != NULL, NULL);

  item = g_object_new (NYX_TYPE_MEDIA_ITEM,
      "uri", uri, "cache-location", location, NULL);

  return gst_object_ref_sink (item);
}

/**
 * nyx_media_item_get_id:
 * @item: a #NyxMediaItem
 *
 * Get the unique ID of #NyxMediaItem.
 *
 * Returns: an ID of #NyxMediaItem.
 */
guint
nyx_media_item_get_id (NyxMediaItem *self)
{
  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), G_MAXUINT);

  return self->id;
}

/**
 * nyx_media_item_get_uri:
 * @item: a #NyxMediaItem
 *
 * Get the URI of #NyxMediaItem.
 *
 * Returns: an URI of #NyxMediaItem.
 */
const gchar *
nyx_media_item_get_uri (NyxMediaItem *self)
{
  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  return self->uri;
}

/**
 * nyx_media_item_set_suburi:
 * @item: a #NyxMediaItem
 * @suburi: an additional URI
 *
 * Set the additional URI of #NyxMediaItem.
 *
 * This is typically used to add an external subtitles URI to the @item.
 */
void
nyx_media_item_set_suburi (NyxMediaItem *self, const gchar *suburi)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  changed = g_set_str (&self->suburi, suburi);
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    NyxPlayer *player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self));

    if (player) {
      nyx_app_bus_post_prop_notify (player->app_bus,
          GST_OBJECT_CAST (self), param_specs[PROP_SUBURI]);
      nyx_playbin_bus_post_item_suburi_change (player->bus, self);

      gst_object_unref (player);
    }
  }
}

/**
 * nyx_media_item_get_suburi:
 * @item: a #NyxMediaItem
 *
 * Get the additional URI of #NyxMediaItem.
 *
 * Returns: (transfer full) (nullable): an additional URI of #NyxMediaItem.
 */
gchar *
nyx_media_item_get_suburi (NyxMediaItem *self)
{
  gchar *suburi;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  suburi = g_strdup (self->suburi);
  GST_OBJECT_UNLOCK (self);

  return suburi;
}

/**
 * nyx_media_item_get_redirect_uri:
 * @item: a #NyxMediaItem
 *
 * Get permanent redirect URI of #NyxMediaItem.
 *
 * Returns: (transfer full) (nullable): a redirected URI of #NyxMediaItem.
 *
 * Since: 0.10
 */
gchar *
nyx_media_item_get_redirect_uri (NyxMediaItem *self)
{
  gchar *redirect_uri;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  redirect_uri = g_strdup (self->redirect_uri);
  GST_OBJECT_UNLOCK (self);

  return redirect_uri;
}

/**
 * nyx_media_item_get_cache_location:
 * @item: a #NyxMediaItem
 *
 * Get downloaded cache file location of #NyxMediaItem.
 *
 * Returns: (transfer full) (type filename) (nullable): a cache file location of #NyxMediaItem.
 *
 * Since: 0.10
 */
gchar *
nyx_media_item_get_cache_location (NyxMediaItem *self)
{
  gchar *cache_location = NULL;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  if (self->cache_uri)
    cache_location = g_filename_from_uri (self->cache_uri, NULL, NULL);
  GST_OBJECT_UNLOCK (self);

  return cache_location;
}

/**
 * nyx_media_item_get_title:
 * @item: a #NyxMediaItem
 *
 * Get media item title.
 *
 * The title can be either text detected by media discovery once it
 * completes. Otherwise whenever possible this will try to return a title
 * extracted from media URI e.g. basename without extension for local files.
 *
 * Returns: (transfer full) (nullable): media title.
 */
gchar *
nyx_media_item_get_title (NyxMediaItem *self)
{
  gchar *title;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  title = g_strdup (self->title);
  GST_OBJECT_UNLOCK (self);

  return title;
}

static inline gboolean
_refresh_tag_prop_unlocked (NyxMediaItem *self, const gchar *tag,
    gboolean allow_overwrite, gchar **tag_ptr)
{
  const gchar *string;

  if ((*tag_ptr && !allow_overwrite)
      || !gst_tag_list_peek_string_index (self->tags, tag, 0, &string) // guarantees non-empty string
      || (g_strcmp0 (*tag_ptr, string) == 0))
    return FALSE;

  GST_LOG_OBJECT (self, "Tag prop \"%s\" update: \"%s\" -> \"%s\"",
      tag, GST_STR_NULL (*tag_ptr), string);

  g_free (*tag_ptr);
  *tag_ptr = g_strdup (string);

  return TRUE;
}

/**
 * nyx_media_item_get_container_format:
 * @item: a #NyxMediaItem
 *
 * Get media item container format.
 *
 * Returns: (transfer full) (nullable): media container format.
 *
 * Deprecated: 0.10: Get `container-format` from [property@Nyx.MediaItem:tags] instead.
 */
gchar *
nyx_media_item_get_container_format (NyxMediaItem *self)
{
  gchar *container_format;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  container_format = g_strdup (self->container_format);
  GST_OBJECT_UNLOCK (self);

  return container_format;
}

gboolean
nyx_media_item_set_duration (NyxMediaItem *self, gdouble duration,
    NyxAppBus *app_bus)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);
  if ((changed = !G_APPROX_VALUE (self->duration, duration, FLT_EPSILON)))
    self->duration = duration;
  GST_OBJECT_UNLOCK (self);

  if (changed) {
    GST_DEBUG_OBJECT (self, "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS (duration * GST_SECOND));
    nyx_app_bus_post_prop_notify (app_bus, GST_OBJECT_CAST (self), param_specs[PROP_DURATION]);
  }

  return changed;
}

/**
 * nyx_media_item_get_duration:
 * @item: a #NyxMediaItem
 *
 * Get media item duration as decimal number in seconds.
 *
 * Returns: media duration.
 */
gdouble
nyx_media_item_get_duration (NyxMediaItem *self)
{
  gdouble duration;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), 0);

  GST_OBJECT_LOCK (self);
  duration = self->duration;
  GST_OBJECT_UNLOCK (self);

  return duration;
}

/**
 * nyx_media_item_get_tags:
 * @item: a #NyxMediaItem
 *
 * Get readable list of tags stored in media item.
 *
 * Returns: (transfer full): a #GstTagList.
 *
 * Since: 0.10
 */
GstTagList *
nyx_media_item_get_tags (NyxMediaItem *self)
{
  GstTagList *tags = NULL;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  GST_OBJECT_LOCK (self);
  tags = gst_tag_list_ref (self->tags);
  GST_OBJECT_UNLOCK (self);

  return tags;
}

static void
_tags_replace_func (const GstTagList *tags, const gchar *tag, NyxMediaItemTagIterData *data)
{
  NyxMediaItem *self = data->item;
  guint index = 0;
  gboolean replace = FALSE;

  while (TRUE) {
    const GValue *old_value = gst_tag_list_get_value_index (self->tags, tag, index);
    const GValue *new_value = gst_tag_list_get_value_index (tags, tag, index);

    /* Number of old values is the same or greater and
     * all values until this iteration were the same */
    if (!new_value)
      break;

    /* A wild new tag appeared */
    if (!old_value) {
      replace = TRUE;
      break;
    }

    /* No overwrites allowed (e.g. user tags) */
    if (!data->allow_overwrite)
      break;

    /* Check with tolerance for doubles */
    if (G_VALUE_TYPE (old_value) == G_TYPE_DOUBLE
        && G_VALUE_TYPE (new_value) == G_TYPE_DOUBLE) {
      gdouble old_dbl, new_dbl;

      old_dbl = g_value_get_double (old_value);
      new_dbl = g_value_get_double (new_value);

      if ((replace = !G_APPROX_VALUE (old_dbl, new_dbl, FLT_EPSILON)))
        break;
    } else if (gst_value_compare (old_value, new_value) != GST_VALUE_EQUAL) {
      replace = TRUE;
      break;
    }

    ++index;
  }

  if (replace) {
    const GValue *value;
    index = 0;

    GST_LOG_OBJECT (self, "Replacing \"%s\" tag value", tag);

    /* Ensure writable, but only when replacing something */
    if (!data->changed) {
      self->tags = gst_tag_list_make_writable (self->tags);
      data->changed = TRUE;
    }

    /* Replace first tag value (so it becomes sole member) */
    value = gst_tag_list_get_value_index (tags, tag, index);
    gst_tag_list_add_value (self->tags, GST_TAG_MERGE_REPLACE, tag, value);

    /* Append any remaining tags (so next time we iterate indexes will match) */
    while ((value = gst_tag_list_get_value_index (tags, tag, ++index)))
      gst_tag_list_add_value (self->tags, GST_TAG_MERGE_APPEND, tag, value);
  }
}

static gboolean
nyx_media_item_insert_tags_internal (NyxMediaItem *self, const GstTagList *tags,
    NyxAppBus *app_bus, gboolean allow_overwrite, NyxReactableItemUpdatedFlags *flags)
{
  NyxMediaItemTagIterData data;
  gboolean title_changed = FALSE, cont_changed = FALSE;

  GST_OBJECT_LOCK (self);

  data.item = self;
  data.changed = FALSE;
  data.allow_overwrite = allow_overwrite;

  if (G_LIKELY (tags != self->tags))
    gst_tag_list_foreach (tags, (GstTagForeachFunc) _tags_replace_func, &data);

  if (data.changed) {
    *flags |= NYX_REACTABLE_ITEM_UPDATED_TAGS;

    if ((title_changed = _refresh_tag_prop_unlocked (self, GST_TAG_TITLE,
        (allow_overwrite || self->title_is_parsed), &self->title))) {
      self->title_is_parsed = FALSE;
      *flags |= NYX_REACTABLE_ITEM_UPDATED_TITLE;
    }
    cont_changed = _refresh_tag_prop_unlocked (self, GST_TAG_CONTAINER_FORMAT,
        allow_overwrite, &self->container_format);
  }

  GST_OBJECT_UNLOCK (self);

  if (!data.changed)
    return FALSE;

  if (app_bus) {
    GstObject *src = GST_OBJECT_CAST (self);

    nyx_app_bus_post_prop_notify (app_bus, src, param_specs[PROP_TAGS]);

    if (title_changed)
      nyx_app_bus_post_prop_notify (app_bus, src, param_specs[PROP_TITLE]);
    if (cont_changed)
      nyx_app_bus_post_prop_notify (app_bus, src, param_specs[PROP_CONTAINER_FORMAT]);
  } else {
    GObject *src = G_OBJECT (self);

    nyx_utils_prop_notify_on_main_sync (src, param_specs[PROP_TAGS]);

    if (title_changed)
      nyx_utils_prop_notify_on_main_sync (src, param_specs[PROP_TITLE]);
    if (cont_changed)
      nyx_utils_prop_notify_on_main_sync (src, param_specs[PROP_CONTAINER_FORMAT]);
  }

  return TRUE;
}

/**
 * nyx_media_item_populate_tags:
 * @item: a #NyxMediaItem
 * @tags: a #GstTagList of GLOBAL scope
 *
 * Populate non-existing tags in @item tag list.
 *
 * Passed @tags must use [enum@Gst.TagScope.GLOBAL] scope.
 *
 * Note that tags are automatically determined during media playback
 * and those take precedence. This function can be useful if an app can
 * determine some tags that are not in media metadata or for filling
 * item with some initial/cached tags to display in UI before playback.
 *
 * When a tag already exists in the tag list (was populated) this
 * function will not overwrite it. If you really need to permanently
 * override some tags in media, you can use `taginject` element as
 * player video/audio filter instead.
 *
 * Returns: whether at least one tag got updated.
 *
 * Since: 0.10
 */
gboolean
nyx_media_item_populate_tags (NyxMediaItem *self, const GstTagList *tags)
{
  NyxPlayer *player;
  NyxAppBus *app_bus = NULL;
  NyxReactableItemUpdatedFlags flags = 0;
  gboolean changed;

  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), FALSE);
  g_return_val_if_fail (tags != NULL, FALSE);

  if (G_UNLIKELY (gst_tag_list_get_scope (tags) != GST_TAG_SCOPE_GLOBAL)) {
    g_warning ("Cannot populate media item tags using a list with non-global tag scope");
    return FALSE;
  }

  if ((player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self))))
    app_bus = player->app_bus;

  changed = nyx_media_item_insert_tags_internal (self, tags, app_bus, FALSE, &flags);

  if (changed && player) {
    NyxFeaturesManager *features_manager;

    if (player->reactables_manager)
      nyx_reactables_manager_trigger_item_updated (player->reactables_manager, self, flags);
    if ((features_manager = nyx_player_get_features_manager (player)))
      nyx_features_manager_trigger_item_updated (features_manager, self);
  }

  gst_clear_object (&player);

  return changed;
}

/**
 * nyx_media_item_get_timeline:
 * @item: a #NyxMediaItem
 *
 * Get the [class@Nyx.Timeline] associated with @item.
 *
 * Returns: (transfer none): a #NyxTimeline of item.
 */
NyxTimeline *
nyx_media_item_get_timeline (NyxMediaItem *self)
{
  g_return_val_if_fail (NYX_IS_MEDIA_ITEM (self), NULL);

  return self->timeline;
}

void
nyx_media_item_update_from_tag_list (NyxMediaItem *self, const GstTagList *tags,
    gboolean allow_overwrite, NyxPlayer *player)
{
  NyxReactableItemUpdatedFlags flags = 0;
  gboolean changed = nyx_media_item_insert_tags_internal (self, tags, player->app_bus, allow_overwrite, &flags);

  if (changed) {
    NyxFeaturesManager *features_manager;

    if (player->reactables_manager)
      nyx_reactables_manager_trigger_item_updated (player->reactables_manager, self, flags);
    if ((features_manager = nyx_player_get_features_manager (player)))
      nyx_features_manager_trigger_item_updated (features_manager, self);
  }
}

void
nyx_media_item_update_from_discoverer_info (NyxMediaItem *self, GstDiscovererInfo *info)
{
  NyxPlayer *player;
  GstDiscovererStreamInfo *sinfo;
  GstClockTime duration;
  NyxReactableItemUpdatedFlags flags = 0;
  gdouble val_dbl;
  gboolean changed = FALSE;

  if (!(player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self))))
    return;

  for (sinfo = gst_discoverer_info_get_stream_info (info);
      sinfo != NULL;
      sinfo = gst_discoverer_stream_info_get_next (sinfo)) {
    const GstTagList *tags;

    if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
      GstDiscovererContainerInfo *cinfo = (GstDiscovererContainerInfo *) sinfo;

      if ((tags = gst_discoverer_container_info_get_tags (cinfo)))
        changed |= nyx_media_item_insert_tags_internal (self, tags, player->app_bus, TRUE, &flags);
    }
    gst_discoverer_stream_info_unref (sinfo);
  }

  duration = gst_discoverer_info_get_duration (info);

  if (G_UNLIKELY (duration == GST_CLOCK_TIME_NONE))
    duration = 0;

  val_dbl = (gdouble) duration / GST_SECOND;
  if (nyx_media_item_set_duration (self, val_dbl, player->app_bus)) {
    changed = TRUE;
    flags |= NYX_REACTABLE_ITEM_UPDATED_DURATION;
  }

  if (changed) {
    NyxFeaturesManager *features_manager;

    if (player->reactables_manager)
      nyx_reactables_manager_trigger_item_updated (player->reactables_manager, self, flags);
    if ((features_manager = nyx_player_get_features_manager (player)))
      nyx_features_manager_trigger_item_updated (features_manager, self);
  }

  gst_object_unref (player);
}

/* XXX: Must be set from player thread */
static inline gboolean
nyx_media_item_set_redirect_uri (NyxMediaItem *self, const gchar *redirect_uri,
    GstObject *redirect_src)
{
  GType src_type;
  gboolean changed;

  /* Safety checks */
  if (G_UNLIKELY (!redirect_uri)) {
    GST_ERROR_OBJECT (self, "Received redirect request without an URI set");
    return FALSE;
  }
  if (G_UNLIKELY (!redirect_src)) {
    GST_ERROR_OBJECT (self, "Received redirect request without source object set");
    return FALSE;
  }

  /* Only allow redirects determined by the same source type, otherwise
   * we would start mixing URIs when multiple sources message them */
  src_type = G_OBJECT_TYPE (redirect_src);

  if (self->redirects_src_type == 0) {
    self->redirects_src_type = src_type;
  } else if (self->redirects_src_type != src_type) {
    GST_LOG_OBJECT (self, "Ignoring redirection from different source: %s",
        G_OBJECT_TYPE_NAME (redirect_src));
    return FALSE;
  }

  GST_OBJECT_LOCK (self);
  changed = g_set_str (&self->redirect_uri, redirect_uri);
  GST_OBJECT_UNLOCK (self);

  GST_DEBUG_OBJECT (self, "Set redirect URI: \"%s\", source: %s",
      redirect_uri, G_OBJECT_TYPE_NAME (redirect_src));

  return changed;
}

gboolean
nyx_media_item_update_from_parsed_playlist (NyxMediaItem *self, GListStore *playlist,
    GstObject *playlist_src, NyxPlayer *player)
{
  NyxMediaItem *other_item;
  const gchar *redirect_uri;
  gboolean success, title_changed = FALSE;

  /* First playlist item URI becomes a redirect URI for item to be updated */
  other_item = g_list_model_get_item (G_LIST_MODEL (playlist), 0);
  redirect_uri = nyx_media_item_get_uri (other_item);

  if ((success = nyx_media_item_set_redirect_uri (self, redirect_uri, playlist_src))) {
    NyxFeaturesManager *features_manager;
    NyxReactableItemUpdatedFlags flags = NYX_REACTABLE_ITEM_UPDATED_REDIRECT_URI;

    /* Notify about URI change before other properties */
    nyx_app_bus_post_prop_notify (player->app_bus, GST_OBJECT_CAST (self),
        param_specs[PROP_REDIRECT_URI]);

    GST_OBJECT_LOCK (other_item);

    if (other_item->tags) // Tags are always GLOBAL here
      nyx_media_item_update_from_tag_list (self, other_item->tags, TRUE, player);

    /* Since its redirect now, we have to update title to describe new file instead of
     * being a playlist title. If other item had parsed title, it also means that tags
     * did not contain it, thus we have to manually update it and notify. */
    if (other_item->title_is_parsed) {
      GST_OBJECT_LOCK (self);
      title_changed = g_set_str (&self->title, other_item->title);
      self->title_is_parsed = TRUE;
      GST_OBJECT_UNLOCK (self);
    }

    GST_OBJECT_UNLOCK (other_item);

    if (title_changed) {
      flags |= NYX_REACTABLE_ITEM_UPDATED_TITLE;
      nyx_app_bus_post_prop_notify (player->app_bus, GST_OBJECT_CAST (self), param_specs[PROP_TITLE]);
    }

    if (player->reactables_manager)
      nyx_reactables_manager_trigger_item_updated (player->reactables_manager, self, flags);
    if ((features_manager = nyx_player_get_features_manager (player)))
      nyx_features_manager_trigger_item_updated (features_manager, self);
  }

  gst_object_unref (other_item);

  return success;
}

/* XXX: Must be set from player thread or upon construction */
void
nyx_media_item_set_cache_location (NyxMediaItem *self, const gchar *location)
{
  gboolean changed;

  GST_OBJECT_LOCK (self);

  /* Skip if both are NULL (no change - called during construction) */
  if ((changed = (self->cache_uri || location))) {
    g_clear_pointer (&self->cache_uri, g_free);

    if (location)
      self->cache_uri = g_filename_to_uri (location, NULL, NULL);

    GST_DEBUG_OBJECT (self, "Set cache URI: \"%s\"",
        GST_STR_NULL (self->cache_uri));
  }

  GST_OBJECT_UNLOCK (self);

  if (changed) {
    NyxPlayer *player;

    if ((player = nyx_player_get_from_ancestor (GST_OBJECT_CAST (self)))) {
      NyxFeaturesManager *features_manager;

      nyx_app_bus_post_prop_notify (player->app_bus,
          GST_OBJECT_CAST (self), param_specs[PROP_CACHE_LOCATION]);

      if (player->reactables_manager) {
        nyx_reactables_manager_trigger_item_updated (player->reactables_manager, self,
            NYX_REACTABLE_ITEM_UPDATED_CACHE_LOCATION);
      }
      if ((features_manager = nyx_player_get_features_manager (player)))
        nyx_features_manager_trigger_item_updated (features_manager, self);

      gst_object_unref (player);
    }
  }
}

/* XXX: Can only be read from player thread.
 * Returns cache URI if available, otherwise redirect or item URI. */
inline const gchar *
nyx_media_item_get_playback_uri (NyxMediaItem *self)
{
  if (self->cache_uri) {
    GFile *file = g_file_new_for_uri (self->cache_uri);
    gboolean exists;

    /* It is an app error if it removes files in non-stopped state,
     * and this function is only called when starting playback */
    exists = g_file_query_exists (file, NULL);
    g_object_unref (file);

    if (exists)
      return self->cache_uri;
  }

  if (self->redirect_uri)
    return self->redirect_uri;

  return self->uri;
}

void
nyx_media_item_set_used (NyxMediaItem *self, gboolean used)
{
  GST_OBJECT_LOCK (self);
  self->used = used;
  GST_OBJECT_UNLOCK (self);
}

gboolean
nyx_media_item_get_used (NyxMediaItem *self)
{
  gboolean used;

  GST_OBJECT_LOCK (self);
  used = self->used;
  GST_OBJECT_UNLOCK (self);

  return used;
}

static void
nyx_media_item_init (NyxMediaItem *self)
{
  self->id = (guint) g_atomic_int_add (&_item_id, 1);

  self->tags = gst_tag_list_new_empty ();
  gst_tag_list_set_scope (self->tags, GST_TAG_SCOPE_GLOBAL);

  self->timeline = nyx_timeline_new ();
  gst_object_set_parent (GST_OBJECT_CAST (self->timeline), GST_OBJECT_CAST (self));
}

static void
nyx_media_item_constructed (GObject *object)
{
  NyxMediaItem *self = NYX_MEDIA_ITEM_CAST (object);

  /* Be safe when someone incorrectly constructs item without URI */
  if (G_UNLIKELY (self->uri == NULL))
    self->uri = g_strdup ("file://");

  self->title = nyx_utils_title_from_uri (self->uri);
  self->title_is_parsed = TRUE;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  GST_TRACE_OBJECT (self, "New media item, ID: %u, URI: \"%s\", title: \"%s\"",
      self->id, self->uri, GST_STR_NULL (self->title));
}

static void
nyx_media_item_finalize (GObject *object)
{
  NyxMediaItem *self = NYX_MEDIA_ITEM_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_free (self->uri);
  g_free (self->title);
  g_free (self->container_format);

  gst_tag_list_unref (self->tags);

  gst_object_unparent (GST_OBJECT_CAST (self->timeline));
  gst_object_unref (self->timeline);

  g_free (self->redirect_uri);
  g_free (self->cache_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_media_item_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxMediaItem *self = NYX_MEDIA_ITEM_CAST (object);

  switch (prop_id) {
    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;
    case PROP_SUBURI:
      nyx_media_item_set_suburi (self, g_value_get_string (value));
      break;
    case PROP_CACHE_LOCATION:
      nyx_media_item_set_cache_location (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_media_item_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxMediaItem *self = NYX_MEDIA_ITEM_CAST (object);

  switch (prop_id) {
    case PROP_ID:
      g_value_set_uint (value, nyx_media_item_get_id (self));
      break;
    case PROP_URI:
      g_value_set_string (value, nyx_media_item_get_uri (self));
      break;
    case PROP_SUBURI:
      g_value_take_string (value, nyx_media_item_get_suburi (self));
      break;
    case PROP_REDIRECT_URI:
      g_value_take_string (value, nyx_media_item_get_redirect_uri (self));
      break;
    case PROP_CACHE_LOCATION:
      g_value_take_string (value, nyx_media_item_get_cache_location (self));
      break;
    case PROP_TAGS:
      g_value_take_boxed (value, nyx_media_item_get_tags (self));
      break;
    case PROP_TITLE:
      g_value_take_string (value, nyx_media_item_get_title (self));
      break;
    case PROP_CONTAINER_FORMAT:
      g_value_take_string (value, nyx_media_item_get_container_format (self));
      break;
    case PROP_DURATION:
      g_value_set_double (value, nyx_media_item_get_duration (self));
      break;
    case PROP_TIMELINE:
      g_value_set_object (value, nyx_media_item_get_timeline (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_media_item_class_init (NyxMediaItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxmediaitem", 0,
      "Nyx Media Item");

  gobject_class->constructed = nyx_media_item_constructed;
  gobject_class->set_property = nyx_media_item_set_property;
  gobject_class->get_property = nyx_media_item_get_property;
  gobject_class->finalize = nyx_media_item_finalize;

  /**
   * NyxMediaItem:id:
   *
   * Media Item ID.
   */
  param_specs[PROP_ID] = g_param_spec_uint ("id",
      NULL, NULL, 0, G_MAXUINT, G_MAXUINT,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMediaItem:uri:
   *
   * Media URI.
   */
  param_specs[PROP_URI] = g_param_spec_string ("uri",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMediaItem:suburi:
   *
   * Media additional URI.
   */
  param_specs[PROP_SUBURI] = g_param_spec_string ("suburi",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMediaItem:redirect-uri:
   *
   * Media permanent redirect URI.
   *
   * Changes when player determines a new redirect for given media item.
   * This will also happen when item URI leads to a playlist. Once playlist
   * is parsed, item is merged with the first item on that playlist and the
   * remaining items are appended to the playback queue after that item position.
   *
   * Once redirect URI in item is present, player will use that URI instead
   * of the default one. Cache location takes precedence over both URIs through.
   *
   * Since: 0.10
   */
  param_specs[PROP_REDIRECT_URI] = g_param_spec_string ("redirect-uri",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMediaItem:cache-location:
   *
   * Media downloaded cache file location.
   *
   * This can be either set for newly created media items or
   * it will be updated after download is completed if
   * [property@Nyx.Player:download-enabled] is set.
   *
   * NOTE: This property was added in 0.8 as write at construct only.
   * It can also be read only since Nyx 0.10.
   *
   * Since: 0.8
   */
  param_specs[PROP_CACHE_LOCATION] = g_param_spec_string ("cache-location",
      NULL, NULL, NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMediaItem:tags:
   *
   * A readable list of tags stored in media item.
   *
   * Since: 0.10
   */
  param_specs[PROP_TAGS] = g_param_spec_boxed ("tags",
      NULL, NULL, GST_TYPE_TAG_LIST,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /* FIXME: 1.0: Consider rename to e.g. "(menu/display)-title"
   * and also make it non-nullable (return URI as final fallback).
   * NOTE: It would probably need to work with redirect URI */
  /**
   * NyxMediaItem:title:
   *
   * Media title.
   *
   * This might be a different string compared to `title` from
   * [property@Nyx.MediaItem:tags], as this gives parsed
   * title from file name/URI as fallback when no `title` tag.
   */
  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMediaItem:container-format:
   *
   * Media container format.
   *
   * Deprecated: 0.10: Get `container-format` from [property@Nyx.MediaItem:tags] instead.
   */
  param_specs[PROP_CONTAINER_FORMAT] = g_param_spec_string ("container-format",
      NULL, NULL, NULL,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS | G_PARAM_DEPRECATED);

  /**
   * NyxMediaItem:duration:
   *
   * Media duration as a decimal number in seconds.
   *
   * This might be a different value compared to `duration` from
   * [property@Nyx.MediaItem:tags], as this value is updated
   * during decoding instead of being a fixed value from metadata.
   */
  param_specs[PROP_DURATION] = g_param_spec_double ("duration",
      NULL, NULL, 0, G_MAXDOUBLE, 0,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxMediaItem:timeline:
   *
   * Media timeline.
   */
  param_specs[PROP_TIMELINE] = g_param_spec_object ("timeline",
      NULL, NULL, NYX_TYPE_TIMELINE,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
