
/*
/*
 * fgcomd: radio conference server for FlightGear
 *
 * Copyrights:
 * Copyright (C) 2006-2011 Holger Wirtz   <dcoredump@gmail.com>
 *
 * This program may be modified and distributed under the
 * terms of the GNU General Public License. You should have received
 * a copy of the GNU General Public License along with this
 * program; if not, write to the Free Software Foundation, Inc.
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnet.h>
#include <glib.h>
#include "fgcomd.h"
#include "config.h"
#include "user.h"
#include "conference.h"
#include "distance.h"
#include "packet.h"
#include "audio.h"

extern struct fgcomd_config config;
extern GHashTable *users;
extern GMutex *users_lock;
extern struct distance_struct distance;
extern GRand *random_generator;

/****************************************************************************
 *  public functions
 ****************************************************************************/

GHashTable *
user_init_users (void)
{
	g_assert (users == NULL);

	g_debug ("create global user hash...");

	// create users hash
	if ((users = g_hash_table_new (g_str_hash, g_str_equal)) == NULL)
	{
		g_critical ("Cannot create users hash!");
	}

	g_assert ((users_lock = g_mutex_new ()) != NULL);

	g_debug ("users hash created");

	return (users);
}

void
user_destroy_users (void)
{
	GList *user_values;

	g_assert (users != NULL);

	g_debug ("destroy global user hash...");

	// first send all user threads a kill
	g_debug ("kill all user threads");

	// get the list of users (needed to avoid dead locking)
	THREAD_MUTEX_LOCK (users_lock);
	user_values = g_hash_table_get_values (users);
	THREAD_MUTEX_UNLOCK (users_lock);

	if (user_values != NULL)
	{
		// walk through the user values and kill the threads
		g_list_foreach (user_values, _user_kill, NULL);
	}

	// destroy the hash
	THREAD_MUTEX_LOCK (users_lock);
	g_hash_table_destroy (users);
	users = NULL;
	THREAD_MUTEX_UNLOCK (users_lock);

	// free the global user mutex
	if (users_lock != NULL)
		g_mutex_free (users_lock);

	g_debug ("global user hash destroyed");
}

struct user *
user_find (gchar * address)
{
	struct user *u = NULL;

	g_assert (address != NULL);
	g_assert (users != NULL);

	THREAD_MUTEX_LOCK (users_lock);
	u = g_hash_table_lookup (users, address);
	THREAD_MUTEX_UNLOCK (users_lock);

	return (u);
}

struct user *
user_new (GInetAddr * src, gint conference_id)
{
	struct user *u = NULL;
	gchar address[22];

	g_assert (users != NULL);
	g_assert (src != NULL);

	g_debug ("creating new user...");

	// get memory for new user struct
	if ((u = (struct user *) g_malloc (sizeof (struct user))) == NULL)
	{
		g_critical ("Cannot malloc for new user structure");
	}

	// init mutex's
	g_assert ((u->in_lock = g_mutex_new ()) != NULL);
	g_assert ((u->out_lock = g_mutex_new ()) != NULL);
	g_assert ((u->position_lock = g_mutex_new ()) != NULL);
	g_assert ((u->user_data_lock = g_mutex_new ()) != NULL);

	// create queues
	g_assert ((u->in = g_queue_new ()) != NULL);
	g_assert ((u->out = g_queue_new ()) != NULL);

	// create conditions
	g_assert ((u->output_ready = g_cond_new ()) != NULL);

	// set initial data
	u->socket = src;
	g_sprintf (address, "%s:%d", gnet_inetaddr_get_canonical_name (src),
			 gnet_inetaddr_get_port (src));
	g_strlcpy (u->socket_name, address, sizeof (u->socket_name));
	g_sprintf (u->callsign, "Unknown (%s)", address);
	g_get_current_time (&u->timestamp);
	u->running = (gint) FALSE;
	u->listen_only = (gint) FALSE;
	u->speak_only = (gint) FALSE;
	u->lat = 999.9;
	u->lon = 999.9;
	u->altitude = 0.0;
	u->heading = 0.0;
	u->transponder = 0;
	u->airspeed = 0.0;
	u->groundspeed = 0.0;
	u->conference_id = conference_id;
	u->conference = NULL;
	g_sprintf (u->model, "Unknown");
	u->audio_filter =
		audio_init_filter (HPF, 3.0, 1.0, 700.0, (gfloat) SAMPLE_FREQUENCY);
	u->squelch_frames = 0;
	u->audio_amp_factor = 1.0;

	g_debug ("user '%s' created", u->socket_name);

	return (u);
}

gboolean
user_add (struct user * u)
{
	GError *error = NULL;

	g_assert (u != NULL);
	g_assert (users != NULL);

	g_debug ("adding user '%s' (%s) to conference %d", u->callsign,
		    u->socket_name, u->conference_id);

	// start user thread
	if ((u->thread =
		g_thread_create ((GThreadFunc) _user_thread, u, TRUE,
					  &error)) == NULL)
	{
		g_warning ("Cannot start user thread: %s\n", error->message);
		return (FALSE);
	}

	// insert user object in global users hash
	THREAD_MUTEX_LOCK (users_lock);
	g_hash_table_insert (users, u->socket_name, u);
	THREAD_MUTEX_UNLOCK (users_lock);

	THREAD_MUTEX_LOCK (u->user_data_lock);
	g_debug ("user '%s' (%s) added to global user hash", u->callsign,
		    u->socket_name);
	THREAD_MUTEX_UNLOCK (u->user_data_lock);

	// just to be sure that the user thread is running
	while (g_atomic_int_get (&u->running) == (gint) FALSE)
		g_usleep (100);
	g_debug ("user thread 0x%x status: %d",u->thread,g_atomic_int_get (&u->running));
	
	// join user to conference
	if (conference_user_join (u) == FALSE)
	{
		THREAD_MUTEX_LOCK (u->user_data_lock);
		g_warning ("user '%s' (%s) can't join conference %d",
				 u->callsign, u->socket_name, u->conference_id);
		THREAD_MUTEX_UNLOCK (u->user_data_lock);

		return (FALSE);
	}

	return (TRUE);
}

void
user_kill (struct user *u)
{
	if (g_atomic_int_get (&u->running) == FALSE)
		return;

	THREAD_MUTEX_LOCK (u->user_data_lock);
	g_debug ("send kill to user '%s' (%s)", u->callsign, u->socket_name);
	THREAD_MUTEX_UNLOCK (u->user_data_lock);

	THREAD_MUTEX_LOCK (u->out_lock);

	// set thread to halt
	u->running = (gint) FALSE;

	// send signal to user thread
	g_cond_signal (u->output_ready);

	THREAD_MUTEX_UNLOCK (u->out_lock);

	if(u!=NULL)
	{
		if (u->thread != NULL)
		{
			g_debug ("waiting for user thread to stop");
			g_thread_join (u->thread);
			g_debug ("user thread stopped");
			u->thread = NULL;
		}
	}
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
static gboolean
_user_remove (struct user *u)
{
	iframe *iframe = NULL;
	gboolean tmp = FALSE;

	// mark the thread as stopped
	u->running = (gint) FALSE;

	THREAD_MUTEX_LOCK (u->user_data_lock);
	g_debug ("removing user '%s' (%s)", u->callsign, u->socket_name);
	THREAD_MUTEX_UNLOCK (u->user_data_lock);

	// remove this user from distance matrix
	distance_remove_one_user (u);
	
	// check if we are inside a conference
	if (u->conference_id >= 0)
	{
		// remove user from conference
		if (conference_user_remove (u) == FALSE)
		{
			THREAD_MUTEX_LOCK (u->user_data_lock);
			g_warning
				("cannot remove user '%s' (%s) from conference %d)",
				 u->callsign, u->socket_name, u->conference_id);
			THREAD_MUTEX_UNLOCK (u->user_data_lock);
		}
		else
		{
			g_debug ("user removed from conference '%d'",
				    u->conference_id);
		}
	}
	else
	{
		THREAD_MUTEX_LOCK (u->user_data_lock);
		g_warning
			("negative conference id for user '%s' - don't know what to do",
			 u->socket_name);
		THREAD_MUTEX_UNLOCK (u->user_data_lock);
		return (FALSE);
	}

	// remove user from global users hash
	if (users != NULL)
	{
		THREAD_MUTEX_LOCK (users_lock);
		tmp = g_hash_table_remove (users, u->socket_name);
		THREAD_MUTEX_UNLOCK (users_lock);

		if (tmp != TRUE)
		{
			THREAD_MUTEX_LOCK (u->user_data_lock);
			g_warning
				("cannot remove user '%s' (%s) from global users hash!",
				 u->callsign, u->socket_name);
			THREAD_MUTEX_UNLOCK (u->user_data_lock);
		}
		else
		{
			THREAD_MUTEX_LOCK (u->user_data_lock);
			g_debug ("user '%s' (%s) removed from global users hash",
				    u->socket_name, u->callsign);
			THREAD_MUTEX_UNLOCK (u->user_data_lock);
		}
	}

	// remove condition
	if (u->output_ready != NULL)
	{
		g_cond_free (u->output_ready);
	}

	// destroy in queue and all iframes
	if (u->in != NULL)
	{
		THREAD_MUTEX_LOCK (u->in_lock);

		while (g_queue_is_empty (u->in) != TRUE)
		{
			if ((iframe = g_queue_pop_head (u->in)) != NULL)
				g_free (iframe);
		}

		// destroy the in-queue itself
		if (u->in != NULL)
			g_queue_free (u->in);

		THREAD_MUTEX_UNLOCK (u->in_lock);
	}

	// destroy the in-mutex
	if (u->in_lock != NULL)
	{
		g_mutex_free (u->in_lock);
	}

	// destroy out queue and all iframes
	if (u->out != NULL)
	{
		//THREAD_MUTEX_LOCK (u->out_lock);
		while (g_queue_is_empty (u->out) != TRUE)
		{
			if ((iframe = g_queue_pop_head (u->out)) != NULL)
				g_free (iframe);
		}

		// destroy the out-queue itself
		if (u->out != NULL)
			g_queue_free (u->out);
	}

	// destroy the out-mutex
	if (u->out_lock != NULL)
	{
		THREAD_MUTEX_UNLOCK (u->out_lock);
		g_mutex_free (u->out_lock);
	}

	// destroy the position-mutex
	if (u->position_lock != NULL)
	{
		g_mutex_free (u->position_lock);
	}

	// destroy the user_data-mutex
	if (u->user_data_lock != NULL)
	{
		g_mutex_free (u->user_data_lock);
	}

	// destroy socket address
	if (u->socket != NULL)
	{
		gnet_inetaddr_delete (u->socket);
		u->socket = NULL;
	}

	g_debug ("user '%s' (%s) removed", u->socket_name, u->callsign);

	if (u != NULL)
		g_free (u);

	return (TRUE);

}

static void
_user_kill (gpointer value, gpointer user_data)
{
	struct user *u = (struct user *) value;

	user_kill (u);
}

static void
_user_thread (struct user *u)
{
	GTimeVal current_time;
	GTimeVal timeout;
	iframe *audio_iframe = NULL;
	frame *audio_frame = NULL;
	glong timestamp_secs = 0L;
	struct packet *p = NULL;
	iframe *squelch_noise_iframe = NULL;
	gint tmp_conf_id = -1;

	g_assert (u != NULL);

	g_message ("starting user thread 0x%x for '%s' (%s)",
			 (glong) g_thread_self (), u->callsign, u->socket_name);

	// lock the out queue
	THREAD_MUTEX_LOCK (u->out_lock);

	// mark the thread as running
	u->running = (gint) TRUE;

	// main thread loop
	while (42 == 42)
	{
		// set the timeout
		g_get_current_time (&timeout);
		g_time_val_add (&timeout, MAX_PACKET_AGE * 1000000);

		// block until we can do something
		g_cond_timed_wait (u->output_ready, u->out_lock, &timeout);

		// check if a kill comes in
		if (u->running != (gint) TRUE || u->conference_id <= 0)
		{
			g_debug ("user thread got a kill");
			break;
		}

		// get actual time
		g_get_current_time (&current_time);

		// get the last timestamp - for this the in queue must be
		// locked!
		THREAD_MUTEX_LOCK (u->in_lock);
		timestamp_secs = u->timestamp.tv_sec;
		THREAD_MUTEX_UNLOCK (u->in_lock);

		// check if last timestamp is older than MAX_PACKET_AGE
		if ((current_time.tv_sec - MAX_PACKET_AGE) > timestamp_secs)
		{
			g_message
				("last received packet more than %d seconds ago, stopping thread for '%s' (%s)!",
				 MAX_PACKET_AGE, u->callsign, u->socket_name);
			break;
		}

		// send iframes from the out queue
		if ((audio_iframe = g_queue_pop_head (u->out)) != NULL)
		{
			THREAD_MUTEX_LOCK (u->user_data_lock);
			g_debug ("sending audio data frame to '%s' (%s)", u->callsign,
				    u->socket_name);
			THREAD_MUTEX_UNLOCK (u->user_data_lock);

			// set the squelch counter to a random value
			if (u->squelch_frames == 0)
				u->squelch_frames =
					g_rand_int_range (random_generator,
								   SQUELCH_MIN_PACKETS,
								   SQUELCH_MAX_PACKETS);

			// convert iframe to frame
			audio_frame = audio_iframe2frame (audio_iframe, FRAME_SIZE);

			// discard the old iframe
			if (audio_iframe != NULL)
				g_free (audio_iframe);

			// create the packet
			p = packet_create (FGC_AUDIO, u->conference_id, audio_frame);

			// free the frame!
			if (audio_frame != NULL)
				g_free (audio_frame);

			// send the iframe
			if (p != NULL)
			{
				net_write (u->socket, (gchar *) p, sizeof (*p));
			}
		}
		else
		{
			// no outstanding packets, send squelch noise?
			if (u->squelch_frames > 0)
			{
				THREAD_MUTEX_LOCK (u->user_data_lock);
				g_debug
					("sending squelch audio data frame (%d to go) to '%s' (%s)",
					 u->squelch_frames, u->callsign, u->socket_name);
				THREAD_MUTEX_UNLOCK (u->user_data_lock);

				// decrement the squelch frmae counter
				u->squelch_frames--;

				// generate noise iframe
				squelch_noise_iframe =
					audio_generate_noise_iframe (0.3, FRAME_SIZE);

				// add squelch iframe to this out queue
				g_queue_push_tail (u->out, squelch_noise_iframe);
			}
		}
	}

	THREAD_MUTEX_LOCK (u->user_data_lock)
		g_message ("stopping user thread for '%s' (%s)", u->callsign,
				 u->socket_name);
	THREAD_MUTEX_UNLOCK (u->user_data_lock) tmp_conf_id = u->conference_id;

	// clean up user data
	_user_remove (u);
	g_debug ("user thread now stopped");

	// exit thread
	g_thread_exit (0);
}
