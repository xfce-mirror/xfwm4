/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Initially inspired by xfwm, fvwm2, enlightment and twm implementations */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <signal.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>

#include "session.h"
#include "main.h"
#include "hints.h"
#include "client.h"

typedef struct _match
{
    unsigned long win;
    unsigned long client_leader;
    char *client_id;
    char *res_name;
    char *res_class;
    char *window_role;
    char *wm_name;
    int wm_command_count;
    char **wm_command;
    int x;
    int y;
    int width;
    int height;
    int old_x;
    int old_y;
    int old_width;
    int old_height;
    int desktop;
    unsigned long flags;
    gboolean used;
}
Match;

static int num_match = 0;
static Match *matches = NULL;

static void my_free_string_list(gchar ** list, gint n)
{
    gchar **s;
    gint i;

    if(!list || !n)
    {
        return;                 /* silently... :) */
    }

    i = 0;
    s = list;

    while((i < n) && (s))
    {
        g_free(*s);
        *s = NULL;
        s++;
        i++;
    }
}

/* 
   2-pass function to compute new string length,
   allocate memory and finally copy string 
   - Returned value must be freed -
 */
static gchar *escape_quote(gchar * s)
{
    gchar *ns;
    gchar *idx1, *idx2;
    gint nbquotes = 0;
    gint lg = 0;

    g_return_val_if_fail(s != NULL, NULL);

    lg = strlen(s);
    /* First, count quotes in string */
    idx1 = s;
    while(*idx1)
    {
        if(*(idx1++) == '"')
            nbquotes++;
    }
    /* If there is no quote in the string, return it */
    if(!nbquotes)
        return (g_strdup(s));

    /* Or else, allocate memory for the new string */
    ns = g_new(gchar, lg + nbquotes + 1);
    /* And prepend a backslash before any quote found in string */
    idx1 = s;
    idx2 = ns;
    while(*idx1)
    {
        if(*idx1 == '"')
        {
            *(idx2++) = '\\';
            *(idx2++) = '"';
        }
        else
        {
            *(idx2++) = *idx1;
        }
        idx1++;
    }
    /* Add null char */
    *idx2 = '\0';
    return ns;
}

/* 
   single-pass function to replace backslash+quotes
   by quotes. 
   - Returned value must be freed -
 */
static gchar *unescape_quote(gchar * s)
{
    gchar *ns;
    gboolean backslash;
    gchar *idx1, *idx2;
    gint lg;

    g_return_val_if_fail(s != NULL, NULL);

    lg = strlen(s);
    backslash = FALSE;
    ns = g_new(gchar, lg + 1);
    idx1 = s;
    idx2 = ns;
    while(*idx1)
    {
        if(*idx1 == '\\')
        {
            *(idx2++) = *idx1;
            backslash = TRUE;
        }
        else if((*idx1 == '"') && backslash)
        {
            /* Move backward to override the "\" */
            *(--idx2) = *idx1;
            idx2++;
            backslash = FALSE;
        }
        else
        {
            *(idx2++) = *idx1;
            backslash = FALSE;
        }
        idx1++;
    }
    *idx2 = '\0';
    return ns;
}

static gchar *getsubstring(gchar * s, gint * length)
{
    gchar pbrk;
    gchar *ns;
    gchar *end, *idx1, *idx2, *skip;
    gint lg;
    gboolean finished = FALSE, backslash = FALSE;

    lg = *length = 0;
    g_return_val_if_fail(s != NULL, NULL);

    end = skip = s;
    while((*skip == ' ') || (*skip == '\t'))
    {
        end = ++skip;
        (*length)++;
    }
    if(*skip == '"')
    {
        pbrk = '"';
        end = ++skip;
        (*length)++;
    }
    else
    {
        pbrk = ' ';
    }

    finished = FALSE;
    while((!finished) && (*end))
    {
        if(*end == '\\')
        {
            backslash = TRUE;
        }
        else if((*end == pbrk) && backslash)
        {
            backslash = FALSE;
        }
        else if(*end == pbrk)
        {
            finished = TRUE;
        }
        end++;
        lg++;
        (*length)++;
    }
    ns = g_new(gchar, lg + 1);
    /* Skip pbrk character */
    end--;
    idx1 = skip;
    idx2 = ns;
    do
    {
        *(idx2++) = *idx1;
    }
    while(++idx1 < end);
    *idx2 = '\0';
    return ns;
}

gboolean sessionSaveWindowStates(gchar * filename)
{
    FILE *f;
    Client *c;
    gint client_idx;
    char *client_id = NULL;
    char *window_role = NULL;
    int wm_command_count = 0;
    char **wm_command = NULL;

    g_return_val_if_fail(filename != NULL, FALSE);

    if((f = fopen(filename, "w")))
    {
        for(c = clients, client_idx = 0; client_idx < client_count; c = c->next, client_idx++)
        {
            if(c->client_leader != None)
            {
                getWindowRole(dpy, c->client_leader, &window_role);
            }
            else
            {
                window_role = NULL;
            }

            fprintf(f, "[CLIENT] 0x%lx\n", c->window);

            getClientID(dpy, c->window, &client_id);
            if(client_id)
            {
                fprintf(f, "  [CLIENT_ID] %s\n", client_id);
                XFree(client_id);
                client_id = NULL;
            }

            if(c->client_leader)
            {
                fprintf(f, "  [CLIENT_LEADER] 0x%lx\n", c->client_leader);
            }

            if(window_role)
            {
                fprintf(f, "  [WINDOW_ROLE] %s\n", window_role);
                XFree(window_role);
                window_role = NULL;
            }

            if(c->class.res_class)
            {
                fprintf(f, "  [RES_NAME] %s\n", c->class.res_name);
            }

            if(c->class.res_name)
            {
                fprintf(f, "  [RES_CLASS] %s\n", c->class.res_class);
            }

            if(c->name)
            {
                fprintf(f, "  [WM_NAME] %s\n", c->name);
            }

            wm_command_count = 0;
            getWindowCommand(dpy, c->window, &wm_command, &wm_command_count);
            if((wm_command_count > 0) && (wm_command))
            {
                gint j;
                fprintf(f, "  [WM_COMMAND] (%i)", wm_command_count);
                for(j = 0; j < wm_command_count; j++)
                {
                    gchar *escaped_string;
                    escaped_string = escape_quote(wm_command[j]);
                    fprintf(f, " \"%s\"", escaped_string);
                    g_free(escaped_string);
                }
                fprintf(f, "\n");
                XFreeStringList(wm_command);
                wm_command = NULL;
                wm_command_count = 0;
            }

            fprintf(f, "  [GEOMETRY] (%i,%i,%i,%i)\n", c->x, c->y, c->width, c->height);
            fprintf(f, "  [GEOMETRY-MAXIMIZED] (%i,%i,%i,%i)\n", c->old_x, c->old_y, c->old_width, c->old_height);
            fprintf(f, "  [DESK] %i\n", c->win_workspace);
            fprintf(f, "  [FLAGS] 0x%lx\n", CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY | CLIENT_FLAG_HIDDEN | CLIENT_FLAG_SHADED | CLIENT_FLAG_MAXIMIZED | CLIENT_FLAG_NAME_CHANGED));
        }
        fclose(f);
        return TRUE;
    }
    return FALSE;
}

gboolean sessionLoadWindowStates(gchar * filename)
{
    FILE *f;
    char s[4096], s1[4096];
    int i, pos, pos1;
    unsigned long w;

    g_return_val_if_fail(filename != NULL, FALSE);
    if((f = fopen(filename, "r")))
    {
        while(fgets(s, sizeof(s), f))
        {
            sscanf(s, "%4000s", s1);
            if(!strcmp(s1, "[CLIENT]"))
            {
                sscanf(s, "%*s %lx", &w);
                num_match++;
                matches = g_realloc(matches, sizeof(Match) * num_match);
                matches[num_match - 1].win = w;
                matches[num_match - 1].client_id = NULL;
                matches[num_match - 1].res_name = NULL;
                matches[num_match - 1].res_class = NULL;
                matches[num_match - 1].window_role = NULL;
                matches[num_match - 1].wm_name = NULL;
                matches[num_match - 1].wm_command_count = 0;
                matches[num_match - 1].wm_command = NULL;
                matches[num_match - 1].x = 0;
                matches[num_match - 1].y = 0;
                matches[num_match - 1].width = 100;
                matches[num_match - 1].height = 100;
                matches[num_match - 1].old_x = matches[num_match - 1].x;
                matches[num_match - 1].old_y = matches[num_match - 1].y;
                matches[num_match - 1].old_width = matches[num_match - 1].width;
                matches[num_match - 1].old_height = matches[num_match - 1].height;
                matches[num_match - 1].desktop = 0;
                matches[num_match - 1].used = FALSE;
                matches[num_match - 1].flags = 0;
            }
            else if(!strcmp(s1, "[GEOMETRY]"))
            {
                sscanf(s, "%*s (%i,%i,%i,%i)", &(matches[num_match - 1].x), &(matches[num_match - 1].y), &(matches[num_match - 1].width), &(matches[num_match - 1].height));
            }
            else if(!strcmp(s1, "[GEOMETRY-MAXIMIZED]"))
            {
                sscanf(s, "%*s (%i,%i,%i,%i)", &(matches[num_match - 1].old_x), &(matches[num_match - 1].old_y), &(matches[num_match - 1].old_width), &(matches[num_match - 1].old_height));
            }
            else if(!strcmp(s1, "[DESK]"))
            {
                sscanf(s, "%*s %i", &(matches[num_match - 1].desktop));
            }
            else if(!strcmp(s1, "[CLIENT_LEADER]"))
            {
                sscanf(s, "%*s 0x%lx", &(matches[num_match - 1].client_leader));
            }
            else if(!strcmp(s1, "[FLAGS]"))
            {
                sscanf(s, "%*s 0x%lx", &(matches[num_match - 1].flags));
            }
            else if(!strcmp(s1, "[CLIENT_ID]"))
            {
                sscanf(s, "%*s %[^\n]", s1);
                matches[num_match - 1].client_id = strdup(s1);
            }
            else if(!strcmp(s1, "[WINDOW_ROLE]"))
            {
                sscanf(s, "%*s %[^\n]", s1);
                matches[num_match - 1].window_role = strdup(s1);
            }
            else if(!strcmp(s1, "[RES_NAME]"))
            {
                sscanf(s, "%*s %[^\n]", s1);
                matches[num_match - 1].res_name = strdup(s1);
            }
            else if(!strcmp(s1, "[RES_CLASS]"))
            {
                sscanf(s, "%*s %[^\n]", s1);
                matches[num_match - 1].res_class = strdup(s1);
            }
            else if(!strcmp(s1, "[WM_NAME]"))
            {
                sscanf(s, "%*s %[^\n]", s1);
                matches[num_match - 1].wm_name = strdup(s1);
            }
            else if(!strcmp(s1, "[WM_COMMAND]"))
            {
                sscanf(s, "%*s (%i)%n", &matches[num_match - 1].wm_command_count, &pos);
                matches[num_match - 1].wm_command = g_new(gchar *, matches[num_match - 1].wm_command_count + 1);
                for(i = 0; i < matches[num_match - 1].wm_command_count; i++)
                {
                    gchar *substring;
                    substring = getsubstring(s + pos, &pos1);
                    pos += pos1;
                    matches[num_match - 1].wm_command[i] = unescape_quote(substring);
                    g_free(substring);
                }
                matches[num_match - 1].wm_command[matches[num_match - 1].wm_command_count] = NULL;
            }
        }
        fclose(f);
        return TRUE;
    }
    return FALSE;
}

void sessionFreeWindowStates(void)
{
    int i;
    for(i = 0; i < num_match; i++)
    {
        if(matches[i].client_id)
        {
            free(matches[i].client_id);
            matches[i].client_id = NULL;
        }
        if(matches[i].res_name)
        {
            free(matches[i].res_name);
            matches[i].res_name = NULL;
        }
        if(matches[i].res_class)
        {
            free(matches[i].res_class);
            matches[i].res_class = NULL;
        }
        if(matches[i].window_role)
        {
            free(matches[i].window_role);
            matches[i].window_role = NULL;
        }
        if(matches[i].wm_name)
        {
            free(matches[i].wm_name);
            matches[i].wm_name = NULL;
        }
        if((matches[i].wm_command_count) && (matches[i].wm_command))
        {
            my_free_string_list(matches[i].wm_command, matches[i].wm_command_count);
            g_free(matches[i].wm_command);
            matches[i].wm_command_count = 0;
            matches[i].wm_command = NULL;
        }
    }
    if(matches)
    {
        g_free(matches);
        matches = NULL;
        num_match = 0;
    }
}

/* This complicated logic is from twm, where it is explained */

#define xstreq(a,b) ((!a && !b) || (a && b && (strcmp(a,b)==0)))

static gboolean matchWin(Client * c, Match * m)
{
    char *client_id = NULL;
    char *window_role = NULL;
    int wm_command_count = 0;
    char **wm_command = NULL;
    gboolean found;
    int i;

    g_return_val_if_fail(c != NULL, FALSE);

    found = FALSE;
    getClientID(dpy, c->window, &client_id);
    if(xstreq(client_id, m->client_id))
    {
        /* client_id's match */
        if(c->client_leader != None)
        {
            getWindowRole(dpy, c->client_leader, &window_role);
        }
        else
        {
            window_role = NULL;
        }
        if((window_role) || (m->window_role))
        {
            /* We have or had a window role, base decision on it */
            found = xstreq(window_role, m->window_role);
        }
        else
        {
            /* Compare res_class, res_name and WM_NAME, unless the
             * WM_NAME has changed
             */
            if(xstreq(c->class.res_name, m->res_name) && (CLIENT_FLAG_TEST(c, CLIENT_FLAG_NAME_CHANGED) || (m->flags & CLIENT_FLAG_NAME_CHANGED) || xstreq(c->name, m->wm_name)))
            {
                if(client_id)
                {
                    /* If we have a client_id, we don't compare
                       WM_COMMAND, since it will be different. */
                    found = TRUE;

                }
                else
                {
                    /* for non-SM-aware clients we also compare WM_COMMAND */
                    wm_command_count = 0;
                    getWindowCommand(dpy, c->window, &wm_command, &wm_command_count);
                    if(wm_command_count == m->wm_command_count)
                    {
                        for(i = 0; i < wm_command_count; i++)
                        {
                            if(strcmp(wm_command[i], m->wm_command[i]) != 0)
                                break;
                        }

                        if((i == wm_command_count) && (wm_command_count))
                        {
                            found = TRUE;
                        }
                    }           /* if (wm_command_count ==... */
                    /* We have to deal with a now-SM-aware client, it means that it won't probably
                     * restore its state in a proper manner.
                     * Thus, we also mark all other instances of this application as used, to avoid
                     * dummy side effects in case we found a matching entry.
                     */
                    if(found)
                    {
                        for(i = 0; i < num_match; i++)
                        {
                            if(!(matches[i].used) && !(&matches[i] == m) && (m->client_leader) && (matches[i].client_leader == m->client_leader))
                            {
                                matches[i].used = TRUE;
                            }
                        }
                    }
                }
            }
        }
    }

    if(client_id)
    {
        XFree(client_id);
        client_id = NULL;
    }

    if(window_role)
    {
        XFree(window_role);
        window_role = NULL;
    }

    if((wm_command_count > 0) && (wm_command))
    {
        XFreeStringList(wm_command);
        wm_command = NULL;
        wm_command_count = 0;
    }

    return found;
}

gboolean sessionMatchWinToSM(Client * c)
{
    int i;

    g_return_val_if_fail(c != NULL, FALSE);
    for(i = 0; i < num_match; i++)
    {
        if(!matches[i].used && matchWin(c, &matches[i]))
        {
            matches[i].used = TRUE;
            c->x = matches[i].x;
            c->y = matches[i].y;
            c->width = matches[i].width;
            c->height = matches[i].height;
            c->old_x = matches[i].old_x;
            c->old_y = matches[i].old_y;
            c->old_width = matches[i].old_width;
            c->old_height = matches[i].old_height;
            c->win_workspace = matches[i].desktop;
            CLIENT_FLAG_SET(c, matches[i].flags & (CLIENT_FLAG_STICKY | CLIENT_FLAG_SHADED | CLIENT_FLAG_MAXIMIZED | CLIENT_FLAG_HIDDEN));
            CLIENT_FLAG_SET(c, CLIENT_FLAG_WORKSPACE_SET);
            return TRUE;
        }
    }
    return FALSE;
}
