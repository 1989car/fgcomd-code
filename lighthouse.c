
/*
 * lighthouse: radio conference testing tool for FlightGear fgcomd
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
#include <signal.h>
#include <glib.h>
#include <gnet.h>
#include "net.h"
#include "fgcomd.h"
#include "data/audiodata.h"
#include "packet.h"

#define ALARM_TIMER  7

GInetAddr *server_socket = NULL;
GUdpSocket *client_socket = NULL;
gint conf = 0;
GRand *random_generator = NULL;

/* public prototypes */
void lighthouse_exit (gchar * text, gint exitcode);

/* private prototypes */
static void _lighthouse_quit (gint exitcode);
static void _lighthouse_alarm (int signal);

int
main (int argc, char *argv[])
{
	gint i = 0;
	struct packet *packet = NULL;

	random_generator = g_rand_new ();

	signal (SIGINT, _lighthouse_quit);
	signal (SIGQUIT, _lighthouse_quit);
	signal (SIGHUP, _lighthouse_quit);
	signal (SIGALRM, _lighthouse_alarm);

	alarm (ALARM_TIMER);

	if ((client_socket = gnet_udp_socket_new ()) == NULL)
	{
		lighthouse_exit ("Cannot create client socket.", 10);
	}

	if ((server_socket =
		gnet_inetaddr_new (argv[1], atoi (argv[2]))) == NULL)
	{
		lighthouse_exit ("Cannot create server socket.", 11);
	}

	conf = atoi (argv[3]);

	_lighthouse_alarm (SIGALRM);

	while (1)
	{
		for (i = 0; i <= audio_data_size; i += FRAME_SIZE)
		{
			// create a packet
			packet = packet_create (FGC_AUDIO, conf, &audio_data[i]);

			//send the data
			gnet_udp_socket_send (client_socket, (gchar *) packet,
							  sizeof (*packet), server_socket);

			if (packet != NULL)
				g_free (packet);

			g_usleep (FRAME_LENGTH);
		}
		sleep (5);
	}
	g_assert_not_reached ();
}

/****************************************************************************
 *  public functions
 ****************************************************************************/
void
lighthouse_exit (gchar * text, gint exitcode)
{
	g_assert (text != NULL);

	if (strlen (text) > 0)
		g_fprintf (stderr, "\nExit: %s\n", text);

	_lighthouse_quit (exitcode);
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
static void
_lighthouse_quit (gint exitcode)
{
	struct packet *p = NULL;

	p = packet_create (FGC_DISCONNECT, conf);

	if (p != NULL)
	{
		gnet_udp_socket_send (client_socket, (gchar *) p, sizeof (*p),
						  server_socket);
		g_free (p);
	}

	gnet_udp_socket_delete (client_socket);
	gnet_inetaddr_delete (server_socket);

	exit ((int) exitcode);
}

static void
_lighthouse_alarm (int signal)
{
	struct packet *p = NULL;

	// fixed position of KSFO Tower
	//if ((p = packet_create (FGC_POSITION, conf, 37.617171, -122.383304, 100.0, 210.0, 0.0, 0.0)) != NULL)
	if ((p =
		packet_create (FGC_POSITION, conf,
					(gfloat) g_rand_double_range (random_generator, 37.3,
											37.8),
					(gfloat) g_rand_double_range (random_generator,
											-122.0, -122.8), 100.0,
					210.0, 0.0, 0.0)) != NULL)
	{
		gnet_udp_socket_send (client_socket, (gchar *) p, sizeof (*p),
						  server_socket);
		g_free (p);
	}

	if ((p =
		packet_create (FGC_USER, conf, "lighthouse", "LIGHTHOUSE",
					0)) != NULL)
	{
		gnet_udp_socket_send (client_socket, (gchar *) p, sizeof (*p),
						  server_socket);
		g_free (p);
	}

	// retrigger alarm
	if ((p = packet_create (FGC_SPEAK_ONLY, conf)) != NULL)
	{
		gnet_udp_socket_send (client_socket, (gchar *) p, sizeof (*p),
						  server_socket);
		g_free (p);
	}

	// retrigger alarm
	alarm (ALARM_TIMER);
}
