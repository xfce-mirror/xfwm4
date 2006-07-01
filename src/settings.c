/*      $Id$
 
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.
 
        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
 
        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2006 Olivier Fourdan
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <pango/pango.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfce4mcs/mcs-client.h>

#include "screen.h"
#include "hints.h"
#include "parserc.h"
#include "client.h"
#include "focus.h"
#include "workspaces.h"
#include "compositor.h"
#include "ui_style.h"

#define CHANNEL1                "xfwm4"
#define CHANNEL2                "margins"
#define CHANNEL3                "workspaces"
#define CHANNEL4                "xfwm4_keys"
#define CHANNEL5                "wmtweaks"

#define DEFAULT_KEYTHEME        "Default"
#define KEYTHEMERC              "keythemerc"
#define THEMERC                 "themerc"

#define TOINT(x)                (x ? atoi(x) : 0)
#define XPM_COLOR_SYMBOL_SIZE   22

/* Forward static decls. */

static void              check_for_grabs      (ScreenInfo *);
static void              set_settings_margin  (ScreenInfo *, 
                                               int , 
                                               int);
static void              notify_cb            (const char *,
                                               const char *,
                                               McsAction, 
                                               McsSetting *,
                                               void *);
static GdkFilterReturn   client_event_filter  (GdkXEvent *,
                                               GdkEvent *, 
                                               gpointer);
static void              watch_cb             (Window, 
                                               Bool, 
                                               long, 
                                               void *);
static void              loadRcData           (ScreenInfo *, 
                                               Settings *);
static void              loadMcsData          (ScreenInfo *,
                                               Settings *);
static void              loadTheme            (ScreenInfo *, 
                                               Settings *);
static gboolean          loadKeyBindings      (ScreenInfo *, 
                                               Settings *);
static void              unloadTheme          (ScreenInfo *);
static void              unloadSettings       (ScreenInfo *);
static gboolean          reloadScreenSettings (ScreenInfo *, 
                                               int);

static void
check_for_grabs (ScreenInfo *screen_info)
{
    if ((screen_info->params->raise_on_click) || (screen_info->params->click_to_focus))
    {
        clientGrabMouseButtonForAll (screen_info);
    }
    else if (!(screen_info->params->raise_on_click) && !(screen_info->params->click_to_focus))
    {
        clientUngrabMouseButtonForAll (screen_info);
    }
}

static void
set_settings_margin (ScreenInfo *screen_info, int idx, int value)
{
    int val;

    switch (idx)
    {
        case LEFT:
        case RIGHT:
            if (value < 0)
            {
                val = 0;
            }
            else if (value > screen_info->width / 4)
            {
                val = screen_info->width / 4;
            }
            else
            {
                val = value;
            }
            screen_info->params->xfwm_margins[idx] = val;
            break;
        case TOP:
        case BOTTOM:
            if (value < 0)
            {
                val = 0;
            }
            else if (value > screen_info->height / 4)
            {
                val = screen_info->height / 4;
            }
            else
            {
                val = value;
            }
            screen_info->params->xfwm_margins[idx] = val;
            break;
        default:
            break;
    }
}

static void
set_easy_click (ScreenInfo *screen_info, char *modifier)
{
    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (modifier != NULL);

    if (!g_ascii_strcasecmp (modifier, "true"))
    {
        screen_info->params->easy_click = AltMask;
    }
    else
    {
        screen_info->params->easy_click = getModifierMap (modifier);
    }
}

static void
notify_cb (const char *name, const char *channel_name, McsAction action, McsSetting * setting, void *data)
{
    ScreenInfo *screen_info;

    screen_info = (ScreenInfo *) data;
    g_return_if_fail (screen_info != NULL);

    if (!g_ascii_strcasecmp (CHANNEL1, channel_name))
    {
        switch (action)
        {
            case MCS_ACTION_NEW:
                /* The following is to reduce initial startup time and reload */
                if (!screen_info->mcs_initted)
                {
                    return;
                }
            case MCS_ACTION_CHANGED:
                if (setting->type == MCS_TYPE_INT)
                {
                    if (!strcmp (name, "Xfwm/ClickToFocus"))
                    {
                        screen_info->params->click_to_focus = setting->data.v_int;
                        check_for_grabs (screen_info);
                    }
                    else if (!strcmp (name, "Xfwm/FocusNewWindow"))
                    {
                        screen_info->params->focus_new = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/FocusRaise"))
                    {
                        screen_info->params->raise_on_focus = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/RaiseDelay"))
                    {
                        screen_info->params->raise_delay = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/RaiseOnClick"))
                    {
                        screen_info->params->raise_on_click = setting->data.v_int;
                        check_for_grabs (screen_info);
                    }
                    else if (!strcmp (name, "Xfwm/SnapToBorder"))
                    {
                        screen_info->params->snap_to_border = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/SnapToWindows"))
                    {
                        screen_info->params->snap_to_windows = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/SnapWidth"))
                    {
                        screen_info->params->snap_width = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/WrapWorkspaces"))
                    {
                        screen_info->params->wrap_workspaces = setting->data.v_int;
                        placeSidewalks (screen_info, screen_info->params->wrap_workspaces);
                    }
                    else if (!strcmp (name, "Xfwm/WrapWindows"))
                    {
                        screen_info->params->wrap_windows = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/WrapResistance"))
                    {
                        screen_info->params->wrap_resistance = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/BoxMove"))
                    {
                        screen_info->params->box_move = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/BoxResize"))
                    {
                        screen_info->params->box_resize = setting->data.v_int;
                    }
                }
                else if (setting->type == MCS_TYPE_STRING)
                {
                    if (!strcmp (name, "Xfwm/DblClickAction"))
                    {
                        reloadScreenSettings (screen_info, NO_UPDATE_FLAG);
                    }
                    else if (!strcmp (name, "Xfwm/ThemeName"))
                    {
                        reloadScreenSettings (screen_info, UPDATE_GRAVITY | UPDATE_CACHE);
                    }
                    else if (!strcmp (name, "Xfwm/ButtonLayout"))
                    {
                        reloadScreenSettings (screen_info, UPDATE_FRAME | UPDATE_CACHE);
                    }
                    if (!strcmp (name, "Xfwm/TitleAlign"))
                    {
                        reloadScreenSettings (screen_info, UPDATE_FRAME | UPDATE_CACHE);
                    }
                    if (!strcmp (name, "Xfwm/TitleFont"))
                    {
                        reloadScreenSettings (screen_info, UPDATE_FRAME | UPDATE_CACHE);
                    }
                }
                break;
            case MCS_ACTION_DELETED:
            default:
                break;
        }
    }
    else if (!g_ascii_strcasecmp (CHANNEL2, channel_name))
    {
        switch (action)
        {
            case MCS_ACTION_NEW:
                /* The following is to reduce initial startup time and reloads */
                if (!screen_info->mcs_initted)
                {
                    return;
                }
            case MCS_ACTION_CHANGED:
                if (setting->type == MCS_TYPE_INT)
                {
                    if (!strcmp (name, "Xfwm/LeftMargin"))
                    {
                        set_settings_margin (screen_info, LEFT, setting->data.v_int);
                    }
                    else if (!strcmp (name, "Xfwm/RightMargin"))
                    {
                        set_settings_margin (screen_info, RIGHT, setting->data.v_int);
                    }
                    else if (!strcmp (name, "Xfwm/BottomMargin"))
                    {
                        set_settings_margin (screen_info, BOTTOM, setting->data.v_int);
                    }
                    else if (!strcmp (name, "Xfwm/TopMargin"))
                    {
                        set_settings_margin (screen_info, TOP, setting->data.v_int);
                    }
                }
                break;
            case MCS_ACTION_DELETED:
            default:
                break;
        }
    }
    else if (!g_ascii_strcasecmp (CHANNEL3, channel_name))
    {
        switch (action)
        {
            case MCS_ACTION_NEW:
                /* The following is to reduce initial startup time and reloads */
                if (!screen_info->mcs_initted)
                {
                    return;
                }
            case MCS_ACTION_CHANGED:
                if (setting->type == MCS_TYPE_INT)
                {
                    if (!strcmp (name, "Xfwm/WorkspaceCount"))
                    {
                        workspaceSetCount(screen_info, setting->data.v_int);
                    }
                }
                break;
            case MCS_ACTION_DELETED:
            default:
                break;
        }
    }
    else if (!g_ascii_strcasecmp (CHANNEL4, channel_name))
    {
        switch (action)
        {
            case MCS_ACTION_NEW:
                /* The following is to reduce initial startup time and reloads */
                if (!screen_info->mcs_initted)
                {
                    return;
                }
            case MCS_ACTION_CHANGED:
                if (setting->type == MCS_TYPE_STRING)
                {
                    if (!strcmp (name, "Xfwm/KeyThemeName"))
                    {
                        reloadScreenSettings (screen_info, UPDATE_KEY_GRABS);
                    }
                }
                break;
            case MCS_ACTION_DELETED:
            default:
                break;
        }
    }
    else if (!g_ascii_strcasecmp (CHANNEL5, channel_name))
    {
        switch (action)
        {
            case MCS_ACTION_NEW:
                /* The following is to reduce initial startup time and reloads */
                if (!screen_info->mcs_initted)
                {
                    return;
                }
            case MCS_ACTION_CHANGED:
                if (setting->type == MCS_TYPE_INT)
                {
                    if (!strcmp (name, "Xfwm/CycleMinimum"))
                    {
                        screen_info->params->cycle_minimum = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/CycleHidden"))
                    {
                        screen_info->params->cycle_hidden = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/CycleWorkspaces"))
                    {
                        screen_info->params->cycle_workspaces = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/FocusHint"))
                    {
                        screen_info->params->focus_hint = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/FrameOpacity"))
                    {
                        screen_info->params->frame_opacity = setting->data.v_int;
                        reloadScreenSettings (screen_info, UPDATE_FRAME);
                    }
                    else if (!strcmp (name, "Xfwm/InactiveOpacity"))
                    {
                        screen_info->params->inactive_opacity = setting->data.v_int;
                        reloadScreenSettings (screen_info, UPDATE_FRAME);
                        clientUpdateOpacity (screen_info, clientGetFocus ());
                    }
                    else if (!strcmp (name, "Xfwm/MoveOpacity"))
                    {
                        screen_info->params->move_opacity = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/ResizeOpacity"))
                    {
                        screen_info->params->resize_opacity = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/PopupOpacity"))
                    {
                        screen_info->params->popup_opacity = setting->data.v_int;
                        reloadScreenSettings (screen_info, UPDATE_FRAME);
                    }
                    else if (!strcmp (name, "Xfwm/PlacementRatio"))
                    {
                        screen_info->params->placement_ratio = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/ShowFrameShadow"))
                    {
                        screen_info->params->show_frame_shadow = setting->data.v_int;
                        reloadScreenSettings (screen_info, UPDATE_FRAME);
                    }
                    else if (!strcmp (name, "Xfwm/ShowPopupShadow"))
                    {
                        screen_info->params->show_popup_shadow = setting->data.v_int;
                        reloadScreenSettings (screen_info, UPDATE_FRAME);
                    }
                    else if (!strcmp (name, "Xfwm/SnapResist"))
                    {
                        screen_info->params->snap_resist = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/PreventFocusStealing"))
                    {
                        screen_info->params->prevent_focus_stealing = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/RaiseWithAnyButton"))
                    {
                        screen_info->params->raise_with_any_button = setting->data.v_int;
                        check_for_grabs (screen_info);
                    }
                    else if (!strcmp (name, "Xfwm/RestoreOnMove"))
                    {
                        screen_info->params->restore_on_move = setting->data.v_int;
                        check_for_grabs (screen_info);
                    }
                    else if (!strcmp (name, "Xfwm/ScrollWorkspaces"))
                    {
                        screen_info->params->scroll_workspaces = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/ToggleWorkspaces"))
                    {
                        screen_info->params->toggle_workspaces = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/WrapLayout"))
                    {
                        screen_info->params->wrap_layout = setting->data.v_int;
                    }
                    else if (!strcmp (name, "Xfwm/WrapCycle"))
                    {
                        screen_info->params->wrap_cycle = setting->data.v_int;
                    }
                }
                else if (setting->type == MCS_TYPE_STRING)
                {
                    if (!strcmp (name, "Xfwm/EasyClick"))
                    {
                        reloadScreenSettings (screen_info, UPDATE_BUTTON_GRABS);
                    }
                }
                break;
            case MCS_ACTION_DELETED:
            default:
                break;
        }
    }
}

static GdkFilterReturn
client_event_filter (GdkXEvent * xevent, GdkEvent * event, gpointer data)
{
    ScreenInfo *screen_info;

    screen_info = (ScreenInfo *) data;
    g_return_val_if_fail (screen_info != NULL, GDK_FILTER_CONTINUE);

    if (mcs_client_process_event (screen_info->mcs_client, (XEvent *) xevent))
    {
        return GDK_FILTER_REMOVE;
    }
    else
    {
        return GDK_FILTER_CONTINUE;
    }
}

static void
watch_cb (Window window, Bool is_start, long mask, void *cb_data)
{
    GdkWindow *gdkwin;

    gdkwin = gdk_window_lookup (window);

    if (is_start)
    {
        if (!gdkwin)
        {
            gdkwin = gdk_window_foreign_new (window);
        }
        else
        {
            g_object_ref (gdkwin);
        }
        gdk_window_add_filter (gdkwin, client_event_filter, cb_data);
    }
    else
    {
        g_assert (gdkwin);
        gdk_window_remove_filter (gdkwin, client_event_filter, cb_data);
        g_object_unref (gdkwin);
    }
}

static void
loadRcData (ScreenInfo *screen_info, Settings *rc)
{
    gchar *homedir;
    gchar *keythemevalue;
    gchar *keytheme;
    gchar *system_keytheme;

    if (!parseRc ("defaults", PACKAGE_DATADIR, rc))
    {
        g_warning ("Missing defaults file");
        exit (1);
    }
    keythemevalue = getValue ("keytheme", rc);
    if (keythemevalue)
    {
        system_keytheme = getSystemThemeDir ();
        parseRc (KEYTHEMERC, system_keytheme, rc);
        
        keytheme = getThemeDir (keythemevalue, KEYTHEMERC);
        if (keytheme)
        {
            /* If there is a custom key theme, merge it with system defaults */
            parseRc (KEYTHEMERC, keytheme, rc);
            g_free (keytheme);
        }
        g_free (system_keytheme);
    }
    homedir = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, 
                                           "xfce4" G_DIR_SEPARATOR_S "xfwm4",
                                           FALSE);
    parseRc ("xfwm4rc", homedir, rc);
    g_free (homedir);
}

static void
loadMcsData (ScreenInfo *screen_info, Settings *rc)
{
    McsSetting *setting;

    if (screen_info->mcs_client)
    {
        /* "Regular" channel */
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ClickToFocus", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("click_to_focus", setting->data.v_int, rc);
            check_for_grabs (screen_info);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/FocusNewWindow", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("focus_new", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/FocusRaise", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("raise_on_focus", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/RaiseDelay", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("raise_delay", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/RaiseOnClick", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("raise_on_click", setting->data.v_int, rc);
            check_for_grabs (screen_info);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/SnapToBorder", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("snap_to_border", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/SnapToWindows", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("snap_to_windows", setting->data.v_int,
                rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/SnapWidth", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("snap_width", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/WrapWorkspaces", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("wrap_workspaces", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/WrapWindows", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("wrap_windows", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/WrapResistance", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("wrap_resistance", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/BoxMove", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("box_move", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/BoxResize", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("box_resize", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/DblClickAction", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setValue ("double_click_action", setting->data.v_string, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ThemeName", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setValue ("theme", setting->data.v_string, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ButtonLayout", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setValue ("button_layout", setting->data.v_string, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/TitleAlign", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setValue ("title_alignment", setting->data.v_string, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/TitleFont", CHANNEL1,
                &setting) == MCS_SUCCESS)
        {
            setValue ("title_font", setting->data.v_string, rc);
            mcs_setting_free (setting);
        }

        /* Margins channel */
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/LeftMargin", CHANNEL2,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("margin_left", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/RightMargin", CHANNEL2,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("margin_right", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/BottomMargin", CHANNEL2,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("margin_bottom", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/TopMargin", CHANNEL2,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("margin_top", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }

        /* Workspaces channel */
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/WorkspaceCount", CHANNEL3,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("workspace_count", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }

        /* Keyboard theme channel */
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/KeyThemeName", CHANNEL4,
                &setting) == MCS_SUCCESS)
        {
            setValue ("keytheme", setting->data.v_string, rc);
            mcs_setting_free (setting);
        }

        /* Tweaks channel */
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/CycleMinimum", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("cycle_minimum", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/CycleHidden", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("cycle_hidden", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/CycleWorkspaces", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("cycle_workspaces", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/EasyClick", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            if (setting->type == MCS_TYPE_STRING)
            {
                setValue ("easy_click", setting->data.v_string, rc);
            }
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/FocusHint", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("focus_hint", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/FrameOpacity", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("frame_opacity", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/PlacementRatio", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("placement_ratio", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/InactiveOpacity", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("inactive_opacity", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/MoveOpacity", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("move_opacity", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ResizeOpacity", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("resize_opacity", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/PopupOpacity", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setIntValueFromInt ("popup_opacity", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ShowFrameShadow", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("show_frame_shadow", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ShowPopupShadow", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("show_popup_shadow", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/SnapResist", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("snap_resist", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/PreventFocusStealing", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("prevent_focus_stealing", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/RaiseWithAnyButton", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("raise_with_any_button", setting->data.v_int, rc);
            check_for_grabs (screen_info);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/RestoreOnMove", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("restore_on_move", setting->data.v_int, rc);
            check_for_grabs (screen_info);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ScrollWorkspaces", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("scroll_workspaces", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/ToggleWorkspaces", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("toggle_workspaces", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/WrapLayout", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("wrap_layout", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
        if (mcs_client_get_setting (screen_info->mcs_client, "Xfwm/WrapCycle", CHANNEL5,
                &setting) == MCS_SUCCESS)
        {
            setBooleanValueFromInt ("wrap_cycle", setting->data.v_int, rc);
            mcs_setting_free (setting);
        }
    }
}

/* Simple helper function to avoid copy/paste of code */
static void
setXfwmColor (ScreenInfo *screen_info, XfwmColor *color, Settings *rc, int id, const gchar * name, const gchar * state)
{
    if (color->allocated)
    {
        gdk_colormap_free_colors (gdk_screen_get_rgb_colormap (screen_info->gscr), &color->col, 1);
        color->allocated = FALSE;
    }

    if (gdk_color_parse (rc[id].value, &color->col))
    {
        if (gdk_colormap_alloc_color (gdk_screen_get_rgb_colormap (screen_info->gscr),
                                      &color->col, FALSE, FALSE))
        {
            color->allocated = TRUE;
            if (color->gc)
            {
                g_object_unref (G_OBJECT (color->gc));
            }
            color->gc = gdk_gc_new (myScreenGetGdkWindow (screen_info));
            gdk_gc_copy (color->gc, getUIStyle_gc (myScreenGetGtkWidget (screen_info), name, state));
            gdk_gc_set_foreground (color->gc, &color->col);
        }
        else
        {
            gdk_beep ();
            g_message (_("%s: Cannot allocate color %s\n"), g_get_prgname (), rc[id].value);
        }
    }
    else
    {
        gdk_beep ();
        g_message (_("%s: Cannot parse color %s\n"), g_get_prgname (), rc[id].value);
    }
}

static void
loadTheme (ScreenInfo *screen_info, Settings *rc)
{
    GValue tmp_val = { 0, };
    DisplayInfo *display_info;
    xfwmColorSymbol colsym[ XPM_COLOR_SYMBOL_SIZE + 1 ];
    GtkWidget *widget;
    gchar *theme;
    gchar *font;
    PangoFontDescription *desc;
    guint i;

    widget = myScreenGetGtkWidget (screen_info);
    display_info = screen_info->display_info;

    desc = NULL;

    rc[0].value  = getUIStyle (widget, "fg",    "selected");
    rc[1].value  = getUIStyle (widget, "fg",    "insensitive");
    rc[2].value  = getUIStyle (widget, "dark",  "selected");
    rc[3].value  = getUIStyle (widget, "dark",  "insensitive");
    rc[4].value  = getUIStyle (widget, "fg",    "normal");
    rc[5].value  = getUIStyle (widget, "fg",    "normal");
    rc[6].value  = getUIStyle (widget, "bg",    "selected");
    rc[7].value  = getUIStyle (widget, "light", "selected");
    rc[8].value  = getUIStyle (widget, "dark",  "selected");
    rc[9].value  = getUIStyle (widget, "mid",   "selected");
    rc[10].value = getUIStyle (widget, "bg",    "normal");
    rc[11].value = getUIStyle (widget, "light", "normal");
    rc[12].value = getUIStyle (widget, "dark",  "normal");
    rc[13].value = getUIStyle (widget, "mid",   "normal");
    rc[14].value = getUIStyle (widget, "bg",    "insensitive");
    rc[15].value = getUIStyle (widget, "light", "insensitive");
    rc[16].value = getUIStyle (widget, "dark",  "insensitive");
    rc[17].value = getUIStyle (widget, "mid",   "insensitive");
    rc[18].value = getUIStyle (widget, "bg",    "normal");
    rc[19].value = getUIStyle (widget, "light", "normal");
    rc[20].value = getUIStyle (widget, "dark",  "normal");
    rc[21].value = getUIStyle (widget, "mid",   "normal");


    theme = getThemeDir (getValue ("theme", rc), THEMERC);
    parseRc (THEMERC, theme, rc);

    screen_info->params->shadow_delta_x = 
        - (TOINT (getValue ("shadow_delta_x", rc)));
    screen_info->params->shadow_delta_y = 
        - (TOINT (getValue ("shadow_delta_y", rc)));
    screen_info->params->shadow_delta_width = 
        - (TOINT (getValue ("shadow_delta_width", rc)));
    screen_info->params->shadow_delta_height = 
        - (TOINT (getValue ("shadow_delta_height", rc)));

    for (i = 0; i < XPM_COLOR_SYMBOL_SIZE; i++)
    {
        colsym[i].name = rc[i].option;
        colsym[i].value = rc[i].value;
    }
    colsym[XPM_COLOR_SYMBOL_SIZE].name = NULL;
    colsym[XPM_COLOR_SYMBOL_SIZE].value = NULL;

    display_info->dbl_click_time = abs (TOINT (getValue ("dbl_click_time", rc)));
    g_value_init (&tmp_val, G_TYPE_INT);
    if (gdk_setting_get ("gtk-double-click-time", &tmp_val))
    {
        display_info->dbl_click_time = abs (g_value_get_int (&tmp_val));
    }

    font = getValue ("title_font", rc);
    if (font && strlen (font))
    {
        desc = pango_font_description_from_string (font);
        if (desc)
        {
            gtk_widget_modify_font (widget, desc);
            pango_font_description_free (desc);
        }
    }
 
    setXfwmColor (screen_info, &screen_info->title_colors[ACTIVE], rc, 0, "fg", "selected");
    setXfwmColor (screen_info, &screen_info->title_colors[INACTIVE], rc, 1, "fg", "insensitive");
    setXfwmColor (screen_info, &screen_info->title_shadow_colors[ACTIVE], rc, 2, "dark", "selected");
    setXfwmColor (screen_info, &screen_info->title_shadow_colors[INACTIVE], rc, 3, "dark", "insensitive");
    
    if (screen_info->black_gc)
    {
        g_object_unref (G_OBJECT (screen_info->black_gc));
    }
    screen_info->black_gc = widget->style->black_gc;
    g_object_ref (G_OBJECT (widget->style->black_gc));

    if (screen_info->white_gc)
    {
        g_object_unref (G_OBJECT (screen_info->white_gc));
    }
    screen_info->white_gc = widget->style->white_gc;
    g_object_ref (G_OBJECT (widget->style->white_gc));

    xfwmPixmapLoad (screen_info, &screen_info->sides[SIDE_LEFT][ACTIVE], theme,
        "left-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->sides[SIDE_LEFT][INACTIVE], theme,
        "left-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->sides[SIDE_RIGHT][ACTIVE], theme,
        "right-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->sides[SIDE_RIGHT][INACTIVE], theme,
        "right-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->sides[SIDE_BOTTOM][ACTIVE], theme,
        "bottom-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->sides[SIDE_BOTTOM][INACTIVE], theme,
        "bottom-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_TOP_LEFT][ACTIVE], theme,
        "top-left-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_TOP_LEFT][INACTIVE], theme,
        "top-left-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_TOP_RIGHT][ACTIVE], theme,
        "top-right-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_TOP_RIGHT][INACTIVE], theme,
        "top-right-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_BOTTOM_LEFT][ACTIVE], theme,
        "bottom-left-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_BOTTOM_LEFT][INACTIVE], theme,
        "bottom-left-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_BOTTOM_RIGHT][ACTIVE], theme,
        "bottom-right-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->corners[CORNER_BOTTOM_RIGHT][INACTIVE], theme,
        "bottom-right-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[HIDE_BUTTON][ACTIVE], theme,
        "hide-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[HIDE_BUTTON][INACTIVE], theme,
        "hide-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[HIDE_BUTTON][PRESSED], theme,
        "hide-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[CLOSE_BUTTON][ACTIVE], theme,
        "close-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[CLOSE_BUTTON][INACTIVE], theme,
        "close-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[CLOSE_BUTTON][PRESSED], theme,
        "close-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MAXIMIZE_BUTTON][ACTIVE], theme,
        "maximize-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MAXIMIZE_BUTTON][INACTIVE], theme,
        "maximize-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MAXIMIZE_BUTTON][PRESSED], theme,
        "maximize-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[SHADE_BUTTON][ACTIVE], theme,
        "shade-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[SHADE_BUTTON][INACTIVE], theme,
        "shade-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[SHADE_BUTTON][PRESSED], theme,
        "shade-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[STICK_BUTTON][ACTIVE], theme,
        "stick-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[STICK_BUTTON][INACTIVE], theme,
        "stick-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[STICK_BUTTON][PRESSED], theme,
        "stick-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MENU_BUTTON][ACTIVE], theme,
        "menu-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MENU_BUTTON][INACTIVE], theme,
        "menu-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MENU_BUTTON][PRESSED], theme,
        "menu-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[SHADE_BUTTON][T_ACTIVE], theme,
        "shade-toggled-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[SHADE_BUTTON][T_INACTIVE], theme,
        "shade-toggled-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[SHADE_BUTTON][T_PRESSED], theme,
        "shade-toggled-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[STICK_BUTTON][T_ACTIVE], theme,
        "stick-toggled-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[STICK_BUTTON][T_INACTIVE], theme,
        "stick-toggled-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[STICK_BUTTON][T_PRESSED], theme,
        "stick-toggled-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MAXIMIZE_BUTTON][T_ACTIVE], theme,
        "maximize-toggled-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MAXIMIZE_BUTTON][T_INACTIVE], theme,
        "maximize-toggled-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->buttons[MAXIMIZE_BUTTON][T_PRESSED], theme,
        "maximize-toggled-pressed", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_1][ACTIVE], theme,
        "title-1-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_1][INACTIVE], theme,
        "title-1-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_2][ACTIVE], theme,
        "title-2-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_2][INACTIVE], theme,
        "title-2-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_3][ACTIVE], theme,
        "title-3-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_3][INACTIVE], theme,
        "title-3-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_4][ACTIVE], theme,
        "title-4-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_4][INACTIVE], theme,
        "title-4-inactive", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_5][ACTIVE], theme,
        "title-5-active", colsym);
    xfwmPixmapLoad (screen_info, &screen_info->title[TITLE_5][INACTIVE], theme,
        "title-5-inactive", colsym);

    screen_info->box_gc = createGC (screen_info, "#FFFFFF", GXxor, NULL, 2, TRUE);

    if (!g_ascii_strcasecmp ("left", getValue ("title_alignment", rc)))
    {
        screen_info->params->title_alignment = ALIGN_LEFT;
    }
    else if (!g_ascii_strcasecmp ("right", getValue ("title_alignment", rc)))
    {
        screen_info->params->title_alignment = ALIGN_RIGHT;
    }
    else
    {
        screen_info->params->title_alignment = ALIGN_CENTER;
    }

    screen_info->params->full_width_title =
        !g_ascii_strcasecmp ("true", getValue ("full_width_title", rc));
    screen_info->params->title_shadow[ACTIVE] =
        !g_ascii_strcasecmp ("true", getValue ("title_shadow_active", rc));
    screen_info->params->title_shadow[INACTIVE] =
        !g_ascii_strcasecmp ("true", getValue ("title_shadow_inactive", rc));

    strncpy (screen_info->params->button_layout, getValue ("button_layout", rc), 7);
    screen_info->params->button_spacing = TOINT (getValue ("button_spacing", rc));
    screen_info->params->button_offset = TOINT (getValue ("button_offset", rc));
    screen_info->params->title_vertical_offset_active =
        TOINT (getValue ("title_vertical_offset_active", rc));
    screen_info->params->title_vertical_offset_inactive =
        TOINT (getValue ("title_vertical_offset_inactive", rc));
    screen_info->params->title_horizontal_offset =
        TOINT (getValue ("title_horizontal_offset", rc));

    g_free (theme);
}

static gboolean
loadKeyBindings (ScreenInfo *screen_info, Settings *rc)
{
    Display *dpy;
    gchar *keytheme;
    gchar *keythemevalue;

    dpy = myScreenGetXDisplay (screen_info);
    /* 
       Load defaults keytheme so that even if there are 
       missing shortcuts in an older user defined key theme
       the missing keys will be taken from the default
     */
    keytheme = getThemeDir (DEFAULT_KEYTHEME, KEYTHEMERC);
    parseRc (KEYTHEMERC, keytheme, rc);
    g_free (keytheme);

    keythemevalue = getValue ("keytheme", rc);
    if (keythemevalue)
    {
        keytheme = getThemeDir (keythemevalue, KEYTHEMERC);
        if (!parseRc (KEYTHEMERC, keytheme, rc))
        {
            g_warning ("Specified key theme \"%s\" missing, using default", keythemevalue);
        }
        g_free (keytheme);

        if (!checkRc (rc))
        {
            g_warning ("Missing values in defaults file");
            return FALSE;
        }
    }

    parseKeyString (dpy, &screen_info->params->keys[KEY_ADD_WORKSPACE], getValue ("add_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_CLOSE_WINDOW], getValue ("close_window_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_CYCLE_WINDOWS], getValue ("cycle_windows_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_DEL_WORKSPACE], getValue ("del_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_DOWN_WORKSPACE], getValue ("down_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_HIDE_WINDOW], getValue ("hide_window_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_LEFT_WORKSPACE], getValue ("left_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_LOWER_WINDOW], getValue ("lower_window_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MAXIMIZE_HORIZ], getValue ("maximize_horiz_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MAXIMIZE_VERT], getValue ("maximize_vert_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MAXIMIZE_WINDOW], getValue ("maximize_window_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_CANCEL], getValue ("move_resize_cancel_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_DOWN], getValue ("move_window_down_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_DOWN_WORKSPACE], getValue ("move_window_down_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_LEFT], getValue ("move_window_left_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_LEFT_WORKSPACE], getValue ("move_window_left_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_NEXT_WORKSPACE], getValue ("move_window_next_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_PREV_WORKSPACE], getValue ("move_window_prev_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_RIGHT], getValue ("move_window_right_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_RIGHT_WORKSPACE], getValue ("move_window_right_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_UP], getValue ("move_window_up_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_UP_WORKSPACE], getValue ("move_window_up_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_1], getValue ("move_window_workspace_1_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_2], getValue ("move_window_workspace_2_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_3], getValue ("move_window_workspace_3_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_4], getValue ("move_window_workspace_4_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_5], getValue ("move_window_workspace_5_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_6], getValue ("move_window_workspace_6_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_7], getValue ("move_window_workspace_7_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_8], getValue ("move_window_workspace_8_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_9], getValue ("move_window_workspace_9_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_10], getValue ("move_window_workspace_10_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_11], getValue ("move_window_workspace_11_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_MOVE_WORKSPACE_12], getValue ("move_window_workspace_12_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_NEXT_WORKSPACE], getValue ("next_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_PREV_WORKSPACE], getValue ("prev_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_RAISE_WINDOW], getValue ("raise_window_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_RESIZE_DOWN], getValue ("resize_window_down_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_RESIZE_LEFT], getValue ("resize_window_left_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_RESIZE_RIGHT], getValue ("resize_window_right_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_RESIZE_UP], getValue ("resize_window_up_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_RIGHT_WORKSPACE], getValue ("right_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_SHADE_WINDOW], getValue ("shade_window_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_SHOW_DESKTOP], getValue("show_desktop_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_STICK_WINDOW], getValue ("stick_window_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_TOGGLE_FULLSCREEN], getValue ("fullscreen_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_UP_WORKSPACE], getValue ("up_workspace_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_1], getValue ("workspace_1_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_2], getValue ("workspace_2_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_3], getValue ("workspace_3_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_4], getValue ("workspace_4_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_5], getValue ("workspace_5_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_6], getValue ("workspace_6_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_7], getValue ("workspace_7_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_8], getValue ("workspace_8_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_9], getValue ("workspace_9_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_10], getValue ("workspace_10_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_11], getValue ("workspace_11_key", rc));
    parseKeyString (dpy, &screen_info->params->keys[KEY_WORKSPACE_12], getValue ("workspace_12_key", rc));

    myScreenGrabKeys (screen_info);

    return TRUE;
}

gboolean
loadSettings (ScreenInfo *screen_info)
{
    Settings rc[] = {
        /* Do not change the order of the following parameters */
        {"active_text_color", NULL, FALSE},
        {"inactive_text_color", NULL, FALSE},
        {"active_text_shadow_color", NULL, FALSE},
        {"inactive_text_shadow_color", NULL, FALSE},
        {"active_border_color", NULL, FALSE},
        {"inactive_border_color", NULL, FALSE},
        {"active_color_1", NULL, FALSE},
        {"active_hilight_1", NULL, FALSE},
        {"active_shadow_1", NULL, FALSE},
        {"active_mid_1", NULL, FALSE},
        {"active_color_2", NULL, FALSE},
        {"active_hilight_2", NULL, FALSE},
        {"active_shadow_2", NULL, FALSE},
        {"active_mid_2", NULL, FALSE},
        {"inactive_color_1", NULL, FALSE},
        {"inactive_hilight_1", NULL, FALSE},
        {"inactive_shadow_1", NULL, FALSE},
        {"inactive_mid_1", NULL, FALSE},
        {"inactive_color_2", NULL, FALSE},
        {"inactive_hilight_2", NULL, FALSE},
        {"inactive_shadow_2", NULL, FALSE},
        {"inactive_mid_2", NULL, FALSE},
        /* You can change the order of the following parameters */
        {"box_move", NULL, TRUE},
        {"box_resize", NULL, TRUE},
        {"button_layout", NULL, TRUE},
        {"button_offset", NULL, TRUE},
        {"button_spacing", NULL, TRUE},
        {"click_to_focus", NULL, TRUE},
        {"cycle_hidden", NULL, TRUE},
        {"cycle_minimum", NULL, TRUE},
        {"cycle_workspaces", NULL, TRUE},
        {"dbl_click_time", NULL, TRUE},
        {"double_click_action", NULL, TRUE},
        {"easy_click", NULL, TRUE},
        {"focus_hint", NULL, TRUE},
        {"focus_new", NULL, TRUE},
        {"frame_opacity", NULL, TRUE},
        {"full_width_title", NULL, TRUE},
        {"inactive_opacity", NULL, TRUE},
        {"keytheme", NULL, TRUE},
        {"margin_bottom", NULL, FALSE},
        {"margin_left", NULL, FALSE},
        {"margin_right", NULL, FALSE},
        {"margin_top", NULL, FALSE},
        {"move_opacity", NULL, TRUE},
        {"placement_ratio", NULL, TRUE},
        {"popup_opacity", NULL, TRUE},
        {"prevent_focus_stealing", NULL, TRUE},
        {"raise_delay", NULL, TRUE},
        {"raise_on_click", NULL, TRUE},
        {"raise_on_focus", NULL, TRUE},
        {"raise_with_any_button", NULL, TRUE},
        {"resize_opacity", NULL, TRUE},
        {"restore_on_move", NULL, TRUE},
        {"scroll_workspaces", NULL, TRUE},
        {"shadow_delta_height", NULL, TRUE},
        {"shadow_delta_width", NULL, TRUE},
        {"shadow_delta_x", NULL, TRUE},
        {"shadow_delta_y", NULL, TRUE},
        {"show_app_icon", NULL, TRUE},
        {"show_frame_shadow", NULL, TRUE},
        {"show_popup_shadow", NULL, TRUE},
        {"snap_resist", NULL, TRUE},
        {"snap_to_border", NULL, TRUE},
        {"snap_to_windows", NULL, TRUE},
        {"snap_width", NULL, TRUE},
        {"theme", NULL, TRUE},
        {"title_alignment", NULL, TRUE},
        {"title_font", NULL, FALSE},
        {"title_horizontal_offset", NULL, TRUE},
        {"title_shadow_active", NULL, TRUE},
        {"title_shadow_inactive", NULL, TRUE},
        {"title_vertical_offset_active", NULL, TRUE},
        {"title_vertical_offset_inactive", NULL, TRUE},
        {"toggle_workspaces", NULL, TRUE},
        {"workspace_count", NULL, TRUE},
        {"wrap_cycle", NULL, TRUE},
        {"wrap_layout", NULL, TRUE},
        {"wrap_resistance", NULL, TRUE},
        {"wrap_windows", NULL, TRUE},
        {"wrap_workspaces", NULL, TRUE},
        /* Keys */
        {"add_workspace_key", NULL, TRUE},
        {"close_window_key", NULL, TRUE},
        {"cycle_windows_key", NULL, TRUE},
        {"del_workspace_key", NULL, TRUE},
        {"down_workspace_key", NULL, TRUE},
        {"fullscreen_key", NULL, TRUE},
        {"hide_window_key", NULL, TRUE},
        {"left_workspace_key", NULL, TRUE},
        {"lower_window_key", NULL, TRUE},
        {"maximize_horiz_key", NULL, TRUE},
        {"maximize_vert_key", NULL, TRUE},
        {"maximize_window_key", NULL, TRUE},
        {"move_resize_cancel_key", NULL, TRUE},
        {"move_window_down_key", NULL, TRUE},
        {"move_window_down_workspace_key", NULL, TRUE},
        {"move_window_left_key", NULL, TRUE},
        {"move_window_left_workspace_key", NULL, TRUE},
        {"move_window_next_workspace_key", NULL, TRUE},
        {"move_window_prev_workspace_key", NULL, TRUE},
        {"move_window_right_key", NULL, TRUE},
        {"move_window_right_workspace_key", NULL, TRUE},
        {"move_window_up_key", NULL, TRUE},
        {"move_window_up_workspace_key", NULL, TRUE},
        {"move_window_workspace_1_key", NULL, TRUE},
        {"move_window_workspace_2_key", NULL, TRUE},
        {"move_window_workspace_3_key", NULL, TRUE},
        {"move_window_workspace_4_key", NULL, TRUE},
        {"move_window_workspace_5_key", NULL, TRUE},
        {"move_window_workspace_6_key", NULL, TRUE},
        {"move_window_workspace_7_key", NULL, TRUE},
        {"move_window_workspace_8_key", NULL, TRUE},
        {"move_window_workspace_9_key", NULL, TRUE},
        {"move_window_workspace_10_key", NULL, TRUE},
        {"move_window_workspace_11_key", NULL, TRUE},
        {"move_window_workspace_12_key", NULL, TRUE},
        {"next_workspace_key", NULL, TRUE},
        {"prev_workspace_key", NULL, TRUE},
        {"raise_window_key", NULL, TRUE},
        {"resize_window_down_key", NULL, TRUE},
        {"resize_window_left_key", NULL, TRUE},
        {"resize_window_right_key", NULL, TRUE},
        {"resize_window_up_key", NULL, TRUE},
        {"right_workspace_key", NULL, TRUE},
        {"shade_window_key", NULL, TRUE},
        {"show_desktop_key", NULL, FALSE},
        {"stick_window_key", NULL, TRUE},
        {"up_workspace_key", NULL, TRUE},
        {"workspace_1_key", NULL, TRUE},
        {"workspace_2_key", NULL, TRUE},
        {"workspace_3_key", NULL, TRUE},
        {"workspace_4_key", NULL, TRUE},
        {"workspace_5_key", NULL, TRUE},
        {"workspace_6_key", NULL, TRUE},
        {"workspace_7_key", NULL, TRUE},
        {"workspace_8_key", NULL, TRUE},
        {"workspace_9_key", NULL, TRUE},
        {"workspace_10_key", NULL, TRUE},
        {"workspace_11_key", NULL, TRUE},
        {"workspace_12_key", NULL, TRUE},
        {NULL, NULL, FALSE}
    };

    TRACE ("entering loadSettings");

    loadRcData (screen_info, rc);
    loadMcsData (screen_info, rc);
    loadTheme (screen_info, rc);

    if (!loadKeyBindings (screen_info, rc))
    {
        freeRc (rc);
        return FALSE;
    }

    screen_info->params->box_resize =
        !g_ascii_strcasecmp ("true", getValue ("box_resize", rc));
    screen_info->params->box_move = !g_ascii_strcasecmp ("true", getValue ("box_move", rc));

    screen_info->params->click_to_focus =
        !g_ascii_strcasecmp ("true", getValue ("click_to_focus", rc));
    screen_info->params->cycle_minimum =
        !g_ascii_strcasecmp ("true", getValue ("cycle_minimum", rc));
    screen_info->params->cycle_hidden =
        !g_ascii_strcasecmp ("true", getValue ("cycle_hidden", rc));
    screen_info->params->cycle_workspaces =
        !g_ascii_strcasecmp ("true", getValue ("cycle_workspaces", rc));
    screen_info->params->focus_hint =
        !g_ascii_strcasecmp ("true", getValue ("focus_hint", rc));
    screen_info->params->focus_new =
        !g_ascii_strcasecmp ("true", getValue ("focus_new", rc));
    screen_info->params->raise_on_focus =
        !g_ascii_strcasecmp ("true", getValue ("raise_on_focus", rc));
    screen_info->params->prevent_focus_stealing =
        !g_ascii_strcasecmp ("true", getValue ("prevent_focus_stealing", rc));
    screen_info->params->raise_delay = abs (TOINT (getValue ("raise_delay", rc)));
    screen_info->params->raise_on_click =
        !g_ascii_strcasecmp ("true", getValue ("raise_on_click", rc));
    screen_info->params->raise_with_any_button =
        !g_ascii_strcasecmp ("true", getValue ("raise_with_any_button", rc));
    screen_info->params->restore_on_move =
        !g_ascii_strcasecmp ("true", getValue ("restore_on_move", rc));
    screen_info->params->frame_opacity = 
        abs (TOINT (getValue ("frame_opacity", rc)));
    screen_info->params->inactive_opacity = 
        abs (TOINT (getValue ("inactive_opacity", rc)));
    screen_info->params->move_opacity = 
        abs (TOINT (getValue ("move_opacity", rc)));
    screen_info->params->resize_opacity = 
        abs (TOINT (getValue ("resize_opacity", rc)));
    screen_info->params->popup_opacity = 
        abs (TOINT (getValue ("popup_opacity", rc)));
    screen_info->params->placement_ratio = 
        abs (TOINT (getValue ("placement_ratio", rc)));
    screen_info->params->show_app_icon = 
        !g_ascii_strcasecmp ("true", getValue ("show_app_icon", rc));
    screen_info->params->show_frame_shadow = 
        !g_ascii_strcasecmp ("true", getValue ("show_frame_shadow", rc));
    screen_info->params->show_popup_shadow =
        !g_ascii_strcasecmp ("true", getValue ("show_popup_shadow", rc));
    screen_info->params->snap_to_border =
        !g_ascii_strcasecmp ("true", getValue ("snap_to_border", rc));
    screen_info->params->snap_to_windows =
        !g_ascii_strcasecmp ("true", getValue ("snap_to_windows", rc));
    screen_info->params->snap_resist =
        !g_ascii_strcasecmp ("true", getValue ("snap_resist", rc));
    screen_info->params->snap_width = abs (TOINT (getValue ("snap_width", rc)));

    set_settings_margin (screen_info, LEFT,   TOINT (getValue ("margin_left", rc)));
    set_settings_margin (screen_info, RIGHT,  TOINT (getValue ("margin_right", rc)));
    set_settings_margin (screen_info, BOTTOM, TOINT (getValue ("margin_bottom", rc)));
    set_settings_margin (screen_info, TOP,    TOINT (getValue ("margin_top", rc)));

    set_easy_click (screen_info, getValue ("easy_click", rc));

    if (!g_ascii_strcasecmp ("shade", getValue ("double_click_action", rc)))
    {
        screen_info->params->double_click_action = ACTION_SHADE;
    }
    else if (!g_ascii_strcasecmp ("hide", getValue ("double_click_action", rc)))
    {
        screen_info->params->double_click_action = ACTION_HIDE;
    }
    else if (!g_ascii_strcasecmp ("maximize", getValue ("double_click_action", rc)))
    {
        screen_info->params->double_click_action = ACTION_MAXIMIZE;
    }
    else
    {
        screen_info->params->double_click_action = ACTION_NONE;
    }

    if (screen_info->workspace_count < 0)
    {
        gint workspace_count;
        workspace_count = abs (TOINT (getValue ("workspace_count", rc)));
        if (workspace_count < 0)
        {
            workspace_count = 0;
        }
        workspaceSetCount (screen_info, workspace_count);
    }

    screen_info->params->toggle_workspaces =
        !g_ascii_strcasecmp ("true", getValue ("toggle_workspaces", rc));
    screen_info->params->wrap_workspaces =
        !g_ascii_strcasecmp ("true", getValue ("wrap_workspaces", rc));
    screen_info->params->wrap_layout =
        !g_ascii_strcasecmp ("true", getValue ("wrap_layout", rc));
    screen_info->params->wrap_windows =
        !g_ascii_strcasecmp ("true", getValue ("wrap_windows", rc));
    screen_info->params->wrap_cycle =
        !g_ascii_strcasecmp ("true", getValue ("wrap_cycle", rc));
    screen_info->params->scroll_workspaces =
        !g_ascii_strcasecmp ("true", getValue ("scroll_workspaces", rc));
    screen_info->params->wrap_resistance = abs (TOINT (getValue ("wrap_resistance", rc)));

    freeRc (rc);
    return TRUE;
}

static void
unloadTheme (ScreenInfo *screen_info)
{
    int i;

    TRACE ("entering unloadTheme");

    for (i = 0; i < 3; i++)
    {
        xfwmPixmapFree (&screen_info->sides[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->sides[i][INACTIVE]);
    }
    for (i = 0; i < 4; i++)
    {
        xfwmPixmapFree (&screen_info->corners[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->corners[i][INACTIVE]);
    }
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        xfwmPixmapFree (&screen_info->buttons[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->buttons[i][INACTIVE]);
        xfwmPixmapFree (&screen_info->buttons[i][PRESSED]);
        xfwmPixmapFree (&screen_info->buttons[i][T_ACTIVE]);
        xfwmPixmapFree (&screen_info->buttons[i][T_INACTIVE]);
        xfwmPixmapFree (&screen_info->buttons[i][T_PRESSED]);
    }
    for (i = 0; i < 5; i++)
    {
        xfwmPixmapFree (&screen_info->title[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->title[i][INACTIVE]);
    }
    if (screen_info->box_gc != None)
    {
        XFreeGC (myScreenGetXDisplay (screen_info), screen_info->box_gc);
        screen_info->box_gc = None;
    }
}

static void
unloadSettings (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info);
    
    TRACE ("entering unloadSettings");

    unloadTheme (screen_info);
}

static gboolean
reloadScreenSettings (ScreenInfo *screen_info, int mask)
{
    g_return_val_if_fail (screen_info, FALSE);

    unloadTheme (screen_info);
    if (!loadSettings (screen_info))
    {
        return FALSE;
    }
    if (mask)
    {
        clientUpdateAllFrames (screen_info, mask);
        if (mask & UPDATE_FRAME)
        {
            compositorRebuildScreen (screen_info);
        }
    }

    return TRUE;
}

gboolean
reloadSettings (DisplayInfo *display_info, int mask)
{
    GSList *screens;

    g_return_val_if_fail (display_info, FALSE);
    
    TRACE ("entering reloadSettings");
    
    /* Refresh all screens, not just one */
    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        ScreenInfo *screen_info = (ScreenInfo *) screens->data;
        if (!reloadScreenSettings (screen_info, mask))
        {
             return FALSE;
        }
    }

    return TRUE;
}

gboolean
initSettings (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    char **names;
    long val;
    int i;

    g_return_val_if_fail (screen_info, FALSE);
    
    TRACE ("entering initSettings");

    display_info = screen_info->display_info;
    names = NULL;
    val = 0;
    i = 0;
    
    if (!mcs_client_check_manager (myScreenGetXDisplay (screen_info), screen_info->screen, "xfce-mcs-manager"))
    {
        g_warning ("MCS manager not running, startup delayed for 5 seconds");
        sleep (5);
    }

    screen_info->mcs_client = mcs_client_new (myScreenGetXDisplay (screen_info), screen_info->screen, notify_cb, watch_cb, (gpointer) screen_info);
    if (screen_info->mcs_client)
    {
        mcs_client_add_channel (screen_info->mcs_client, CHANNEL1);
        mcs_client_add_channel (screen_info->mcs_client, CHANNEL2);
        mcs_client_add_channel (screen_info->mcs_client, CHANNEL3);
        mcs_client_add_channel (screen_info->mcs_client, CHANNEL4);
        mcs_client_add_channel (screen_info->mcs_client, CHANNEL5);
        mcs_client_set_raw_channel (screen_info->mcs_client, CHANNEL4, TRUE);
    }
    else
    {
        g_warning ("Cannot create MCS client channel");
    }
    screen_info->mcs_initted = TRUE;

    if (!loadSettings (screen_info))
    {
        return FALSE;
    }
    if (getHint (display_info, screen_info->xroot, NET_NUMBER_OF_DESKTOPS, &val))
    {
        workspaceSetCount (screen_info, val);
    }
    else if (getHint (display_info, screen_info->xroot, WIN_WORKSPACE_COUNT, &val))
    {
        workspaceSetCount (screen_info, val);
    }

    if (getUTF8StringList (display_info, screen_info->xroot, NET_DESKTOP_NAMES, &names, &i))
    {
        workspaceSetNames (screen_info, names, i);
    }
    else
    {
        screen_info->workspace_names = NULL;
        screen_info->workspace_names_items = 0;
    }

    getDesktopLayout(display_info, screen_info->xroot, screen_info->workspace_count, &screen_info->desktop_layout);
    placeSidewalks (screen_info, screen_info->params->wrap_workspaces);

    return TRUE;
}

void
closeSettings (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info);
    
    if (screen_info->mcs_client)
    {
        mcs_client_destroy (screen_info->mcs_client);
        screen_info->mcs_client = NULL;
    }
    unloadSettings (screen_info);
}
