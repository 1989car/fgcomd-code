
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

#ifndef _conference_h
#define _conference_h

// conference struct
struct conference
{
	GThread *thread;
	GCond *input_ready;
	GMutex *lock;
	gint running;			  // atomic r/w
	guint8 type;
	gint id;
	GSList *userlist;

	// data
	guint usercount;		  //atomic r/w
	guint speakers;		  //atomic r/w
	guint listeners;		  //atomic r/w

	// distance
	GTimeVal distance_timestamp;
	GMutex *distance_lock;
	GHashTable *distance_matrix;
	GThread *distance_thread;
};

enum conference_type
{
	FGC_CONF_NORMAL,
	FGC_CONF_DISTANCE,
	FGC_CONF_ECHO
};

/* public prototypes */
GHashTable *conference_init_conferences (void);
void conference_destroy_conferences (void);
struct conference *conference_find (gint id);
struct conference *conference_new (gint id);
gboolean conference_user_join (struct user *u);
gboolean conference_user_remove (struct user *u);
void conference_kill (struct conference *c);
void conference_for_every_conference_signal (gpointer key, gpointer value,
									gpointer user_data);
void conference_count (struct conference *c);

/* private prototypes */
static void _conference_count (struct conference *c);
static void _conference_kill (gpointer value, gpointer user_data);
static void _conference_user_change (gpointer value, gpointer user_data);
static void _conference_for_every_user_count (gpointer value,
									 gpointer user_data);
static gboolean _conference_remove (struct conference *c);
static void _conference_normal_for_every_user (gpointer value,
									  gpointer user_data);
static void _conference_for_every_user_signal (gpointer value,
									  gpointer user_data);
static void _conference_echo_mix_frame (gpointer value, gpointer user_data);
static void _conference_normal_mix_frame (gpointer value, gpointer user_data);
static void _conference_distance_mix_frame (gpointer value,
								    gpointer user_data);
static void _conference_for_every_user_signal (gpointer value,
									  gpointer user_data);
static void _conference_thread (struct conference *c);

#endif
