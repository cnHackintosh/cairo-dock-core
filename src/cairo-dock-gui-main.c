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
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "config.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-gui-commons.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-widget-items.h"
#include "cairo-dock-widget-themes.h"
#include "cairo-dock-widget-config-group.h"
#include "cairo-dock-widget-module.h"
#include "cairo-dock-widget-shortkeys.h"
#include "cairo-dock-widget.h"
#include "cairo-dock-gui-main.h"

#define CAIRO_DOCK_GROUP_ICON_SIZE 28  // 32  // size of the icon in the buttons
#define CAIRO_DOCK_CATEGORY_ICON_SIZE 28  // 32  // size of the category icons in the left vertical toolbar.
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW 4
#define CAIRO_DOCK_NB_BUTTONS_BY_ROW_MIN 3
#define CAIRO_DOCK_TABLE_MARGIN 12  // vertical space between 2 category frames.
#define CAIRO_DOCK_CONF_PANEL_WIDTH 1250
#define CAIRO_DOCK_CONF_PANEL_WIDTH_MIN 800
#define CAIRO_DOCK_CONF_PANEL_HEIGHT 700
#define CAIRO_DOCK_PREVIEW_WIDTH 200
#define CAIRO_DOCK_PREVIEW_WIDTH_MIN 100
#define CAIRO_DOCK_PREVIEW_HEIGHT 250
#define CAIRO_DOCK_ICON_MARGIN 6

extern CairoDockDesktopGeometry g_desktopGeometry;

extern gchar *g_cConfFile;
extern CairoContainer *g_pPrimaryContainer;
extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cCairoDockDataDir;
extern gboolean g_bEasterEggs;

typedef struct _CairoDockGroupDescription CairoDockGroupDescription;
typedef struct _CairoDockCategoryWidgetTable CairoDockCategoryWidgetTable;

struct _CairoDockCategoryWidgetTable {
	GtkWidget *pFrame;
	GtkWidget *pTable;
	gint iNbRows;
	gint iNbItemsInCurrentRow;
	GtkToolItem *pCategoryButton;
	} ;

struct _CairoDockGroupDescription {
	gchar *cGroupName;
	const gchar *cTitle;
	gint iCategory;
	gchar *cDescription;
	gchar *cPreviewFilePath;
	GtkWidget *pActivateButton;
	GtkWidget *pLabel;
	GtkWidget *pGroupHBox;  // box containing the check-button and the button, and placed in its category frame.
	gchar *cIcon;
	const gchar *cGettextDomain;
	CDWidget* (*build_widget) (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *pInstance);
	GList *pManagers;
	} ;

static CairoDockCategoryWidgetTable s_pCategoryWidgetTables[CAIRO_DOCK_NB_CATEGORY+1];
GSList *s_pCurrentWidgetList;  // liste des widgets du module courant.
GSList *s_pExtraCurrentWidgetList;  // liste des widgets des eventuels modules lies.
static GList *s_pGroupDescriptionList = NULL;
static GtkWidget *s_pPreviewImage = NULL;
static GtkWidget *s_pOkButton = NULL;
static GtkWidget *s_pApplyButton = NULL;
static GtkWidget *s_pBackButton = NULL;
static GtkWidget *s_pMainWindow = NULL;
static GtkWidget *s_pGroupsVBox = NULL;
static CairoDockGroupDescription *s_pCurrentGroup = NULL;
static GtkWidget *s_pCurrentGroupWidget = NULL;
static GtkWidget *s_pToolBar = NULL;
static GtkWidget *s_pGroupFrame = NULL;
static GtkWidget *s_pFilterEntry = NULL;
static GtkWidget *s_pActivateButton = NULL;
static GtkWidget *s_pStatusBar = NULL;
static GSList *s_path = NULL;
static int s_iPreviewWidth, s_iNbButtonsByRow;
static CairoDialog *s_pDialog = NULL;
static int s_iSidShowGroupDialog = 0;

static CDWidget *s_pCurrentGroupWidget2 = NULL;

static const gchar *s_cCategoriesDescription[2*(CAIRO_DOCK_NB_CATEGORY+1)] = {
	N_("Behaviour"), "icon-behavior.svg",
	N_("Appearance"), "icon-appearance.svg",
	N_("Files"), "icon-files.svg",
	N_("Internet"), "icon-internet.svg",
	N_("Desktop"), "icon-desktop.svg",
	N_("Accessories"), "icon-accessories.svg",
	N_("System"), "icon-system.svg",
	N_("Fun"), "icon-fun.svg",
	N_("All"), "icon-all.svg" };

static void cairo_dock_enable_apply_button (GtkWidget *pMainWindow, gboolean bEnable);
static void _present_group_widget (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *pModuleInstance);
static void cairo_dock_hide_all_categories (void);
static void cairo_dock_show_all_categories (void);
static void cairo_dock_show_one_category (int iCategory);
static void cairo_dock_toggle_category_button (int iCategory);
static void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription);
static CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName);
static void cairo_dock_apply_current_filter (const gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther);

  ////////////
 // FILTER //
////////////

static GString *sBuffer = NULL;
static inline void _copy_string_to_buffer (const gchar *cSentence)
{
	g_string_assign (sBuffer, cSentence);
	gchar *str;
	for (str = sBuffer->str; *str != '\0'; str ++)
	{
		if (*str >= 'A' && *str <= 'Z')
		{
			*str = *str - 'A' + 'a';
		}
	}
}
#define _search_in_buffer(cKeyWord) g_strstr_len (sBuffer->str, -1, cKeyWord)
static gchar *cairo_dock_highlight_key_word (const gchar *cSentence, const gchar *cKeyWord, gboolean bBold)
{
	//_copy_string_to_buffer (cSentence);
	gchar *cModifiedString = NULL;
	gchar *str = _search_in_buffer (cKeyWord);
	if (str != NULL)
	{
		//g_print ("* trouve %s dans '%s'\n", cKeyWord, sBuffer->str);
		gchar *cBuffer = g_strdup (cSentence);
		str = cBuffer + (str - sBuffer->str);
		*str = '\0';
		cModifiedString = g_strdup_printf ("%s<span color=\"red\">%s%s%s</span>%s", cBuffer, (bBold?"<b>":""), cKeyWord, (bBold?"</b>":""), str + strlen (cKeyWord));
		g_free (cBuffer);
	}
	
	return cModifiedString;
}

static gboolean _cairo_dock_search_words_in_frame_title (const gchar **pKeyWords, GtkWidget *pCurrentFrame, gboolean bAllWords, gboolean bHighLightText, gboolean bHideOther)
{
	//\______________ On recupere son titre.
	GtkWidget *pFrameLabel = NULL;
	GtkWidget *pLabelContainer = (GTK_IS_FRAME (pCurrentFrame) ?
		gtk_frame_get_label_widget (GTK_FRAME (pCurrentFrame)) :
		gtk_expander_get_label_widget (GTK_EXPANDER (pCurrentFrame)));
	//g_print ("pLabelContainer : %x\n", pLabelContainer);
	if (GTK_IS_LABEL (pLabelContainer))
	{
		pFrameLabel = pLabelContainer;
	}
	else if (pLabelContainer != NULL)
	{
		GList *pChildList = gtk_container_get_children (GTK_CONTAINER (pLabelContainer));
		if (pChildList != NULL && pChildList->next != NULL)
			pFrameLabel = pChildList->next->data;
	}
	
	//\______________ On cherche les mots-cles dedans.
	gchar *cModifiedText = NULL;
	const gchar *str = NULL, *cKeyWord;
	gboolean bFoundInFrameTitle = FALSE;
	if (pFrameLabel != NULL)
	{
		const gchar *cFrameTitle = gtk_label_get_text (GTK_LABEL (pFrameLabel));
		int i;
		for (i = 0; pKeyWords[i] != NULL; i ++)
		{
			cKeyWord = pKeyWords[i];
			_copy_string_to_buffer (cFrameTitle);
			if (bHighLightText)
				cModifiedText = cairo_dock_highlight_key_word (cFrameTitle, cKeyWord, TRUE);
			else
				str = _search_in_buffer (cKeyWord);
			if (cModifiedText != NULL || str != NULL)  // on a trouve ce mot.
			{
				//g_print ("  on a trouve %s dans le titre\n", cKeyWord);
				bFoundInFrameTitle = TRUE;
				if (cModifiedText != NULL)
				{
					gtk_label_set_markup (GTK_LABEL (pFrameLabel), cModifiedText);
					cFrameTitle = gtk_label_get_label (GTK_LABEL (pFrameLabel));  // Pango inclus.
					g_free (cModifiedText);
					cModifiedText = NULL;
				}
				else
					str = NULL;
				if (! bAllWords)
					break ;
			}
			else if (bAllWords)
			{
				bFoundInFrameTitle = FALSE;
				break ;
			}
		}
		if (! bFoundInFrameTitle)  // on remet le texte par defaut.
		{
			cModifiedText = g_strdup_printf ("<b>%s</b>", cFrameTitle);
			gtk_label_set_markup (GTK_LABEL (pFrameLabel), cModifiedText);
			g_free (cModifiedText);
			cModifiedText = NULL;
		}
	}
	return bFoundInFrameTitle;
}


void cairo_dock_apply_filter_on_group_widget (const gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther, GSList *pWidgetList)
{
	//g_print ("%s ()\n", __func__);
	if (sBuffer == NULL)
		sBuffer = g_string_new ("");
	CairoDockGroupKeyWidget *pGroupKeyWidget;
	GSList *pSubWidgetList;
	GtkWidget *pLabel, *pKeyBox, *pVBox, *pFrame, *pCurrentFrame = NULL, *pExpander;
	const gchar *cDescription;
	gchar *cToolTip = NULL;
	gchar *cModifiedText=NULL, *str=NULL;
	gboolean bFound, bFrameVisible = !bHideOther;
	int i;
	const gchar *cKeyWord;
	GSList *w;
	for (w = pWidgetList; w != NULL; w = w->next)
	{
		bFound = FALSE;
		pGroupKeyWidget = w->data;
		pSubWidgetList = pGroupKeyWidget->pSubWidgetList;
		if (pSubWidgetList == NULL)
			continue;
		pLabel = pGroupKeyWidget->pLabel;
		if (pLabel == NULL)
			continue;
		pKeyBox = pGroupKeyWidget->pKeyBox;
		if (pKeyBox == NULL)
			continue;
		pVBox = gtk_widget_get_parent (pKeyBox);
		pFrame = gtk_widget_get_parent (pVBox);
		
		//\______________ On cache une frame vide, ou au contraire on montre son contenu si elle contient les mots-cles.
		if (pFrame != pCurrentFrame)  // on a change de frame.
		{
			if (pCurrentFrame)
			{
				gboolean bFoundInFrameTitle = _cairo_dock_search_words_in_frame_title (pKeyWords, pCurrentFrame, bAllWords, bHighLightText, bHideOther);
				if (! bFrameVisible && bHideOther)
				{
					if (! bFoundInFrameTitle)
						gtk_widget_hide (pCurrentFrame);
					else
						gtk_widget_show_all (pCurrentFrame);  // montre tous les widgets du groupe.
				}
				else
					gtk_widget_show (pCurrentFrame);
			}
			
			if (GTK_IS_FRAME (pFrame))  // devient la frame courante.
			{
				pExpander = gtk_widget_get_parent (pFrame);
				if (GTK_IS_EXPANDER (pExpander))
					pFrame = pExpander;  // c'est l'expander qui a le texte, c'est donc ca qu'on veut cacher.
				pCurrentFrame = pFrame;
				bFrameVisible = FALSE;
			}
			else
			{
				pCurrentFrame = NULL;
			}
			//g_print ("pCurrentFrame <- %x\n", pCurrentFrame);
		}
		
		cDescription = gtk_label_get_text (GTK_LABEL (pLabel));  // sans les markup Pango.
		if (bSearchInToolTip)
			cToolTip = gtk_widget_get_tooltip_text (pKeyBox);
		//g_print ("cDescription : %s (%s)\n", cDescription, cToolTip);
		
		bFound = FALSE;
		for (i = 0; pKeyWords[i] != NULL; i ++)
		{
			cKeyWord = pKeyWords[i];
			_copy_string_to_buffer (cDescription);
			if (bHighLightText)
				cModifiedText = cairo_dock_highlight_key_word (cDescription, cKeyWord, FALSE);
			else
				str = _search_in_buffer (cKeyWord);
			if (cModifiedText == NULL && str == NULL)
			{
				if (cToolTip != NULL)
				{
					_copy_string_to_buffer (cToolTip);
					str = _search_in_buffer (cKeyWord);
				}
			}
			
			if (cModifiedText != NULL || str != NULL)
			{
				//g_print ("  on a trouve %s\n", cKeyWord);
				bFound = TRUE;
				if (cModifiedText != NULL)
				{
					gtk_label_set_markup (GTK_LABEL (pLabel), cModifiedText);
					cDescription = gtk_label_get_label (GTK_LABEL (pLabel));  // Pango inclus.
					g_free (cModifiedText);
					cModifiedText = NULL;
				}
				else
				{
					gtk_label_set_text (GTK_LABEL (pLabel), cDescription);
					str = NULL;
				}
				if (! bAllWords)
					break ;
			}
			else if (bAllWords)
			{
				bFound = FALSE;
				break ;
			}
		}
		if (bFound)
		{
			//g_print ("on montre ce widget\n");
			gtk_widget_show (pKeyBox);
			if (pCurrentFrame != NULL)
				bFrameVisible = TRUE;
		}
		else if (bHideOther)
		{
			//g_print ("on cache ce widget\n");
			gtk_widget_hide (pKeyBox);
		}
		else
			gtk_widget_show (pKeyBox);
		g_free (cToolTip);
	}
	if (pCurrentFrame)  // la derniere frame.
	{
		gboolean bFoundInFrameTitle = _cairo_dock_search_words_in_frame_title (pKeyWords, pCurrentFrame, bAllWords, bHighLightText, bHideOther);
		if (! bFrameVisible && bHideOther)
		{
			if (! bFoundInFrameTitle)
				gtk_widget_hide (pCurrentFrame);
			else
				gtk_widget_show_all (pCurrentFrame);  // montre tous les widgets du groupe.
		}
		else
			gtk_widget_show (pCurrentFrame);
	}
}


static gboolean _search_in_key_file (const gchar **pKeyWords, GKeyFile *pKeyFile, const gchar *cGroup, const gchar *cGettextDomain, gboolean bAllWords, gboolean bSearchInToolTip)
{
	g_return_val_if_fail (pKeyFile != NULL, FALSE);
	gboolean bFound = FALSE;
	
	// get the groups
	gchar **pGroupList = NULL;
	if (cGroup == NULL)  // no group specified, check all
	{
		gsize length = 0;
		pGroupList = g_key_file_get_groups (pKeyFile, &length);
	}
	else
	{
		pGroupList = g_new0 (gchar *, 2);
		pGroupList[0] = g_strdup (cGroup);
	}
	g_return_val_if_fail (pGroupList != NULL, FALSE);
	
	// search in each (group, key).
	int iNbWords;
	for (iNbWords = 0; pKeyWords[iNbWords] != NULL; iNbWords ++);
	gboolean *bFoundWords = g_new0 (gboolean , iNbWords);

	const gchar *cUsefulComment;
	gchar iElementType;
	guint iNbElements;
	gchar **pAuthorizedValuesList;
	const gchar *cTipString;
	gboolean bIsAligned;
	gchar **pKeyList;
	gchar *cGroupName, *cKeyName, *cKeyComment;
	int i, j, k;
	for (k = 0; pGroupList[k] != NULL; k ++)
	{
		cGroupName = pGroupList[k];
		pKeyList = g_key_file_get_keys (pKeyFile, cGroupName, NULL, NULL);
		if (! pKeyList)
			continue;
		for (j = 0; pKeyList[j] != NULL; j ++)
		{
			cKeyName = pKeyList[j];
			//\_______________ On recupere la description + bulle d'aide de la cle.
			cKeyComment =  g_key_file_get_comment (pKeyFile, cGroupName, cKeyName, NULL);
			cUsefulComment = cairo_dock_parse_key_comment (cKeyComment, &iElementType, &iNbElements, &pAuthorizedValuesList, &bIsAligned, &cTipString);
			if (cUsefulComment == NULL)
			{
				g_free (cKeyComment);
				continue;
			}

			cUsefulComment = dgettext (cGettextDomain, cUsefulComment);
			if (cTipString != NULL)
			{
				if (bSearchInToolTip)
					cTipString = dgettext (cGettextDomain, cTipString);
				else
					cTipString = NULL;
			}
			//\_______________ On y cherche les mots-cles.
			const gchar *cKeyWord, *str;
			for (i = 0; pKeyWords[i] != NULL; i ++)
			{
				if (bFoundWords[i])
					continue;
				cKeyWord = pKeyWords[i];
				str = NULL;
				if (cUsefulComment)
				{
					_copy_string_to_buffer (cUsefulComment);
					str = _search_in_buffer (cKeyWord);
				}
				if (! str && cTipString)
				{
					_copy_string_to_buffer (cTipString);
					str = _search_in_buffer (cKeyWord);
				}
				if (! str && pAuthorizedValuesList)
				{
					int l;
					for (l = 0; pAuthorizedValuesList[l] != NULL; l ++)
					{
						_copy_string_to_buffer (dgettext (cGettextDomain, pAuthorizedValuesList[l]));
						str = _search_in_buffer (cKeyWord);
						if (str != NULL)
							break ;
					}
				}

				if (str != NULL)
				{
					//g_print (">>>on a trouve %s\n", pKeyWords[i]);
					bFound = TRUE;
					str = NULL;
					if (! bAllWords)
					{
						break ;
					}
					bFoundWords[i] = TRUE;
				}
			}

			g_free (cKeyComment);
			if (! bAllWords && bFound)
				break ;
		}  // fin de parcours du groupe.
		g_strfreev (pKeyList);
		if (! bAllWords && bFound)
			break ;
	}  // fin de parcours des groupes.
	g_strfreev (pGroupList);

	if (bAllWords && bFound)  // check that all words have been found
	{
		for (i = 0; i < iNbWords; i ++)
		{
			if (! bFoundWords[i])
			{
				//g_print ("par contre il manque %s, dommage\n", pKeyWords[i]);
				bFound = FALSE;
				break;
			}
		}
	}
	g_free (bFoundWords);
	
	return bFound;
}

static void cairo_dock_apply_filter_on_group_list (const gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther, GList *pGroupDescriptionList)
{
	//g_print ("%s ()\n", __func__);
	if (sBuffer == NULL)
		sBuffer = g_string_new ("");
	CairoDockGroupDescription *pGroupDescription;
	const gchar *cKeyWord, *str = NULL;
	gchar *cModifiedText = NULL;
	const gchar *cTitle, *cToolTip = NULL;
	gboolean bFound, bFrameVisible = FALSE;
	GtkWidget *pGroupBox, *pLabel, *pCategoryFrame, *pCurrentCategoryFrame = NULL;
	GKeyFile *pKeyFile, *pMainKeyFile = cairo_dock_open_key_file (g_cConfFile);
	
	// reset groups and frames
	GList *gd;
	const gchar *cGettextDomain;
	for (gd = pGroupDescriptionList; gd != NULL; gd = gd->next)
	{
		pGroupDescription = gd->data;
		gtk_label_set_use_markup (GTK_LABEL (pGroupDescription->pLabel), FALSE);
		gtk_label_set_markup (GTK_LABEL (pGroupDescription->pLabel), pGroupDescription->cTitle);
		gtk_widget_show (pGroupDescription->pGroupHBox);
	}
	
	CairoDockCategoryWidgetTable *pCategoryWidget;
	guint i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		gtk_widget_show (pCategoryWidget->pFrame);
	}
	
	// for each group, search in its title/description/config
	for (gd = pGroupDescriptionList; gd != NULL; gd = gd->next)
	{
		pGroupDescription = gd->data;
		pGroupBox = pGroupDescription->pGroupHBox;
		pLabel = pGroupDescription->pLabel;
		pCategoryFrame = gtk_widget_get_parent (pGroupBox);
		cGettextDomain = pGroupDescription->cGettextDomain;
		bFound = FALSE;
		
		cTitle = pGroupDescription->cTitle;  // already translated.
		if (bSearchInToolTip)
			cToolTip = dgettext (cGettextDomain, pGroupDescription->cDescription);
		
		//\_______________ before we start a new category frame, hide the current one if nothing has been found inside.
		if (pCategoryFrame != pCurrentCategoryFrame)  // on a change de frame.
		{
			if (pCurrentCategoryFrame)
			{
				if (! bFrameVisible && bHideOther)
				{
					gtk_widget_hide (pCurrentCategoryFrame);
				}
			}
			pCurrentCategoryFrame = pCategoryFrame;
			bFrameVisible = FALSE;
		}
		
		//\_______________ look for each word in the title+description
		for (i = 0; pKeyWords[i] != NULL; i ++)
		{
			cKeyWord = pKeyWords[i];
			_copy_string_to_buffer (cTitle);
			str = _search_in_buffer (cKeyWord);
			if (!str && cToolTip != NULL)
			{
				_copy_string_to_buffer (cToolTip);
				str = _search_in_buffer (cKeyWord);
			}
			if (str != NULL)  // found something
			{
				bFound = TRUE;
				if (! bAllWords)  // 1 word is enough => break
					break ;
			}
			else if (bAllWords)  // word not found, and we want a strike => fail.
			{
				bFound = FALSE;
				break ;
			}
		}
		
		// highlight the title accordingly
		if (bFound && bHighLightText)
		{
			for (i = 0; pKeyWords[i] != NULL; i ++)
			{
				cKeyWord = pKeyWords[i];
				cModifiedText = cairo_dock_highlight_key_word (cTitle, cKeyWord, TRUE);
				if (cModifiedText)
				{
					gtk_label_set_use_markup (GTK_LABEL (pLabel), TRUE);
					gtk_label_set_markup (GTK_LABEL (pLabel), cModifiedText);
					g_free (cModifiedText);
					cModifiedText = NULL;
					break;
				}
			}
			if (pKeyWords[i] == NULL)  // not found in the title, hightlight in blue.
			{
				cModifiedText = g_strdup_printf ("<span color=\"blue\">%s</span>", cTitle);
				gtk_label_set_markup (GTK_LABEL (pLabel), cModifiedText);
				g_free (cModifiedText);
				cModifiedText = NULL;
			}
		}
		
		//\_______________ look for each word in the config file.
		if (! bFound)
		{
			CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
			if (pModule != NULL)  // module, search in its default config file
			{
				pKeyFile = cairo_dock_open_key_file (pModule->cConfFilePath);  // search in default conf file
				bFound = _search_in_key_file (pKeyWords, pKeyFile, NULL, cGettextDomain, bAllWords, bSearchInToolTip);
				g_key_file_free (pKeyFile);
			}
			else if (pGroupDescription->pManagers != NULL)  // internal group, search in the main conf and possibly in the extensions
			{
				bFound = _search_in_key_file (pKeyWords, pMainKeyFile, pGroupDescription->cGroupName, cGettextDomain, bAllWords, bSearchInToolTip);
				GList *m, *e;
				for (m = pGroupDescription->pManagers; m != NULL; m = m->next)
				{
					const gchar *cManagerName = m->data;
					GldiManager *pManager = gldi_get_manager (cManagerName);
					g_return_if_fail (pManager != NULL);
					for (e = pManager->pExternalModules; e != NULL; e = e->next)
					{
						const gchar *cModuleName = e->data;
						pModule = cairo_dock_find_module_from_name (cModuleName);
						if (!pModule)
							continue;
						pKeyFile = cairo_dock_open_key_file (pModule->cConfFilePath);
						bFound |= _search_in_key_file (pKeyWords, pKeyFile, NULL, cGettextDomain, bAllWords, bSearchInToolTip);
						g_key_file_free (pKeyFile);
					}
				}
			}  // else any other widget, ignore.
			
			if (bHighLightText && bFound)  // on passe le label du groupe en bleu.
			{
				cModifiedText = g_strdup_printf ("<span color=\"blue\">%s</span>", cTitle);
				//g_print ("cModifiedText : %s\n", cModifiedText);
				gtk_label_set_markup (GTK_LABEL (pLabel), dgettext (cGettextDomain, cModifiedText));
				g_free (cModifiedText);
				cModifiedText = NULL;
			}
		}

		bFrameVisible |= bFound;
		
		if (!bFound && bHideOther)
		{
			gtk_widget_hide (pGroupBox);
		}
	}
	g_key_file_free (pMainKeyFile);
}


  ///////////////
 // CALLBACKS //
///////////////

static void on_click_category_button (GtkButton *button, gpointer data)
{
	int iCategory = GPOINTER_TO_INT (data);
	//g_print ("%s (%d)\n", __func__, iCategory);
	cairo_dock_show_one_category (iCategory);
}

static void on_click_all_button (GtkButton *button, gpointer data)
{
	//g_print ("%s ()\n", __func__);
	cairo_dock_show_all_categories ();
}

static void on_click_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cGroupName);
	cairo_dock_show_group (pGroupDescription);
	cairo_dock_toggle_category_button (pGroupDescription->iCategory);
}

static void _show_group_or_category (gpointer pPlace)
{
	if (pPlace == NULL)
		cairo_dock_show_all_categories ();
	else if (GPOINTER_TO_INT (pPlace) < CAIRO_DOCK_NB_CATEGORY+1)  // categorie.
	{
		if (pPlace == 0)
			cairo_dock_show_all_categories ();
		else
		{
			int iCategory = GPOINTER_TO_INT (pPlace) - 1;
			cairo_dock_show_one_category (iCategory);
		}
	}
	else  // groupe.
	{
		cairo_dock_show_group (pPlace);
	}
}

static gpointer _get_previous_widget (void)
{
	if (s_path == NULL || s_path->next == NULL)
	{
		if (s_path != NULL)  // utile ?...
		{
			g_slist_free (s_path);
			s_path = NULL;
		}
		return 0;
	}
	
	//s_path = g_list_delete_link (s_path, s_path);
	GSList *tmp = s_path;
	s_path = s_path->next;
	tmp->next = NULL;
	g_slist_free (tmp);
	
	return s_path->data;
}
static void on_click_back_button (GtkButton *button, gpointer data)
{
	gpointer pPrevPlace = _get_previous_widget ();
	_show_group_or_category (pPrevPlace);
}

static gboolean _show_group_dialog (CairoDockGroupDescription *pGroupDescription)
{
	int iPreviewWidgetWidth = s_iPreviewWidth;
	GtkWidget *pPreviewImage = s_pPreviewImage;
	if (pGroupDescription->cPreviewFilePath != NULL && strcmp (pGroupDescription->cPreviewFilePath, "none") != 0)
	{
		//g_print ("on recupere la prevue de %s\n", pGroupDescription->cPreviewFilePath);
		int iPreviewWidth, iPreviewHeight;
		GdkPixbuf *pPreviewPixbuf = NULL;
		if (gdk_pixbuf_get_file_info (pGroupDescription->cPreviewFilePath, &iPreviewWidth, &iPreviewHeight) != NULL)
		{
			if (iPreviewWidth > CAIRO_DOCK_PREVIEW_WIDTH)
			{
				iPreviewHeight *= (double)CAIRO_DOCK_PREVIEW_WIDTH/iPreviewWidth;
				iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH;
			}
			if (iPreviewHeight > CAIRO_DOCK_PREVIEW_HEIGHT)
			{
				iPreviewWidth *= (double)CAIRO_DOCK_PREVIEW_HEIGHT/iPreviewHeight;
				iPreviewHeight = CAIRO_DOCK_PREVIEW_HEIGHT;
			}
			if (iPreviewWidth > iPreviewWidgetWidth)
			{
				iPreviewHeight *= (double)iPreviewWidgetWidth/iPreviewWidth;
				iPreviewWidth = iPreviewWidgetWidth;
			}
			//g_print ("preview : %dx%d\n", iPreviewWidth, iPreviewHeight);
			pPreviewPixbuf = gdk_pixbuf_new_from_file_at_size (pGroupDescription->cPreviewFilePath, iPreviewWidth, iPreviewHeight, NULL);
		}
		if (pPreviewPixbuf == NULL)
		{
			cd_warning ("no preview available");
			pPreviewPixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				TRUE,
				8,
				1,
				1);
		}
		gtk_image_set_from_pixbuf (GTK_IMAGE (pPreviewImage), pPreviewPixbuf);
		gdk_pixbuf_unref (pPreviewPixbuf);
		gtk_widget_show (pPreviewImage);
	}
	
	if (s_pDialog != NULL)
		if (! cairo_dock_dialog_unreference (s_pDialog))
			cairo_dock_dialog_unreference (s_pDialog);
	Icon *pIcon = cairo_dock_get_current_active_icon ();
	if (pIcon == NULL || pIcon->cParentDockName == NULL || cairo_dock_icon_is_being_removed (pIcon))
		pIcon = cairo_dock_get_dialogless_icon ();
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon != NULL ? pIcon->cParentDockName : NULL);
	
	CairoDialogAttribute attr;
	memset (&attr, 0, sizeof (CairoDialogAttribute));
	attr.cText = dgettext (pGroupDescription->cGettextDomain, pGroupDescription->cDescription);
	attr.cImageFilePath = pGroupDescription->cIcon;
	attr.bNoInput = TRUE;
	myDialogsParam.dialogTextDescription.bUseMarkup = TRUE;
	s_pDialog = cairo_dock_build_dialog (&attr, pIcon, CAIRO_CONTAINER (pDock));
	myDialogsParam.dialogTextDescription.bUseMarkup = FALSE;
	
	cairo_dock_dialog_reference (s_pDialog);
	
	gtk_window_set_transient_for (GTK_WINDOW (s_pDialog->container.pWidget), GTK_WINDOW (s_pMainWindow));

	s_iSidShowGroupDialog = 0;
	return FALSE;
}
static void on_enter_group_button (GtkButton *button, CairoDockGroupDescription *pGroupDescription)
{
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cDescription);
	if (g_pPrimaryContainer == NULL)  // inutile en maintenance, le dialogue risque d'apparaitre sur la souris.
		return ;
	
	if (s_iSidShowGroupDialog != 0)
		g_source_remove (s_iSidShowGroupDialog);
	
	s_iSidShowGroupDialog = g_timeout_add (330, (GSourceFunc)_show_group_dialog, (gpointer) pGroupDescription);
}
static void on_leave_group_button (GtkButton *button, gpointer *data)
{
	//g_print ("%s ()\n", __func__);
	if (s_iSidShowGroupDialog != 0)
	{
		g_source_remove (s_iSidShowGroupDialog);
		s_iSidShowGroupDialog = 0;
	}

	int iPreviewWidgetWidth = s_iPreviewWidth;
	GtkWidget *pPreviewImage = s_pPreviewImage;
	gtk_widget_hide (pPreviewImage);
	
	if (! cairo_dock_dialog_unreference (s_pDialog))
		cairo_dock_dialog_unreference (s_pDialog);
	s_pDialog = NULL;
}



static void _destroy_current_widget (gboolean bDestroyGtkWidget)
{
	if (! s_pCurrentGroupWidget2)
		return;
	
	if (bDestroyGtkWidget)
		cairo_dock_widget_destroy_widget (s_pCurrentGroupWidget2);
	
	cairo_dock_widget_free (s_pCurrentGroupWidget2);
	s_pCurrentGroupWidget2 = NULL;
}

static void _cairo_dock_free_group_description (CairoDockGroupDescription *pGroupDescription)
{
	if (pGroupDescription == NULL)
		return;
	g_free (pGroupDescription->cGroupName);
	g_free (pGroupDescription->cDescription);
	g_free (pGroupDescription->cPreviewFilePath);
	g_free (pGroupDescription->cIcon);
	g_list_free (pGroupDescription->pManagers);
	g_free (pGroupDescription);
}

static void cairo_dock_free_categories (void)
{
	memset (s_pCategoryWidgetTables, 0, sizeof (s_pCategoryWidgetTables));  // les widgets a l'interieur sont detruits avec la fenetre.
	
	g_list_foreach (s_pGroupDescriptionList, (GFunc)_cairo_dock_free_group_description, NULL);
	g_list_free (s_pGroupDescriptionList);
	s_pGroupDescriptionList = NULL;
	s_pCurrentGroup = NULL;
	
	s_pPreviewImage = NULL;
	
	s_pOkButton = NULL;
	s_pApplyButton = NULL;
	
	s_pMainWindow = NULL;
	s_pToolBar = NULL;
	s_pStatusBar = NULL;
	
	_destroy_current_widget (FALSE);  // the gtk widget is destroyed with the window
	
	g_slist_free (s_path);
	s_path = NULL;
}

static gboolean on_delete_main_gui (GtkWidget *pWidget, gpointer data)
{
	cairo_dock_free_categories ();
	if (s_iSidShowGroupDialog != 0)
	{
		g_source_remove (s_iSidShowGroupDialog);
		s_iSidShowGroupDialog = 0;
	}
	return FALSE;
}


  ///////////////
 /// WIDGETS ///
///////////////

static CDWidget *_build_module_widget (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *pInstance)
{
	// get the associated module
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	
	// build its widget
	ModuleWidget *pModuleWidget = cairo_dock_module_widget_new (pModule, pInstance);
	
	return CD_WIDGET (pModuleWidget);
}

static CDWidget *_build_config_group_widget (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *unused)
{
	ConfigGroupWidget *pConfigGroupWidget = cairo_dock_config_group_widget_new (pGroupDescription->cGroupName, pGroupDescription->pManagers, pGroupDescription->cTitle, pGroupDescription->cIcon);
	
	return CD_WIDGET (pConfigGroupWidget);
}

static CDWidget *_build_themes_widget (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *unused)
{
	ThemesWidget *pThemesWidget = cairo_dock_themes_widget_new (GTK_WINDOW (s_pMainWindow));
	
	return CD_WIDGET (pThemesWidget);
}

static CDWidget *_build_items_widget (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *unused)
{
	ItemsWidget *pItemsWidget = cairo_dock_items_widget_new ();
	
	return CD_WIDGET (pItemsWidget);
}

static CDWidget *_build_shortkeys_widget (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *unused)
{
	ShortkeysWidget *pShortkeysWidget = cairo_dock_shortkeys_widget_new ();
	
	return CD_WIDGET (pShortkeysWidget);
}

static void on_click_apply (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	cairo_dock_widget_apply (s_pCurrentGroupWidget2);
}

static void on_click_quit (GtkButton *button, GtkWidget *pWindow)
{
	gtk_widget_destroy (pWindow);
}

static void on_click_ok (GtkButton *button, GtkWidget *pWindow)
{
	//g_print ("%s ()\n", __func__);
	
	on_click_apply (button, pWindow);
	on_click_quit (button, pWindow);
}

static void on_click_activate_given_group (GtkToggleButton *button, CairoDockGroupDescription *pGroupDescription)
{
	g_return_if_fail (pGroupDescription != NULL);
	//g_print ("%s (%s)\n", __func__, pGroupDescription->cGroupName);
	
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	g_return_if_fail (pModule != NULL);  // only modules have their button sensitive (because only them can be (de)activated).
	if (g_pPrimaryContainer == NULL)
	{
		cairo_dock_add_remove_element_to_key (g_cConfFile, "System", "modules", pGroupDescription->cGroupName, gtk_toggle_button_get_active (button));
	}
	else if (pModule->pInstancesList == NULL)
	{
		cairo_dock_activate_module_and_load (pGroupDescription->cGroupName);
	}
	else
	{
		cairo_dock_deactivate_module_and_unload (pGroupDescription->cGroupName);
	}
}

static void on_click_activate_current_group (GtkToggleButton *button, gpointer *data)
{
	CairoDockGroupDescription *pGroupDescription = s_pCurrentGroup;
	on_click_activate_given_group (button, pGroupDescription);
	
	if (pGroupDescription->pActivateButton != NULL)  // on repercute le changement sur le bouton d'activation du groupe.
	{
		CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
		g_return_if_fail (pModule != NULL);
		g_signal_handlers_block_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), pModule->pInstancesList != NULL);
		g_signal_handlers_unblock_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
	}
}


  ////////////
 // FILTER //
////////////

static gboolean bAllWords = FALSE;
static gboolean bSearchInToolTip = FALSE;
static gboolean bHighLightText = FALSE;
static gboolean bHideOther = FALSE;

static inline void _reset_filter_state (void)
{
	bAllWords = FALSE;
	bSearchInToolTip = TRUE;
	bHighLightText = TRUE;
	bHideOther = TRUE;
}

static void on_activate_filter (GtkEntry *pEntry, gpointer data)
{
	const gchar *cFilterText = gtk_entry_get_text (pEntry);
	if (cFilterText == NULL || *cFilterText == '\0')
	{
		return;
	}
	
	gchar **pKeyWords = g_strsplit (cFilterText, " ", 0);
	if (pKeyWords == NULL)  // 1 seul mot.
	{
		pKeyWords = g_new0 (gchar*, 2);
		pKeyWords[0] = (gchar *) cFilterText;
	}
	gchar *str;
	int i,j;
	for (i = 0; pKeyWords[i] != NULL; i ++)
	{
		for (str = pKeyWords[i]; *str != '\0'; str ++)
		{
			if (*str >= 'A' && *str <= 'Z')
			{
				*str = *str - 'A' + 'a';
			}
		}
	}
	cairo_dock_apply_current_filter ((const gchar **)pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther);
	
	g_strfreev (pKeyWords);
}

static void _trigger_current_filter (void)
{
	gboolean bReturn;
	g_signal_emit_by_name (s_pFilterEntry, "activate", NULL, &bReturn);
}
static void on_toggle_all_words (GtkCheckMenuItem *pMenuItem, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bAllWords = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
static void on_toggle_search_in_tooltip (GtkCheckMenuItem *pMenuItem, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bSearchInToolTip = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
static void on_toggle_highlight_words (GtkCheckMenuItem *pMenuItem, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bHighLightText = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
static void on_toggle_hide_others (GtkCheckMenuItem *pMenuItem, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, gtk_toggle_button_get_active (pButton));
	bHideOther = gtk_check_menu_item_get_active (pMenuItem);
	_trigger_current_filter ();
}
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
static void on_clear_filter (GtkEntry *pEntry, GtkEntryIconPosition icon_pos, GdkEvent *event, gpointer data)
{
	gtk_entry_set_text (pEntry, "");
	//gpointer pCurrentPlace = cairo_dock_get_current_widget ();
	//g_print ("pCurrentPlace : %x\n", pCurrentPlace);
	//_show_group_or_category (pCurrentPlace);
	const gchar *keyword[2] = {"fabounetfabounetfabounet", NULL};
	cairo_dock_apply_current_filter (keyword, FALSE, FALSE, FALSE, FALSE);
}
#endif


  ////////////
 // WINDOW //
////////////

static inline GtkWidget *_make_image (const gchar *cImage, int iSize)
{
	GtkWidget *pImage = NULL;
	if (strncmp (cImage, "gtk-", 4) == 0)
	{
		if (iSize >= 48)
			iSize = GTK_ICON_SIZE_DIALOG;
		else if (iSize >= 32)
			iSize = GTK_ICON_SIZE_LARGE_TOOLBAR;
		else
			iSize = GTK_ICON_SIZE_BUTTON;
		pImage = gtk_image_new_from_stock (cImage, iSize);
	}
	else
	{
		gchar *cIconPath = NULL;
		if (*cImage != '/')
		{
			cIconPath = g_strconcat (g_cCairoDockDataDir, "/config-panel/", cImage, NULL);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/icons/", cImage, NULL);
			}
		}
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cIconPath ? cIconPath : cImage, iSize, iSize, NULL);
		g_free (cIconPath);
		if (pixbuf != NULL)
		{
			pImage = gtk_image_new_from_pixbuf (pixbuf);
			gdk_pixbuf_unref (pixbuf);
		}
	}
	return pImage;
}
static GtkToolItem *_make_toolbutton (const gchar *cLabel, const gchar *cImage, int iSize)
{
	if (cImage == NULL)
	{
		GtkToolItem *pWidget = gtk_toggle_tool_button_new ();
		gtk_tool_button_set_label (GTK_TOOL_BUTTON (pWidget), cLabel);
		return pWidget;
	}
	GtkWidget *pImage = _make_image (cImage, iSize);
	GtkToolItem *pWidget = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (pWidget), pImage);
	if (cLabel == NULL)
		return pWidget;
	
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel2 = g_strdup_printf ("<span><b>%s</b></span>", cLabel);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel2);
	g_free (cLabel2);
	
	GtkWidget *pAlign = gtk_alignment_new (0., 0.5, 0., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (pAlign), 0, 0, CAIRO_DOCK_FRAME_MARGIN, 0);
	gtk_container_add (GTK_CONTAINER (pAlign), pLabel);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (pWidget), pAlign);
	
	return pWidget;
}

static inline CairoDockGroupDescription *_add_group_button (const gchar *cGroupName, const gchar *cIcon, int iCategory, const gchar *cDescription, const gchar *cPreviewFilePath, int iActivation, gboolean bConfigurable, const gchar *cGettextDomain, const gchar *cTitle)
{
	//\____________ On garde une trace de ses caracteristiques.
	CairoDockGroupDescription *pGroupDescription = g_new0 (CairoDockGroupDescription, 1);
	pGroupDescription->cGroupName = g_strdup (cGroupName);
	pGroupDescription->cDescription = g_strdup (cDescription);
	pGroupDescription->iCategory = iCategory;
	pGroupDescription->cPreviewFilePath = g_strdup (cPreviewFilePath);
	gchar *cIconPath = NULL;
	if (cIcon)
	{
		if (*cIcon == '/')  // on ecrase les chemins des icons d'applets.
		{
			cIconPath = g_strdup_printf ("%s/config-panel/%s.png", g_cCairoDockDataDir, cGroupName);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strdup (cIcon);
			}
		}
		else if (strncmp (cIcon, "gtk-", 4) == 0)
		{
			cIconPath = g_strdup (cIcon);
		}
		else  // categorie ou module interne.
		{
			cIconPath = g_strconcat (g_cCairoDockDataDir, "/config-panel/", cIcon, NULL);
			if (!g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strconcat (CAIRO_DOCK_SHARE_DATA_DIR"/icons/", cIcon, NULL);
			}
		}
	}
	pGroupDescription->cIcon = cIconPath;
	pGroupDescription->cGettextDomain = cGettextDomain;
	pGroupDescription->cTitle = cTitle;
	
	s_pGroupDescriptionList = g_list_prepend (s_pGroupDescriptionList, pGroupDescription);
	
	//\____________ On construit le bouton du groupe.
	GtkWidget *pGroupHBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
	pGroupDescription->pGroupHBox = pGroupHBox;
	
	pGroupDescription->pActivateButton = gtk_check_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), iActivation);
	g_signal_connect (G_OBJECT (pGroupDescription->pActivateButton), "clicked", G_CALLBACK(on_click_activate_given_group), pGroupDescription);
	if (iActivation == -1)
		gtk_widget_set_sensitive (pGroupDescription->pActivateButton, FALSE);
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupDescription->pActivateButton,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pGroupButton = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (pGroupButton), GTK_RELIEF_NONE);
	if (bConfigurable)
		g_signal_connect (G_OBJECT (pGroupButton), "clicked", G_CALLBACK(on_click_group_button), pGroupDescription);
	else
		gtk_widget_set_sensitive (pGroupButton, FALSE);
	g_signal_connect (G_OBJECT (pGroupButton), "enter", G_CALLBACK(on_enter_group_button), pGroupDescription);
	g_signal_connect (G_OBJECT (pGroupButton), "leave", G_CALLBACK(on_leave_group_button), NULL);

	GtkWidget *pButtonHBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
	GtkWidget *pImage = _make_image (cIconPath, CAIRO_DOCK_GROUP_ICON_SIZE);
	gtk_box_pack_start (GTK_BOX (pButtonHBox), pImage, FALSE, FALSE, 0);
	pGroupDescription->pLabel = gtk_label_new (pGroupDescription->cTitle);
	gtk_box_pack_start (GTK_BOX (pButtonHBox),
		pGroupDescription->pLabel,
		FALSE,
		FALSE,
		0);
	gtk_container_add (GTK_CONTAINER (pGroupButton), pButtonHBox);
	gtk_box_pack_start (GTK_BOX (pGroupHBox),
		pGroupButton,
		FALSE,
		FALSE,
		0);

	//\____________ On place le bouton dans sa table.
	CairoDockCategoryWidgetTable *pCategoryWidget = &s_pCategoryWidgetTables[iCategory];
	if (pCategoryWidget->iNbItemsInCurrentRow == s_iNbButtonsByRow)
	{
		pCategoryWidget->iNbItemsInCurrentRow = 0;
		pCategoryWidget->iNbRows ++;
	}
	gtk_table_attach_defaults (GTK_TABLE (pCategoryWidget->pTable),
		pGroupHBox,
		pCategoryWidget->iNbItemsInCurrentRow,
		pCategoryWidget->iNbItemsInCurrentRow+1,
		pCategoryWidget->iNbRows,
		pCategoryWidget->iNbRows+1);
	pCategoryWidget->iNbItemsInCurrentRow ++;
	return pGroupDescription;
}

static gboolean _cairo_dock_add_one_module_widget (CairoDockModule *pModule, const gchar *cActiveModules)
{
	if (pModule->pVisitCard->cInternalModule != NULL)  // this module extends a manager, it will be merged with this one.
		return TRUE;  // continue.
	
	const gchar *cModuleName = pModule->pVisitCard->cModuleName;
	///if (pModule->cConfFilePath == NULL && ! g_bEasterEggs)  // option perso : les plug-ins non utilises sont grises et ne rajoutent pas leur .conf au theme courant.
	///	pModule->cConfFilePath = cairo_dock_check_module_conf_file (pModule->pVisitCard);
	int iActive;
	if (! pModule->pInterface->stopModule)
		iActive = -1;
	else if (g_pPrimaryContainer == NULL && cActiveModules != NULL)  // avant chargement du theme.
	{
		gchar *str = g_strstr_len (cActiveModules, strlen (cActiveModules), cModuleName);
		iActive = (str != NULL &&
			(str[strlen(cModuleName)] == '\0' || str[strlen(cModuleName)] == ';') &&
			(str == cActiveModules || *(str-1) == ';'));
	}
	else
		iActive = (pModule->pInstancesList != NULL);
	
	CairoDockGroupDescription *pGroupDescription = _add_group_button (cModuleName,
		pModule->pVisitCard->cIconFilePath,
		pModule->pVisitCard->iCategory,
		pModule->pVisitCard->cDescription,
		pModule->pVisitCard->cPreviewFilePath,
		iActive,
		pModule->pVisitCard->cConfFileName != NULL,
		pModule->pVisitCard->cGettextDomain,
		pModule->pVisitCard->cTitle);
	//g_print ("+ %s : %x;%x\n", cModuleName,pGroupDescription, pGroupDescription->pActivateButton);
	pGroupDescription->build_widget = _build_module_widget;
	return TRUE;  // continue.
}

#define _add_one_main_group_button(cGroupName, cIcon, iCategory, cDescription, cTitle) \
_add_group_button (cGroupName,\
		cIcon,\
		iCategory,\
		cDescription,\
		NULL,  /* pas de prevue*/\
		-1,  /* <=> non desactivable*/\
		TRUE,  /* <=> configurable*/\
		NULL,  /* domaine de traduction : celui du dock.*/\
		cTitle)
static void _add_main_groups_buttons (void)
{
	CairoDockGroupDescription *pGroupDescription;
	pGroupDescription = _add_one_main_group_button ("Position",
		"icon-position.svg",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Set the position of the main dock."),
		_("Position"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Accessibility",
		"icon-visibility.svg",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Do you like your dock to be always visible,\n or on the contrary unobtrusive?\nConfigure the way you access your docks and sub-docks!"),
		_("Visibility"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("TaskBar",
		"icon-taskbar.png",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Display and interact with currently open windows."),
		_("Taskbar"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Taskbar");
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Shortkeys",
		"gtk-select-font",  /// TODO: trouver une meilleure icone, et l'utiliser aussi pour le backend "simple"...
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("Define all the keyboard shortcuts currently available."),
		_("Shortkeys"));
	pGroupDescription->build_widget = _build_shortkeys_widget;
	
	pGroupDescription = _add_one_main_group_button ("System",
		"icon-system.svg",
		CAIRO_DOCK_CATEGORY_BEHAVIOR,
		N_("All of the parameters you will never want to tweak."),
		_("System"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	pGroupDescription->pManagers = g_list_prepend (pGroupDescription->pManagers, (gchar*)"Connection");
	pGroupDescription->pManagers = g_list_prepend (pGroupDescription->pManagers, (gchar*)"Containers");
	pGroupDescription->pManagers = g_list_prepend (pGroupDescription->pManagers, (gchar*)"Backends");
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Background",
		"icon-background.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Set a background for your dock."),
		_("Background"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Docks");
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Views",
		"icon-views.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Select a view for each of your docks."),
		_("Views"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Backends");  // -> "dock rendering"
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Dialogs",
		"icon-dialogs.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Configure text bubble appearance."),
		_("Dialog boxes"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Dialogs");  // -> "dialog rendering"
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Desklets",
		"icon-desklets.png",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Applets can be displayed on your desktop as widgets."),
		_("Desklets"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Desklets");  // -> "desklet rendering"
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Icons",
		"icon-icons.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("All about icons:\n size, reflection, icon theme,..."),
		_("Icons"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Icons");
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Indicators",
		"icon-indicators.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Indicators are additional markers for your icons."),
		_("Indicators"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Indicators");  // -> "drop indicator"
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Labels",
		"icon-labels.svg",
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Define icon caption and quick-info style."),
		_("Captions"));
	pGroupDescription->pManagers = g_list_prepend (NULL, (gchar*)"Icons");
	pGroupDescription->build_widget = _build_config_group_widget;
	
	pGroupDescription = _add_one_main_group_button ("Themes",
		"icon-controler.svg",  /// TODO: find an icon...
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Try new themes and save your theme."),
		_("Themes"));
	pGroupDescription->build_widget = _build_themes_widget;
	
	pGroupDescription = _add_one_main_group_button ("Items",
		"icon-all.svg",  /// TODO: find an icon...
		CAIRO_DOCK_CATEGORY_THEME,
		N_("Current items in your dock(s)."),
		_("Current items"));
	pGroupDescription->build_widget = _build_items_widget;
}

static CairoDockGroupDescription *cairo_dock_find_module_description (const gchar *cModuleName)
{
	g_return_val_if_fail (cModuleName != NULL, NULL);
	CairoDockGroupDescription *pGroupDescription = NULL;
	GList *pElement = NULL;
	for (pElement = s_pGroupDescriptionList; pElement != NULL; pElement = pElement->next)
	{
		pGroupDescription = pElement->data;
		if (strcmp (pGroupDescription->cGroupName, cModuleName) == 0)
			return pGroupDescription ;
	}
	return NULL;
}

static GtkWidget *cairo_dock_build_main_ihm (const gchar *cConfFilePath)  // 'cConfFilePath' is just used to read the list of active modules in maintenance mode.
{
	//\_____________ On construit la fenetre.
	if (s_pMainWindow != NULL)
	{
		gtk_window_present (GTK_WINDOW (s_pMainWindow));
		return s_pMainWindow;
	}
	s_pMainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	//gtk_container_set_border_width (s_pMainWindow, CAIRO_DOCK_FRAME_MARGIN);
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);
	gtk_window_set_icon_from_file (GTK_WINDOW (s_pMainWindow), cIconPath, NULL);
	g_free (cIconPath);

	GtkWidget *pMainHBox = _gtk_hbox_new (0);
	gtk_container_add (GTK_CONTAINER (s_pMainWindow), pMainHBox);
	
	if (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] > CAIRO_DOCK_CONF_PANEL_WIDTH)
	{
		s_iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW;
	}
	else if (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] > CAIRO_DOCK_CONF_PANEL_WIDTH_MIN)
	{
		double a = 1.*(CAIRO_DOCK_PREVIEW_WIDTH - CAIRO_DOCK_PREVIEW_WIDTH_MIN) / (CAIRO_DOCK_CONF_PANEL_WIDTH - CAIRO_DOCK_CONF_PANEL_WIDTH_MIN);
		double b = CAIRO_DOCK_PREVIEW_WIDTH_MIN - CAIRO_DOCK_CONF_PANEL_WIDTH_MIN * a;
		s_iPreviewWidth = a * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + b;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW - 1;
	}
	else
	{
		s_iPreviewWidth = CAIRO_DOCK_PREVIEW_WIDTH_MIN;
		s_iNbButtonsByRow = CAIRO_DOCK_NB_BUTTONS_BY_ROW_MIN;
	}

	GtkWidget *pCategoriesVBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_widget_set_size_request (pCategoriesVBox, s_iPreviewWidth+2*CAIRO_DOCK_FRAME_MARGIN, CAIRO_DOCK_PREVIEW_HEIGHT);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pCategoriesVBox,
		FALSE,
		FALSE,
		0);

	GtkWidget *pVBox = _gtk_vbox_new (0);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pVBox,
		TRUE,
		TRUE,
		0);
	s_pGroupsVBox = _gtk_vbox_new (CAIRO_DOCK_TABLE_MARGIN);
	GtkWidget *pScrolledWindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (pScrolledWindow), s_pGroupsVBox);
	gtk_box_pack_start (GTK_BOX (pVBox),
		pScrolledWindow,
		TRUE,
		TRUE,
		0);
	
	//\_____________ On construit les boutons de chaque categorie.
	GtkWidget *pCategoriesFrame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (pCategoriesFrame), CAIRO_DOCK_FRAME_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (pCategoriesFrame), GTK_SHADOW_OUT);
	
	GtkWidget *pLabel;
	gchar *cLabel = g_strdup_printf ("<span color=\"#81728C\"><b><u>%s :</u></b></span>", _("Categories"));
	pLabel = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_frame_set_label_widget (GTK_FRAME (pCategoriesFrame), pLabel);
	
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pCategoriesFrame,
		TRUE,  /// FALSE
		TRUE,  /// FALSE
		0);
	
	s_pToolBar = gtk_toolbar_new ();
	#if (GTK_MAJOR_VERSION < 3 && GTK_MINOR_VERSION < 16)
	gtk_toolbar_set_orientation (GTK_TOOLBAR (s_pToolBar), GTK_ORIENTATION_VERTICAL);
	#else
	gtk_orientable_set_orientation (GTK_ORIENTABLE (s_pToolBar), GTK_ORIENTATION_VERTICAL);
	#endif
	gtk_toolbar_set_style (GTK_TOOLBAR (s_pToolBar), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (s_pToolBar), TRUE);  /// FALSE
	//gtk_widget_set (s_pToolBar, "height-request", 300, NULL);
	//g_object_set (s_pToolBar, "expand", TRUE, NULL);
	///gtk_toolbar_set_icon_size (GTK_TOOLBAR (s_pToolBar), GTK_ICON_SIZE_LARGE_TOOLBAR);  /// GTK_ICON_SIZE_LARGE_TOOLBAR
	gtk_container_add (GTK_CONTAINER (pCategoriesFrame), s_pToolBar);
	
	CairoDockCategoryWidgetTable *pCategoryWidget;
	GtkToolItem *pCategoryButton;
	pCategoryWidget = &s_pCategoryWidgetTables[CAIRO_DOCK_NB_CATEGORY];
	pCategoryButton = _make_toolbutton (_("All"),
		"icon-all.svg",
		CAIRO_DOCK_CATEGORY_ICON_SIZE);
	gtk_toolbar_insert (GTK_TOOLBAR (s_pToolBar) , pCategoryButton, -1);
	pCategoryWidget->pCategoryButton = pCategoryButton;
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryButton), TRUE);
	g_signal_connect (G_OBJECT (pCategoryButton), "clicked", G_CALLBACK(on_click_all_button), NULL);
	
	guint i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryButton = _make_toolbutton (gettext (s_cCategoriesDescription[2*i]),
			s_cCategoriesDescription[2*i+1],
			CAIRO_DOCK_CATEGORY_ICON_SIZE);
		g_signal_connect (G_OBJECT (pCategoryButton), "clicked", G_CALLBACK(on_click_category_button), GINT_TO_POINTER (i));
		gtk_toolbar_insert (GTK_TOOLBAR (s_pToolBar) , pCategoryButton,-1);
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		pCategoryWidget->pCategoryButton = pCategoryButton;
	}
	
	//\_____________ On construit les widgets table de chaque categorie.
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		
		pCategoryWidget->pFrame = gtk_frame_new (NULL);
		gtk_container_set_border_width (GTK_CONTAINER (pCategoryWidget->pFrame), CAIRO_DOCK_FRAME_MARGIN);
		gtk_frame_set_shadow_type (GTK_FRAME (pCategoryWidget->pFrame), GTK_SHADOW_OUT);
		
		pLabel = gtk_label_new (NULL);
		cLabel = g_strdup_printf ("<span><b>%s</b></span>", gettext (s_cCategoriesDescription[2*i]));
		gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
		g_free (cLabel);
		gtk_frame_set_label_widget (GTK_FRAME (pCategoryWidget->pFrame), pLabel);
		
		pCategoryWidget->pTable = gtk_table_new (1,
			s_iNbButtonsByRow,
			TRUE);
		gtk_table_set_row_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_FRAME_MARGIN);
		///gtk_table_set_col_spacings (GTK_TABLE (pCategoryWidget->pTable), CAIRO_DOCK_FRAME_MARGIN);
		gtk_container_add (GTK_CONTAINER (pCategoryWidget->pFrame),
			pCategoryWidget->pTable);
		gtk_box_pack_start (GTK_BOX (s_pGroupsVBox),
			pCategoryWidget->pFrame,
			FALSE,
			FALSE,
			0);
	}
	
	
	//\_____________ On remplit avec les groupes du fichier.
	_add_main_groups_buttons ();
	
	//\_____________ On remplit avec les modules.
	gchar *cActiveModules;
	if (g_pPrimaryContainer == NULL)
	{
		GKeyFile* pKeyFile = g_key_file_new();
		g_key_file_load_from_file (pKeyFile, cConfFilePath, 0, NULL);  // inutile de garder les commentaires ici.
		cActiveModules = g_key_file_get_string (pKeyFile, "System", "modules", NULL);
		g_key_file_free (pKeyFile);
	}
	else
		cActiveModules = NULL;
	cairo_dock_foreach_module_in_alphabetical_order ((GCompareFunc) _cairo_dock_add_one_module_widget, cActiveModules);
	g_free (cActiveModules);
	
	//\_____________ On ajoute le filtre.
	// frame
	GtkWidget *pFilterFrame = gtk_frame_new (NULL);
	cLabel = g_strdup_printf ("<span color=\"#81728C\"><b><u>%s :</u></b></span>", _("Filter"));
	GtkWidget *pFilterLabelContainer = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
	GtkWidget *pImage = gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (pFilterLabelContainer), pImage);
	
	pLabel = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_container_add (GTK_CONTAINER (pFilterLabelContainer), pLabel);
	
	gtk_frame_set_label_widget (GTK_FRAME (pFilterFrame), pFilterLabelContainer);
	gtk_container_set_border_width (GTK_CONTAINER (pFilterFrame), CAIRO_DOCK_FRAME_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (pFilterFrame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pFilterFrame,
		FALSE,
		FALSE,
		0);

	GtkWidget *pOptionVBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_container_add (GTK_CONTAINER (pFilterFrame), pOptionVBox);
	
	// entree de texte
	GtkWidget *pFilterBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pFilterBox,
		FALSE,
		FALSE,
		0);
	s_pFilterEntry = gtk_entry_new ();
	g_signal_connect (s_pFilterEntry, "activate", G_CALLBACK (on_activate_filter), NULL);
	gtk_box_pack_start (GTK_BOX (pFilterBox),
		s_pFilterEntry,
		FALSE,
		FALSE,
		0);
	
	#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
	gtk_entry_set_icon_activatable (GTK_ENTRY (s_pFilterEntry), GTK_ENTRY_ICON_SECONDARY, TRUE);
	gtk_entry_set_icon_from_stock (GTK_ENTRY (s_pFilterEntry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);
	g_signal_connect (s_pFilterEntry, "icon-press", G_CALLBACK (on_clear_filter), NULL);
	#endif
	
	// options
	_reset_filter_state ();
	
	GtkWidget *pMenuBar = gtk_menu_bar_new ();
	GtkWidget *pMenuItem = gtk_menu_item_new_with_label (_("Options"));
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenuBar), pMenuItem);
	gtk_box_pack_start (GTK_BOX (pOptionVBox),
		pMenuBar,
		FALSE,
		FALSE,
		0);
	GtkWidget *pMenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pMenu);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("All words"));
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_all_words), NULL);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("Highlighted words"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pMenuItem), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_highlight_words), NULL);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("Hide others"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pMenuItem), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_hide_others), NULL);
	
	pMenuItem = gtk_check_menu_item_new_with_label (_("Search in description"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (pMenuItem), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	g_signal_connect (pMenuItem, "toggled", G_CALLBACK (on_toggle_search_in_tooltip), NULL);
	
	//\_____________ On ajoute le cadre d'activation du module.
	s_pGroupFrame = gtk_frame_new ("pouet");
	gtk_container_set_border_width (GTK_CONTAINER (s_pGroupFrame), CAIRO_DOCK_FRAME_MARGIN);
	gtk_frame_set_shadow_type (GTK_FRAME (s_pGroupFrame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		s_pGroupFrame,
		FALSE,
		FALSE,
		0);
	s_pActivateButton = gtk_check_button_new_with_label (_("Enable this module"));
	g_signal_connect (G_OBJECT (s_pActivateButton), "clicked", G_CALLBACK(on_click_activate_current_group), NULL);
	gtk_container_add (GTK_CONTAINER (s_pGroupFrame), s_pActivateButton);
	gtk_widget_show_all (s_pActivateButton);
	
	//\_____________ On ajoute la zone de prevue.
	GtkWidget *pInfoVBox = _gtk_vbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_box_pack_start (GTK_BOX (pCategoriesVBox),
		pInfoVBox,
		FALSE,
		FALSE,
		0);
	
	s_pPreviewImage = gtk_image_new_from_pixbuf (NULL);
	gtk_container_add (GTK_CONTAINER (pInfoVBox), s_pPreviewImage);
	
	//\_____________ On ajoute les boutons.
	GtkWidget *pButtonsHBox = _gtk_hbox_new (CAIRO_DOCK_FRAME_MARGIN);
	gtk_box_pack_end (GTK_BOX (pVBox),
		pButtonsHBox,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pQuitButton = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect (G_OBJECT (pQuitButton), "clicked", G_CALLBACK(on_click_quit), s_pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		pQuitButton,
		FALSE,
		FALSE,
		0);
	
	s_pBackButton = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	g_signal_connect (G_OBJECT (s_pBackButton), "clicked", G_CALLBACK(on_click_back_button), NULL);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		s_pBackButton,
		FALSE,
		FALSE,
		0);
	
	s_pOkButton = gtk_button_new_from_stock (GTK_STOCK_OK);
	g_signal_connect (G_OBJECT (s_pOkButton), "clicked", G_CALLBACK(on_click_ok), s_pMainWindow);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		s_pOkButton,
		FALSE,
		FALSE,
		0);
	
	s_pApplyButton = gtk_button_new_from_stock (GTK_STOCK_APPLY);
	g_signal_connect (G_OBJECT (s_pApplyButton), "clicked", G_CALLBACK(on_click_apply), NULL);
	gtk_box_pack_end (GTK_BOX (pButtonsHBox),
		s_pApplyButton,
		FALSE,
		FALSE,
		0);
	
	GtkWidget *pSwitchButton = cairo_dock_make_switch_gui_button ();
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		pSwitchButton,
		FALSE,
		FALSE,
		0);
	
	gchar *cLink = cairo_dock_get_third_party_applets_link ();
	GtkWidget *pThirdPartyButton = gtk_link_button_new_with_label (cLink, _("More applets"));
	gtk_widget_set_tooltip_text (pThirdPartyButton, _("Get more applets online !"));
	pImage = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (pThirdPartyButton), pImage);
	g_free (cLink);
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		pThirdPartyButton,
		FALSE,
		FALSE,
		0);
	
	//\_____________ On ajoute la barre de status a la fin.
	s_pStatusBar = gtk_statusbar_new ();
	#if (GTK_MAJOR_VERSION < 3)
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (s_pStatusBar), FALSE);
	#endif
	gtk_box_pack_start (GTK_BOX (pButtonsHBox),
		s_pStatusBar,
		TRUE,
		TRUE,
		0);
	g_object_set_data (G_OBJECT (s_pMainWindow), "status-bar", s_pStatusBar);
	g_object_set_data (G_OBJECT (s_pMainWindow), "frame-width", GINT_TO_POINTER (200));
	
	gtk_window_resize (GTK_WINDOW (s_pMainWindow),
		MIN (CAIRO_DOCK_CONF_PANEL_WIDTH, g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]),
		MIN (CAIRO_DOCK_CONF_PANEL_HEIGHT, g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - (g_pMainDock && g_pMainDock->container.bIsHorizontal ? g_pMainDock->iMaxDockHeight : 0)));
	
	
	gtk_widget_show_all (s_pMainWindow);
	cairo_dock_enable_apply_button (s_pMainWindow, FALSE);
	gtk_widget_hide (s_pGroupFrame);
	gtk_widget_hide (s_pPreviewImage);
	
	g_signal_connect (G_OBJECT (s_pMainWindow),
		"destroy",
		G_CALLBACK (on_delete_main_gui),
		NULL);
	return s_pMainWindow;
}

  ////////////////
 // CATEGORIES //
////////////////

static void cairo_dock_hide_all_categories (void)
{
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		gtk_widget_hide (pCategoryWidget->pFrame);
	}
}

static void cairo_dock_show_all_categories (void)
{
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	
	//\_______________ On detruit le widget du groupe courant.
	_destroy_current_widget (TRUE);
	s_pCurrentGroup = NULL;
	
	//\_______________ On montre chaque module.
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		gtk_widget_show_all (pCategoryWidget->pFrame);
		g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), FALSE);
		g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
	}
	pCategoryWidget = &s_pCategoryWidgetTables[i];
	g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), TRUE);
	g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	
	cairo_dock_enable_apply_button (s_pMainWindow, FALSE);
	gtk_widget_hide (s_pGroupFrame);
	
	//\_______________ On actualise le titre de la fenetre.
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), _("Cairo-Dock configuration"));
	if (s_path == NULL || s_path->data != GINT_TO_POINTER (0))
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (0));

	//\_______________ On declenche le filtre.
	_trigger_current_filter ();
}

static void cairo_dock_show_one_category (int iCategory)
{
	if (s_pStatusBar)
		gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);
	
	//\_______________ On detruit le widget du groupe courant.
	if (s_pCurrentGroupWidget2 != NULL)
	{
		_destroy_current_widget (TRUE);
	}
	s_pCurrentGroup = NULL;
	
	//\_______________ On declenche le filtre.
	_trigger_current_filter ();  // do it before we hide the category frames.
	
	//\_______________ On montre chaque module de la categorie.
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		if (i != iCategory)
			gtk_widget_hide (pCategoryWidget->pFrame);
		else
			gtk_widget_show_all (pCategoryWidget->pFrame);
		
		g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), (i == iCategory));
		g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
	}
	pCategoryWidget = &s_pCategoryWidgetTables[i];
	g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), FALSE);
	g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	
	cairo_dock_enable_apply_button (s_pMainWindow, FALSE);
	gtk_widget_hide (s_pGroupFrame);
	
	//\_______________ On actualise le titre de la fenetre.
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), gettext (s_cCategoriesDescription[2*iCategory]));
	if (s_path == NULL || s_path->data != GINT_TO_POINTER (iCategory+1))
		s_path = g_slist_prepend (s_path, GINT_TO_POINTER (iCategory+1));
}

static void cairo_dock_toggle_category_button (int iCategory)
{
	if (s_pMainWindow == NULL || s_pCurrentGroup == NULL || s_pCurrentGroup->cGroupName == NULL)
		return ;
	
	CairoDockCategoryWidgetTable *pCategoryWidget;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), (i == iCategory));
		g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
			(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, on_click_category_button, NULL);
	}
	pCategoryWidget = &s_pCategoryWidgetTables[i];
	g_signal_handlers_block_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (pCategoryWidget->pCategoryButton), FALSE);
	g_signal_handlers_unblock_matched (pCategoryWidget->pCategoryButton,
		(GSignalMatchType) G_SIGNAL_MATCH_FUNC,
		0, 0, NULL, on_click_all_button, NULL);
}


static void _reload_current_module_widget (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	// ensure the module is currently displayed.
	g_return_if_fail (pModuleInstance != NULL);
	if (!s_pCurrentGroupWidget2)
		return;
	
	const gchar *cGroupName = (pModuleInstance->pModule->pVisitCard->cInternalModule ? pModuleInstance->pModule->pVisitCard->cInternalModule : pModuleInstance->pModule->pVisitCard->cModuleName);  // if the module extends a manager, it's this one that we have to show.
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cGroupName);
	g_return_if_fail (pGroupDescription != NULL);
	if (pGroupDescription != s_pCurrentGroup)
		return;
	
	// if the page is not specified, remember the current one.
	int iNotebookPage = iShowPage;
	if (iShowPage < 0 && s_pCurrentGroupWidget2 && GTK_IS_NOTEBOOK (s_pCurrentGroupWidget2->pWidget))
		iShowPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget2->pWidget));
	
	// re-build the widget
	_present_group_widget (pGroupDescription, pModuleInstance);
	
	// set the current page.
	if (s_pCurrentGroupWidget2 && iNotebookPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget2->pWidget), iNotebookPage);
	}
}

static inline gboolean _module_is_opened (CairoDockModuleInstance *pInstance)
{
	if (s_pMainWindow == NULL || s_pCurrentGroup == NULL || s_pCurrentGroup->cGroupName == NULL || pInstance == NULL)
		return FALSE;
	
	if (strcmp (pInstance->pModule->pVisitCard->cModuleName, s_pCurrentGroup->cGroupName) != 0)  // est-on est en train d'editer ce module dans le panneau de conf.
		return FALSE;
	
	if (IS_MODULE_WIDGET (s_pCurrentGroupWidget2))
	{
		return (MODULE_WIDGET (s_pCurrentGroupWidget2)->pModuleInstance == pInstance);
	}
	
	if (IS_ITEMS_WIDGET (s_pCurrentGroupWidget2))
	{
		return (ITEMS_WIDGET (s_pCurrentGroupWidget2)->pCurrentModuleInstance == pInstance);
	}
	
	if (IS_CONFIG_GROUP_WIDGET (s_pCurrentGroupWidget2))
	{
		/// TODO...
	}
	
	return FALSE;
}

static void cairo_dock_enable_apply_button (GtkWidget *pMainWindow, gboolean bEnable)
{
	if (bEnable)
	{
		gtk_widget_show (s_pApplyButton);
		gtk_widget_show (s_pOkButton);
	}
	else
	{
		gtk_widget_hide (s_pApplyButton);
		gtk_widget_hide (s_pOkButton);
	}
}
static void _present_group_widget (CairoDockGroupDescription *pGroupDescription, CairoDockModuleInstance *pModuleInstance)
{
	g_return_if_fail (pGroupDescription != NULL && s_pMainWindow != NULL);
	
	// destroy/hide the current widget
	_destroy_current_widget (TRUE);
	s_pCurrentGroup = NULL;
	cairo_dock_hide_all_categories ();
	
	// build the new  widget
	s_pCurrentGroupWidget2 = pGroupDescription->build_widget (pGroupDescription, pModuleInstance);
	g_return_if_fail (s_pCurrentGroupWidget2 != NULL);
	s_pCurrentGroup = pGroupDescription;
	
	// insert and show it
	gtk_box_pack_start (GTK_BOX (s_pGroupsVBox),
		s_pCurrentGroupWidget2->pWidget,
		TRUE,
		TRUE,
		CAIRO_DOCK_FRAME_MARGIN);
	gtk_widget_show_all (s_pCurrentGroupWidget2->pWidget);
	
	cairo_dock_enable_apply_button (s_pMainWindow, cairo_dock_widget_can_apply (s_pCurrentGroupWidget2));
	
	gtk_window_set_title (GTK_WINDOW (s_pMainWindow), pGroupDescription->cTitle);
	
	// update the current-group frame (label + check-button).
	GtkWidget *pLabel = gtk_label_new (NULL);
	gchar *cLabel = g_strdup_printf ("<span color=\"#81728C\"><u><b>%s</b></u></span>", pGroupDescription->cTitle);
	gtk_label_set_markup (GTK_LABEL (pLabel), cLabel);
	g_free (cLabel);
	gtk_frame_set_label_widget (GTK_FRAME (s_pGroupFrame), pLabel);
	gtk_widget_show_all (s_pGroupFrame);
	
	g_signal_handlers_block_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
	CairoDockModule *pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
	if (pModule != NULL && pModule->pInterface->stopModule != NULL)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s_pActivateButton), pModule->pInstancesList != NULL);
		gtk_widget_set_sensitive (s_pActivateButton, TRUE);
	}
	else
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s_pActivateButton), TRUE);
		gtk_widget_set_sensitive (s_pActivateButton, FALSE);
	}
	g_signal_handlers_unblock_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
	
	if (s_path == NULL || s_path->data != pGroupDescription)
		s_path = g_slist_prepend (s_path, pGroupDescription);
	
	// trigger the filter
	_trigger_current_filter ();
}

static void cairo_dock_present_module_instance_gui (CairoDockModuleInstance *pModuleInstance)
{
	g_return_if_fail (pModuleInstance != NULL);
	
	const gchar *cGroupName = (pModuleInstance->pModule->pVisitCard->cInternalModule ? pModuleInstance->pModule->pVisitCard->cInternalModule : pModuleInstance->pModule->pVisitCard->cModuleName);  // if the module extends a manager, it's this one that we have to show.
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cGroupName);
	g_return_if_fail (pGroupDescription != NULL);
	
	_present_group_widget (pGroupDescription, pModuleInstance);
	cairo_dock_toggle_category_button (pGroupDescription->iCategory);
}

static void cairo_dock_show_group (CairoDockGroupDescription *pGroupDescription)
{
	_present_group_widget (pGroupDescription, NULL);
}


static void cairo_dock_apply_current_filter (const gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther)
{
	cd_debug ("");
	if (s_pCurrentGroup != NULL)
	{
		GSList *pCurrentWidgetList = s_pCurrentGroupWidget2->pWidgetList;
		cairo_dock_apply_filter_on_group_widget (pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther, pCurrentWidgetList);
		if (IS_CONFIG_GROUP_WIDGET (s_pCurrentGroupWidget2))
		{
			GSList *l;
			for (l = CONFIG_GROUP_WIDGET (s_pCurrentGroupWidget2)->pExtraWidgets; l != NULL; l = l->next)
			{
				cairo_dock_apply_filter_on_group_widget (pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther, l->data);
			}
		}
	}
	else
	{
		cairo_dock_apply_filter_on_group_list (pKeyWords, bAllWords, bSearchInToolTip, bHighLightText, bHideOther, s_pGroupDescriptionList);
	}
}


static GtkWidget * show_main_gui (void)
{
	GtkWidget *pWindow = cairo_dock_build_main_ihm (g_cConfFile);
	return pWindow;
}

  /////////////
 // BACKEND //
/////////////

static void show_module_gui (const gchar *cModuleName)
{
	cairo_dock_build_main_ihm (g_cConfFile);
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cModuleName);
	g_return_if_fail (pGroupDescription != NULL);
	
	cairo_dock_show_group (pGroupDescription);
	cairo_dock_toggle_category_button (pGroupDescription->iCategory);
}

static void close_gui (void)
{
	if (s_pMainWindow != NULL)
	{
		on_click_quit (NULL, s_pMainWindow);
	}
}

static void update_module_state (const gchar *cModuleName, gboolean bActive)
{
	if (s_pMainWindow == NULL || cModuleName == NULL)
		return ;
	
	if (s_pCurrentGroup != NULL && s_pCurrentGroup->cGroupName != NULL && strcmp (cModuleName, s_pCurrentGroup->cGroupName) == 0)  // on est en train d'editer ce module dans le panneau de conf.
	{
		g_signal_handlers_block_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (s_pActivateButton), bActive);
		g_signal_handlers_unblock_by_func (s_pActivateButton, on_click_activate_current_group, NULL);
	}
	
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cModuleName);
	g_return_if_fail (pGroupDescription != NULL && pGroupDescription->pActivateButton != NULL);
	g_signal_handlers_block_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), bActive);
	g_signal_handlers_unblock_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
}

static inline gboolean _desklet_is_opened (CairoDesklet *pDesklet)
{
	if (s_pMainWindow == NULL || pDesklet == NULL)
		return FALSE;
	Icon *pIcon = pDesklet->pIcon;
	g_return_val_if_fail (pIcon != NULL, FALSE);
	
	CairoDockModuleInstance *pModuleInstance = pIcon->pModuleInstance;
	g_return_val_if_fail (pModuleInstance != NULL, FALSE);
	
	return _module_is_opened (pModuleInstance);
}
static void update_desklet_params (CairoDesklet *pDesklet)
{
	if (_desklet_is_opened (pDesklet))
	{
		cairo_dock_update_desklet_widgets (pDesklet, s_pCurrentGroupWidget2->pWidgetList);
	}
}

static void update_desklet_visibility_params (CairoDesklet *pDesklet)
{
	if (_desklet_is_opened (pDesklet))
	{
		cairo_dock_update_desklet_visibility_widgets (pDesklet, s_pCurrentGroupWidget2->pWidgetList);
	}
}

static void update_module_instance_container (CairoDockModuleInstance *pInstance, gboolean bDetached)
{
	if (_module_is_opened (pInstance))
	{
		cairo_dock_update_is_detached_widget (bDetached, s_pCurrentGroupWidget2->pWidgetList);
	}
}

static void update_modules_list (void)
{
	if (s_pMainWindow == NULL)
		return ;
	
	// On detruit la liste des boutons de chaque groupe.
	gchar *cCurrentGroupName = (s_pCurrentGroup ? g_strdup (s_pCurrentGroup->cGroupName) : NULL);
	GList *gd;
	CairoDockGroupDescription *pGroupDescription;
	for (gd = s_pGroupDescriptionList; gd != NULL; gd = gd->next)
	{
		pGroupDescription = gd->data;
		gtk_widget_destroy (pGroupDescription->pGroupHBox);
		pGroupDescription->pGroupHBox = NULL;
		_cairo_dock_free_group_description (pGroupDescription);
	}
	g_list_free (s_pGroupDescriptionList);
	s_pGroupDescriptionList = NULL;
	s_pCurrentGroup = NULL;
	
	g_slist_free (s_path);
	s_path = NULL;
		
	// on reset les tables de chaque categorie.
	int i;
	CairoDockCategoryWidgetTable *pCategoryWidget;
	for (i = 0; i < CAIRO_DOCK_NB_CATEGORY; i ++)
	{
		pCategoryWidget = &s_pCategoryWidgetTables[i];
		pCategoryWidget->iNbItemsInCurrentRow = 0;
		pCategoryWidget->iNbRows = 0;
	}
	
	// on recree chaque groupe.
	_add_main_groups_buttons ();
	
	cairo_dock_foreach_module_in_alphabetical_order ((GCompareFunc) _cairo_dock_add_one_module_widget, NULL);
	
	// on retrouve le groupe courant.
	if (cCurrentGroupName != NULL)
	{
		for (gd = s_pGroupDescriptionList; gd != NULL; gd = gd->next)
		{
			pGroupDescription = gd->data;
			if (pGroupDescription->cGroupName && strcmp (cCurrentGroupName, pGroupDescription->cGroupName) == 0)
				s_pCurrentGroup = pGroupDescription;
		}
		g_free (cCurrentGroupName);
	}
	
	gtk_widget_show_all (s_pMainWindow);
}

static void update_shortkeys (void)
{
	if (s_pMainWindow == NULL
	|| s_pCurrentGroup == NULL
	|| s_pCurrentGroup->cGroupName == NULL
	|| strcmp (s_pCurrentGroup->cGroupName, "Shortkeys") != 0)  // the Shortkeys widget is not currently displayed => nothing to do.
		return ;
	
	cairo_dock_shortkeys_widget_reload (SHORKEYS_WIDGET (s_pCurrentGroupWidget2));
}

static GtkWidget *show_gui (Icon *pIcon, CairoContainer *pContainer, CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	cairo_dock_build_main_ihm (g_cConfFile);
	
	CairoDockGroupDescription *pGroupDescription;
	if (pModuleInstance != NULL || (CAIRO_DOCK_IS_APPLET (pIcon)))  // for an applet, prefer the module widget
	{
		if (! pModuleInstance)
			pModuleInstance = pIcon->pModuleInstance;
		const gchar *cGroupName = (pModuleInstance->pModule->pVisitCard->cInternalModule ? pModuleInstance->pModule->pVisitCard->cInternalModule : pModuleInstance->pModule->pVisitCard->cModuleName);  // if the module extends a manager, it's this one that we have to show.
		
		pGroupDescription = cairo_dock_find_module_description (cGroupName);
		g_return_val_if_fail (pGroupDescription != NULL, s_pMainWindow);
		
		_present_group_widget (pGroupDescription, pModuleInstance);
	}
	else
	{
		pGroupDescription = cairo_dock_find_module_description ("Items");
		g_return_val_if_fail (pGroupDescription != NULL, s_pMainWindow);
		
		cairo_dock_show_group (pGroupDescription);
		
		g_return_val_if_fail (s_pCurrentGroupWidget2 != NULL, s_pMainWindow);
		cairo_dock_items_widget_select_item (ITEMS_WIDGET (s_pCurrentGroupWidget2), pIcon, pContainer, pModuleInstance, iShowPage);
	}
	cairo_dock_toggle_category_button (pGroupDescription->iCategory);
	return s_pMainWindow;
}

static void reload_items (void)
{
	if (s_pMainWindow == NULL)
		return;
	
	if (IS_ITEMS_WIDGET (s_pCurrentGroupWidget2))  // currently displayed widget is the "items" one.
	{
		cairo_dock_items_widget_reload (ITEMS_WIDGET (s_pCurrentGroupWidget2));
	}
}

static void reload (void)
{
	if (s_pMainWindow == NULL)
		return;
	
	// since this function can only be triggered by a new theme, we don't need to care the current widget (it's the 'theme' widget, it will reload itself), only the state of the modules.
	CairoDockGroupDescription *pGroupDescription;
	CairoDockModule *pModule;
	GList *gd;
	for (gd = s_pGroupDescriptionList; gd != NULL; gd = gd->next)
	{
		pGroupDescription = gd->data;
		pModule = cairo_dock_find_module_from_name (pGroupDescription->cGroupName);
		if (pModule != NULL)
		{
			g_signal_handlers_block_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pGroupDescription->pActivateButton), pModule->pInstancesList != NULL);
			g_signal_handlers_unblock_by_func (pGroupDescription->pActivateButton, on_click_activate_given_group, pGroupDescription);
		}
	}
}


////////////////////
/// CORE BACKEND ///
////////////////////

static void set_status_message_on_gui (const gchar *cMessage)
{
	if (s_pStatusBar == NULL)
		return;
	
	gtk_statusbar_pop (GTK_STATUSBAR (s_pStatusBar), 0);  // clear any previous message, underflow is allowed.
	gtk_statusbar_push (GTK_STATUSBAR (s_pStatusBar), 0, cMessage);
}

static void show_module_instance_gui (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	g_return_if_fail (pModuleInstance != NULL);
	
	// build the widget
	cairo_dock_build_main_ihm (g_cConfFile);
	
	const gchar *cGroupName = (pModuleInstance->pModule->pVisitCard->cInternalModule ? pModuleInstance->pModule->pVisitCard->cInternalModule : pModuleInstance->pModule->pVisitCard->cModuleName);  // if the module extends a manager, it's this one that we have to show.
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cGroupName);
	g_return_if_fail (pGroupDescription != NULL);
	
	_present_group_widget (pGroupDescription, pModuleInstance);
	
	cairo_dock_toggle_category_button (pGroupDescription->iCategory);  // on active la categorie du module.
	
	// set the current page.
	if (s_pCurrentGroupWidget2 && iShowPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget2->pWidget), iShowPage);
	}
}

static void reload_current_widget (CairoDockModuleInstance *pModuleInstance, int iShowPage)
{
	// ensure the module is currently displayed.
	g_return_if_fail (pModuleInstance != NULL);
	if (!s_pMainWindow || !s_pCurrentGroup)
		return;
	
	const gchar *cGroupName = (pModuleInstance->pModule->pVisitCard->cInternalModule ? pModuleInstance->pModule->pVisitCard->cInternalModule : pModuleInstance->pModule->pVisitCard->cModuleName);  // if the module extends a manager, it's this one that we have to show.
	CairoDockGroupDescription *pGroupDescription = cairo_dock_find_module_description (cGroupName);
	g_return_if_fail (pGroupDescription != NULL);
	if (pGroupDescription != s_pCurrentGroup)
		return;
	
	// if the page is not specified, remember the current one.
	int iNotebookPage = iShowPage;
	if (iShowPage < 0 && s_pCurrentGroupWidget2 && GTK_IS_NOTEBOOK (s_pCurrentGroupWidget2->pWidget))
		iShowPage = gtk_notebook_get_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget2->pWidget));
	
	// re-build the widget
	_present_group_widget (pGroupDescription, pModuleInstance);
	
	// set the current page.
	if (s_pCurrentGroupWidget2 && iNotebookPage != -1)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (s_pCurrentGroupWidget2->pWidget), iNotebookPage);
	};
}

static CairoDockGroupKeyWidget *get_widget_from_name (CairoDockModuleInstance *pInstance, const gchar *cGroupName, const gchar *cKeyName)
{
	if (!_module_is_opened (pInstance))
		return NULL;
	
	if (IS_CONFIG_GROUP_WIDGET (s_pCurrentGroupWidget2))
	{
		/// TODO...
	}  // other widgets are straightforward.
	
	CairoDockGroupKeyWidget *pGroupKeyWidget = cairo_dock_gui_find_group_key_widget_in_list (s_pCurrentGroupWidget2->pWidgetList, cGroupName, cKeyName);
	return pGroupKeyWidget;
}


void cairo_dock_register_main_gui_backend (void)
{
	CairoDockMainGuiBackend *pBackend = g_new0 (CairoDockMainGuiBackend, 1);
	
	pBackend->show_main_gui 					= show_main_gui;
	pBackend->show_module_gui 					= show_module_gui;
	pBackend->close_gui 						= close_gui;
	pBackend->update_module_state 				= update_module_state;
	pBackend->update_desklet_params 			= update_desklet_params;
	pBackend->update_desklet_visibility_params 	= update_desklet_visibility_params;
	pBackend->update_module_instance_container 	= update_module_instance_container;
	pBackend->update_modules_list 				= update_modules_list;
	pBackend->update_shortkeys 					= update_shortkeys;
	pBackend->show_gui 							= show_gui;
	pBackend->reload_items 						= reload_items;
	pBackend->reload 							= reload;
	pBackend->cDisplayedName 					= _("Simple Mode");
	pBackend->cTooltip 							= NULL;
	
	cairo_dock_register_config_gui_backend (pBackend);
	
	CairoDockGuiBackend *pConfigBackend = g_new0 (CairoDockGuiBackend, 1);
	
	pConfigBackend->set_status_message_on_gui 	= set_status_message_on_gui;
	pConfigBackend->reload_current_widget 		= reload_current_widget;
	pConfigBackend->show_module_instance_gui 	= show_module_instance_gui;
	pConfigBackend->get_widget_from_name 		= get_widget_from_name;
	
	cairo_dock_register_gui_backend (pConfigBackend);
}
