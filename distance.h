
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

#ifndef _distance_h
#define _distance_h

static const gfloat earthradius = 3959.0;	// radius of earth in mi/1000
static const gdouble uf = 0.01745329251994329509;	// conversion factor pi/180 degree->rad

/* public prototypes */
void distance_remove_one_user (struct user *u);
gfloat distance_get (struct user *u1, struct user *u2);
void distance_calculation (struct conference *c);
void distance_calculation_for_one_user (struct user *u);

/* private prototypes */
static void _distance_destroy_distance_matrix (GHashTable * distance_matrix);
static void _distance_calculate_for_every_user (gpointer value,
									   gpointer user_data);
static void _distance_remove_one_user (gpointer key, gpointer value,
							    gpointer user_data);
static void _distance_calculate_between_users (gpointer value,
									  gpointer user_data);
static gfloat _distance_calculate (gfloat lat1, gfloat lon1, gfloat lat2,
							gfloat lon2);
static void _distance_calculation (struct conference *c);
static void _distance_calculation_for_one_user (struct user *u);
#endif
