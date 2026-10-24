
/*
 * listener: radio conference testing tool for FlightGear fgcomd
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
#include <AL/al.h>
#include <AL/alut.h>
#include "net.h"
#include "fgcomd.h"
#include "packet.h"

#define ALARM_TIMER  7

#define OPENAL_SAMPLERATE 8000
#define OPENAL_FORMAT AL_FORMAT_MONO8

GInetAddr *server_socket = NULL;
GUdpSocket *client_socket = NULL;
GMutex *out_lock = NULL;
GCond *output_ready = NULL;
GQueue *out = NULL;
GQueue *out_processed = NULL;
gint conf = -1;
ALuint openal_error = 0;

/* public prototypes */
void listener_exit (gchar * text, gint exitcode);

/* private prototypes */
static void _listener_quit (gint exitcode);
static void _listener_alarm (int signal);
static void _listener_openal_playback_thread (void);

int
main (int argc, char *argv[])
{
	gint n = 0;
	gint i = 0;
	struct packet *packet = NULL;
	gchar data[NET_READ_BUFFER];
	gchar address[22];
	ALuint *openal_buffer_p = NULL;
	GError *glib_error = NULL;

	signal (SIGINT, _listener_quit);
	signal (SIGQUIT, _listener_quit);
	signal (SIGHUP, _listener_quit);
	signal (SIGALRM, _listener_alarm);

	// init glib thread system
	if (!g_thread_supported ())
		g_thread_init (NULL);

	// check if glib threads got initialized
	//if(g_thread_get_initialized()!=TRUE)
	//       g_critical("Could not initialize GLib-Threads");

	g_assert ((out_lock = g_mutex_new ()) != NULL);
	g_assert ((output_ready = g_cond_new ()) != NULL);

	alarm (ALARM_TIMER);

	// create the output queue
	if ((out = g_queue_new ()) == NULL)
	{
		g_critical ("cannot create output queue");
		exit (100);
	}
	if ((out_processed = g_queue_new ()) == NULL)
	{
		g_critical ("cannot create out_processed queue");
		exit (101);
	}

	// initialize OpenAL
	alutInit (&argc, argv);

	// start playback thread
	if (g_thread_create
	    ((GThreadFunc) _listener_openal_playback_thread, NULL, TRUE,
		&glib_error) == NULL)
	{
		g_critical ("Cannot start conference thread: %s\n",
				  glib_error->message);
		exit (102);
	}

	if ((client_socket = gnet_udp_socket_new ()) == NULL)
	{
		listener_exit ("Cannot create client socket.", 103);
	}

	if ((server_socket =
		gnet_inetaddr_new (argv[1], atoi (argv[2]))) == NULL)
	{
		listener_exit ("Cannot create server socket.", 104);
	}

	// set the address string
	g_sprintf (address, "%s:%d", argv[1], atoi (argv[2]));

	conf = (gint) atoi (argv[3]);

	_listener_alarm (SIGALRM);

	while (1)
	{
		// blocking read
		if ((n =
			gnet_udp_socket_receive (client_socket, data, sizeof (data),
								NULL)) < 0)
		{
			// there was a network read error!
			g_critical ("Error reading from network\n");
		}

		// convert the packet data, so we can use it
		packet = (struct packet *) data;

		// print some debug information
		g_debug ("got %d bytes of data from %s", n, address);
		g_debug ("       version:   %d", packet->version);
		g_debug ("       command:   %d", packet->command);
		g_debug ("       packet_id: %d", packet->packet_id);
		g_debug ("   conference_id: %ld", packet->conference_id);

		if (packet->version == 1 && n > 0)
		{
			// check the packet type
			switch (packet->command)
			{
			case FGC_AUDIO:
				g_debug ("got audio frame (%d) from server",
					    packet->command);

				// create an output Buffer
				if ((openal_buffer_p =
					g_malloc (sizeof (ALuint))) == NULL)
				{
					g_critical ("cannot malloc for Buffer");
					exit (105);
				}

				alGenBuffers (1, openal_buffer_p);
				if (alGetError () != AL_NO_ERROR)
				{
					fprintf (stderr, "Cannot create Buffer\n");
					exit (106);
				}

				// fill the output buffer
				alBufferData (*openal_buffer_p, OPENAL_FORMAT,
						    packet->data.audio.frame, FRAME_SIZE,
						    OPENAL_SAMPLERATE);
				if ((openal_error = alGetError ()) != AL_NO_ERROR)
				{
					g_critical ("cannot fill buffer: 0x%x\n",
							  openal_error);
					exit (107);
				}

				// add the Buffer to the end of the queue
				g_mutex_lock (out_lock);
				g_queue_push_tail (out, openal_buffer_p);
				g_cond_signal (output_ready);
				g_mutex_unlock (out_lock);
				break;
			default:
				g_debug ("got unknown command (%d) from server",
					    packet->command);
				break;
			}
		}
	}

	g_assert_not_reached ();
}

/****************************************************************************
 *  public functions
 ****************************************************************************/
void
listener_exit (gchar * text, gint exitcode)
{
	g_assert (text != NULL);

	if (strlen (text) > 0)
		g_fprintf (stderr, "\nExit: %s\n", text);

	_listener_quit (exitcode);
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
static void
_listener_quit (gint exitcode)
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
_listener_alarm (int signal)
{
	struct packet *p;

	//if((p=packet_create(FGC_POSITION,conf,37.725498,-122.215940,100.0,210.0,0.0,0.0))!=NULL)
	if ((p =
		packet_create (FGC_POSITION, conf, 37.618500, -122.374525, 100.0,
					210.0, 0.0, 0.0)) != NULL)
	{
		gnet_udp_socket_send (client_socket, (gchar *) p, sizeof (*p),
						  server_socket);
		g_free (p);
	}

	if ((p =
		packet_create (FGC_USER, conf, "listener", "LISTENER", 0)) != NULL)
	{
		gnet_udp_socket_send (client_socket, (gchar *) p, sizeof (*p),
						  server_socket);
	}

	if ((p = packet_create (FGC_LISTEN_ONLY, conf)) != NULL)
	{
		gnet_udp_socket_send (client_socket, (gchar *) p, sizeof (*p),
						  server_socket);
		g_free (p);
	}

	// retrigger alarm
	alarm (ALARM_TIMER);
}

static void
_listener_openal_playback_thread (void)
{
	ALuint openal_buffer_processed = 0;
	ALuint openal_source = 0;
	ALuint openal_source_status = 0;
	ALuint *openal_buffer_p = NULL;
	ALuint openal_error = 0;
	gint i = 0;
	gint n = 0;

	// Query version information
	g_debug ("Version:    %s", alGetString (AL_VERSION));
	g_debug ("Renderer:   %s", alGetString (AL_RENDERER));
	g_debug ("Vendor:     %s", alGetString (AL_VENDOR));
	g_debug ("Extensions: %s", alGetString (AL_EXTENSIONS));

	alGetError ();			  // clear any error messages

	alGenSources (1, &openal_source);
	if (alGetError () != AL_NO_ERROR)
	{
		fprintf (stderr, "cannot create source\n");
		exit (108);
	}
	// lock the output queue
	g_mutex_lock (out_lock);

	// main output loop
	while (42 == 42)
	{
		// wait for a signal to process outgoing frames
		g_cond_wait (output_ready, out_lock);

		// get the queue length
		if ((n = g_queue_get_length (out)) > 0)
		{
			g_debug ("playing %d buffers", n);

			// play all buffer in ready state
			for (i = 0; i < n; i++)
			{
				if ((openal_buffer_p = g_queue_pop_head (out)) != NULL)
				{
					alSourceQueueBuffers (openal_source, 1,
									  openal_buffer_p);
					if (alGetError () != AL_NO_ERROR)
					{
						g_critical ("cannot queue buffer");
						exit (109);
					}

					// add the buffer to the processed queue (for alter freeing it)
					g_queue_push_tail (out_processed, openal_buffer_p);
				}
			}
		}
		// unlock the output queue
		g_mutex_unlock (out_lock);

		// get the current status of the source
		alGetSourcei (openal_source, AL_SOURCE_STATE,
				    &openal_source_status);
		if (openal_source_status != AL_PLAYING)
		{
			// start playing
			alSourcePlay (openal_source);
		}

		// clean up old buffers
		alGetSourcei (openal_source, AL_BUFFERS_PROCESSED,
				    &openal_buffer_processed);
		if (alGetError () != AL_NO_ERROR)
		{
			g_critical ("cannot get number of processed buffers");
			exit (110);
		}

		// if there are buffer for freeing
		if (openal_buffer_processed > 0)
		{
			g_debug ("freeing %d buffers", openal_buffer_processed);

			for (i = 0; i < openal_buffer_processed; i++)
			{
				if ((openal_buffer_p =
					g_queue_pop_head (out_processed)) != NULL)
				{
					alSourceUnqueueBuffers (openal_source, 1,
									    openal_buffer_p);
					if (alGetError () != AL_NO_ERROR)
					{
						g_critical ("cannot unqueue buffer");
						exit (111);
					}
					alDeleteBuffers (1, openal_buffer_p);
					if (alGetError () != AL_NO_ERROR)
					{
						g_critical ("cannot delete buffer");
						exit (112);
					}
					g_free (openal_buffer_p);
				}
			}
		}
	}
}
