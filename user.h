
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

#ifndef _user_h
#define _user_h

// user struct
struct user
{
	// user thread
	GThread *thread;
	gint running;			  // atomic r/w

	// user data
	GMutex *user_data_lock;
	GInetAddr *socket;
	gchar socket_name[22];
	gchar callsign[64];
	gint transponder;
	gchar model[64];
	GTimeVal timestamp;
	gint conference_id;
	struct conference *conference;

	// In-Queue
	GMutex *in_lock;
	GQueue *in;

	// Out-Queue
	GCond *output_ready;
	GMutex *out_lock;
	GQueue *out;

	// position
	GMutex *position_lock;
	gfloat lat;
	gfloat lon;
	gfloat altitude;
	gfloat heading;
	gfloat airspeed;
	gfloat groundspeed;

	// flags
	gint listen_only;
	gint speak_only;

	// audio related stuff
	struct audio_filter *audio_filter;
	gfloat audio_amp_factor;
	gint squelch_frames;
};

/* public prototypes */
GHashTable *user_init_users (void);
void user_destroy_users (void);
struct user *user_find (gchar * address);
struct user *user_new (GInetAddr * src, gint conference_id);
gboolean user_add (struct user *user);
void user_kill (struct user *u);

/* private prototypes */
static gboolean _user_remove (struct user *u);
static void _user_kill (gpointer value, gpointer user_data);
static void _user_thread (struct user *u);

#endif
