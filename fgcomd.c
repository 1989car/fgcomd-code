
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
#include <signal.h>
#include <glib.h>
#include <gnet.h>
#include "fgcomd.h"
#include "config.h"
#include "net.h"
#include "user.h"
#include "conference.h"
#include "distance.h"

// global variables
struct fgcomd_config config;
GHashTable *users = NULL;
GMutex *users_lock = NULL;
GHashTable *conferences = NULL;
GMutex *conferences_lock = NULL;
GRand *random_generator = NULL;
GThread *net_thread=NULL;
int
main (int argc, char *argv[])
{
	GError *error = NULL;

	g_printf ("fgcomd Version %s (Revision %s)\n", VERSION, SVN_REV);
	g_printf ("(c)Holger Wirtz <dcoredump@gmail.com>\n\n");

	// parse configuration
	if (config_parse_cmd_options ("/etc/fgcomd.conf", argc, argv) == FALSE)
	{
		fgcomd_exit ("Program stop due to option errors.", 100);
	}

	if (config.verbose == TRUE)
		g_message ("fgcomd starting up...");

	// catch signals
	signal (SIGINT, _fgcomd_quit);
	signal (SIGQUIT, _fgcomd_quit);
	signal (SIGHUP, _fgcomd_quit);

	// initialize random generator
	random_generator = g_rand_new ();

	// init glib thread system
	if (!g_thread_supported ())
		g_thread_init (NULL);

	// check if glib threads got initialized
	//if(g_thread_get_initialized()!=TRUE)
	//   g_critical("Could not initialize GLib-Threads");

	// create users hash
	if ((users = user_init_users ()) == NULL)
		fgcomd_exit ("cannot create users hash", 100);

	// create conferences hash
	if ((conferences = conference_init_conferences ()) == NULL)
		fgcomd_exit ("cannot create conferences hash", 100);

	// open UDP port
	net_open ();

	// start the network dispatcher as a thread
	if ((net_thread =
		g_thread_create ((GThreadFunc) net_dispatcher, NULL, TRUE, &error)) == NULL)
	{
		g_critical ("Cannot start network thread: %s\n",
				  error->message);
				  fgcomd_exit ("cannot work without network", 101);
	}

	// Main Loop for maintanance
	while(42==42)
	{
		sleep(100);
	}
	
	g_assert_not_reached ();
}

/****************************************************************************
 *  public functions
 ****************************************************************************/

void
fgcomd_exit (gchar * text, gint exitcode)
{
	g_assert (text != NULL);

	if (strlen (text) > 0)
		g_message ("Exit: %s\n", text);

	_fgcomd_quit (exitcode);
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
static void
_fgcomd_quit (gint exitcode)
{
	if (config.verbose == TRUE)
		g_message ("shutting down - please wait...\n");

	// destroy users
	user_destroy_users ();

	// destroy conferences
	conference_destroy_conferences ();

	// close network device
	if (config.fgcomd_socket != NULL)
		net_close ();

	exit ((int) exitcode);
}
