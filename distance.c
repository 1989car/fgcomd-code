
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

#include <glib.h>
#include <gnet.h>
#include <math.h>
#include "fgcomd.h"
#include "user.h"
#include "conference.h"
#include "distance.h"

extern GHashTable *users;
extern GMutex *users_lock;

/****************************************************************************
 *  public functions
 ****************************************************************************/
void
distance_remove_one_user (struct user *u)
{
	struct conference *c = NULL;

	g_assert (users != NULL);
	g_assert (u != NULL);

	THREAD_MUTEX_LOCK (u->user_data_lock);
	g_debug ("removing user '%s' (%s) from distance matrix", u->socket_name,
		    u->callsign);
	THREAD_MUTEX_UNLOCK (u->user_data_lock);

	// get the conference
	if ((c = u->conference) == NULL)
	{
		g_critical ("conference not found!");
		return;
	}

	THREAD_MUTEX_LOCK (c->distance_lock);
	g_hash_table_foreach (c->distance_matrix, _distance_remove_one_user, u);
	THREAD_MUTEX_UNLOCK (c->distance_lock);
}

gfloat
distance_get (struct user * u1, struct user * u2)
{
	gchar distance_key1[43];
	gchar distance_key2[43];
	gfloat *d = NULL;
	struct conference *c = NULL;

	g_assert (u1 != NULL);
	g_assert (u2 != NULL);

	THREAD_MUTEX_LOCK (u1->user_data_lock);
	THREAD_MUTEX_LOCK (u2->user_data_lock);
	g_sprintf (distance_key1, "%s->%s", u1->socket_name, u2->socket_name);
	g_sprintf (distance_key2, "%s->%s", u2->socket_name, u1->socket_name);
	THREAD_MUTEX_UNLOCK (u2->user_data_lock);
	THREAD_MUTEX_UNLOCK (u1->user_data_lock);

	// get the conference
	if ((c = u1->conference) == NULL)
	{
		g_critical ("conference not found!");
		return (-1.0);
	}

	// lock the distance matrix
	THREAD_MUTEX_LOCK (c->distance_lock);

	if ((d = g_hash_table_lookup (c->distance_matrix, distance_key1)) == NULL)
	{
		// try the reversed key
		if ((d =
			g_hash_table_lookup (c->distance_matrix,
							 distance_key2)) == NULL)
			return(-1.0);
	}

	// unlock the distance matrix
	THREAD_MUTEX_UNLOCK (c->distance_lock);

	return (*d);
}

void
distance_calculation (struct conference *c)
{
	GError *error = NULL;

	g_assert (c != NULL);

	if ((c->distance_thread =
		g_thread_create ((GThreadFunc) _distance_calculation, c, TRUE,
					  &error)) == NULL)
	{
		g_critical ("Cannot start distance calculation thread for: %s",
				  error->message);
	}
}

void
distance_calculation_for_one_user (struct user *u)
{
	struct conference *c = NULL;

	GError *error = NULL;

	g_assert (u != NULL);

	if ((c = u->conference) == NULL)
	{
		g_critical ("Cannot find conference!");
		return;
	}

	if ((c->distance_thread =
		g_thread_create ((GThreadFunc) _distance_calculation_for_one_user,
					  u, TRUE, &error)) == NULL)
	{
		g_critical ("Cannot start distance calculation thread for: %s",
				  error->message);
	}
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
static void
_distance_destroy_distance_matrix (GHashTable * distance_matrix)
{
	// remove the hash entries
	g_debug ("removing entries from distance matrix");
	g_hash_table_remove_all (distance_matrix);
	g_debug ("all hash entries removed from distance matrix");
}

static void
_distance_calculate_for_every_user (gpointer value, gpointer user_data)
{
	struct user *u = (struct user *) value;	// fixed user
	struct conference *c = (struct conference *) user_data;

	g_slist_foreach (c->userlist, _distance_calculate_between_users, u);

	g_debug ("number of entries in the distance matrix: %d",
		    g_hash_table_size (c->distance_matrix));
}

static void
_distance_remove_one_user (gpointer key, gpointer value, gpointer user_data)
{
	gchar *k = (gchar *) key;
	float *d = (float *) value;
	struct user *u = (struct user *) user_data;	// fixed user
	struct conference *c = NULL;

	g_assert (u != NULL);
	g_assert (u->socket_name != NULL);

	if (g_strrstr (key, u->socket_name) != NULL)
	{
		g_debug ("'%s' found in key '%s' in distance matrix",
			    u->socket_name, key);

		// get the conference
		if ((c = u->conference) == NULL)
		{
			g_critical ("conference not found!");
			return;
		}

		if (g_hash_table_remove (c->distance_matrix, key) == TRUE)
		{
			g_debug ("removed %s from distance matrix", u->socket_name);
		}
		else
		{
			g_debug ("cannot remove %s from distance matrix", key);
		}
	}
	else
		g_debug ("'%s' NOT found in key '%s' in distance matrix",
			    u->socket_name, key);
}

static void
_distance_calculate_between_users (gpointer value, gpointer user_data)
{
	struct user *u1 = (struct user *) value;	// variable user
	struct user *u2 = (struct user *) user_data;	// fixed user
	struct conference *c = NULL;
	gchar distance_key[43];
	
	gfloat *d = NULL;

	// same user?
	if (u1 == u2)
		return;

	// generate the keys
	THREAD_MUTEX_LOCK (u1->user_data_lock);
	THREAD_MUTEX_LOCK (u2->user_data_lock);
	g_sprintf (distance_key, "%s->%s", u1->socket_name, u2->socket_name);
	THREAD_MUTEX_UNLOCK (u2->user_data_lock);
	THREAD_MUTEX_UNLOCK (u1->user_data_lock);
	
	// get the conference
	if ((c = u2->conference) == NULL)
	{
		g_critical ("conference not found!");
		return;
	}

	// lock the distance matrix
	THREAD_MUTEX_LOCK (c->distance_lock);

	// check if there is already a distance calculated for distance_key
	if ((d = g_hash_table_lookup (c->distance_matrix, distance_key)) == NULL)
	{
		// no? so we have to create memory for the result
		if ((d = g_malloc (sizeof (gfloat))) == NULL)
		{
			g_critical ("cannot malloc for distance result (%bytes)",
					  sizeof (gfloat));
		}

		g_debug ("new distance matrix entry: %s", distance_key);

		// insert/update distance matrix
		g_hash_table_insert (c->distance_matrix, g_strdup (distance_key),
						 d);
	}
	else
		g_debug ("update distance matrix entry: %s", distance_key);

	// calculate the distance
	THREAD_MUTEX_LOCK (u1->position_lock);
	THREAD_MUTEX_LOCK (u2->position_lock);
	if (u1->lat > -180.0 && u1->lat < 180.0 &&
	    u1->lon > -180.0 && u1->lon < 180.0 &&
	    u2->lat > -180.0 && u2->lat < 180.0 &&
	    u2->lon > -180.0 && u2->lon < 180.0)
	{
		*d = _distance_calculate (u1->lat, u1->lon, u2->lat, u2->lon);
	}
	else
		*d = -1.0;
	THREAD_MUTEX_UNLOCK (u2->position_lock);
	THREAD_MUTEX_UNLOCK (u1->position_lock);
	
	g_debug ("distance for '%s': %f miles", distance_key, *d);
	
	// unlock the distance matrix
	THREAD_MUTEX_UNLOCK (c->distance_lock);
}

static gfloat
_distance_calculate (gfloat lat1, gfloat lon1, gfloat lat2, gfloat lon2)
{
	gdouble d = 0.0;

	d = sin (lat1 * uf) * sin (lat2 * uf);
	d += cos (lat1 * uf) * cos (lat2 * uf) * cos ((lon2 - lon1) * uf);

	return ((gfloat) (acos (d) * earthradius));
}

static void
_distance_calculation (struct conference *c)
{
	g_assert (users != NULL);
	g_assert (c != NULL);

	g_message ("starting distance matrix thread 0x%x",
			 (glong) g_thread_self ());

	g_debug ("recalculate the distance matrix for every user of this conference");

	// walk through the user hash
	THREAD_MUTEX_LOCK (c->lock);
	g_slist_foreach (c->userlist, _distance_calculate_for_every_user, c);
	THREAD_MUTEX_UNLOCK (c->lock);

	g_message ("stopping distance thread");

	c->distance_thread = NULL;
	
	// exit thread
	g_thread_exit (0);
}

static void
_distance_calculation_for_one_user (struct user *u)
{
	g_assert (users != NULL);
	g_assert (u != NULL);

	g_message ("starting distance matrix thread 0x%x for one user",
			 (glong) g_thread_self ());

	g_debug ("recalculate the distance matrix for one user");

	THREAD_MUTEX_LOCK (u->conference->lock);
	g_slist_foreach (u->conference->userlist,
				  _distance_calculate_between_users, u);
	THREAD_MUTEX_UNLOCK (u->conference->lock);
  
	g_message ("stopping distance thread");

	u->conference->distance_thread = NULL;
	
	// exit thread
	g_thread_exit (0);
}
