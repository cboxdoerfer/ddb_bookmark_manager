/*
    Bookmark Manager Plugin for DeaDBeeF audio player
    Copyright (C) 2014 Christian Boxdörfer <christian.boxdoerfer@posteo.de>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <deadbeef/deadbeef.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

//#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define trace(fmt,...)

#define METADATA_FIELD ":last_playpos"
#define BOOKMARK_FIELD ":bookmarks"
#define MAX_BOOKMARKS 20

static DB_misc_t plugin;
static DB_functions_t *deadbeef;

static char menu_item_title_01[100];
static char bookmarks[MAX_BOOKMARKS][20];
static char name[MAX_BOOKMARKS][20];

static int CONFIG_ENABLED = 0;
static int CONFIG_AUTO_CONTINUE = 0;
static int CONFIG_MIN_DURATION = 0;
static int CONFIG_REWIND_TIME = 0;

DB_plugin_t *
ddb_misc_bookmark_manager_load (DB_functions_t *api)
{
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}

static void
load_config ()
{
    CONFIG_ENABLED =        deadbeef->conf_get_int ("bookmark_manager.save_playpos_enabled", 0);
    CONFIG_AUTO_CONTINUE =  deadbeef->conf_get_int ("bookmark_manager.auto_continue", 0);
    CONFIG_MIN_DURATION =   deadbeef->conf_get_int ("bookmark_manager.min_duration", 0);
    CONFIG_REWIND_TIME =    deadbeef->conf_get_int ("bookmark_manager.rewind_time", 0);
}

static void
get_time (int time, int *hr, int *mn, int *sc)
{
    *hr = time/3600;
    *mn = (time-(*hr)*3600)/60;
    *sc = time-(*hr)*3600-(*mn)*60;
    return 0;
}

static int
bookmark_write_metadata (DB_playItem_t *it)
{
    if (!it) {
        return 0;
    }
    ddb_playlist_t *plt = deadbeef->pl_get_playlist (it);
    if (plt) {
        deadbeef->plt_save_config (plt);
        deadbeef->plt_unref (plt);
    }
    else {
        return 0;
    }
    return 1;
}

static int
bookmark_add (DB_playItem_t *trk, int pos)
{
    if (!trk) {
        return 0;
    }
    char value[20];
    snprintf (value, sizeof (value), "%d", pos);
    deadbeef->pl_append_meta (trk, BOOKMARK_FIELD, value);
    bookmark_write_metadata (trk);
    return 1;
}

static int
bookmark_save ()
{
    DB_playItem_t *it = deadbeef->streamer_get_playing_track ();
    if (!it) {
        return 0;
    }
    if (!CONFIG_ENABLED || deadbeef->pl_get_item_duration (it) < CONFIG_MIN_DURATION) {
        goto out;
    }
    int playpos = deadbeef->streamer_get_playpos ();
    deadbeef->pl_set_meta_int (it, METADATA_FIELD, playpos);
    bookmark_write_metadata (it);
out:
    if (it) {
        deadbeef->pl_item_unref (it);
    }
    return 0;
}

static int
bookmark_resume (ddb_event_track_t *ev)
{
    if (!CONFIG_ENABLED || !CONFIG_AUTO_CONTINUE || !ev->track) {
        return 0;
    }

    int playpos = (deadbeef->pl_find_meta_int (ev->track, METADATA_FIELD, 0) - CONFIG_REWIND_TIME) * 1000;
    if (playpos > 0) {
        deadbeef->sendmessage (DB_EV_SEEK, 0, playpos, 0);
    }
    return 0;
}

static int
bookmark_song_finished (ddb_event_track_t *ev)
{
    if (!ev->track) {
        return 0;
    }
    int playpos = deadbeef->streamer_get_playpos ();
    int duration = deadbeef->pl_get_item_duration (ev->track);

    if (playpos >= duration - 4) {
        deadbeef->pl_set_meta_int (ev->track, METADATA_FIELD, 0);
        bookmark_write_metadata (ev->track);
    }
    return 0;
}

static int
bookmark_message (uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2)
{
    switch (id) {
        case DB_EV_TERMINATE:
        case DB_EV_PAUSE:
        case DB_EV_PAUSED:
        case DB_EV_STOP:
        case DB_EV_NEXT:
        case DB_EV_PREV:
        case DB_EV_PLAY_NUM:
        case DB_EV_PLAY_RANDOM:
            bookmark_save ();
            break;
        case DB_EV_SONGFINISHED:
            bookmark_song_finished ((ddb_event_track_t *)ctx);
            break;
        case DB_EV_SONGSTARTED:
            bookmark_resume ((ddb_event_track_t *)ctx);
            break;
        case DB_EV_CONFIGCHANGED:
            load_config ();
            break;
    }
    return 0;
}

static int
bookmark_start (void)
{
    load_config ();
    return 0;
}

static int
bookmark_stop (void)
{
    trace ("bookmark_stop\n");
    return 0;
}

static DB_playItem_t *
bookmark_get_selected_track (int ctx)
{
    DB_playItem_t *it = NULL;

    if (ctx == DDB_ACTION_CTX_SELECTION) {
        // find first selected
        ddb_playlist_t *plt = deadbeef->plt_get_curr ();
        if (plt) {
            it = deadbeef->plt_get_first (plt, PL_MAIN);
            while (it) {
                if (deadbeef->pl_is_selected (it)) {
                    break;
                }
                DB_playItem_t *next = deadbeef->pl_get_next (it, PL_MAIN);
                deadbeef->pl_item_unref (it);
                it = next;
            }
            deadbeef->plt_unref (plt);
        }
    }
    else if (ctx == DDB_ACTION_CTX_NOWPLAYING) {
        it = deadbeef->streamer_get_playing_track ();
    }
    return it;
}

static int
bookmark_action_select (DB_plugin_action_t *action, int ctx)
{
    DB_playItem_t *it = bookmark_get_selected_track (ctx);
    if (!it) {
        goto out;
    }
    //deadbeef->pl_set_meta_int (it, METADATA_FIELD, 0);
out:
    if (it) {
        deadbeef->pl_item_unref (it);
    }
    return 0;
}

static int
bookmark_action_reset (DB_plugin_action_t *action, int ctx)
{
    DB_playItem_t *it = bookmark_get_selected_track (ctx);
    if (!it) {
        goto out;
    }

    deadbeef->pl_set_meta_int (it, METADATA_FIELD, 0);
out:
    if (it) {
        deadbeef->pl_item_unref (it);
    }
    return 0;
}

static int
bookmark_action_resume (DB_plugin_action_t *action, int ctx)
{
    DB_playItem_t *it = bookmark_get_selected_track (ctx);
    if (!it) {
        goto out;
    }

    int playpos = (deadbeef->pl_find_meta_int (it, METADATA_FIELD, 0) - CONFIG_REWIND_TIME) * 1000;
    playpos = MAX (0, playpos);

    deadbeef->sendmessage (DB_EV_PLAY_NUM, 0, deadbeef->pl_get_idx_of (it), 0);
    deadbeef->sendmessage (DB_EV_SEEK, 0, playpos, 0);
out:
    if (it) {
        deadbeef->pl_item_unref (it);
    }
    return 0;
}

static DB_plugin_action_t bookmarks_action[MAX_BOOKMARKS];

static DB_plugin_action_t
reset_action = {
    .title = "Reset playback position",
    .name = "bookmark_reset",
    .flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_ADD_MENU,
    .callback2 = bookmark_action_reset,
    .next = &bookmarks_action[0]
};

static DB_plugin_action_t
lookup_action = {
    .title = "Resume last at position",
    .name = "bookmark_lookup",
    .flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_ADD_MENU,
    .callback2 = bookmark_action_resume,
    .next = &reset_action
};

static DB_plugin_action_t *
bookmark_get_actions (DB_playItem_t *it)
{
    if (!it ||
        !CONFIG_ENABLED ||
        !deadbeef->pl_meta_exists (it, METADATA_FIELD) ||
        !deadbeef->pl_find_meta_int (it, METADATA_FIELD, 0))
    {
        lookup_action.flags &= ~DB_ACTION_ADD_MENU;
        reset_action.flags &= ~DB_ACTION_ADD_MENU;
        return &lookup_action;
    }
    else
    {
        lookup_action.flags |= DB_ACTION_ADD_MENU;
        reset_action.flags |= DB_ACTION_ADD_MENU;
    }

    int playpos = 0;

    if (it) {
        deadbeef->pl_lock ();
        const char *field = deadbeef->pl_find_meta (it, BOOKMARK_FIELD);
        if (field) {
            int value = 0;
            int value_before = -1;
            int bytes_read = 0;
            int i = 0;
            while (sscanf (field, "%d\n%n", &value, &bytes_read) && i < MAX_BOOKMARKS && value != value_before) {
                value_before = value;
                field += bytes_read;
                int hr, mn, sc = 0;
                get_time (value, &hr, &mn, &sc);
                snprintf (bookmarks[i], sizeof (bookmarks[i]), "Bookmarks/%02d:%02d:%02d", hr, mn, sc);
                bookmarks_action[i].name = name[i];
                bookmarks_action[i].flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_ADD_MENU;
                bookmarks_action[i].callback2 = bookmark_action_select;
                bookmarks_action[i].next = NULL;
                i++;
            }
        }
        else {
            bookmarks_action[0].title = "Bookmarks/---";
            bookmarks_action[0].name = "bookmark_00";
            bookmarks_action[0].flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_ADD_MENU | DB_ACTION_DISABLED;
            bookmarks_action[0].next = NULL;
        }

        playpos = MAX (deadbeef->pl_find_meta_int (it, METADATA_FIELD, 0) - CONFIG_REWIND_TIME, 0);
        deadbeef->pl_unlock ();
    }
    int hr, mn, sc = 0;
    get_time (playpos, &hr, &mn, &sc);
    snprintf (menu_item_title_01, sizeof (menu_item_title_01), "Resume at last position (%02d:%02d:%02d)", hr, mn, sc);
    lookup_action.title = menu_item_title_01;

    return &lookup_action;
}

static const char settings_dlg[] =
    "property \"Enable \" checkbox bookmark_manager.save_playpos_enabled 0;"
    "property \"Automatically continue from last playback position \" checkbox bookmark_manager.auto_continue 0;"
    "property \"Only save playback position of tracks longer than (seconds): \" spinbtn[0,10000,1] bookmark_manager.min_duration 0;"
    "property \"Start playback before saved playback position (seconds): \" spinbtn[0,1000,1] bookmark_manager.rewind_time 0;"
;

// define plugin interface
static DB_misc_t plugin = {
    .plugin.api_vmajor = 1,
    .plugin.api_vminor = 5,
    .plugin.version_major = 0,
    .plugin.version_minor = 1,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.name = "Bookmark Manager",
    .plugin.descr = "Saves and restores playback position of tracks",
    .plugin.copyright =
        "Copyright (C) 2014 Christian Boxdörfer <christian.boxdoerfer@posteo.de>\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website = "http://www.github.com/cboxdoerfer/ddb_bookmark_manager",
    .plugin.start = bookmark_start,
    .plugin.stop = bookmark_stop,
    .plugin.configdialog = settings_dlg,
    .plugin.get_actions = bookmark_get_actions,
    .plugin.message = bookmark_message,
};
