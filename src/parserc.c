/*
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; You may only use version 2 of the License,
        you have no option to use any other version.
 
        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
 
        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <libxfce4util/debug.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "parserc.h"
#include "settings.h"

#ifndef DEFAULT_THEME
#define DEFAULT_THEME "default"
#endif

gboolean
parseRc (const gchar * file, const gchar * dir, Settings rc[])
{
    gchar buf[80];
    gchar *filename, *lvalue, *rvalue;
    FILE *fp;

    TRACE ("entering parseRc");

    g_return_val_if_fail (file != NULL, FALSE);

    if (dir)
    {
        filename = g_build_filename (dir, G_DIR_SEPARATOR_S, file, NULL);
    }
    else
    {
        filename = g_strdup (file);
    }

    fp = fopen (filename, "r");
    g_free (filename);

    if (!fp)
    {
        return FALSE;
    }
    while (fgets (buf, sizeof (buf), fp))
    {
        lvalue = strtok (buf, "=");
        rvalue = strtok (NULL, "\n");
        if ((lvalue) && (rvalue))
        {
            setValue (lvalue, rvalue, rc);
        }
    }
    fclose (fp);
    return TRUE;
}

gboolean
checkRc (Settings rc[])
{
    gint i;
    gboolean rval = TRUE;

    TRACE ("entering checkRc");

    for (i = 0; rc[i].option; i++)
    {
        if (rc[i].required && !rc[i].value)
        {
            fprintf (stderr, "missing value for option %s\n", rc[i].option);
            rval = FALSE;
        }
    }
    return rval;
}

gchar *
getValue (const gchar * option, Settings rc[])
{
    gint i;

    TRACE ("entering getValue");

    g_return_val_if_fail (option != NULL, NULL);

    for (i = 0; rc[i].option; i++)
    {
        if (!g_ascii_strcasecmp (option, rc[i].option))
        {
            return rc[i].value;
        }
    }
    return NULL;
}

gboolean
setValue (const gchar * lvalue, const gchar * rvalue, Settings rc[])
{
    gint i;

    TRACE ("entering setValue");

    g_return_val_if_fail (lvalue != NULL, FALSE);
    g_return_val_if_fail (rvalue != NULL, FALSE);

    for (i = 0; rc[i].option; i++)
    {
        if (!g_ascii_strcasecmp (lvalue, rc[i].option))
        {
            if (rvalue)
            {
                if (rc[i].value)
                {
                    g_free (rc[i].value);
                }
                rc[i].value = g_strdup (rvalue);
                TRACE ("%s=%s", rc[i].option, rc[i].value);
                return TRUE;
            }
        }
    }
    return FALSE;
}

gboolean
setBooleanValueFromInt (const gchar * lvalue, int value, Settings rc[])
{
    return setValue (lvalue, value ? "true" : "false", rc);
}

gboolean
setIntValueFromInt (const gchar * lvalue, int value, Settings rc[])
{
    gchar *s;
    gboolean result;

    s = g_strdup_printf ("%i", value);
    result = setValue (lvalue, s, rc);
    g_free (s);

    return result;
}

gchar *
getThemeDir (const gchar * theme)
{
    if (!theme)
    {
        return g_build_filename (DATADIR, G_DIR_SEPARATOR_S, "themes",
            G_DIR_SEPARATOR_S, DEFAULT_THEME, NULL);
    }
    else if (g_path_is_absolute (theme))
    {
        if (g_file_test (theme, G_FILE_TEST_IS_DIR))
        {
            return g_strdup (theme);
        }
        else
        {
            return g_build_filename (DATADIR, G_DIR_SEPARATOR_S, "themes",
                G_DIR_SEPARATOR_S, DEFAULT_THEME, NULL);
        }
    }
    else
    {
        gchar *test = g_build_filename (g_get_home_dir (), G_DIR_SEPARATOR_S,
            ".themes",
            G_DIR_SEPARATOR_S, "xfwm4",
            G_DIR_SEPARATOR_S,
            theme, NULL);
        if (g_file_test (test, G_FILE_TEST_IS_DIR))
        {
            return test;
        }
        g_free (test);
        test =
            g_build_filename (DATADIR, G_DIR_SEPARATOR_S, "themes",
            G_DIR_SEPARATOR_S, theme, NULL);
        if (g_file_test (test, G_FILE_TEST_IS_DIR))
        {
            return test;
        }
        g_free (test);
    }
    return g_build_filename (DATADIR, G_DIR_SEPARATOR_S, "themes",
        G_DIR_SEPARATOR_S, DEFAULT_THEME, NULL);
}

void
freeRc (Settings rc[])
{
    gint i;

    TRACE ("entering freeRc");

    for (i = 0; rc[i].option; i++)
    {
        if (rc[i].value)
        {
            g_free (rc[i].value);
            rc[i].value = NULL;
        }
    }
}
