
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

#ifndef _config_h
#define _config_h

#include <glib.h>
#include <gnet.h>

/* configuration data */
struct fgcomd_config
{
	/* group main options */
	gboolean verbose;
	gchar hostname[65];
	gint port;

	/* program internal data */
	GInetAddr *fgcomd_addr;
	GUdpSocket *fgcomd_socket;
} config;

/* public prototypes */
gboolean config_parse_cmd_options (char *filename, int argc, char *argv[]);

/* private prototypes */
static void _config_show_version (void);
static gboolean _config_cb_host (gchar * option_name, gchar * value,
						   gpointer data, GError ** error);

#endif
