
/*
 * fgcomd: a radio conference server for FlightGear
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

extern struct fgcomd_config config;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "fgcomd.h"
#include "config.h"

/* command line configuration options */
static GOptionEntry main_entries[] = {
	{"verbose", 'v', 0, G_OPTION_ARG_NONE, &config.verbose,
	 "show more information while working", NULL},
	{"host", 'h', 0, G_OPTION_ARG_CALLBACK, (GOptionArgFunc) _config_cb_host,
	 "server listen hostname (default: localhost)", NULL},
	{"port", 'p', 0, G_OPTION_ARG_INT, &config.port,
	 "listen on port number (default: 6463)", NULL},
	{NULL}
};

/****************************************************************************
 *  public functions
 ****************************************************************************/

/* function for parsing all command line configuration options after reading
   the own configuration file '/etc/fgcomdrc' */
gboolean
config_parse_cmd_options (char *filename, int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;

	/* set the defaults */
	config.verbose = FALSE;
	g_strlcpy (config.hostname, "localhost", sizeof ("localhost"));
	config.port = 6463;

	/* create the cmd line options */
	context = g_option_context_new ("- radio simulation conference server");
	g_option_context_add_main_entries (context, main_entries, NULL);

	/* parse the cmd line options */
	if (!g_option_context_parse (context, &argc, &argv, &error))
	{
		g_print ("option parsing failed: %s\n", error->message);
		return (FALSE);
	}

	return (TRUE);
}

/****************************************************************************
 *  private functions
 ****************************************************************************/

static void
_config_show_version (void)
{
	g_printf ("(c)2006-2011 by H. Wirtz <dcoredump@gmail.com>\n");
	g_printf ("Version %3.2f build TOBEFIXED\n", VERSION);
#ifdef __DEBUG__
	g_printf ("Debug version, build time %s, %s\n", __DATE__, __TIME__);
#endif

	fgcomd_exit ("", 0);
}

static gboolean
_config_cb_host (gchar * option_name, gchar * value, gpointer data,
			  GError ** error)
{
	GError *tmp_error = NULL;

	g_strlcpy (config.hostname, value, sizeof (config.hostname));

	return (TRUE);
}
