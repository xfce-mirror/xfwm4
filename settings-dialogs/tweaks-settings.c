/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>

#include <glib.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>
#include "xfwm4-tweaks-dialog_glade.h"

static gboolean version = FALSE;

void
cb_easy_click_combo_box_changed (GtkComboBox *combo, XfconfChannel *channel)
{
    switch (gtk_combo_box_get_active(combo))
    {
        case 0:
            xfconf_channel_set_string (channel, "/general/easy_click", "Alt");
            break;
        case 1:
            xfconf_channel_set_string (channel, "/general/easy_click", "Control");
            break;
        default:
            xfconf_channel_set_string (channel, "/general/easy_click", "Alt");
            break;
    }
}

void
cb_use_compositing_check_button_toggled (GtkToggleButton *toggle, GtkWidget *box)
{
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active(toggle));
}

void
cb_prevent_focus_stealing_check_button_toggled (GtkToggleButton *toggle, GtkWidget *box)
{
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active(toggle));
}

void
cb_activate_action_bring_radio_toggled (GtkToggleButton *toggle, XfconfChannel *channel)
{
    if (gtk_toggle_button_get_active (toggle))
    {
        xfconf_channel_set_string (channel, "/general/activate_action", "bring");
    }
}

void
cb_activate_action_switch_radio_toggled (GtkToggleButton *toggle, XfconfChannel *channel)
{
    if (gtk_toggle_button_get_active (toggle))
    {
        xfconf_channel_set_string (channel, "/general/activate_action", "switch");
    }
}

void
cb_activate_action_none_radio_toggled (GtkToggleButton *toggle, XfconfChannel *channel)
{
    if (gtk_toggle_button_get_active (toggle))
    {
        xfconf_channel_set_string (channel, "/general/activate_action", "none");
    }
}

GtkWidget *
wm_tweaks_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget *dialog, *vbox;
    GtkTreeIter iter;
    GtkListStore *list_store;
    GtkCellRenderer *renderer;
    XfconfChannel *xfwm4_channel = xfconf_channel_new("xfwm4");
    gchar *easy_click = NULL;
    gchar *activate_action = NULL;
    
    /* Cycling tab */
    GtkWidget *cycle_workspaces_check = glade_xml_get_widget (gxml, "cycle_workspaces_check");
    GtkWidget *cycle_hidden_check = glade_xml_get_widget (gxml, "cycle_hidden_check");
    GtkWidget *cycle_minimum_check = glade_xml_get_widget (gxml, "cycle_minimum_check");

    /* Focus tab */
    GtkWidget *prevent_focus_stealing_check = glade_xml_get_widget (gxml, "prevent_focus_stealing_check");
    GtkWidget *prevent_focus_stealing_box = glade_xml_get_widget (gxml, "prevent_focus_stealing_box");
    GtkWidget *focus_hint_check = glade_xml_get_widget (gxml, "focus_hint_check");

    GtkWidget *activate_action_bring_option = glade_xml_get_widget (gxml, "activate_action_bring_option");
    GtkWidget *activate_action_switch_option = glade_xml_get_widget (gxml, "activate_action_switch_option");
    GtkWidget *activate_action_none_option = glade_xml_get_widget (gxml, "activate_action_none_option");

    /* Accessibility tab */
    GtkWidget *easy_click_combo_box = glade_xml_get_widget (gxml, "easy_click_combo_box");
    GtkWidget *raise_on_click_check = glade_xml_get_widget (gxml, "raise_on_click_check");
    GtkWidget *borderless_maximize_check = glade_xml_get_widget (gxml, "borderless_maximize_check");
    GtkWidget *restore_on_move_check = glade_xml_get_widget (gxml, "restore_on_move_check");
    GtkWidget *snap_resist_check = glade_xml_get_widget (gxml, "snap_resist_check");

    /* Workspaces tab */
    GtkWidget *scroll_workspaces_check = glade_xml_get_widget (gxml, "scroll_workspaces_check");
    GtkWidget *toggle_workspaces_check = glade_xml_get_widget (gxml, "toggle_workspaces_check");
    GtkWidget *wrap_layout_check = glade_xml_get_widget (gxml, "wrap_layout_check");
    GtkWidget *wrap_cycle_check = glade_xml_get_widget (gxml, "wrap_cycle_check");


    /* Placement tab */
    GtkWidget *placement_ratio_scale = (GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "placement_ratio_scale")));

    /* Compositing tab */
    GtkWidget *use_compositing_check = glade_xml_get_widget (gxml, "use_compositing_check");
    GtkWidget *use_compositing_box = glade_xml_get_widget (gxml, "use_compositing_box");

    GtkWidget *unredirect_overlays_check = glade_xml_get_widget (gxml, "unredirect_overlays_check");
    GtkWidget *show_frame_shadow_check = glade_xml_get_widget (gxml, "show_frame_shadow_check");
    GtkWidget *show_popup_shadow_check = glade_xml_get_widget (gxml, "show_popup_shadow_check");
    GtkWidget *show_dock_shadow_check = glade_xml_get_widget (gxml, "show_dock_shadow_check");

    GtkWidget *frame_opacity_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "frame_opacity_scale")));
    GtkWidget *inactive_opacity_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "inactive_opacity_scale")));
    GtkWidget *move_opacity_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "move_opacity_scale")));
    GtkWidget *popup_opacity_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "popup_opacity_scale")));
    GtkWidget *resize_opacity_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "resize_opacity_scale")));


    /* Hinting Combo */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Alt"), -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, N_("Ctrl"), -1);

    /* Fill combo-box */
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (easy_click_combo_box));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (easy_click_combo_box), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (easy_click_combo_box), renderer, "text", 0);

    gtk_combo_box_set_model (GTK_COMBO_BOX (easy_click_combo_box), GTK_TREE_MODEL(list_store));

    easy_click = xfconf_channel_get_string (xfwm4_channel, "/general/easy_click", "Alt");
    gtk_combo_box_set_active (GTK_COMBO_BOX(easy_click_combo_box), 0);
    if (!strcmp(easy_click, "Ctrl"))
        gtk_combo_box_set_active (GTK_COMBO_BOX(easy_click_combo_box), 1);

    activate_action = xfconf_channel_get_string (xfwm4_channel, "/general/activate_action", "bring");
    if (!strcmp (activate_action, "switch"))
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(activate_action_bring_option), FALSE);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(activate_action_switch_option), TRUE);
    }
    if (!strcmp (activate_action, "none"))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(activate_action_none_option), TRUE);


    /* not so easy properties */
    g_signal_connect (G_OBJECT (activate_action_bring_option),
                      "toggled",
                      G_CALLBACK (cb_activate_action_bring_radio_toggled),
                      xfwm4_channel);
    g_signal_connect (G_OBJECT (activate_action_switch_option),
                      "toggled",
                      G_CALLBACK (cb_activate_action_switch_radio_toggled),
                      xfwm4_channel);
    g_signal_connect (G_OBJECT (activate_action_none_option),
                      "toggled",
                      G_CALLBACK (cb_activate_action_none_radio_toggled),
                      xfwm4_channel);

    g_signal_connect (G_OBJECT (prevent_focus_stealing_check),
                      "toggled",
                      G_CALLBACK (cb_prevent_focus_stealing_check_button_toggled),
                      prevent_focus_stealing_box);
    g_signal_connect (G_OBJECT (use_compositing_check),
                      "toggled",
                      G_CALLBACK (cb_use_compositing_check_button_toggled),
                      use_compositing_box);
    g_signal_connect (G_OBJECT (easy_click_combo_box),
                      "changed",
                      G_CALLBACK (cb_easy_click_combo_box_changed),
                      xfwm4_channel);



    /* Bind easy properties */
    /* Cycling tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/cycle_minimum",
                            G_TYPE_BOOLEAN,
                            (GObject *)cycle_minimum_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/cycle_hidden",
                            G_TYPE_BOOLEAN,
                            (GObject *)cycle_hidden_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/cycle_workspaces",
                            G_TYPE_BOOLEAN,
                            (GObject *)cycle_workspaces_check, "active");

    /* Focus tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/prevent_focus_stealing",
                            G_TYPE_BOOLEAN,
                            (GObject *)prevent_focus_stealing_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/focus_hint",
                            G_TYPE_BOOLEAN,
                            (GObject *)focus_hint_check, "active");
    
    /* Accessibility tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/raise_on_click",
                            G_TYPE_BOOLEAN,
                            (GObject *)raise_on_click_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/borderless_maximize",
                            G_TYPE_BOOLEAN,
                            (GObject *)borderless_maximize_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/restore_on_move",
                            G_TYPE_BOOLEAN,
                            (GObject *)restore_on_move_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/snap_resist",
                            G_TYPE_BOOLEAN,
                            (GObject *)snap_resist_check, "active");

    /* Workspaces tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/toggle_workspaces",
                            G_TYPE_BOOLEAN,
                            (GObject *)toggle_workspaces_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/scroll_workspaces",
                            G_TYPE_BOOLEAN,
                            (GObject *)scroll_workspaces_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/wrap_layout",
                            G_TYPE_BOOLEAN,
                            (GObject *)wrap_layout_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/wrap_cycle",
                            G_TYPE_BOOLEAN,
                            (GObject *)wrap_cycle_check, "active");

    /* Placement tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/placement_ratio",
                            G_TYPE_INT,
                            (GObject *)placement_ratio_scale, "value");

    /* Compositing tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/use_compositing",
                            G_TYPE_BOOLEAN,
                            (GObject *)use_compositing_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/unredirect_overlays",
                            G_TYPE_BOOLEAN,
                            (GObject *)unredirect_overlays_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/show_frame_shadow",
                            G_TYPE_BOOLEAN,
                            (GObject *)show_frame_shadow_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/show_popup_shadow",
                            G_TYPE_BOOLEAN,
                            (GObject *)show_popup_shadow_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/show_dock_shadow",
                            G_TYPE_BOOLEAN,
                            (GObject *)show_dock_shadow_check, "active");

    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/frame_opacity",
                            G_TYPE_INT,
                            (GObject *)frame_opacity_scale, "value");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/resize_opacity",
                            G_TYPE_INT,
                            (GObject *)resize_opacity_scale, "value");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/move_opacity",
                            G_TYPE_INT,
                            (GObject *)move_opacity_scale, "value");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/inactive_opacity",
                            G_TYPE_INT,
                            (GObject *)inactive_opacity_scale, "value");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/popup_opacity",
                            G_TYPE_INT,
                            (GObject *)popup_opacity_scale, "value");

    vbox = glade_xml_get_widget (gxml, "main-vbox");
    dialog = glade_xml_get_widget (gxml, "main-dialog");

    gtk_widget_show_all(vbox);

    if (easy_click)
        g_free (easy_click);
    if (activate_action)
        g_free (activate_action);

    return dialog;
}

static GOptionEntry entries[] =
{
    {    "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    { NULL }
};

int 
main(int argc, gchar **argv)
{
    GladeXML *gxml;
    GtkWidget *dialog;
    GError *cli_error = NULL;

    #ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    if(!gtk_init_with_args(&argc, &argv, _("."), entries, PACKAGE, &cli_error))
    {
        if (cli_error != NULL)
        {
            g_print (_("%s: %s\nTry %s --help to see a full list of available command line options.\n"), PACKAGE, cli_error->message, PACKAGE_NAME);
            g_error_free (cli_error);
            return 1;
        }
    }

    if(version)
    {
        g_print("%s\n", PACKAGE_STRING);
        return 0;
    }

    xfconf_init(NULL);

    gxml = glade_xml_new_from_buffer (tweaks_dialog_glade,
                                      tweaks_dialog_glade_length,
                                      NULL, NULL);

    dialog = wm_tweaks_dialog_new_from_xml (gxml);

    gtk_dialog_run(GTK_DIALOG(dialog));

    xfconf_shutdown();

    return 0;
}
