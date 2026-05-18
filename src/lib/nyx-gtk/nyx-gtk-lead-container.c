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
 * NyxGtkLeadContainer:
 *
 * A #NyxGtkContainer that can take priority in user interactions with the #NyxGtkVideo.
 *
 * #NyxGtkLeadContainer is a special type of [class@NyxGtk.Container] that can
 * lead in interaction events. When "leading", it is assumed that user interactions
 * over it which would normally trigger actions can be blocked/ignored when set in mask
 * of actions that this widget should block.
 *
 * This kind of container is useful when creating some statically visible overlays
 * covering top of [class@NyxGtk.Video] that you want to take priority instead of
 * triggering default actions such as toggle play on click or revealing fading overlays.
 *
 * For more info how container widget works see [class@NyxGtk.Container] documentation.
 */

#include "nyx-gtk-lead-container.h"

#define DEFAULT_LEADING TRUE
#define DEFAULT_BLOCKED_ACTIONS NYX_GTK_VIDEO_ACTION_NONE

typedef struct _NyxGtkLeadContainerPrivate NyxGtkLeadContainerPrivate;

struct _NyxGtkLeadContainerPrivate
{
  gboolean leading;
  NyxGtkVideoActionMask blocked_actions;
};

#define parent_class nyx_gtk_lead_container_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (NyxGtkLeadContainer, nyx_gtk_lead_container, NYX_GTK_TYPE_CONTAINER)

enum
{
  PROP_0,
  PROP_LEADING,
  PROP_BLOCKED_ACTIONS,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/**
 * nyx_gtk_lead_container_new:
 *
 * Creates a new #NyxGtkLeadContainer instance.
 *
 * Returns: a new lead container #GtkWidget.
 */
GtkWidget *
nyx_gtk_lead_container_new (void)
{
  return g_object_new (NYX_GTK_TYPE_LEAD_CONTAINER, NULL);
}

/**
 * nyx_gtk_lead_container_set_leading:
 * @lead_container: a #NyxGtkLeadContainer
 * @leading: enable leadership
 *
 * Set if @lead_container leadership should be enabled.
 *
 * When enabled, interactions with @lead_container will not trigger
 * their default behavior, instead container and its contents will take priority.
 */
void
nyx_gtk_lead_container_set_leading (NyxGtkLeadContainer *self, gboolean leading)
{
  NyxGtkLeadContainerPrivate *priv;

  g_return_if_fail (NYX_GTK_IS_LEAD_CONTAINER (self));

  priv = nyx_gtk_lead_container_get_instance_private (self);

  priv->leading = leading;
}

/**
 * nyx_gtk_lead_container_get_leading:
 * @lead_container: a #NyxGtkLeadContainer
 *
 * Get a whenever @lead_container has leadership set.
 *
 * Returns: %TRUE if container is leading, %FALSE otherwise.
 */
gboolean
nyx_gtk_lead_container_get_leading (NyxGtkLeadContainer *self)
{
  NyxGtkLeadContainerPrivate *priv;

  g_return_val_if_fail (NYX_GTK_IS_LEAD_CONTAINER (self), FALSE);

  priv = nyx_gtk_lead_container_get_instance_private (self);

  return priv->leading;
}

/**
 * nyx_gtk_lead_container_set_blocked_actions:
 * @lead_container: a #NyxGtkLeadContainer
 * @actions: a #NyxGtkVideoActionMask of actions to block
 *
 * Set @actions that #NyxGtkVideo should skip when #GdkEvent which
 * would normally trigger them happens inside @lead_container.
 */
void
nyx_gtk_lead_container_set_blocked_actions (NyxGtkLeadContainer *self, NyxGtkVideoActionMask actions)
{
  NyxGtkLeadContainerPrivate *priv;

  g_return_if_fail (NYX_GTK_IS_LEAD_CONTAINER (self));

  priv = nyx_gtk_lead_container_get_instance_private (self);

  priv->blocked_actions = actions;
}

/**
 * nyx_gtk_lead_container_get_blocked_actions:
 * @lead_container: a #NyxGtkLeadContainer
 *
 * Get @actions that were set for this @lead_container to block.
 *
 * Returns: a mask of actions that container blocks from being triggered on video.
 */
NyxGtkVideoActionMask
nyx_gtk_lead_container_get_blocked_actions (NyxGtkLeadContainer *self)
{
  NyxGtkLeadContainerPrivate *priv;

  g_return_val_if_fail (NYX_GTK_IS_LEAD_CONTAINER (self), 0);

  priv = nyx_gtk_lead_container_get_instance_private (self);

  return priv->blocked_actions;
}

static void
nyx_gtk_lead_container_init (NyxGtkLeadContainer *self)
{
  NyxGtkLeadContainerPrivate *priv = nyx_gtk_lead_container_get_instance_private (self);

  priv->leading = DEFAULT_LEADING;
  priv->blocked_actions = DEFAULT_BLOCKED_ACTIONS;
}

static void
nyx_gtk_lead_container_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  NyxGtkLeadContainer *self = NYX_GTK_LEAD_CONTAINER_CAST (object);

  switch (prop_id) {
    case PROP_LEADING:
      g_value_set_boolean (value, nyx_gtk_lead_container_get_leading (self));
      break;
    case PROP_BLOCKED_ACTIONS:
      g_value_set_flags (value, nyx_gtk_lead_container_get_blocked_actions (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_gtk_lead_container_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  NyxGtkLeadContainer *self = NYX_GTK_LEAD_CONTAINER_CAST (object);

  switch (prop_id) {
    case PROP_LEADING:
      nyx_gtk_lead_container_set_leading (self, g_value_get_boolean (value));
      break;
    case PROP_BLOCKED_ACTIONS:
      nyx_gtk_lead_container_set_blocked_actions (self, g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
nyx_gtk_lead_container_class_init (NyxGtkLeadContainerClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  gobject_class->get_property = nyx_gtk_lead_container_get_property;
  gobject_class->set_property = nyx_gtk_lead_container_set_property;

  /**
   * NyxGtkLeadContainer:leading:
   *
   * Width that container should target.
   */
  param_specs[PROP_LEADING] = g_param_spec_boolean ("leading",
      NULL, NULL, DEFAULT_LEADING,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * NyxGtkLeadContainer:blocked-actions:
   *
   * Mask of actions that container blocks from being triggered on video.
   */
  param_specs[PROP_BLOCKED_ACTIONS] = g_param_spec_flags ("blocked-actions",
      NULL, NULL, NYX_GTK_TYPE_VIDEO_ACTION_MASK, DEFAULT_BLOCKED_ACTIONS,
      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_set_css_name (widget_class, "nyx-gtk-lead-container");
}
