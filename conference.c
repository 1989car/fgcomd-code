
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
#include "audio.h"

extern struct conference conf;
extern struct fgcomd_config config;
extern GHashTable *conferences;
extern GMutex *conferences_lock;

/****************************************************************************
 *  public functions
 ****************************************************************************/
GHashTable *
conference_init_conferences (void)
{
	g_assert (conferences == NULL);

	// create conference hash
	if ((conferences = g_hash_table_new (g_int_hash, g_int_equal)) == NULL)
	{
		g_critical ("Cannot create conferences hash!");
	}

	g_assert ((conferences_lock = g_mutex_new ()) != NULL);

	g_debug ("conferences hash created");

	return (conferences);
}

void
conference_destroy_conferences (void)
{
	GList *conference_values;

	g_assert (conferences != NULL);

	g_debug ("destroy global conference hash...");

	// first send all conference threads a kill
	g_debug ("kill all conference threads");

	// get the list of conferences (needed to avoid dead locking)
	THREAD_MUTEX_LOCK (conferences_lock);
	conference_values = g_hash_table_get_values (conferences);
	THREAD_MUTEX_UNLOCK (conferences_lock);

	if (conference_values != NULL)
	{
		// walk through the conferences values and kill the threads
		g_list_foreach (conference_values, _conference_kill, NULL);
	}

	// destroy the hash table
	THREAD_MUTEX_LOCK (conferences_lock);
	g_hash_table_destroy (conferences);
	conferences = NULL;
	THREAD_MUTEX_UNLOCK (conferences_lock);

	// free the global conference mutex
	if (conferences_lock != NULL)
		g_mutex_free (conferences_lock);

	g_debug ("global conference hash destroyed");
}

struct conference *
conference_find (gint id)
{
	struct conference *c = NULL;

	g_assert (conferences != NULL);

	if (id <= 0)
		return (NULL);

	THREAD_MUTEX_LOCK (conferences_lock);
	c = g_hash_table_lookup (conferences, &id);
	THREAD_MUTEX_UNLOCK (conferences_lock);

	return (c);
}

struct conference *
conference_new (gint id)
{
	struct conference *c = NULL;

	g_assert (conferences != NULL);

	g_debug ("creating new conference...");

	// get memory for new conference struct
	if ((c =
		(struct conference *) g_malloc (sizeof (struct conference))) ==
	    NULL)
	{
		g_critical ("Cannot malloc for new conference structure");
	}

	// init mutex's
	g_assert ((c->lock = g_mutex_new ()) != NULL);
	g_assert ((c->distance_lock = g_mutex_new ()) != NULL);

	// create conditions
	g_assert ((c->input_ready = g_cond_new ()) != NULL);

	if ((c->distance_matrix =
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
						   g_free)) == NULL)
	{
		g_critical ("Cannot create distance_matrix hash!");
	}

	// set initial data
	c->id = id;
	//c->type = (guint8) FGC_CONF_NORMAL;
	c->type = (guint8) FGC_CONF_DISTANCE;
	c->userlist = NULL;
	c->running = (gint) FALSE;
	c->usercount = 0;
	c->speakers = 0;
	c->listeners = 0;
	g_get_current_time (&c->distance_timestamp);
	c->distance_thread = NULL;

	g_debug ("conference '%d' created", c->id);

	return (c);
}

gboolean
conference_user_join (struct user * u)
{
	struct conference *c = NULL;
	GError *error = NULL;

	g_assert (conferences != NULL);
	g_assert (u != NULL);

	THREAD_MUTEX_LOCK (u->user_data_lock);
	g_debug ("joining user '%s' (%s) to conference %d...", u->callsign,
		    u->socket_name, u->conference_id);
	THREAD_MUTEX_UNLOCK (u->user_data_lock);

	// check if conference already exists
	if ((c = conference_find (u->conference_id)) == NULL)
	{
		g_debug ("no conference found, creating a new one...");

		// conference does not exists, so we create one
		if ((c = conference_new (u->conference_id)) == NULL)
		{
			g_critical ("Cannot create conference %d!", u->conference_id);
			return (FALSE);
		}

		// prepend (is faster) user to conference list
		// do this _before_ starting the thread!

		THREAD_MUTEX_LOCK (c->lock);
		c->userlist = g_slist_prepend (c->userlist, u);

		THREAD_MUTEX_UNLOCK (c->lock);

		// start conference thread
		if ((c->thread =
			g_thread_create ((GThreadFunc) _conference_thread, c,
						  TRUE, &error)) == NULL)
		{
			g_critical ("Cannot start conference thread: %s\n",
					  error->message);
			return (FALSE);
		}

		// just to be sure that the conference thread is running
		while (g_atomic_int_get (&c->running) == (gint) FALSE)
			g_usleep (100);

		// insert new conference object in global conferences hash
		THREAD_MUTEX_LOCK (conferences_lock);
		g_hash_table_insert (conferences, &c->id, c);
		THREAD_MUTEX_UNLOCK (conferences_lock);
	}
	else
	{
		// prepend (is faster) user to conference list
		THREAD_MUTEX_LOCK (c->lock);
		c->userlist = g_slist_prepend (c->userlist, u);

		THREAD_MUTEX_UNLOCK (c->lock);
	}

	// set the conference address
	g_atomic_pointer_set (&u->conference, c);

	THREAD_MUTEX_LOCK (u->user_data_lock);
	g_debug ("user '%s' (%s) added to conference '%d'", u->socket_name,
		    u->callsign, u->conference_id);
	THREAD_MUTEX_UNLOCK (u->user_data_lock);

	return (TRUE);
}

gboolean
conference_user_remove (struct user * u)
{
	struct conference *c = NULL;

	g_assert (conferences != NULL);
	g_assert (u != NULL);

	THREAD_MUTEX_LOCK (u->user_data_lock);
	g_debug ("removing user '%s' (%s) from conference '%d'...", u->callsign,
		    u->socket_name, u->conference_id);
	THREAD_MUTEX_UNLOCK (u->user_data_lock);

	// only if the conference exists
	if ((c = conference_find (u->conference_id)) != NULL)
	{
		// lock the conference
		THREAD_MUTEX_LOCK (c->lock);

		// remove user from user list
		if (c->userlist != NULL)
			c->userlist = g_slist_remove (c->userlist, u);

		// unlock the conference

		THREAD_MUTEX_UNLOCK (c->lock);

		// update the conference counters
		conference_count (c);

		THREAD_MUTEX_LOCK (u->user_data_lock);
		g_debug ("user '%s' (%s) removed from conference '%d'", u->callsign,
			    u->socket_name, u->conference_id);
		THREAD_MUTEX_UNLOCK (u->user_data_lock);

		return (TRUE);
	}
	else
	{
		g_debug
			("conference '%d' does not exist - cannot remove user '%s' (%s) from conference",
			 u->conference_id, u->socket_name, u->conference_id);
		return (FALSE);
	}
}

void
conference_kill (struct conference *c)
{
	if (g_atomic_int_get (&c->running) == (gint) FALSE)
		return;

	g_debug ("send kill to conference '%d'", c->id);


	THREAD_MUTEX_LOCK (c->lock);

	// set threat to halt
	c->running = (gint) FALSE;

	// send signal to user thread
	g_cond_signal (c->input_ready);


	THREAD_MUTEX_UNLOCK (c->lock);

	if (c->thread != NULL)
	{
		g_debug ("waiting for conference thread to stop");
		g_thread_join (c->thread);
		g_debug ("conference thread stopped");
	}
}

void
conference_for_every_conference_signal (gpointer key, gpointer value,
								gpointer user_data)
{
	gchar *k = (gchar *) key;
	struct conference *c = (struct conference *) value;

	// lock the conference

	THREAD_MUTEX_LOCK (c->lock);

	// send a signal to the conference thread
	g_cond_signal (c->input_ready);

	// unlock the conference

	THREAD_MUTEX_UNLOCK (c->lock);
}

void
conference_count (struct conference *c)
{
	g_assert (c != NULL);

	THREAD_MUTEX_LOCK (c->lock);
	if (c->running == (gint) FALSE)
	{
		c->usercount=0;
		c->listeners=0;
		c->speakers=0;
		THREAD_MUTEX_UNLOCK (c->lock);
		return;
	}

	// init the coutners
	c->speakers = 0;
	c->listeners = 0;

	// check the users
	if ((c->usercount = g_slist_length (c->userlist)) > 1)
	{
		// look for every user and count him
		if(c->userlist!=NULL)
			g_slist_foreach (c->userlist, _conference_for_every_user_count, c);
	}
	//else
	//	c->listeners = 1;

	// send a wakeup signal to conference
	//g_cond_signal (c->input_ready);

	g_debug ("    users in conference '%d': %d", c->id, c->usercount);
	g_debug ("listeners in conference '%d': %d", c->id, c->listeners);
	g_debug (" speakers in conference '%d': %d", c->id, c->speakers);

	THREAD_MUTEX_UNLOCK (c->lock);
}

/****************************************************************************
 *  private functions
 ****************************************************************************/

static void
_conference_kill (gpointer value, gpointer user_data)
{
	struct conference *c = (struct conference *) value;

	if (c != NULL)
		conference_kill (c);
}

static void
_conference_user_change (gpointer value, gpointer user_data)
{
	struct user *u = (struct user *) value;

	g_atomic_int_set (&u->conference_id, -1);
}

static void
_conference_for_every_user_count (gpointer value, gpointer user_data)
{
	struct user *u = (struct user *) value;
	struct conference *c = (struct conference *) user_data;

	// lock the in queue
	THREAD_MUTEX_LOCK (u->in_lock);

	// increase the speakers
	if (g_queue_is_empty (u->in) == TRUE)
	{
		c->listeners++;
	}
	else
	{
		c->speakers++;
	}

	// unlock the in queue
	THREAD_MUTEX_UNLOCK (u->in_lock);
}

static gboolean
_conference_remove (struct conference *c)
{
	// mark the thread as stopped
	c->running = (gint) FALSE;

	g_assert (conferences != NULL);
	g_assert (c != NULL);

	g_debug ("removing conference '%d'...", c->id);

	// mark all users conference as -1
	g_slist_foreach (c->userlist, _conference_user_change, NULL);

	// remove the userlist of this conference
	if (c->userlist != NULL)
	{
		g_slist_free (c->userlist);
		c->userlist = NULL;
	}

	// wait for the distance thread to stop
	if (c->distance_thread != NULL)
	{
		g_debug ("Waiting for distance thread for '%d' to stop", c->id);
		g_thread_join (c->distance_thread);
		g_debug ("distance thread stopped");

		c->distance_thread = NULL;
	}

	if (g_hash_table_remove (conferences, &c->id) != TRUE)
	{
		g_warning
			("cannot remove conference '%d' from global conferences hash!",
			 c->id);
		return (FALSE);
	}
	else
	{
		g_debug ("conference '%d' removed from global conference hash",
			    c->id);
	}

	// remove distance_matrix
	if (c->distance_matrix != NULL)
	{
		g_hash_table_destroy (c->distance_matrix);
		g_debug ("distance matrix removed");
		c->distance_matrix = NULL;
	}

	// remove mutex
	if (c->lock != NULL)
	{

		THREAD_MUTEX_UNLOCK (c->lock);
		g_mutex_free (c->lock);
	}
	if (c->distance_lock != NULL)
	{
		g_mutex_free (c->distance_lock);
	}

	// remove condition
	if (c->input_ready != NULL)
	{
		g_cond_free (c->input_ready);
	}

	g_debug ("conference '%d' removed", c->id);

	// free conference memory
	if (c != NULL)
		g_free (c);

	return (TRUE);
}

static void
_conference_for_every_user_signal (gpointer value, gpointer user_data)
{
	struct user *u = (struct user *) value;

	// lock the users out queue
	THREAD_MUTEX_LOCK (u->out_lock);

	// send a signal to the user thread
	g_cond_signal (u->output_ready);

	// unlock the users out queue
	THREAD_MUTEX_UNLOCK (u->out_lock);
}

static void
_conference_for_every_user (gpointer value, gpointer user_data)
{
	struct user *u = (struct user *) value;	// fixed user
	struct conference *c = (struct conference *) user_data;
	iframe *tmp = NULL;

	// if this user is listen-only there should be no data in the in-queue
	// for processing, so we cann safely return
	if (u->listen_only == (gint) TRUE)
	{
		THREAD_MUTEX_LOCK (u->user_data_lock);
		g_debug ("no incoming data because client '%s' is listen-only",
			    u->callsign);
		THREAD_MUTEX_UNLOCK (u->user_data_lock);
		return;
	}

	// check if there is an incoming iframe
	THREAD_MUTEX_LOCK (u->in_lock);
	if (g_queue_is_empty (u->in) == FALSE)
	{
		// for each user mix the frame
		switch (c->type)
		{
		case FGC_CONF_NORMAL:
			g_slist_foreach (c->userlist, _conference_normal_mix_frame, u);
			break;
		case FGC_CONF_DISTANCE:
			g_slist_foreach (c->userlist, _conference_distance_mix_frame,
						  u);
			break;
		default:
			g_warning ("unknown conference type: %d", c->type);
		}

		// drop the in iframe from user u
		if ((tmp = g_queue_pop_head (u->in)) != NULL)
			g_free (tmp);
	}
	else
	{
		g_debug ("No incoming data for user '%s'", u->socket_name);
	}
	THREAD_MUTEX_UNLOCK (u->in_lock);
}

static void
_conference_echo_mix_frame (gpointer value, gpointer user_data)
{
	struct user *u = (struct user *) value;
	iframe *f_in = (iframe *) user_data;
	iframe *f_out = NULL;
	int i = 0;

	// lock the users out queue
	THREAD_MUTEX_LOCK (u->out_lock);

	// get the user's out iframe
	if ((f_out = g_queue_peek_tail (u->out)) != NULL)
	{
		// only copy the frame to the other members
		memcpy (f_out, f_in, FRAME_SIZE * sizeof (iframe));
	}

	// unlock the users out queue
	THREAD_MUTEX_UNLOCK (u->out_lock);
}

static void
_conference_normal_mix_frame (gpointer value, gpointer user_data)
{
	struct user *u1 = (struct user *) value;	//variable user
	struct user *u2 = (struct user *) user_data;	// fixed user (sender)
	iframe *f_in = NULL;
	iframe *f_out = NULL;
	guint i = 0;

	// same user or user speak-only?
	if (u1 == u2 || g_atomic_int_get (&u1->speak_only) == (gint) TRUE)
		return;

	// get the sending user's in iframe
	if((f_in = g_queue_peek_head (u2->in))==NULL)
		return;

	// lock the receivings user out queue
	THREAD_MUTEX_LOCK (u1->out_lock);

	// check for an outgoing iframe
	if ((f_out = g_queue_pop_tail (u1->out)) == NULL)
	{
		if ((f_out =
			(iframe *) g_memdup (f_in,
							 FRAME_SIZE * sizeof (iframe))) == NULL)
		{
			g_critical
				("Cannot get memory for a copy of an incoming user iframe!");
			return;
		}

	}
	else
	{
		// we have an outgoing iframe, so add the current incoming iframe
		for (i = 0; i < FRAME_SIZE; i++)
		{
			f_out[i] += f_in[i];
		}
	}
	// add the iframe to the outgoing queue
	g_queue_push_tail (u1->out, f_out);

	// unlock the users out queue
	THREAD_MUTEX_UNLOCK (u1->out_lock);
}

static void
_conference_distance_mix_frame (gpointer value, gpointer user_data)
{
	struct user *u1 = (struct user *) value;	//variable user
	struct user *u2 = (struct user *) user_data;	// fixed user (sender)
	iframe *f_in = NULL;
	iframe *f_out = NULL;
	guint i = 0;
	gfloat d = 0.0;
	gfloat dp = 0.0;
	gfloat np = 0;

	// same user or user speak-only?
	if (u1 == u2 || g_atomic_int_get (&u1->speak_only) == (gint) TRUE)
		return;

	// check if both users are in range
	d = distance_get (u1, u2);
	if (d < 0.0 || d > MAX_DISTANCE)
	{
		THREAD_MUTEX_LOCK (u1->user_data_lock);
		THREAD_MUTEX_LOCK (u2->user_data_lock);
		g_debug ("'%s' (%s) and '%s' (%s): out of range", u1->callsign,
			    u1->socket_name, u2->callsign, u2->socket_name);
		THREAD_MUTEX_UNLOCK (u2->user_data_lock);
		THREAD_MUTEX_UNLOCK (u1->user_data_lock);
		return;
	}

	// check the senders incoming queue
	if(g_queue_is_empty (u2->in)==TRUE)
		return;

	// get a copy of the sending user's in iframe - the original iframe must be left intact for others mixes!
	if ((f_in =
		(iframe *) g_memdup (g_queue_peek_head (u2->in),
						 FRAME_SIZE * sizeof (iframe))) == NULL)
	{
		g_critical
			("Cannot get memory for a copy of an incoming user iframe!");
		return;
	}

	// equalizer
	audio_fir_filter (u1->audio_filter, f_in, FRAME_SIZE);

	// calculated some values which are fixed to this frame
	dp = d / MAX_DISTANCE;

	// loudness and distortion
	if (dp > 0.8)
	{
		audio_amp (0.6, f_in, FRAME_SIZE);
		audio_distortion (1.0-MAX_DISTORTION, f_in, FRAME_SIZE);
	}
	else
	{
		audio_amp (1-(0.75*dp), f_in, FRAME_SIZE);
		audio_distortion (dp*MAX_DISTORTION, f_in, FRAME_SIZE);
	}

	// noise
	if (dp > 0.8)
		np = MAX_NOISE;
	else
		np = (1.0-dp) * MAX_NOISE;
	audio_generate_noise (np, f_in, FRAME_SIZE);

	// lock the receivings user out queue
	THREAD_MUTEX_LOCK (u1->out_lock);

	// check for an outgoing iframe
	if (g_queue_is_empty (u1->out) == TRUE)
	{
		// use the copied and scrambled incoming frame
		f_out = f_in;
	}
	else
	{
		// get the last outgoing frame
		f_out = g_queue_pop_tail (u1->out);

		// now we have an outgoing iframe, add the current incoming iframe
		for (i = 0; i < FRAME_SIZE; i++)
		{
			f_out[i] += f_in[i];
		}

		if (f_in != NULL)
			g_free (f_in);
	}

	// add the iframe to the outgoing queue
	g_queue_push_tail (u1->out, f_out);

	// unlock the users out queue
	THREAD_MUTEX_UNLOCK (u1->out_lock);
}

static void
_conference_thread (struct conference *c)
{
	GError *error = NULL;
	GTimeVal start;
	GTimeVal stop_1;
	GTimeVal stop_2;
	GTimeVal distance_wakeup_time;
	gint sleep_time = FRAME_LENGTH;

	g_assert (c != NULL);

	g_message ("starting conference thread 0x%x '%d'",
			 (glong) g_thread_self (), c->id);

	// mark the thread as running
	g_atomic_int_set (&c->running, (gint) TRUE);

	// set the initial wait time for running the first time the distance calculation
	g_get_current_time (&distance_wakeup_time);
	g_time_val_add (&distance_wakeup_time, 2 * 1e6);

	// main thread loop
	while (42 == 42)
	{
		// get the current time
		g_get_current_time (&start);

		// sleep for the (calculated) time of one frame
		g_usleep (sleep_time);

		// count the users
		conference_count (c);
		
		// lock the conference
		THREAD_MUTEX_LOCK (c->lock);

		// block until we can do something
		//g_cond_wait (c->input_ready, c->lock);

		// check if we got a kill
		if (c->running != (gint) TRUE)
		{
			g_debug ("conference thread got a kill");
			break;
		}

		// check if there are no users anymore
		if (c->usercount == 0)
		{
			g_debug ("no users in conference thread '%d'", c->id);
			break;
		}

		// get the current time
		g_get_current_time (&stop_1);

		// start the recalculation of the distance matrix in a thread
		if (stop_1.tv_sec > distance_wakeup_time.tv_sec)
		{
			// start the recalculation
			distance_calculation (c);

			// calculate the time for the next start
			g_time_val_add (&distance_wakeup_time, DISTANCE_SLEEP * 1e6);
		}

		// only mix, when there is a speaker and a listener
		if (c->listeners > 0 && c->speakers > 0)
		{
			switch (c->type)
			{
			case FGC_CONF_ECHO:
				// echo mixing
				break;
			case FGC_CONF_DISTANCE:
			case FGC_CONF_NORMAL:
				// do the mixing for this conference
				g_slist_foreach (c->userlist,
							  _conference_for_every_user, c);
				break;
			default:
				g_warning ("conference type is unknown");
				break;
			}

			// signal the user thread that there is work
			g_slist_foreach (c->userlist,
						  _conference_for_every_user_signal, NULL);
		}
		else
		{
			g_debug
				("No mixing, because no speakers (%d) in this conference (listeners: %d)",
				 c->speakers, c->listeners);
		}

		// unlock the conference
		THREAD_MUTEX_UNLOCK (c->lock);

		// get the current time
		g_get_current_time (&stop_2);

		// evaluate the running time
		g_debug ("conference thread time:         %ld usec",
			    (stop_2.tv_sec - start.tv_sec) * 1000000 + stop_2.tv_usec -
			    start.tv_usec);
		g_debug ("conference thread waiting time: %ld usec",
			    (stop_1.tv_sec - start.tv_sec) * 1000000 + stop_1.tv_usec -
			    start.tv_usec);
		g_debug ("conference thread working time: %ld usec",
			    (stop_2.tv_sec - stop_1.tv_sec) * 1000000 +
			    stop_2.tv_usec - stop_1.tv_usec);

		// calculate new waiting time
		sleep_time -=
			(((stop_2.tv_sec - start.tv_sec) * 1000000 + stop_2.tv_usec -
			  start.tv_usec) - FRAME_LENGTH);

		if (sleep_time < 0)
		{
			g_warning ("conference overload in '%d' (sleep time=%d)",
					 c->id, sleep_time);
			sleep_time = 0;
		}

		g_debug ("new sleeping time for conference '%d': %d usec", c->id,
			    sleep_time);
	}

	g_message ("stopping conference thread '%d'", c->id);

	g_usleep (1e5);

	// clean up conference data
	_conference_remove (c);

	g_debug ("conference thread now stopped");

	// exit thread
	g_thread_exit (0);
}
