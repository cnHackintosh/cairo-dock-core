/*
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef __CAIRO_DOCK_USER_INTERACTION__
#define  __CAIRO_DOCK_USER_INTERACTION__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


gboolean cairo_dock_notification_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, guint iButtonState);

gboolean cairo_dock_notification_middle_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock);

gboolean cairo_dock_notification_scroll_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, int iDirection);

gboolean cairo_dock_notification_drop_data (gpointer pUserData, const gchar *cReceivedData, Icon *icon, double fOrder, CairoContainer *pContainer);


void cairo_dock_set_custom_icon_on_appli (const gchar *cFilePath, Icon *icon, CairoContainer *pContainer);


G_END_DECLS
#endif
