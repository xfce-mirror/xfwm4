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
 
        Copyright (C) 2002-2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>

#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4mcs/mcs-manager.h>
#include <xfce-mcs-manager/manager-plugin.h>

#include "plugin.h"
#include "workspaces.h"

#define MAX_COUNT 32
#define DEFAULT_NBR_WS 4
#define WS_SEP ';'
#define WS_SEP_S ";"

static McsManager *mcs_manager;
static NetkScreen *netk_screen = NULL;

static int ws_count = 1;
static char **ws_names = NULL;

static GtkWidget *treeview;
static int treerows;

enum
{
    NUMBER_COLUMN,
    NAME_COLUMN,
    N_COLUMNS
};

static void save_workspaces_channel (McsManager * manager);
static void set_workspace_count (McsManager * manager, int count);
static void set_workspace_names (McsManager * manager, char **names);
static void watch_workspaces_hint (McsManager * manager);

/* very useful functions */
static int
array_size (char **array)
{
    char **p;
    int len = 0;

    for (p = array; p && *p; p++)
	len++;

    return len;
}

static void
update_names (McsManager * manager, int n)
{
    char **tmpnames;
    int i, len;

    len = array_size (ws_names);

    tmpnames = g_new (char *, n + 1);
    tmpnames[n] = NULL;

    for (i = 0; i < n; i++)
    {
	if (i < len)
	    tmpnames[i] = g_strdup (ws_names[i]);
	else
	{
	    const char *name;
	    NetkWorkspace *ws = netk_screen_get_workspace (netk_screen, i);

	    name = netk_workspace_get_name (ws);

	    if (name && strlen (name))
	    {
		tmpnames[i] = g_strdup (name);
	    }
	    else
	    {
		char num[4];

		snprintf (num, 3, "%d", i + 1);
		tmpnames[i] = g_strdup (num);
	    }
	}
    }

    g_strfreev (ws_names);
    ws_names = tmpnames;

    set_workspace_names (manager, ws_names);
}

/* the settings channel */
void
create_workspaces_channel (McsPlugin * mcs_plugin)
{
    McsSetting *setting;
    int len, n;

    mcs_manager = mcs_plugin->manager;

    netk_screen = netk_screen_get_default ();
    netk_screen_force_update (netk_screen);

    ws_create_channel (mcs_manager, CHANNEL1, RCFILE1);

    /* ws names */
    setting = mcs_manager_setting_lookup (mcs_manager, "names", CHANNEL1);

    if (setting)
    {
	ws_names = g_strsplit (setting->data.v_string, WS_SEP_S, -1);
    }

    /* ws count */
    ws_count = netk_screen_get_workspace_count (netk_screen);

    setting = mcs_manager_setting_lookup (mcs_manager, "Xfwm/WorkspaceCount", 
	    				  CHANNEL1);

    if (setting)
    {
	ws_count = setting->data.v_int;
    }
    else
    {
	/* backward compatibility */
	setting = mcs_manager_setting_lookup (mcs_manager, "count", CHANNEL1);

	if (setting)
	{
	    ws_count = setting->data.v_int;
	    mcs_manager_delete_setting(mcs_manager, "count", CHANNEL1);
	}
        else
        {
	    ws_count = DEFAULT_NBR_WS;
        }
	set_workspace_count (mcs_manager, ws_count);
    }

    len = (ws_names) ? array_size (ws_names) : 0;
    n = (len > ws_count) ? len : ws_count;

    update_names (mcs_manager, n);

    save_workspaces_channel (mcs_manager);

    /* check for changes made by other programs */
    /* XXX I don't think this should be called at all.
       Problem is when the module is unloaded, the callback
       remains and crashes the MCS manager on workspace count
       change.
       Safer to remove it for now.
       
       watch_workspaces_hint (mcs_manager);
     */
}

static void
save_workspaces_channel (McsManager * manager)
{
    ws_save_channel (manager, CHANNEL1, RCFILE1);
}

static void
set_workspace_count (McsManager * manager, int count)
{
    int len;

    mcs_manager_set_int (manager, "Xfwm/WorkspaceCount", CHANNEL1, ws_count);

    mcs_manager_notify (manager, CHANNEL1);
    save_workspaces_channel (manager);

    len = array_size (ws_names);

    if (len < ws_count)
	update_names (manager, ws_count);
}

static void
set_workspace_names (McsManager * manager, char **names)
{
    char *string;
    static Atom xa_NET_DESKTOP_NAMES = 0;
    int len;

    string = g_strjoinv (WS_SEP_S, names);

    mcs_manager_set_string (manager, "names", CHANNEL1, string);

    mcs_manager_notify (manager, CHANNEL1);
    save_workspaces_channel (manager);

    if (!xa_NET_DESKTOP_NAMES)
    {
	xa_NET_DESKTOP_NAMES = XInternAtom(GDK_DISPLAY(), 
					   "_NET_DESKTOP_NAMES", False);
    }

    len = strlen(string);
    
    string = g_strdelimit(string, WS_SEP_S, '\0');
    
    gdk_error_trap_push();
    gdk_property_change(gdk_get_default_root_window(), 
	    		gdk_x11_xatom_to_atom(xa_NET_DESKTOP_NAMES),
			gdk_atom_intern("UTF8_STRING", FALSE),
			8, GDK_PROP_MODE_REPLACE,
			string, len);
    gdk_flush();
    gdk_error_trap_pop();

    g_free(string);
}

/* the dialog notebook page */
static void
treeview_set_rows (McsManager * manager, int n)
{
    int i;
    GtkListStore *store;
    GtkTreeModel *model;

    DBG ("set %d treerows (current number: %d)\n", n, treerows);

    if (n == treerows)
	return;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
    store = GTK_LIST_STORE (model);

    if (n < treerows)
    {
	GtkTreePath *path;
	GtkTreeIter iter;
	char num[4];

	/* we have a list so the path string is only the node index */
	snprintf (num, 3, "%d", n);
	path = gtk_tree_path_new_from_string (num);

	if (!gtk_tree_model_get_iter (model, &iter, path))
	{
	    g_critical ("Can't get a pointer to treeview row %d", n);
	    return;
	}

	for (i = n; i < treerows; i++)
	{
	    /* iter gets set to next valid row, so this should work */
	    gtk_list_store_remove (store, &iter);
	}

	if (gtk_tree_path_prev (path))
	{
	    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), path,
					  NULL, FALSE, 0, 0);
	    gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL,
				      FALSE);
	}

	gtk_tree_path_free (path);
    }
    else
    {
	GtkTreeIter iter;

	for (i = treerows; i < n; i++)
	{
	    char *name;
	    GtkTreePath *path;

	    name = ws_names[i];

	    gtk_list_store_append (store, &iter);

	    gtk_list_store_set (store, &iter, NUMBER_COLUMN, i + 1,
				NAME_COLUMN, name, -1);

	    path = gtk_tree_model_get_path (model, &iter);
	    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), path,
					  NULL, FALSE, 0, 0);
	    gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL,
				      FALSE);

	    gtk_tree_path_free (path);
	}
    }

    treerows = n;
}

static void
edit_name_dialog (GtkTreeModel * model, GtkTreeIter * iter,
		  int number, const char *name, McsManager * manager)
{
    GtkWidget *dialog, *mainvbox, *header, *hbox, *label, *entry;
    char title[512];
    int response;
    const char *tmp;

    dialog = gtk_dialog_new_with_buttons (_("Change name"), NULL,
					  GTK_DIALOG_NO_SEPARATOR,
					  GTK_STOCK_CANCEL,
					  GTK_RESPONSE_CANCEL,
					  GTK_STOCK_APPLY, GTK_RESPONSE_OK,
					  NULL);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    mainvbox = GTK_DIALOG (dialog)->vbox;

    sprintf (title, _("Workspace %d"), number);
    header = xfce_create_header (NULL, title);
    gtk_widget_show (header);
    gtk_box_pack_start (GTK_BOX (mainvbox), header, TRUE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (mainvbox), hbox, TRUE, FALSE, 0);

    label = gtk_label_new (_("Name:"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_widget_show (entry);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

    gtk_entry_set_text (GTK_ENTRY (entry), name);

    response = GTK_RESPONSE_NONE;

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    tmp = gtk_entry_get_text (GTK_ENTRY (entry));

    if (response == GTK_RESPONSE_OK && tmp && strlen (tmp))
    {
	int n = number - 1;
	char *s;

	g_free (ws_names[n]);
	ws_names[n] = g_strdup (tmp);

	for (s = strchr (ws_names[n], WS_SEP); s; s = strchr (s + 1, WS_SEP))
	{
	    /* just don't use our separator character! */
	    *s = ' ';
	}

	gtk_list_store_set (GTK_LIST_STORE (model), iter,
			    NAME_COLUMN, ws_names[n], -1);

	set_workspace_names (manager, ws_names);
    }

    gtk_widget_destroy (dialog);
}

static gboolean
button_pressed (GtkTreeView * tree, GdkEventButton * event,
		McsManager * manager)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_view_get_path_at_pos (tree, event->x, event->y,
				       &path, NULL, NULL, NULL))
    {
	char *name;
	int number;

	model = gtk_tree_view_get_model (tree);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_view_set_cursor (tree, path, NULL, FALSE);

	gtk_tree_model_get (model, &iter,
			    NUMBER_COLUMN, &number, NAME_COLUMN, &name, -1);

	edit_name_dialog (model, &iter, number, name, manager);
	g_free (name);
    }

    return TRUE;
}

static void
treeview_destroyed (GtkWidget *tv)
{
    GtkTreeModel *store;

    store = gtk_tree_view_get_model (GTK_TREE_VIEW (tv));
    gtk_list_store_clear (GTK_LIST_STORE (store));
}

static void
add_names_treeview (GtkWidget * vbox, McsManager * manager)
{
    GtkWidget *treeview_scroll;
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeModel *model;
    char *markup;
    GtkWidget *label;

    markup = g_strconcat ("<i>", _("Click on a workspace name to edit it"),
			  "</i>", NULL);
    label = gtk_label_new (markup);
    g_free (markup);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

    treeview_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (treeview_scroll);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (treeview_scroll),
				    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW
					 (treeview_scroll), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (vbox), treeview_scroll, TRUE, TRUE, 0);

    store = gtk_list_store_new (N_COLUMNS, G_TYPE_INT, G_TYPE_STRING);
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));
    gtk_widget_show (treeview);
    gtk_container_add (GTK_CONTAINER (treeview_scroll), treeview);

    /* clean up list store */
    g_signal_connect(treeview, "destroy-event", 
	             G_CALLBACK(treeview_destroyed), NULL);

    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

    treerows = 0;
    treeview_set_rows (manager, ws_count);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Number", renderer,
						       "text", NUMBER_COLUMN,
						       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
						       "text", NAME_COLUMN,
						       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

    g_signal_connect (treeview, "button-press-event",
		      G_CALLBACK (button_pressed), manager);
}

/* workspace count */
static void
count_changed (GtkSpinButton * spin, McsManager * manager)
{
    int n = gtk_spin_button_get_value_as_int (spin);

    ws_count = n;
    set_workspace_count (manager, n);

    treeview_set_rows (manager, n);
}

static void
add_count_spinbox (GtkWidget * vbox, McsManager * manager)
{
    GtkWidget *hbox, *label, *spin;

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new (_("Number of workspaces:"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    spin = gtk_spin_button_new_with_range (1, MAX_COUNT, 1);
    gtk_widget_show (spin);
    gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 0);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), ws_count);

    g_signal_connect (spin, "changed", G_CALLBACK (count_changed), manager);
}

void
add_workspaces_page (GtkBox * box)
{
    GtkWidget *frame, *vbox;

    /* Number of workspaces */
    frame = xfce_framebox_new (_("Workspaces"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (box, frame, FALSE, FALSE, 0);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_widget_show (vbox);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

    add_count_spinbox (vbox, mcs_manager);

    /* Workspace names */
    frame = xfce_framebox_new (_("Workspace names"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (box), frame, TRUE, TRUE, 0);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_widget_show (vbox);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

    add_names_treeview (vbox, mcs_manager);
}

/* watch for changes by other programs */
static void
update_channel (NetkScreen * screen, NetkWorkspace * ws, McsManager * manager)
{
    ws_count = netk_screen_get_workspace_count (screen);

    set_workspace_count (manager, ws_count);
}

static void
watch_workspaces_hint (McsManager * manager)
{
    /* make GCC happy */
    (void)&watch_workspaces_hint;

    g_signal_connect (netk_screen, "workspace-created",
		      G_CALLBACK (update_channel), manager);
    g_signal_connect (netk_screen, "workspace-destroyed",
		      G_CALLBACK (update_channel), manager);
}
