
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

#ifndef _fgcomd_h
#define _fgcomd_h

// version number
#define VERSION "4.0"

// when do we assume that the client is dead?
#define MAX_PACKET_AGE 10	  // seconds

// size of a frame
#define FRAME_SIZE 256

// sample frequency
#define SAMPLE_FREQUENCY 8000

// length of a frame (usec)
#define FRAME_LENGTH 1000000/(8000/FRAME_SIZE)

// maximum read buffer for network connection
//#define NET_READ_BUFFER 256+FRAME_SIZE
#define NET_READ_BUFFER sizeof(struct packet)

// maximum waiting time [s] for recalculation of the distance matrix
#define DISTANCE_SLEEP 10

// dead zone for AGC
#define AGC_DEAD_ZONE 0.2

// maximum amplification for AGC
#define AGC_MAX 0.8

// stepping for AGC
#define AGC_STEP 3.0

// threshold for AGC
#define AGC_THRESHOLD 0.08

// minimum packats for random squelch noise
#define SQUELCH_MIN_PACKETS 3

// maximum packats for random squelch noise
#define SQUELCH_MAX_PACKETS 10

// maximun distance between two users for mixing
#define MAX_DISTANCE 50

// Maximum noise level
#define MAX_NOISE 0.05

// maximum distortion level
#define MAX_DISTORTION 0.4

#ifdef __DEBUG__
#define THREAD_MUTEX_LOCK(mutex) g_debug("BEFORE THREAD_MUTEX_LOCK (%s)%s:%d",__FILE__,__FUNCTION__,__LINE__);g_mutex_lock(mutex);g_debug("AFTER THREAD_MUTEX_LOCK (%s)%s:%d",__FILE__,__FUNCTION__,__LINE__);
#define THREAD_MUTEX_UNLOCK(mutex) g_debug("BEFORE THREAD_MUTEX_UNLOCK (%s)%s:%d",__FILE__,__FUNCTION__,__LINE__);g_mutex_unlock(mutex);g_debug("AFTER THREAD_MUTEX_UNLOCK (%s)%s:%d",__FILE__,__FUNCTION__,__LINE__);
#define DEBUG_MARKER(d) g_debug("MARKER (%s)%s:%d: %s",__FILE__,__FUNCTION__,__LINE__,d);
#else
#define THREAD_MUTEX_LOCK(mutex) g_mutex_lock(mutex);
#define THREAD_MUTEX_UNLOCK(mutex) g_mutex_unlock(mutex);
#define DEBUG_MARKER(d)
#endif

// type of an audio frame
typedef guint8 frame;

// type of an internal audio frame
typedef gfloat iframe;

extern struct fgcomd_config config;

// public prototypes
void fgcomd_exit (gchar * text, gint exitcode);

// private prototypes
static void _fgcomd_quit (gint exitcode);

#endif
