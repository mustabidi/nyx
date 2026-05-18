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

/**
 * NyxGtkBillboard:
 *
 * A layer where various messages can be displayed.
 *
 * #NyxGtkBillboard widget is meant to be overlaid on top of
 * [class@NyxGtk.Video] as a normal (non-fading) overlay.
 *
 * It is used to display various messages/announcements and later
 * takes care of fading them on its own.
 *
 * If automatic volume/speed change notifications when their values do
 * change are desired, functions for announcing them can be run in callbacks
 * to corresponding property notify signals on the [class@Nyx.Player].
 */

#include "config.h"

#include <math.h>
#include <gst/gst.h>

#include "nyx-gtk-billboard.h"
#include "nyx-gtk-utils-private.h"

#define PERCENTAGE_ROUND(a) (round ((gdouble) a / 0.01) * 0.01)
#define WORDS_PER_MSECOND 0.004

#define GST_CAT_DEFAULT nyx_gtk_billboard_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxGtkBillboard
{
  NyxGtkContainer parent;

  GtkWidget *side_revealer;
  GtkWidget *progress_revealer;
  GtkWidget *progress_box;
  GtkWidget *top_progress;
  GtkWidget *bottom_progress;
  GtkWidget *progress_image;
  GtkWidget *progress_label;

  GtkWidget *message_revealer;
  GtkWidget *message_image;
  GtkWidget *message_label;

  gboolean mute;

  gboolean has_pinned;

  guint side_timeout;
  guint message_timeout;

  NyxPlayer *player;
};

#define parent_class nyx_gtk_billboard_parent_class
G_DEFINE_TYPE (NyxGtkBillboard, nyx_gtk_billboard, NYX_GTK_TYPE_CONTAINER)

/* We calculate estimated read time. This allows
 * translated text to be displayed as long as
 * necessary without app developer caring. */
static guint
_estimate_read_time (const gchar *text)
{
  guint i, n_words = 1;
  guint read_time;

  for (i = 0; text[i] != '\0'; ++i) {
    if (text[i] == ' ' || text[i] == '\n')
      n_words++;
  }

  read_time = MAX (1500, (n_words / WORDS_PER_MSECOND) + 500);
  GST_DEBUG ("Estimated message read time: %u", read_time);

  return read_time;
}

static void
_unreveal_side_delay_cb (NyxGtkBillboard *self)
{
  GST_LOG_OBJECT (self, "Unreveal side handler reached");
  self->side_timeout = 0;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->side_revealer), FALSE);
}

static void
_unreveal_message_delay_cb (NyxGtkBillboard *self)
{
  GST_LOG_OBJECT (self, "Unreveal message handler reached");
  self->message_timeout = 0;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->message_revealer), FALSE);
}

static void
_reset_fade_side_timeout (NyxGtkBillboard *self)
{
  GST_TRACE_OBJECT (self, "Fade side timeout reset");

  g_clear_handle_id (&self->side_timeout, g_source_remove);
  self->side_timeout = g_timeout_add_once (1500,
      (GSourceOnceFunc) _unreveal_side_delay_cb, self);
}

static void
_reset_fade_message_timeout (NyxGtkBillboard *self)
{
  const gchar *text = gtk_label_get_text (GTK_LABEL (self->message_label));

  GST_TRACE_OBJECT (self, "Fade side timeout reset");

  g_clear_handle_id (&self->message_timeout, g_source_remove);
  self->message_timeout = g_timeout_add_once (
      _estimate_read_time (text),
      (GSourceOnceFunc) _unreveal_message_delay_cb, self);
}

static void
adapt_cb (NyxGtkContainer *container, gboolean adapt,
    NyxGtkBillboard *self)
{
  GST_DEBUG_OBJECT (self, "Adapted: %s", (adapt) ? "yes" : "no");

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->progress_revealer), !adapt);
}

static void
revealer_revealed_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkBillboard *self)
{
  if (!gtk_revealer_get_child_revealed (revealer)) {
    GtkWidget *other_revealer = (GTK_WIDGET (revealer) == self->side_revealer)
        ? self->message_revealer
        : self->side_revealer;

    gtk_widget_set_visible (GTK_WIDGET (revealer), FALSE);

    /* We only hide here when nothing is posted on the board,
     * visiblity is set to TRUE when post is made */
    if (!gtk_revealer_get_child_revealed (GTK_REVEALER (other_revealer)))
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
  } else {
    if ((GTK_WIDGET (revealer) == self->side_revealer))
      _reset_fade_side_timeout (self);
    else if (!self->has_pinned)
      _reset_fade_message_timeout (self);
  }
}

static void
reveal_side (NyxGtkBillboard *self)
{
  g_clear_handle_id (&self->side_timeout, g_source_remove);

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  gtk_widget_set_visible (self->side_revealer, TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->side_revealer), TRUE);

  if (gtk_revealer_get_child_revealed (GTK_REVEALER (self->side_revealer)))
    _reset_fade_side_timeout (self);
}

static void
_post_message_internal (NyxGtkBillboard *self,
    const gchar *icon_name, const gchar *message, gboolean pin)
{
  if (self->has_pinned)
    return;

  self->has_pinned = pin;

  gtk_image_set_from_icon_name (GTK_IMAGE (self->message_image), icon_name);
  gtk_label_set_label (GTK_LABEL (self->message_label), message);

  g_clear_handle_id (&self->message_timeout, g_source_remove);

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  gtk_widget_set_visible (self->message_revealer, TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->message_revealer), TRUE);

  if (!self->has_pinned
      && gtk_revealer_get_child_revealed (GTK_REVEALER (self->message_revealer)))
    _reset_fade_message_timeout (self);
}

static void
_player_mute_changed_cb (NyxPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkBillboard *self)
{
  self->mute = nyx_player_get_mute (player);

  nyx_gtk_billboard_announce_volume (self);
}

/**
 * nyx_gtk_billboard_new:
 *
 * Creates a new #NyxGtkBillboard instance.
 *
 * Returns: a new billboard #GtkWidget.
 */
GtkWidget *
nyx_gtk_billboard_new (void)
{
  return g_object_new (NYX_GTK_TYPE_BILLBOARD, NULL);
}

/**
 * nyx_gtk_billboard_post_message:
 * @billboard: a #NyxGtkBillboard
 * @icon_name: an icon name
 * @message: a message text
 *
 * Posts a temporary message on the @billboard.
 *
 * Duration how long a message will stay is automatically
 * calculated based on amount of text.
 */
void
nyx_gtk_billboard_post_message (NyxGtkBillboard *self,
    const gchar *icon_name, const gchar *message)
{
  _post_message_internal (self, icon_name, message, FALSE);
}

/**
 * nyx_gtk_billboard_pin_message:
 * @billboard: a #NyxGtkBillboard
 * @icon_name: an icon name
 * @message: a message text
 *
 * Pins a permanent message on the @billboard.
 *
 * The message will stay on the @billboard until a
 * [method@NyxGtk.Billboard.unpin_pinned_message] is called.
 */
void
nyx_gtk_billboard_pin_message (NyxGtkBillboard *self,
    const gchar *icon_name, const gchar *message)
{
  _post_message_internal (self, icon_name, message, TRUE);
}

/**
 * nyx_gtk_billboard_unpin_pinned_message:
 * @billboard: a #NyxGtkBillboard
 *
 * Unpins previously pinned message on the @billboard.
 *
 * If no message was pinned this function will do nothing,
 * so it is safe to call when unsure.
 */
void
nyx_gtk_billboard_unpin_pinned_message (NyxGtkBillboard *self)
{
  if (!self->has_pinned)
    return;

  _unreveal_message_delay_cb (self);
  self->has_pinned = FALSE;
}

/**
 * nyx_gtk_billboard_announce_volume:
 * @billboard: a #NyxGtkBillboard
 *
 * Temporarily displays current volume level on the
 * side of @billboard.
 *
 * Use this if you want to present current volume level to the user.
 */
void
nyx_gtk_billboard_announce_volume (NyxGtkBillboard *self)
{
  gdouble volume = PERCENTAGE_ROUND (nyx_player_get_volume (self->player));
  gchar *percent_str;
  gboolean has_overamp;

  /* Revert popup_speed changes */
  gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (self->bottom_progress), TRUE);

  has_overamp = gtk_widget_has_css_class (self->progress_box, "overamp");
  percent_str = g_strdup_printf ("%.0lf%%", volume * 100);

  if (volume <= 1.0) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), 0.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), volume);

    if (has_overamp)
      gtk_widget_remove_css_class (self->progress_box, "overamp");
  } else {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), volume - 1.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), 1.0);

    if (!has_overamp)
      gtk_widget_add_css_class (self->progress_box, "overamp");
  }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->progress_image),
      nyx_gtk_get_icon_name_for_volume ((!self->mute) ? volume : 0));
  gtk_label_set_label (GTK_LABEL (self->progress_label), percent_str);

  g_free (percent_str);

  reveal_side (self);
}

/**
 * nyx_gtk_billboard_announce_speed:
 * @billboard: a #NyxGtkBillboard
 *
 * Temporarily displays current speed value on the
 * side of @billboard.
 *
 * Use this if you want to present current speed value to the user.
 */
void
nyx_gtk_billboard_announce_speed (NyxGtkBillboard *self)
{
  gdouble speed = PERCENTAGE_ROUND (nyx_player_get_speed (self->player));
  gchar *speed_str;

  /* Revert popup_volume changes */
  if (gtk_widget_has_css_class (self->progress_box, "overamp"))
    gtk_widget_remove_css_class (self->progress_box, "overamp");

  gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR (self->bottom_progress), FALSE);

  speed_str = g_strdup_printf ("%.2lfx", speed);

  if (speed <= 1.0) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), 0.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), 1.0 - speed);
  } else {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->top_progress), speed - 1.0);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->bottom_progress), 0.0);
  }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->progress_image),
      nyx_gtk_get_icon_name_for_speed (speed));
  gtk_label_set_label (GTK_LABEL (self->progress_label), speed_str);

  g_free (speed_str);

  reveal_side (self);
}

static void
nyx_gtk_billboard_root (GtkWidget *widget)
{
  NyxGtkBillboard *self = NYX_GTK_BILLBOARD_CAST (widget);

  GTK_WIDGET_CLASS (parent_class)->root (widget);

  if ((self->player = nyx_gtk_get_player_from_ancestor (widget))) {
    g_signal_connect (self->player, "notify::mute",
        G_CALLBACK (_player_mute_changed_cb), self);
    self->mute = nyx_player_get_mute (self->player);
  }
}

static void
nyx_gtk_billboard_unroot (GtkWidget *widget)
{
  NyxGtkBillboard *self = NYX_GTK_BILLBOARD_CAST (widget);

  if (self->player) {
    g_signal_handlers_disconnect_by_func (self->player, _player_mute_changed_cb, self);

    self->player = NULL;
  }

  /* Reset in case of rooted again not within video widget */
  self->mute = FALSE;

  GTK_WIDGET_CLASS (parent_class)->unroot (widget);
}

static void
nyx_gtk_billboard_init (NyxGtkBillboard *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
nyx_gtk_billboard_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), NYX_GTK_TYPE_BILLBOARD);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_gtk_billboard_finalize (GObject *object)
{
  NyxGtkBillboard *self = NYX_GTK_BILLBOARD_CAST (object);

  g_clear_handle_id (&self->side_timeout, g_source_remove);
  g_clear_handle_id (&self->message_timeout, g_source_remove);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_gtk_billboard_class_init (NyxGtkBillboardClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxgtkbillboard", 0,
      "Nyx GTK Billboard");

  gobject_class->dispose = nyx_gtk_billboard_dispose;
  gobject_class->finalize = nyx_gtk_billboard_finalize;

  /* Using root/unroot since initially invisible (unrealized)
   * and we want for signals to stay connected as long as parented */
  widget_class->root = nyx_gtk_billboard_root;
  widget_class->unroot = nyx_gtk_billboard_unroot;

  gtk_widget_class_set_template_from_resource (widget_class,
      NYX_GTK_RESOURCE_PREFIX "/ui/nyx-gtk-billboard.ui");

  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, side_revealer);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, progress_box);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, progress_revealer);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, top_progress);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, bottom_progress);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, progress_image);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, progress_label);

  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, message_revealer);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, message_image);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkBillboard, message_label);

  gtk_widget_class_bind_template_callback (widget_class, adapt_cb);
  gtk_widget_class_bind_template_callback (widget_class, revealer_revealed_cb);

  gtk_widget_class_set_css_name (widget_class, "nyx-gtk-billboard");
}
