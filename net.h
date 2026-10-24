
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

#ifndef _net_h
#define _net_h

/* public prototypes */
gboolean net_open (void);
void net_dispatcher (void);
void net_close (void);
gboolean net_write (GInetAddr * dest, gchar * buffer, gint size);

/* private prototypes */

#endif
