
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
#include "packet.h"

static guint32 packet_id = 0;

/****************************************************************************
 *  public functions
 ****************************************************************************/

struct packet *
packet_create (gint command, glong conference_id, ...)
{
	va_list argPtr;
	struct packet *ret = NULL;
	struct packet p;
	gchar *tmp = NULL;

	p.version = 1;
	p.packet_id = packet_id;
	p.command = command;
	p.conference_id = conference_id;

	va_start (argPtr, conference_id);

	switch (p.command)
	{
	case FGC_DISCONNECT:
		g_debug ("command: FGC_DISCONNECT(%d):", FGC_DISCONNECT);
		g_debug ("   conference_id=%ld", p.conference_id);
		break;
	case FGC_LISTEN_ONLY:
		break;
	case FGC_SPEAK_ONLY:
		break;
	case FGC_POSITION:
		p.data.position.lat = (gdouble) va_arg (argPtr, gdouble);
		p.data.position.lon = (gdouble) va_arg (argPtr, gdouble);
		p.data.position.altitude = (gdouble) va_arg (argPtr, gdouble);
		p.data.position.heading = (gdouble) va_arg (argPtr, gdouble);
		p.data.position.airspeed = (gdouble) va_arg (argPtr, gdouble);
		p.data.position.groundspeed = (gdouble) va_arg (argPtr, gdouble);

		g_debug ("command: FGC_POSITION(%d):", FGC_POSITION);
		g_debug ("   conference_id=%ld", p.conference_id);
		g_debug ("             lat=%f", p.data.position.lat);
		g_debug ("             lon=%f", p.data.position.lon);
		g_debug ("        altitude=%f", p.data.position.altitude);
		g_debug ("         heading=%f", p.data.position.heading);
		g_debug ("        airspeed=%f", p.data.position.airspeed);
		g_debug ("     groundspeed=%f", p.data.position.groundspeed);
		break;
	case FGC_USER:
		tmp = va_arg (argPtr, gchar *);
		g_strlcpy (p.data.user.callsign, tmp, strlen (tmp) + 1);
		tmp = va_arg (argPtr, gchar *);
		g_strlcpy (p.data.user.model, tmp, strlen (tmp) + 1);
		p.data.user.transponder = (gint) va_arg (argPtr, gint);

		g_debug ("command: FGC_USER(%d):", FGC_USER);
		g_debug ("   conference_id=%ld", p.conference_id);
		g_debug ("        callsign=[%s]", p.data.user.callsign);
		g_debug ("           model=[%s]", p.data.user.model);
		g_debug ("     transponder=%d", p.data.user.transponder);
		break;
	case FGC_TEXT:
		break;
	case FGC_CHAT:
		break;
	case FGC_AUDIO:
		// audio
		memcpy (&p.data.audio.frame, va_arg (argPtr, gchar *), FRAME_SIZE);
		g_debug ("command: FGC_AUDIO(%d): size=%d", FGC_AUDIO, FRAME_SIZE);
		break;
	case FGC_PING:
		g_debug ("command: FGC_PING(%d)", FGC_PING);

		break;
	default:
		g_debug ("unknown command: %d:", p.command);
		break;
	}

	va_end (argPtr);

	// get memory and copy the complete packet
	if ((ret = g_memdup (&p, sizeof (p))) == NULL)
	{
		g_critical ("Cannot g_malloc0() for command %d", command);
	}

	// increment the packet counter
	if (packet_id == 2147483647)
		packet_id = 0;
	else
		packet_id++;

	return (ret);
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
