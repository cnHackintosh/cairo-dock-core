/**
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

#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <sys/stat.h>
#define __USE_POSIX
#include <time.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#include "config.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-task.h"
#include "cairo-dock-log.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-gui-commons.h"

#define CAIRO_DOCK_PLUGINS_EXTRAS_URL "http://extras.glx-dock.org"

extern CairoDockDesktopGeometry g_desktopGeometry;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;


void cairo_dock_update_desklet_widgets (CairoDesklet *pDesklet, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "locked");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), pDesklet->bPositionLocked);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "size");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	g_signal_handlers_block_matched (pOneWidget,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), pDesklet->container.iWidth);
	g_signal_handlers_unblock_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	if (pGroupKeyWidget->pSubWidgetList->next != NULL)
	{
		pOneWidget = pGroupKeyWidget->pSubWidgetList->next->data;
		g_signal_handlers_block_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), pDesklet->container.iHeight);
		g_signal_handlers_unblock_matched (pOneWidget,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, _cairo_dock_set_value_in_pair, NULL);
	}
	
	int iRelativePositionX = (pDesklet->container.iWindowPositionX + pDesklet->container.iWidth/2 <= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]/2 ? pDesklet->container.iWindowPositionX : pDesklet->container.iWindowPositionX - g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
	int iRelativePositionY = (pDesklet->container.iWindowPositionY + pDesklet->container.iHeight/2 <= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]/2 ? pDesklet->container.iWindowPositionY : pDesklet->container.iWindowPositionY - g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "x position");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iRelativePositionX);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "y position");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (pOneWidget), iRelativePositionY);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "rotation");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_range_set_value (GTK_RANGE (pOneWidget), pDesklet->fRotation/G_PI*180.);
}

void cairo_dock_update_desklet_visibility_widgets (CairoDesklet *pDesklet, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	gboolean bIsSticky = cairo_dock_desklet_is_sticky (pDesklet);
	CairoDeskletVisibility iVisibility = pDesklet->iVisibility;
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "accessibility");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_combo_box_set_active (GTK_COMBO_BOX (pOneWidget), iVisibility);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "sticky");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bIsSticky);
	
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "locked");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), pDesklet->bPositionLocked);
}

void cairo_dock_update_is_detached_widget (gboolean bIsDetached, GSList *pWidgetList)
{
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GtkWidget *pOneWidget;
	pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (pWidgetList, "Desklet", "initially detached");
	g_return_if_fail (pGroupKeyWidget != NULL && pGroupKeyWidget->pSubWidgetList != NULL);
	pOneWidget = pGroupKeyWidget->pSubWidgetList->data;
	gtk_toggle_button_set_active  (GTK_TOGGLE_BUTTON (pOneWidget), bIsDetached);
}


gchar *cairo_dock_get_third_party_applets_link (void)
{
	return g_strdup_printf (CAIRO_DOCK_PLUGINS_EXTRAS_URL"/%d.%d.%d", g_iMajorVersion, g_iMinorVersion, g_iMicroVersion);
}
