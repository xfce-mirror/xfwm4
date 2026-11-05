/*
 *  Copyright (c) 2026 Olivier Fourdan <fourdan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include <glib.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "memory.h"

#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT 21
#endif
#ifndef MADV_DONTNEED
#define MADV_DONTNEED 4
#endif

#define PERIODIC_TRIM_INTERVAL_SECONDS 300 /* 5 minutes */

static guint memory_trim_source_id = 0;
static guint periodic_trim_source_id = 0;

static void
xfwm_memory_reclaim_mappings (void)
{
    FILE *f;
    char line[512];

    f = fopen ("/proc/self/maps", "r");
    if (!f)
    {
        return;
    }

    while (fgets (line, sizeof (line), f))
    {
        unsigned long start, end;
        char perms[5];
        char path[256] = {0};
        int n;

        n = sscanf (line, "%lx-%lx %4s %*s %*s %*s %255s", &start, &end, perms, path);
        if (n >= 2)
        {
            /* Exclude special kernel regions that must not be advised */
            if (strstr (path, "[v") != NULL || strstr (path, "[stack") != NULL)
            {
                continue;
            }

            /* Drop clean shared mappings (fontconfig cache, locale, icon caches) */
            if (perms[3] == 's')
            {
                madvise ((void *) start, end - start, MADV_DONTNEED);
            }
#if defined(MADV_PAGEOUT)
            else
            {
                /* Prompt kernel to reclaim cold pages in private anonymous & file mappings */
                madvise ((void *) start, end - start, MADV_PAGEOUT);
            }
#endif
        }
    }

    fclose (f);
}

static gboolean
xfwm_memory_trim_cb (gpointer data)
{
    memory_trim_source_id = 0;
    xfwm_memory_trim ();
    return G_SOURCE_REMOVE;
}

static gboolean
xfwm_periodic_trim_cb (gpointer data)
{
    xfwm_memory_trim ();
    return G_SOURCE_CONTINUE;
}

void
xfwm_memory_init (void)
{
#if defined(HAVE_MALLOPT) && defined(HAVE_MALLOC_H)
#ifdef M_ARENA_MAX
    /* Restrict memory arena count to minimize fragmentation and multi-arena bloat */
    mallopt (M_ARENA_MAX, 2);
#endif
#ifdef M_TRIM_THRESHOLD
    /* Maintain lower trim threshold to release heap pages back to OS */
    mallopt (M_TRIM_THRESHOLD, 64 * 1024);
#endif
#ifdef M_MMAP_THRESHOLD
    /* Allocate larger buffers directly via mmap for immediate kernel return upon free */
    mallopt (M_MMAP_THRESHOLD, 64 * 1024);
#endif
#endif

    /* Setup periodic background trim during low priority idle (every 5 min) */
    periodic_trim_source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                         PERIODIC_TRIM_INTERVAL_SECONDS,
                                                         xfwm_periodic_trim_cb,
                                                         NULL,
                                                         NULL);
}

void
xfwm_memory_trim (void)
{
#if defined(HAVE_MALLOC_TRIM)
    malloc_trim (0);
#endif

    xfwm_memory_reclaim_mappings ();
}

void
xfwm_memory_trim_schedule (guint delay_seconds)
{
    /* Debounce: if a trim is already scheduled, let it run */
    if (memory_trim_source_id != 0)
    {
        return;
    }

    if (delay_seconds == 0)
    {
        memory_trim_source_id = g_idle_add_full (G_PRIORITY_LOW,
                                                xfwm_memory_trim_cb,
                                                NULL,
                                                NULL);
    }
    else
    {
        memory_trim_source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                           delay_seconds,
                                                           xfwm_memory_trim_cb,
                                                           NULL,
                                                           NULL);
    }
}

void
xfwm_memory_cleanup (void)
{
    if (memory_trim_source_id != 0)
    {
        g_source_remove (memory_trim_source_id);
        memory_trim_source_id = 0;
    }
    if (periodic_trim_source_id != 0)
    {
        g_source_remove (periodic_trim_source_id);
        periodic_trim_source_id = 0;
    }
}
