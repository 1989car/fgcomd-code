
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

#ifndef _packet_h
#define _packet_h

// packet structs
struct packet_command_position
{
	gdouble lat;
	gdouble lon;
	gdouble altitude;
	gdouble heading;
	gdouble airspeed;
	gdouble groundspeed;
};

struct packet_command_user
{
	gchar model[64];
	gchar callsign[64];
	gint transponder;
};

struct packet_command_text_chat
{
	gchar message[128];
};

struct packet_command_audio
{
	frame frame[FRAME_SIZE];
};

struct packet_command_ping
{
	guint16 packet_id;
};

struct packet
{
	guint16 version;
	guint32 packet_id;
	guint16 command;
	gint conference_id;

	union
	{
		struct packet_command_position position;
		struct packet_command_user user;
		struct packet_command_text_chat text_chat;
		struct packet_command_audio audio;
		struct packet_command_ping ping;
	} data;
};

enum command
{
	FGC_DISCONNECT,
	FGC_LISTEN_ONLY,
	FGC_SPEAK_ONLY,
	FGC_POSITION,
	FGC_USER,
	FGC_TEXT,
	FGC_CHAT,
	FGC_AUDIO,
	FGC_PING
};

/* public prototypes */
struct packet *packet_create (gint command, glong conference_id, ...);

/* private prototypes */

#endif
