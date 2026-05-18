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
 * NyxGtkVideo:
 *
 * A ready to be used GTK video widget implementing Nyx API.
 *
 * #NyxGtkVideo is the main widget exposed by `NyxGtk` API. It both displays
 * videos played by [class@Nyx.Player] (exposed as [property@NyxGtk.Av:player] property)
 * and manages revealing and fading of any additional widgets overlaid on top of it.
 *
 * Other widgets provided by `NyxGtk` library, once placed anywhere on video
 * (including nesting within another widget like [class@Gtk.Box]) will automatically
 * control #NyxGtkVideo they were overlaid on top of. This allows to freely create
 * custom playback control panels best suited for specific application. Additionally,
 * pre-made widgets such as [class@NyxGtk.SimpleControls] are also available.
 *
 * # Basic usage
 *
 * A typical use case is to embed video widget as part of your app where video playback
 * is needed. Get the [class@Nyx.Player] belonging to the AV widget and start adding
 * new [class@Nyx.MediaItem] items to the [class@Nyx.Queue] for playback.
 * For more information please refer to the Nyx playback library documentation.
 *
 * #NyxGtkVideo can automatically take care of revealing and later fading overlaid
 * content when interacting with the video. To do this, simply add your widgets with
 * [method@NyxGtk.Video.add_fading_overlay]. If you want to display some static content
 * on top of video (or take care of visibility within overlaid widget itself) you can add
 * it to the video as a normal overlay with [method@NyxGtk.Video.add_overlay].
 *
 * # Actions
 *
 * You can use built-in actions of parent [class@NyxGtk.Av].
 * See its documentation, for the list of available ones.
 *
 * # NyxGtkVideo as GtkBuildable
 *
 * #NyxGtkVideo implementation of the [iface@Gtk.Buildable] interface supports
 * placing children as either normal overlay by specifying `overlay` or a fading
 * one by specifying `fading-overlay` as the `type` attribute of a `<child>` element.
 * Position of overlaid content is determined by `valign/halign` properties.
 *
 * ```xml
 * <object class="NyxGtkVideo" id="video">
 *   <child type="fading-overlay">
 *     <object class="NyxGtkTitleHeader">
 *       <property name="valign">start</property>
 *     </object>
 *   </child>
 *   <child type="fading-overlay">
 *     <object class="NyxGtkSimpleControls">
 *       <property name="valign">end</property>
 *     </object>
 *   </child>
 * </object>
 * ```
 */

#include "config.h"

#include "nyx-gtk-enums.h"
#include "nyx-gtk-video.h"
#include "nyx-gtk-lead-container.h"
#include "nyx-gtk-status-private.h"
#include "nyx-gtk-buffering-animation-private.h"
#include "nyx-gtk-video-placeholder-private.h"

#define DEFAULT_FADE_DELAY 3000
#define DEFAULT_TOUCH_FADE_DELAY 5000

#define MIN_MOTION_DELAY 100000

#define GST_CAT_DEFAULT nyx_gtk_video_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _NyxGtkVideo
{
  NyxGtkAv parent;

  GtkWidget *overlay;
  GtkWidget *status;
  GtkWidget *buffering_animation;

  GtkGesture *touch_gesture;
  GtkGesture *click_gesture;

  /* Props */
  guint fade_delay;
  guint touch_fade_delay;

  GPtrArray *overlays;
  GPtrArray *fading_overlays;

  gboolean buffering;
  gboolean showing_status;

  gulong notify_revealed_id;
  guint fade_timeout;
  gboolean reveal, revealed;

  /* Current pointer coords and type */
  gdouble x, y;
  gboolean is_touch;
  gboolean touching;
  gint64 last_motion_time;
  gboolean pending_toggle_play;
};

static void
nyx_gtk_video_add_child (GtkBuildable *buildable,
    GtkBuilder *builder, GObject *child, const char *type)
{
  if (GTK_IS_WIDGET (child)) {
    if (g_strcmp0 (type, "overlay") == 0)
      nyx_gtk_video_add_overlay (NYX_GTK_VIDEO (buildable), GTK_WIDGET (child));
    else if (g_strcmp0 (type, "fading-overlay") == 0)
      nyx_gtk_video_add_fading_overlay (NYX_GTK_VIDEO (buildable), GTK_WIDGET (child));
    else
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
  } else {
    GtkBuildableIface *parent_iface = g_type_interface_peek_parent (GTK_BUILDABLE_GET_IFACE (buildable));
    parent_iface->add_child (buildable, builder, child, type);
  }
}

static void
_buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = nyx_gtk_video_add_child;
}

#define parent_class nyx_gtk_video_parent_class
G_DEFINE_TYPE_WITH_CODE (NyxGtkVideo, nyx_gtk_video, NYX_GTK_TYPE_AV,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, _buildable_iface_init))

enum
{
  PROP_0,
  PROP_FADE_DELAY,
  PROP_TOUCH_FADE_DELAY,
  PROP_LAST
};

enum
{
  SIGNAL_TOGGLE_FULLSCREEN,
  SIGNAL_SEEK_REQUEST,
  SIGNAL_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };
static guint signals[SIGNAL_LAST] = { 0, };

/* FIXME: 1.0: Remove these compat actions, since they were moved to base class */

static void
toggle_play_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.toggle-play", parameter);
}

static void
play_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.play", parameter);
}

static void
pause_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.pause", parameter);
}

static void
stop_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.stop", parameter);
}

static void
seek_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.seek", parameter);
}

static void
seek_custom_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.seek-custom", parameter);
}

static void
toggle_mute_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.toggle-mute", parameter);
}

static void
set_mute_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.set-mute", parameter);
}

static void
volume_up_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.volume-up", parameter);
}

static void
volume_down_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.volume-down", parameter);
}

static void
set_volume_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.set-volume", parameter);
}

static void
speed_up_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.speed-up", parameter);
}

static void
speed_down_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.speed-down", parameter);
}

static void
set_speed_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.set-speed", parameter);
}

static void
previous_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.previous-item", parameter);
}

static void
next_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.next-item", parameter);
}

static void
select_item_action_cb (GtkWidget *widget, const gchar *action_name, GVariant *parameter)
{
  gtk_widget_activate_action_variant (widget, "av.select-item", parameter);
}

static void
_set_reveal_fading_overlays (NyxGtkVideo *self, gboolean reveal)
{
  GdkCursor *cursor = gdk_cursor_new_from_name ((reveal) ? "default" : "none", NULL);
  guint i;

  self->reveal = reveal;
  GST_LOG_OBJECT (self, "%s requested", (self->reveal) ? "Reveal" : "Fade");

  gtk_widget_set_cursor (GTK_WIDGET (self), cursor);
  g_object_unref (cursor);

  for (i = 0; i < self->fading_overlays->len; ++i) {
    GtkRevealer *revealer = (GtkRevealer *) g_ptr_array_index (self->fading_overlays, i);

    if (reveal)
      gtk_widget_set_visible ((GtkWidget *) revealer, TRUE);

    gtk_revealer_set_reveal_child (revealer, reveal);
  }
}

static inline gboolean
_is_on_leading_overlay (NyxGtkVideo *self, NyxGtkVideoActionMask blocked_action)
{
  GtkWidget *video = (GtkWidget *) self;
  GtkWidget *tmp_widget = gtk_widget_pick (video, self->x, self->y, GTK_PICK_DEFAULT);
  gboolean is_leading = FALSE;

  GST_LOG_OBJECT (self, "Checking if is on leading overlay...");

  while (tmp_widget && tmp_widget != video) {
    if (NYX_GTK_IS_LEAD_CONTAINER (tmp_widget)) {
      NyxGtkLeadContainer *lead_container = NYX_GTK_LEAD_CONTAINER_CAST (tmp_widget);

      if (nyx_gtk_lead_container_get_leading (lead_container)
          && (nyx_gtk_lead_container_get_blocked_actions (lead_container) & blocked_action)) {
        is_leading = TRUE;
        break;
      }
    }
    tmp_widget = gtk_widget_get_parent (tmp_widget);
  }

  GST_LOG_OBJECT (self, "Is on leading overlay: %s", (is_leading) ? "yes" : "no");

  return is_leading;
}

static inline gboolean
_determine_can_fade (NyxGtkVideo *self)
{
  GtkWidget *video = (GtkWidget *) self;
  GtkRoot *root;
  GtkNative *native, *child_native;
  GtkWidget *focus_child;
  gboolean in_fading_overlay = FALSE;

  GST_LOG_OBJECT (self, "Checking if overlays can fade...");

  if (self->is_touch) {
    if (self->touching) {
      GST_LOG_OBJECT (self, "Cannot fade while interacting with touchscreen");
      return FALSE;
    }
  } else if (self->x > 0 && self->y > 0) {
    GtkWidget *tmp_widget = gtk_widget_pick (video, self->x, self->y, GTK_PICK_DEFAULT);
    guint i;

    if (!tmp_widget) {
      GST_LOG_OBJECT (self, "Can fade, since no widget under pointer");
      return TRUE;
    }

    for (i = 0; i < self->fading_overlays->len; ++i) {
      GtkWidget *revealer = (GtkWidget *) g_ptr_array_index (self->fading_overlays, i);

      if (tmp_widget == revealer || gtk_widget_is_ancestor (tmp_widget, revealer)) {
        in_fading_overlay = TRUE;
        break;
      }
    }

    if (!in_fading_overlay) {
      GST_LOG_OBJECT (self, "Can fade, since pointer not within fading overlay");
      return TRUE;
    }

    while (tmp_widget && tmp_widget != video) {
      GtkStateFlags state_flags = gtk_widget_get_state_flags (tmp_widget);

      if (GTK_IS_ACTIONABLE (tmp_widget)
          && (state_flags & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE))) {
        GST_LOG_OBJECT (self, "Cannot fade while on activatable widget");
        return FALSE;
      }
      if ((state_flags & GTK_STATE_FLAG_DROP_ACTIVE)) {
        GST_LOG_OBJECT (self, "Cannot fade on drop-active widget");
        return FALSE;
      }
      if (GTK_IS_ACCESSIBLE (tmp_widget) && gtk_widget_get_can_target (tmp_widget)) {
        GtkAccessibleRole role = gtk_accessible_get_accessible_role ((GtkAccessible *) tmp_widget);

        switch (role) {
          case GTK_ACCESSIBLE_ROLE_LIST:
            GST_LOG_OBJECT (self, "Cannot fade while browsing list");
            return FALSE;
          case GTK_ACCESSIBLE_ROLE_SLIDER:
          case GTK_ACCESSIBLE_ROLE_SCROLLBAR:
            GST_LOG_OBJECT (self, "Cannot fade while on slider/scrollbar");
            return FALSE;
          default:
            break;
        }
      }

      tmp_widget = gtk_widget_get_parent (tmp_widget);
    };
  }

  root = gtk_widget_get_root (video);

  if (G_UNLIKELY (root == NULL))
    return FALSE;

  focus_child = gtk_root_get_focus (root);

  if (!focus_child
      || !gtk_widget_has_focus (focus_child)
      || !gtk_widget_is_ancestor (focus_child, video)) {
    GST_LOG_OBJECT (self, "Can fade, since no focused child in video");
    return TRUE;
  }

  native = gtk_widget_get_native (video);
  child_native = gtk_widget_get_native (focus_child);

  if (native != child_native) {
    GST_LOG_OBJECT (self, "Cannot fade while another surface is open");
    return FALSE;
  }

  GST_LOG_OBJECT (self, "Can fade");
  return TRUE;
}

static void
_fade_overlay_delay_cb (NyxGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Fade handler reached");
  self->fade_timeout = 0;

  if (self->reveal) {
    gboolean can_fade = _determine_can_fade (self);

    GST_DEBUG_OBJECT (self, "Can fade overlays: %s", (can_fade) ? "yes" : "no");

    if (can_fade)
      _set_reveal_fading_overlays (self, FALSE);
  }
}

static void
_reset_fade_timeout (NyxGtkVideo *self)
{
  GST_TRACE_OBJECT (self, "Fade timeout reset");

  g_clear_handle_id (&self->fade_timeout, g_source_remove);
  self->fade_timeout = g_timeout_add_once (
      (self->is_touch) ? self->touch_fade_delay : self->fade_delay,
      (GSourceOnceFunc) _fade_overlay_delay_cb, self);
}

static void
_window_is_active_cb (GtkWindow *window,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkVideo *self)
{
  gboolean active = gtk_window_is_active (window);

  GST_DEBUG_OBJECT (self, "Window is now %sactive",
      (active) ? "" : "in");

  if (!active) {
    /* Needs to set when drag starts during touch,
     * we do not get touch release then */
    self->touching = FALSE;

    /* Ensure our overlays will fade eventually */
    if (self->revealed && !self->fade_timeout)
      _reset_fade_timeout (self);
  }
}

static void
_handle_motion (NyxGtkVideo *self, GtkEventController *controller, gdouble x, gdouble y)
{
  gint64 now;

  /* Start with points comparison as its faster,
   * otherwise we will check if threshold exceeded */
  if (self->x == x && self->y == y)
    return;

  now = g_get_monotonic_time ();

  /* We do not want to reset timeout too often
   * (especially on high refresh rate screens). */
  if (now - self->last_motion_time >= MIN_MOTION_DELAY) {
    GdkDevice *device = gtk_event_controller_get_current_event_device (controller);
    gboolean is_threshold = (ABS (self->x - x) > 1 || ABS (self->y - y) > 1);

    self->x = x;
    self->y = y;
    self->is_touch = (device && gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);

    if (is_threshold) {
      if (!self->reveal && !_is_on_leading_overlay (self, NYX_GTK_VIDEO_ACTION_REVEAL_OVERLAYS))
        _set_reveal_fading_overlays (self, TRUE);

      /* Extend time until fade */
      if (self->revealed)
        _reset_fade_timeout (self);
    }

    self->last_motion_time = now;
  }
}

static void
_handle_motion_leave (NyxGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Motion leave");

  /* On leave we only reset coords to let overlays fade,
   * device is not expected to change here */
  self->x = -1;
  self->y = -1;

  /* Ensure our overlays will fade eventually */
  if (self->revealed && !self->fade_timeout)
    _reset_fade_timeout (self);
}

static void
motion_enter_cb (GtkEventControllerMotion *motion,
    gdouble x, gdouble y, NyxGtkVideo *self)
{
  GdkDevice *device = gtk_event_controller_get_current_event_device (GTK_EVENT_CONTROLLER (motion));

  /* XXX: We do not update x/y coords here in order to not mislead us
   * that we are not on non-fading overlay when another surface is open */

  self->is_touch = (device && gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);

  /* Tap to reveal is handled elsewhere */
  if (self->is_touch)
    return;

  if (!self->reveal && !_is_on_leading_overlay (self, NYX_GTK_VIDEO_ACTION_REVEAL_OVERLAYS))
    _set_reveal_fading_overlays (self, TRUE);

  /* Extend time until fade */
  if (self->revealed)
    _reset_fade_timeout (self);
}

static void
motion_cb (GtkEventControllerMotion *motion,
    gdouble x, gdouble y, NyxGtkVideo *self)
{
  _handle_motion (self, GTK_EVENT_CONTROLLER (motion), x, y);
}

static void
motion_leave_cb (GtkEventControllerMotion *motion, NyxGtkVideo *self)
{
  _handle_motion_leave (self);
}

static void
drop_motion_cb (GtkDropControllerMotion *drop_motion,
    gdouble x, gdouble y, NyxGtkVideo *self)
{
  /* We do not actually support D&D here, just want to track
   * drop motion events from it and reveal overlays as one
   * or more widgets overlaid may support current drop */

  _handle_motion (self, GTK_EVENT_CONTROLLER (drop_motion), x, y);
}

static void
drop_motion_leave_cb (GtkDropControllerMotion *drop_motion, NyxGtkVideo *self)
{
  _handle_motion_leave (self);
}

static void
left_click_pressed_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, NyxGtkVideo *self)
{
  GdkDevice *device;

  GST_LOG_OBJECT (self, "Left click pressed");

  /* Need to always clear click timeout,
   * so we will not pause after double click */
  self->pending_toggle_play = FALSE;

  device = gtk_gesture_get_device (GTK_GESTURE (click));

  self->x = x;
  self->y = y;
  self->is_touch = (device && gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);
}

static gboolean
_touch_in_lr_area (NyxGtkVideo *self, gboolean *forward)
{
  gint video_w = gtk_widget_get_width (GTK_WIDGET (self));
  gdouble area_w = (video_w / 4.);
  gboolean in_area;

  if ((in_area = (self->x <= area_w))) {
    if (forward)
      *forward = FALSE;
  } else if ((in_area = (self->x >= video_w - area_w))) {
    if (forward)
      *forward = TRUE;
  }

  if (in_area && forward)
    *forward ^= (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

  GST_LOG_OBJECT (self, "Touch in area: %s (x: %.2lf, video_w: %i, area_w: %.0lf)",
      (in_area) ? "yes" : "no", self->x, video_w, area_w);

  return in_area;
}

static inline void
_handle_single_click (NyxGtkVideo *self, GtkGestureClick *click)
{
  GdkDevice *device = gtk_gesture_get_device (GTK_GESTURE (click));

  /* FIXME: Try GstNavigation first and do below logic only when not handled
   * by upstream elements (maybe use sequence claiming for that?) */

  switch (gdk_device_get_source (device)) {
    case GDK_SOURCE_TOUCHSCREEN:
      /* First tap should only reveal overlays if fading/faded */
      if (!self->reveal && !_is_on_leading_overlay (self, NYX_GTK_VIDEO_ACTION_REVEAL_OVERLAYS)) {
        _set_reveal_fading_overlays (self, TRUE);
        gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
        break;
      }
      G_GNUC_FALLTHROUGH;
    default:
      if (!_is_on_leading_overlay (self, NYX_GTK_VIDEO_ACTION_TOGGLE_PLAY)) {
        self->pending_toggle_play = TRUE;
        gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
      }
      break;
  }
}

static inline void
_handle_double_click (NyxGtkVideo *self, GtkGestureClick *click)
{
  gboolean handled = FALSE;

  if (self->is_touch) {
    gboolean forward = FALSE;

    if (_touch_in_lr_area (self, &forward)
        && !_is_on_leading_overlay (self, NYX_GTK_VIDEO_ACTION_SEEK_REQUEST)
        && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
           signals[SIGNAL_SEEK_REQUEST], 0, NULL, NULL, NULL) != 0) {
      g_signal_emit (self, signals[SIGNAL_SEEK_REQUEST], 0, forward);
      handled = TRUE;
    }
  }

  if (!handled) {
    if ((handled = !_is_on_leading_overlay (self, NYX_GTK_VIDEO_ACTION_TOGGLE_FULLSCREEN)))
      g_signal_emit (self, signals[SIGNAL_TOGGLE_FULLSCREEN], 0);
  }

  if (handled)
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static inline void
_handle_nth_click (NyxGtkVideo *self, GtkGestureClick *click)
{
  gboolean forward = FALSE;

  if (_touch_in_lr_area (self, &forward)
      && !_is_on_leading_overlay (self, NYX_GTK_VIDEO_ACTION_SEEK_REQUEST)) {
    g_signal_emit (self, signals[SIGNAL_SEEK_REQUEST], 0, forward);
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
  }
}

static void
left_click_released_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, NyxGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Left click released");

  if (self->x < 0 || self->y < 0) {
    GST_LOG_OBJECT (self, "Ignoring click release outside of video");
    return;
  }

  self->x = x;
  self->y = y;

  switch (n_press) {
    case 1:
      _handle_single_click (self, click);
      break;
    case 2:
      _handle_double_click (self, click);
      break;
    default:
      _handle_nth_click (self, click);
      break;
  }

  /* Keep fading overlays revealed while clicking/tapping on video */
  if (self->revealed)
    _reset_fade_timeout (self);
}

static void
left_click_stopped_cb (GtkGestureClick *click, NyxGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Left click stopped");

  if (self->pending_toggle_play) {
    toggle_play_action_cb (GTK_WIDGET (self), NULL, NULL);
    self->pending_toggle_play = FALSE;
  }
}

static void
touch_pressed_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, NyxGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Touch pressed");

  self->is_touch = TRUE;
  self->touching = TRUE;

  if (self->revealed)
    _reset_fade_timeout (self);
}

static void
touch_released_cb (GtkGestureClick *click, gint n_press,
    gdouble x, gdouble y, NyxGtkVideo *self)
{
  GST_LOG_OBJECT (self, "Touch released");

  self->touching = FALSE;

  /* Ensure our overlays will fade eventually */
  if (self->revealed)
    _reset_fade_timeout (self);
}

static inline void
_set_buffering_animation_enabled (NyxGtkVideo *self, gboolean enabled)
{
  NyxGtkBufferingAnimation *animation;

  if (self->buffering == enabled)
    return;

  animation = NYX_GTK_BUFFERING_ANIMATION_CAST (self->buffering_animation);
  gtk_widget_set_visible (self->buffering_animation, enabled);

  if (enabled)
    nyx_gtk_buffering_animation_start (animation);
  else
    nyx_gtk_buffering_animation_stop (animation);

  self->buffering = enabled;
}

static void
_player_state_changed_cb (NyxPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkVideo *self)
{
  NyxPlayerState state = nyx_player_get_state (player);

  _set_buffering_animation_enabled (self, state == NYX_PLAYER_STATE_BUFFERING);
}

static GtkWidget *
_get_widget_from_video_sink (GstElement *vsink)
{
  GtkWidget *widget = NULL;
  GParamSpec *pspec;

  if ((pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (vsink), "widget"))
      && g_type_is_a (pspec->value_type, GTK_TYPE_WIDGET)) {
    GST_DEBUG ("Video sink provides a widget");
    g_object_get (vsink, "widget", &widget, NULL);
  } else if ((pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (vsink), "paintable"))
      && g_type_is_a (pspec->value_type, G_TYPE_OBJECT)) {
    GObject *paintable = NULL;

    GST_DEBUG ("Video sink provides a paintable");
    g_object_get (vsink, "paintable", &paintable, NULL);

    if (G_LIKELY (paintable != NULL)) {
      if (GDK_IS_PAINTABLE (paintable)) {
        widget = g_object_ref_sink (gtk_picture_new ());
        gtk_picture_set_paintable (GTK_PICTURE (widget), GDK_PAINTABLE (paintable));
      }
      g_object_unref (paintable);
    }
  }

  return widget;
}

static void
_video_sink_changed_cb (NyxPlayer *player,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkVideo *self)
{
  GstElement *vsink = nyx_player_get_video_sink (player);
  GtkWidget *widget = NULL;

  GST_DEBUG_OBJECT (self, "Video sink changed to: %" GST_PTR_FORMAT, vsink);

  if (vsink) {
    widget = _get_widget_from_video_sink (vsink);

    if (!widget && GST_IS_BIN (vsink)) {
      GstIterator *iter;
      GValue value = G_VALUE_INIT;

      iter = gst_bin_iterate_recurse (GST_BIN_CAST (vsink));

      while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
        GstElement *element = g_value_get_object (&value);

        if (GST_OBJECT_FLAG_IS_SET (element, GST_ELEMENT_FLAG_SINK))
          widget = _get_widget_from_video_sink (element);

        g_value_unset (&value);

        if (widget)
          break;
      }

      gst_iterator_free (iter);
    }

    gst_object_unref (vsink);
  }

  if (!widget) {
    GST_DEBUG_OBJECT (self, "No widget from video sink, using placeholder");
    widget = g_object_ref_sink (nyx_gtk_video_placeholder_new ());
  }

  gtk_overlay_set_child (GTK_OVERLAY (self->overlay), widget);
  g_object_unref (widget);

  GST_DEBUG_OBJECT (self, "Set new video widget");
}

static void
_player_error_cb (NyxPlayer *player, GError *error,
    const gchar *debug_info, NyxGtkVideo *self)
{
  /* FIXME: Handle authentication error (pop dialog to set credentials and retry) */

  /* Buffering will not finish anymore if we were in middle of it */
  _set_buffering_animation_enabled (self, FALSE);

  if (!self->showing_status) {
    nyx_gtk_status_set_error (NYX_GTK_STATUS_CAST (self->status), error);
    self->showing_status = TRUE;
  }
}

static void
_player_missing_plugin_cb (NyxPlayer *player, const gchar *name,
    const gchar *installer_detail, NyxGtkVideo *self)
{
  /* Some media files have custom/proprietary metadata,
   * it should be safe to simply ignore these */
  if (strstr (name, "meta/") != NULL)
    return;

  /* XXX: Playbin2 seems to not emit state change here,
   * so manually stop buffering animation just in case */
  _set_buffering_animation_enabled (self, FALSE);

  /* XXX: Some content can still be played partially (e.g. without audio),
   * but it should be better to stop and notify user that something is missing */
  nyx_player_stop (player);

  /* We might get "missing-plugin" followed by "error" signal. This boolean prevents
   * immediately overwriting status and lets user deal with problems in order. */
  if (!self->showing_status) {
    nyx_gtk_status_set_missing_plugin (NYX_GTK_STATUS_CAST (self->status), name);
    self->showing_status = TRUE;
  }
}

static void
_queue_current_item_changed_cb (NyxQueue *queue,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkVideo *self)
{
  nyx_gtk_status_clear (NYX_GTK_STATUS_CAST (self->status));
  self->showing_status = FALSE;
}

static void
_fading_overlay_revealed_cb (GtkRevealer *revealer,
    GParamSpec *pspec G_GNUC_UNUSED, NyxGtkVideo *self)
{
  self->revealed = gtk_revealer_get_child_revealed (revealer);

  /* Start fade timeout once fully revealed */
  if (self->revealed)
    _reset_fade_timeout (self);
}

/**
 * nyx_gtk_video_new:
 *
 * Creates a new #NyxGtkVideo instance.
 *
 * Newly created video widget will also have set some default GStreamer elements
 * on its [class@Nyx.Player]. This includes Nyx own video sink and
 * a "scaletempo" element as audio filter. Both can still be changed after
 * construction by setting corresponding player properties.
 *
 * Returns: a new video #GtkWidget.
 */
GtkWidget *
nyx_gtk_video_new (void)
{
  return g_object_new (NYX_GTK_TYPE_VIDEO, NULL);
}

/**
 * nyx_gtk_video_add_overlay:
 * @video: a #NyxGtkVideo
 * @widget: a #GtkWidget
 *
 * Add another #GtkWidget to be overlaid on top of video.
 *
 * The position at which @widget is placed is determined from
 * [property@Gtk.Widget:halign] and [property@Gtk.Widget:valign] properties.
 *
 * This function will overlay @widget as-is meaning that widget is responsible
 * for managing its own visablity if needed. If you want to add a #GtkWidget
 * that will reveal and fade itself automatically when interacting with @video
 * (e.g. controls panel) you can use nyx_gtk_video_add_fading_overlay()
 * function for convenience.
 */
void
nyx_gtk_video_add_overlay (NyxGtkVideo *self, GtkWidget *widget)
{
  g_return_if_fail (NYX_GTK_IS_VIDEO (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_ptr_array_add (self->overlays, widget);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), widget);
}

/**
 * nyx_gtk_video_add_fading_overlay:
 * @video: a #NyxGtkVideo
 * @widget: a #GtkWidget
 *
 * Similiar as nyx_gtk_video_add_overlay() but will also automatically
 * add fading functionality to overlaid #GtkWidget for convenience. This will
 * make widget reveal itself when interacting with @video and fade otherwise.
 * Useful when placing widgets such as playback controls panels.
 */
void
nyx_gtk_video_add_fading_overlay (NyxGtkVideo *self, GtkWidget *widget)
{
  GtkWidget *revealer;

  g_return_if_fail (NYX_GTK_IS_VIDEO (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  revealer = gtk_revealer_new ();

  g_object_bind_property (revealer, "child-revealed", revealer, "visible", G_BINDING_DEFAULT);

  g_object_bind_property (widget, "halign", revealer, "halign", G_BINDING_SYNC_CREATE);
  g_object_bind_property (widget, "valign", revealer, "valign", G_BINDING_SYNC_CREATE);

  /* Since we reveal/fade all at once, one signal connection is enough */
  if (self->notify_revealed_id == 0) {
    self->notify_revealed_id = g_signal_connect (revealer, "notify::child-revealed",
        G_CALLBACK (_fading_overlay_revealed_cb), self);
  }

  gtk_widget_set_visible (revealer, self->reveal);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), self->reveal);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 800);
  gtk_revealer_set_child (GTK_REVEALER (revealer), widget);

  g_ptr_array_add (self->fading_overlays, revealer);
  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), revealer);
}

/**
 * nyx_gtk_video_get_player: (skip)
 * @video: a #NyxGtkVideo
 *
 * Get #NyxPlayer used by this #NyxGtkVideo instance.
 *
 * Returns: (transfer none): a #NyxPlayer used by video.
 *
 * Deprecated: 0.10: Use [method@NyxGtk.Av.get_player] instead.
 */
NyxPlayer *
nyx_gtk_video_get_player (NyxGtkVideo *self)
{
  g_return_val_if_fail (NYX_GTK_IS_VIDEO (self), NULL);

  return nyx_gtk_av_get_player (NYX_GTK_AV_CAST (self));
}

/**
 * nyx_gtk_video_set_fade_delay:
 * @video: a #NyxGtkVideo
 * @delay: a fade delay
 *
 * Set time in milliseconds after which fading overlays should fade.
 */
void
nyx_gtk_video_set_fade_delay (NyxGtkVideo *self, guint delay)
{
  g_return_if_fail (NYX_GTK_IS_VIDEO (self));
  g_return_if_fail (delay >= 1000);

  self->fade_delay = delay;
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_FADE_DELAY]);
}

/**
 * nyx_gtk_video_get_fade_delay:
 * @video: a #NyxGtkVideo
 *
 * Get time in milliseconds after which fading overlays should fade.
 *
 * Returns: currently set fade delay.
 */
guint
nyx_gtk_video_get_fade_delay (NyxGtkVideo *self)
{
  g_return_val_if_fail (NYX_GTK_IS_VIDEO (self), 0);

  return self->fade_delay;
}

/**
 * nyx_gtk_video_set_touch_fade_delay:
 * @video: a #NyxGtkVideo
 * @delay: a touch fade delay
 *
 * Set time in milliseconds after which fading overlays should fade
 * when using touchscreen.
 *
 * It is often useful to set this higher then normal fade delay property,
 * as in case of touch events user do not have a moving pointer that would
 * extend fade timeout, so he can have more time to decide what to press next.
 */
void
nyx_gtk_video_set_touch_fade_delay (NyxGtkVideo *self, guint delay)
{
  g_return_if_fail (NYX_GTK_IS_VIDEO (self));
  g_return_if_fail (delay >= 1);

  self->touch_fade_delay = delay;
  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_TOUCH_FADE_DELAY]);
}

/**
 * nyx_gtk_video_get_touch_fade_delay:
 * @video: a #NyxGtkVideo
 *
 * Get time in milliseconds after which fading overlays should fade
 * when revealed using touch device.
 *
 * Returns: currently set touch fade delay.
 */
guint
nyx_gtk_video_get_touch_fade_delay (NyxGtkVideo *self)
{
  g_return_val_if_fail (NYX_GTK_IS_VIDEO (self), 0);

  return self->touch_fade_delay;
}

/**
 * nyx_gtk_video_set_auto_inhibit: (skip)
 * @video: a #NyxGtkVideo
 * @inhibit: whether to enable automatic session inhibit
 *
 * Set whether video should try to automatically inhibit session
 * from idling (and possibly screen going black) when video is playing.
 *
 * Deprecated: 0.10: Use [method@NyxGtk.Av.set_auto_inhibit] instead.
 */
void
nyx_gtk_video_set_auto_inhibit (NyxGtkVideo *self, gboolean inhibit)
{
  g_return_if_fail (NYX_GTK_IS_VIDEO (self));

  nyx_gtk_av_set_auto_inhibit (NYX_GTK_AV_CAST (self), inhibit);
}

/**
 * nyx_gtk_video_get_auto_inhibit: (skip)
 * @video: a #NyxGtkVideo
 *
 * Get whether automatic session inhibit is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise.
 *
 * Deprecated: 0.10: Use [method@NyxGtk.Av.get_auto_inhibit] instead.
 */
gboolean
nyx_gtk_video_get_auto_inhibit (NyxGtkVideo *self)
{
  g_return_val_if_fail (NYX_GTK_IS_VIDEO (self), FALSE);

  return nyx_gtk_av_get_auto_inhibit (NYX_GTK_AV_CAST (self));
}

/**
 * nyx_gtk_video_get_inhibited: (skip)
 * @video: a #NyxGtkVideo
 *
 * Get whether session is currently inhibited by
 * [property@NyxGtk.Av:auto-inhibit].
 *
 * Returns: %TRUE if inhibited, %FALSE otherwise.
 *
 * Deprecated: 0.10: Use [method@NyxGtk.Av.get_inhibited] instead.
 */
gboolean
nyx_gtk_video_get_inhibited (NyxGtkVideo *self)
{
  g_return_val_if_fail (NYX_GTK_IS_VIDEO (self), FALSE);

  return nyx_gtk_av_get_inhibited (NYX_GTK_AV_CAST (self));
}

static void
nyx_gtk_video_root (GtkWidget *widget)
{
  NyxGtkVideo *self = NYX_GTK_VIDEO_CAST (widget);
  GtkRoot *root;

  GTK_WIDGET_CLASS (parent_class)->root (widget);

  root = gtk_widget_get_root (widget);

  if (root && GTK_IS_WINDOW (root)) {
    GtkWindow *window = GTK_WINDOW (root);

    g_signal_connect (window, "notify::is-active",
        G_CALLBACK (_window_is_active_cb), self);
    _window_is_active_cb (window, NULL, self);
  }
}

static void
nyx_gtk_video_unroot (GtkWidget *widget)
{
  NyxGtkVideo *self = NYX_GTK_VIDEO_CAST (widget);
  GtkRoot *root = gtk_widget_get_root (widget);

  if (root && GTK_IS_WINDOW (root)) {
    g_signal_handlers_disconnect_by_func (GTK_WINDOW (root),
        _window_is_active_cb, self);
  }

  GTK_WIDGET_CLASS (parent_class)->unroot (widget);
}

static void
nyx_gtk_video_init (NyxGtkVideo *self)
{
  self->overlay = gtk_overlay_new ();
  gtk_widget_set_overflow (self->overlay, GTK_OVERFLOW_HIDDEN);
  gtk_widget_set_parent (self->overlay, GTK_WIDGET (self));

  self->overlays = g_ptr_array_new ();
  self->fading_overlays = g_ptr_array_new ();

  self->fade_delay = DEFAULT_FADE_DELAY;
  self->touch_fade_delay = DEFAULT_TOUCH_FADE_DELAY;

  /* Ensure private types */
  g_type_ensure (NYX_GTK_TYPE_STATUS);
  g_type_ensure (NYX_GTK_TYPE_BUFFERING_ANIMATION);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_gesture_group (self->touch_gesture, self->click_gesture);
}

static void
nyx_gtk_video_constructed (GObject *object)
{
  NyxGtkVideo *self = NYX_GTK_VIDEO_CAST (object);
  GstElement *vsink;
  NyxPlayer *player;
  NyxQueue *queue;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  player = nyx_gtk_av_get_player (NYX_GTK_AV_CAST (self));
  queue = nyx_player_get_queue (player);

  g_signal_connect (player, "notify::state",
      G_CALLBACK (_player_state_changed_cb), self);
  g_signal_connect (player, "notify::video-sink",
      G_CALLBACK (_video_sink_changed_cb), self);

  vsink = gst_element_factory_make ("nyxsink", NULL);

  /* FIXME: This is a temporary workaround for lack
   * of DMA_DRM negotiation support in sink itself */
  if (G_LIKELY (vsink != NULL)) {
    guint major = 0, minor = 0, micro = 0, nano = 0;

    gst_version (&major, &minor, &micro, &nano);
    if (major == 1 && minor >= 24) {
      GstElement *bin;

      if ((bin = gst_element_factory_make ("glsinkbin", NULL))) {
        g_object_set (bin, "sink", vsink, NULL);
        vsink = bin;
      }
    }

    nyx_player_set_video_sink (player, vsink);
  }

  g_signal_connect (player, "error",
      G_CALLBACK (_player_error_cb), self);
  g_signal_connect (player, "missing-plugin",
      G_CALLBACK (_player_missing_plugin_cb), self);

  g_signal_connect (queue, "notify::current-item",
      G_CALLBACK (_queue_current_item_changed_cb), self);
}

static void
nyx_gtk_video_dispose (GObject *object)
{
  NyxGtkVideo *self = NYX_GTK_VIDEO_CAST (object);
  NyxPlayer *player;

  if (self->notify_revealed_id != 0) {
    GtkRevealer *revealer = GTK_REVEALER (g_ptr_array_index (self->fading_overlays, 0));

    g_signal_handler_disconnect (revealer, self->notify_revealed_id);
    self->notify_revealed_id = 0;
  }

  g_clear_handle_id (&self->fade_timeout, g_source_remove);

  player = nyx_gtk_av_get_player (NYX_GTK_AV_CAST (self));

  /* Something else might still be holding a reference on the player,
   * thus we should disconnect everything before disposing template */
  if (player) { // NULL if dispose run multiple times
    NyxQueue *queue = nyx_player_get_queue (player);

    g_signal_handlers_disconnect_by_func (player,
        _player_state_changed_cb, self);
    g_signal_handlers_disconnect_by_func (player,
        _video_sink_changed_cb, self);
    g_signal_handlers_disconnect_by_func (player,
        _player_error_cb, self);
    g_signal_handlers_disconnect_by_func (player,
        _player_missing_plugin_cb, self);

    g_signal_handlers_disconnect_by_func (queue,
        _queue_current_item_changed_cb, self);
  }

  gtk_widget_dispose_template (GTK_WIDGET (object), NYX_GTK_TYPE_VIDEO);

  g_clear_pointer (&self->overlay, gtk_widget_unparent);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nyx_gtk_video_finalize (GObject *object)
{
  NyxGtkVideo *self = NYX_GTK_VIDEO_CAST (object);

  g_ptr_array_unref (self->overlays);
  g_ptr_array_unref (self->fading_overlays);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nyx_gtk_video_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxGtkVideo *self = NYX_GTK_VIDEO_CAST (object);

  switch (prop_id) {
    case PROP_FADE_DELAY:
      g_value_set_uint (value, nyx_gtk_video_get_fade_delay (self));
      break;
    case PROP_TOUCH_FADE_DELAY:
      g_value_set_uint (value, nyx_gtk_video_get_touch_fade_delay (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_gtk_video_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxGtkVideo *self = NYX_GTK_VIDEO_CAST (object);

  switch (prop_id) {
    case PROP_FADE_DELAY:
      nyx_gtk_video_set_fade_delay (self, g_value_get_uint (value));
      break;
    case PROP_TOUCH_FADE_DELAY:
      nyx_gtk_video_set_touch_fade_delay (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_gtk_video_class_init (NyxGtkVideoClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "nyxgtkvideo", GST_DEBUG_FG_MAGENTA,
      "Nyx GTK Video");

  widget_class->root = nyx_gtk_video_root;
  widget_class->unroot = nyx_gtk_video_unroot;

  gobject_class->constructed = nyx_gtk_video_constructed;
  gobject_class->get_property = nyx_gtk_video_get_property;
  gobject_class->set_property = nyx_gtk_video_set_property;
  gobject_class->dispose = nyx_gtk_video_dispose;
  gobject_class->finalize = nyx_gtk_video_finalize;

  /**
   * NyxGtkVideo:fade-delay:
   *
   * A delay in milliseconds before trying to fade all fading overlays.
   */
  param_specs[PROP_FADE_DELAY] = g_param_spec_uint ("fade-delay",
      NULL, NULL, 1, G_MAXUINT, DEFAULT_FADE_DELAY,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxGtkVideo:touch-fade-delay:
   *
   * A delay in milliseconds before trying to fade all fading overlays
   * after revealed using touchscreen.
   */
  param_specs[PROP_TOUCH_FADE_DELAY] = g_param_spec_uint ("touch-fade-delay",
      NULL, NULL, 1, G_MAXUINT, DEFAULT_TOUCH_FADE_DELAY,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxGtkVideo::toggle-fullscreen:
   * @video: a #NyxGtkVideo
   *
   * A signal that user requested a change in fullscreen state of the video.
   *
   * Note that when going fullscreen from this signal, user will expect
   * for only video to be fullscreened and not the whole app window.
   * It is up to implementation to decide how to handle that.
   */
  signals[SIGNAL_TOGGLE_FULLSCREEN] = g_signal_new ("toggle-fullscreen",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * NyxGtkVideo::seek-request:
   * @video: a #NyxGtkVideo
   * @forward: %TRUE if seek should be forward, %FALSE if backward
   *
   * A helper signal for implementing common seeking by double tap
   * on screen side for touchscreen devices.
   *
   * Note that @forward already takes into account RTL direction,
   * so implementation does not have to check.
   */
  signals[SIGNAL_SEEK_REQUEST] = g_signal_new ("seek-request",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  /* FIXME: 1.0: Remove these actions, since they were moved to
   * base class AV widget, but are here for compat reasons. */
  gtk_widget_class_install_action (widget_class, "video.toggle-play", NULL, toggle_play_action_cb);
  gtk_widget_class_install_action (widget_class, "video.play", NULL, play_action_cb);
  gtk_widget_class_install_action (widget_class, "video.pause", NULL, pause_action_cb);
  gtk_widget_class_install_action (widget_class, "video.stop", NULL, stop_action_cb);
  gtk_widget_class_install_action (widget_class, "video.seek", "d", seek_action_cb);
  gtk_widget_class_install_action (widget_class, "video.seek-custom", "(di)", seek_custom_action_cb);
  gtk_widget_class_install_action (widget_class, "video.toggle-mute", NULL, toggle_mute_action_cb);
  gtk_widget_class_install_action (widget_class, "video.set-mute", "b", set_mute_action_cb);
  gtk_widget_class_install_action (widget_class, "video.volume-up", NULL, volume_up_action_cb);
  gtk_widget_class_install_action (widget_class, "video.volume-down", NULL, volume_down_action_cb);
  gtk_widget_class_install_action (widget_class, "video.set-volume", "d", set_volume_action_cb);
  gtk_widget_class_install_action (widget_class, "video.speed-up", NULL, speed_up_action_cb);
  gtk_widget_class_install_action (widget_class, "video.speed-down", NULL, speed_down_action_cb);
  gtk_widget_class_install_action (widget_class, "video.set-speed", "d", set_speed_action_cb);
  gtk_widget_class_install_action (widget_class, "video.previous-item", NULL, previous_item_action_cb);
  gtk_widget_class_install_action (widget_class, "video.next-item", NULL, next_item_action_cb);
  gtk_widget_class_install_action (widget_class, "video.select-item", "u", select_item_action_cb);

  gtk_widget_class_set_template_from_resource (widget_class,
      NYX_GTK_RESOURCE_PREFIX "/ui/nyx-gtk-video.ui");

  gtk_widget_class_bind_template_child (widget_class, NyxGtkVideo, status);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkVideo, buffering_animation);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkVideo, touch_gesture);
  gtk_widget_class_bind_template_child (widget_class, NyxGtkVideo, click_gesture);

  gtk_widget_class_bind_template_callback (widget_class, left_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, left_click_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, left_click_stopped_cb);
  gtk_widget_class_bind_template_callback (widget_class, touch_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, touch_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, motion_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, drop_motion_leave_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
  gtk_widget_class_set_css_name (widget_class, "nyx-gtk-video");
}
