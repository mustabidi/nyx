/* Nyx GTK Integration Library
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

#include "config.h"

#include <glib/gi18n-lib.h>
#include <nyx/nyx.h>

#include "nyx-gtk-stream-check-button-private.h"
#include "nyx-gtk-utils-private.h"

#define MAX_SIGNALS 4

struct _NyxGtkStreamCheckButton
{
  GtkCheckButton parent;

  NyxStream *stream;

  GtkWidget *fallback_check_button;

  gulong signal_ids[MAX_SIGNALS];
  gboolean grouped;
};

#define parent_class nyx_gtk_stream_check_button_parent_class
G_DEFINE_TYPE (NyxGtkStreamCheckButton, nyx_gtk_stream_check_button, GTK_TYPE_CHECK_BUTTON)

enum
{
  PROP_0,
  PROP_STREAM,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static inline gchar *
_get_video_stream_label (NyxVideoStream *vstream)
{
  gchar *label, *codec = nyx_video_stream_get_codec (vstream);
  gint height = nyx_video_stream_get_height (vstream);
  gdouble fps = nyx_video_stream_get_fps (vstream);

  if (codec) {
    label = g_strdup_printf ("%ip@%.3f [%s]", height, fps, codec);
    g_free (codec);
  } else {
    label = g_strdup_printf ("%ip@%.3f", height, fps);
  }

  return label;
}

static inline gchar *
_get_audio_stream_label (NyxAudioStream *astream)
{
  gchar *label, *title, *codec = nyx_audio_stream_get_codec (astream);
  gint channels = nyx_audio_stream_get_channels (astream);

  if (!(title = nyx_stream_get_title (NYX_STREAM_CAST (astream))))
    title = nyx_audio_stream_get_lang_name (astream);

  if (codec) {
    label = g_strdup_printf ("%s [%s, %i %s]",
        (title) ? title : _("Undetermined"), codec, channels, _("Channels"));
    g_free (title);
    g_free (codec);
  } else {
    label = title;
  }

  return label;
}

static inline gchar *
_get_subtitle_stream_label (NyxSubtitleStream *sstream)
{
  gchar *title;

  if (!(title = nyx_stream_get_title (NYX_STREAM_CAST (sstream))))
    title = nyx_subtitle_stream_get_lang_name (sstream);

  return title;
}

static void
_refresh_label_cb (NyxStream *stream,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkStreamCheckButton *self)
{
  gchar *label = NULL;

  if (stream) {
    switch (nyx_stream_get_stream_type (stream)) {
      case NYX_STREAM_TYPE_VIDEO:
        label = _get_video_stream_label (NYX_VIDEO_STREAM (stream));
        break;
      case NYX_STREAM_TYPE_AUDIO:
        label = _get_audio_stream_label (NYX_AUDIO_STREAM (stream));
        break;
      case NYX_STREAM_TYPE_SUBTITLE:
        label = _get_subtitle_stream_label (NYX_SUBTITLE_STREAM (stream));
        break;
      default:
        break;
    }
  }

  gtk_check_button_set_label (GTK_CHECK_BUTTON (self),
      (label != NULL) ? label : _("Undetermined"));
  g_free (label);
}

static inline void
_disconnect_current_signals (NyxGtkStreamCheckButton *self)
{
  guint i;

  for (i = 0; i < MAX_SIGNALS; ++i) {
    /* No more signals connected */
    if (self->signal_ids[i] == 0)
      break;

    g_signal_handler_disconnect (self->stream, self->signal_ids[i]);
    self->signal_ids[i] = 0;
  }
}

static inline void
nyx_gtk_stream_check_button_set_stream (NyxGtkStreamCheckButton *self, NyxStream *stream)
{
  guint i = 0;

  _disconnect_current_signals (self);
  gst_object_replace ((GstObject **) &self->stream, GST_OBJECT_CAST (stream));

  if (!self->stream)
    return;

  switch (nyx_stream_get_stream_type (stream)) {
    case NYX_STREAM_TYPE_VIDEO:
      self->signal_ids[i++] = g_signal_connect (self->stream, "notify::codec",
          G_CALLBACK (_refresh_label_cb), self);
      self->signal_ids[i++] = g_signal_connect (self->stream, "notify::height",
          G_CALLBACK (_refresh_label_cb), self);
      self->signal_ids[i++] = g_signal_connect (self->stream, "notify::fps",
          G_CALLBACK (_refresh_label_cb), self);
      break;
    case NYX_STREAM_TYPE_AUDIO:
      self->signal_ids[i++] = g_signal_connect (self->stream, "notify::codec",
          G_CALLBACK (_refresh_label_cb), self);
      self->signal_ids[i++] = g_signal_connect (self->stream, "notify::channels",
          G_CALLBACK (_refresh_label_cb), self);
      G_GNUC_FALLTHROUGH;
    case NYX_STREAM_TYPE_SUBTITLE:
      self->signal_ids[i++] = g_signal_connect (self->stream, "notify::title",
          G_CALLBACK (_refresh_label_cb), self);
      self->signal_ids[i++] = g_signal_connect (self->stream, "notify::lang-name",
          G_CALLBACK (_refresh_label_cb), self);
      break;
    default:
      break;
  }

  _refresh_label_cb (self->stream, NULL, self);
}

static void
nyx_gtk_stream_check_button_realize (GtkWidget *widget)
{
  NyxGtkStreamCheckButton *self = NYX_GTK_STREAM_CHECK_BUTTON_CAST (widget);
  GtkWidget *other_widget;

  GST_TRACE_OBJECT (self, "Realize");

  /* Set same group as previous check button in the same list view */
  if (!self->grouped) {
    if ((other_widget = gtk_widget_get_parent (widget))
        && (other_widget = gtk_widget_get_prev_sibling (other_widget))
        && (other_widget = gtk_widget_get_first_child (other_widget))
        && NYX_GTK_IS_STREAM_CHECK_BUTTON (other_widget)) {
      gtk_check_button_set_group (GTK_CHECK_BUTTON (self), GTK_CHECK_BUTTON (other_widget));
    } else {
      if (!self->fallback_check_button)
        self->fallback_check_button = g_object_ref_sink (gtk_check_button_new ());

      gtk_check_button_set_group (GTK_CHECK_BUTTON (self), GTK_CHECK_BUTTON (self->fallback_check_button));
    }
    self->grouped = TRUE;
  }

  GTK_WIDGET_CLASS (parent_class)->realize (widget);
}

static void
nyx_gtk_stream_check_button_toggled (GtkCheckButton *check_button)
{
  NyxGtkStreamCheckButton *self = NYX_GTK_STREAM_CHECK_BUTTON_CAST (check_button);

  if (gtk_check_button_get_active (check_button) && self->stream) {
    NyxStreamList *stream_list = NYX_STREAM_LIST (gst_object_get_parent (GST_OBJECT (self->stream)));

    if (G_LIKELY (stream_list != NULL)) {
      GST_INFO_OBJECT (self, "Toggled: %" GST_PTR_FORMAT, self->stream);
      nyx_stream_list_select_stream (stream_list, self->stream);
      gst_object_unref (stream_list);
    }
  }
}

static void
nyx_gtk_stream_check_button_init (NyxGtkStreamCheckButton *self)
{
}

static void
nyx_gtk_stream_check_button_finalize (GObject *object)
{
  NyxGtkStreamCheckButton *self = NYX_GTK_STREAM_CHECK_BUTTON_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  _disconnect_current_signals (self);
  gst_clear_object (&self->stream);

  g_clear_object (&self->fallback_check_button);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_gtk_stream_check_button_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxGtkStreamCheckButton *self = NYX_GTK_STREAM_CHECK_BUTTON_CAST (object);

  switch (prop_id) {
    case PROP_STREAM:
      nyx_gtk_stream_check_button_set_stream (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_gtk_stream_check_button_class_init (NyxGtkStreamCheckButtonClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
  GtkCheckButtonClass *check_button_class = (GtkCheckButtonClass *) klass;

  nyx_gtk_init_translations ();

  gobject_class->set_property = nyx_gtk_stream_check_button_set_property;
  gobject_class->finalize = nyx_gtk_stream_check_button_finalize;

  widget_class->realize = nyx_gtk_stream_check_button_realize;

  check_button_class->toggled = nyx_gtk_stream_check_button_toggled;

  param_specs[PROP_STREAM] = g_param_spec_object ("stream",
      NULL, NULL, NYX_TYPE_STREAM,
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}
