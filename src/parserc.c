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
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "parserc.h"
#include "settings.h"
#include "debug.h"

#define DEFAULT_THEME "exocet"

gboolean parseRc(gchar * file, gchar * dir, Settings rc[])
{
    gchar filename[512], buf[512], *lvalue, *rvalue;
    FILE *fp;
    gint i;

    DBG("entering parseRc\n");

    if(dir)
    {
        g_snprintf(filename, sizeof(filename), "%s/%s", dir, file);
    }
    else
    {
        strncpy(filename, file, sizeof(filename));
    }
    fp = fopen(filename, "r");
    if(!fp)
    {
        return FALSE;
    }
    while(fgets(buf, sizeof(buf), fp))
    {
        lvalue = strtok(buf, "=");
        if(lvalue)
        {
            for(i = 0; rc[i].option; i++)
            {
                if(!g_ascii_strcasecmp(lvalue, rc[i].option))
                {
                    rvalue = strtok(NULL, "\n");
                    if(rvalue)
                    {
                        if(rc[i].value)
                        {
                            g_free(rc[i].value);
                        }
                        rc[i].value = g_strdup(rvalue);
                        DBG("%s=%s\n", rc[i].option, rc[i].value);
                    }
                }
            }
        }
    }
    fclose(fp);
    return TRUE;
}

gboolean checkRc(Settings rc[])
{
    gint i;
    gboolean rval = True;

    DBG("entering checkRc\n");

    for(i = 0; rc[i].option; i++)
    {
        if(rc[i].required && !rc[i].value)
        {
            fprintf(stderr, "missing value for option %s\n", rc[i].option);
            rval = FALSE;
        }
    }
    return rval;
}

gchar *getValue(gchar * option, Settings rc[])
{
    gint i;

    DBG("entering getValue\n");

    for(i = 0; rc[i].option; i++)
    {
        if(!g_ascii_strcasecmp(option, rc[i].option))
        {
            return rc[i].value;
        }
    }
    return NULL;
}

gchar *getThemeDir(const gchar *theme)
{
    if (!theme)
    {
        return g_strdup_printf("%s/themes/%s", DATADIR, DEFAULT_THEME);
    }
    else if (g_path_is_absolute(theme))
    {
        if (g_file_test(theme, G_FILE_TEST_IS_DIR))
	{
	    return g_strdup (theme);
	}
	else
	{
	    return g_strdup_printf("%s/themes/%s", DATADIR, DEFAULT_THEME);
	}
    }
    else
    {
        gchar *test = g_strdup_printf("%s/.themes/xfwm4/%s", g_get_home_dir (), theme);
	if (g_file_test(test, G_FILE_TEST_IS_DIR))
	{
	    return test;
	}
	g_free (test);
        test = g_strdup_printf("%s/themes/%s", DATADIR, theme);
	if (g_file_test(test, G_FILE_TEST_IS_DIR))
	{
	    return test;
	}
	g_free (test);
    }
    return g_strdup_printf("%s/themes/%s", DATADIR, DEFAULT_THEME);
}

void freeRc(Settings rc[])
{
    gint i;

    DBG("entering freeRc\n");

    for(i = 0; rc[i].option; i++)
    {
        g_free(rc[i].value);
    }
}
