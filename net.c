
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
#include "net.h"
#include "user.h"
#include "conference.h"
#include "distance.h"
#include "packet.h"
#include "audio.h"

extern struct fgcomd_config config;
extern GHashTable *users;
extern GMutex *users_lock;
extern struct distance_struct distance;

/****************************************************************************
 *  public functions
 ****************************************************************************/

gboolean
net_open (void)
{
	gnet_init ();

	g_debug ("Hostname: %s", config.hostname);
	g_debug ("Port:     %d", config.port);

	/* Create the address */
	if ((config.fgcomd_addr =
		gnet_inetaddr_new (config.hostname, config.port)) == NULL)
	{
		g_critical ("cannot create network address");
		fgcomd_exit ("stopping now", 100);
	}

	/* Create the socket */
	if ((config.fgcomd_socket =
		gnet_udp_socket_new_full (config.fgcomd_addr, config.port)) == NULL)
	{
		g_critical ("cannot create network address");
		fgcomd_exit ("stopping now", 100);
	}

	return (TRUE);
}

void
net_close (void)
{
	if (config.fgcomd_socket != NULL)
	{
		gnet_udp_socket_delete (config.fgcomd_socket);
		config.fgcomd_socket = NULL;
	}
	if (config.fgcomd_addr != NULL)
	{
		gnet_inetaddr_delete (config.fgcomd_addr);
		config.fgcomd_addr = NULL;
	}
}

void
net_dispatcher (void)
{
	gchar address[22];
	struct user *u = NULL;
	struct conference *c = NULL;
	GInetAddr *src = NULL;
	gint n = -1;
	struct packet *p = NULL;
	gchar data[NET_READ_BUFFER];
	iframe *audio_iframe = NULL;
	GError *error = NULL;

	// the main loop
	while (42 == 42)
	{
		// init buffer
		audio_iframe = NULL;
		src = NULL;

		// read (blocking) from network
		n = gnet_udp_socket_receive (config.fgcomd_socket, data,
							    NET_READ_BUFFER, &src);
		if (n < 0)
		{
			// there was a network read error!
			g_critical ("network read error!");
		}
		else if (n == 0)
		{
			g_warning ("no data from network!");
			continue;
		}

		// build address string (for the hash key)
		g_sprintf (address, "%s:%d", gnet_inetaddr_get_canonical_name (src),
				 gnet_inetaddr_get_port (src));

		// convert the packet data, so we can use it
		p = (struct packet *) data;

		// print some debug information
		g_debug ("got %d bytes of data from %s", n, address);
		g_debug ("	version:   %d", p->version);
		g_debug ("	command:   %d", p->command);
		g_debug ("	packet_id: %d", p->packet_id);
		g_debug ("	conference_id: %ld", p->conference_id);

		// we only know protocol version 1 (until now)
		if (p->version == 1 && p->conference_id >= 0)
		{
			// check for user
			if ((u = user_find (address)) != NULL)
			{

				if (g_atomic_int_get (&u->conference_id) < 0)
				{
					// user exists but conference id is not useful
					g_debug
						("don't know what to do, because the user exists but has a negative conference id, so killing him!");
					user_kill (u);
					continue;
				}
			}
			else
			{
				// create new user
				if ((u = user_new (src, p->conference_id)) == NULL)
				{
					THREAD_MUTEX_LOCK (u->user_data_lock);
					g_critical ("cannot create user %s!", address);
					THREAD_MUTEX_UNLOCK (u->user_data_lock);
					continue;
				}

				// add to the global users hash and start user thread
				if (user_add (u) == FALSE)
				{
					THREAD_MUTEX_LOCK (u->user_data_lock);
					g_error ("cannot add user %s to conference!",
						    u->socket_name);
					THREAD_MUTEX_UNLOCK (u->user_data_lock);

					// kill the user thread
					user_kill (u);
					continue;
				}

				// recalculate the distance matrix
				distance_calculation_for_one_user (u);
			}

			// update timestamp of the user
			THREAD_MUTEX_LOCK (u->in_lock);
			g_get_current_time (&u->timestamp);
			g_debug ("setting timestamp for '%s' (%s) to '%d'",
				    u->callsign, u->socket_name, u->timestamp.tv_sec);
			THREAD_MUTEX_UNLOCK (u->in_lock);

			// detect conference change
			if (u->conference_id != p->conference_id)
			{
				// change detected, remove user from its conference ...
				conference_user_remove (u);

				// ... change the conference_id ...
				g_atomic_int_set (&u->conference_id, p->conference_id);

				// ... and join him to the new one
				conference_user_join (u);
			}

			// check the packet type
			switch (p->command)
			{
			case FGC_DISCONNECT:
				// this is very simple :-)
				user_kill (u);
				break;
			case FGC_LISTEN_ONLY:
				// very simple :-)
				g_atomic_int_set (&u->listen_only, (gint) TRUE);
				break;
			case FGC_SPEAK_ONLY:
				// also very simple :-)
				g_atomic_int_set (&u->speak_only, (gint) TRUE);
				break;
			case FGC_POSITION:
				// lock the position
				THREAD_MUTEX_LOCK (u->position_lock);
				// write the data
				u->lat = p->data.position.lat;
				u->lon = p->data.position.lon;
				u->altitude = p->data.position.altitude;
				u->heading = p->data.position.heading;
				u->airspeed = p->data.position.airspeed;
				u->groundspeed = p->data.position.groundspeed;
				// unlock the position
				THREAD_MUTEX_UNLOCK (u->position_lock);
				break;
			case FGC_USER:
				// lock the user_data
				THREAD_MUTEX_LOCK (u->user_data_lock);
				// write the data
				g_strlcpy (u->callsign, p->data.user.callsign,
						 strlen (p->data.user.callsign) + 1);
				g_strlcpy (u->model, p->data.user.model,
						 strlen (p->data.user.model) + 1);
				u->transponder = p->data.user.transponder;
				// unlock the user_data
				THREAD_MUTEX_UNLOCK (u->user_data_lock);
				break;
			case FGC_TEXT:
				break;
			case FGC_CHAT:
				break;
			case FGC_AUDIO:
				c = g_atomic_pointer_get (&u->conference);
				if (c != NULL)
				{
					// check if there is more than one member in the conference
					if (g_atomic_int_get (&c->usercount) > 1)
					{
						// create a buffer for the iframe
						if ((audio_iframe =
							audio_frame2iframe ((frame *) p->data.
											audio.frame,
											FRAME_SIZE)) == NULL)
						{
							g_critical
								("cannot convert audio frame to iframe for %s",
								 address);
							fgcomd_exit ("stopping now", 10);
						}
						// normalize audio level and make some sound changes
						/*audio_amp (u->audio_amp_factor, audio_iframe,
						   FRAME_SIZE);
						   u->audio_amp_factor =
						   audio_calculate_amp_factor
						   (u->audio_amp_factor, audio_iframe,
						   FRAME_SIZE); */

						// lock the in queue
						THREAD_MUTEX_LOCK (u->in_lock);

						// add iframe to in queue
						g_queue_push_tail (u->in, audio_iframe);

						// unlock the queue
						THREAD_MUTEX_UNLOCK (u->in_lock);
					}
				}
				break;
			case FGC_PING:
				break;
			default:
				g_debug ("got unknown command (%d) from %s", p->command,
					    address);
			}
			g_debug ("end of command checking");
		}
	}

	g_debug ("F**K! THIS SHOULD NOT HAPPEN! [net_dispatch end]");

	// close UDP port
	net_close ();

	g_assert_not_reached ();
}

gboolean
net_write (GInetAddr * dest, gchar * buffer, gint size)
{
	g_assert (buffer != NULL);

	if (dest != NULL)
	{
		if (gnet_udp_socket_send (config.fgcomd_socket, buffer, size, dest)
		    != 0)
		{
			g_warning ("cannot send data packet!");
			return (FALSE);
		}
		else
		{
			if (buffer != NULL)
				g_free (buffer);
		}
	}
	else
	{
		g_message ("cannot send to NULL address!");
		return (FALSE);
	}

	return (TRUE);
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
