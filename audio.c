
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
#include <math.h>
#include <gnet.h>
#include <glib.h>
#include "fgcomd.h"
#include "audio.h"

extern GRand *random_generator;

/****************************************************************************
 *  public functions
 ****************************************************************************/
iframe *
audio_frame2iframe (frame * f, guint size)
{
	guint i = 0;
	iframe *r = NULL;

	// get memory for the iframe
	if ((r = audio_generate_iframe (size)) != NULL)
	{
		for (i = 0; i < size; i++)
		{
			r[i] = (f[i] / 127.5) - 1.0;
		}
	}

	return (r);
}

frame *
audio_iframe2frame (iframe * f, guint size)
{
	guint i = 0;
	frame *r = NULL;

	// get memory for the frame
	if ((r = audio_generate_frame (size)) != NULL)
	{
		for (i = 0; i < size; i++)
		{
			r[i] = ((f[i] * 127.5) + 127.5);
		}
	}

	return (r);
}

iframe *
audio_generate_iframe (guint size)
{
	iframe *f = NULL;

	// get memory for the iframe
	if ((f = g_malloc0 (size * sizeof (iframe))) == NULL)
	{
		g_critical ("cannot malloc for iframe (%bytes)",
				  size * sizeof (iframe));
	}

	return (f);
}

frame *
audio_generate_frame (guint size)
{
	frame *f = NULL;

	// get memory for the frame
	if ((f = g_malloc0 (size * sizeof (f))) == NULL)
	{
		g_critical ("cannot malloc for frame (%bytes)",
				  size * sizeof (frame));
	}

	return (f);
}

void
audio_generate_noise (gfloat level, iframe * f, guint size)
{
	guint i = 0;

	for (i = 0; i <= size; i++)
	{
		f[i] =
			(1.0 - level) * f[i] + g_rand_double_range (random_generator,
											    level, level * -1);
	}
}

iframe *
audio_generate_noise_iframe (gfloat level, guint size)
{
	guint i = 0;
	iframe *f = NULL;

	if ((f = audio_generate_iframe (size)) != NULL)
	{
		for (i = 0; i <= size; i++)
		{
			f[i] =
				g_rand_double_range (random_generator, level, level * -1);
		}
	}

	return (f);
}

frame *
audio_generate_noise_frame (gfloat level, guint size)
{
	guint i = 0;
	frame *f = NULL;
	guint tmp = level * 255;

	if ((f = audio_generate_frame (size)) != NULL)
	{
		for (i = 0; i < size; i++)
		{
			f[i] = g_rand_int_range (random_generator, 0, tmp);
		}
	}

	return (f);
}

void
audio_dsp_crusher (gfloat frq, guint bits, iframe * f, guint size)
{
	guint i;
	gfloat step = 0.0f;
	gfloat phasor = 0.0f;
	gfloat last = 0.0f;

	step = powf (0.5, bits);

	phasor = phasor + frq;

	for (i = 0; i < size; i++)
	{
		if (phasor >= 1.0)
		{
			phasor = phasor - 1.0;
			last = (gfloat) (step * (gint) ((gfloat) f[i] / step + 0.5));
		}
		f[i] = (iframe) last;
	}
}

void
audio_distortion (gfloat threshold, iframe * f, guint size)
{
	guint i = 0;
	gfloat in = 0;

	for (i = 0; i < size; i++)
	{
		if (f[i] > threshold || f[i] < -threshold)
		{
			f[i] =
				fabs (fabs (fmod (f[i] - threshold, threshold * 4)) -
					 threshold * 2) - threshold;
		}
	}
}

void
audio_amp (gfloat level, iframe * f, guint size)
{
	guint i = 0;

	for (i = 0; i < size; i++)
	{
		f[i] *= level;
	}
}

gfloat
audio_calculate_amp_factor (gfloat amp_factor, iframe * f, guint size)
{
	guint i = 0;
	gfloat max = 0.0;
	gfloat all = 0.0;
	gfloat tmp = 0.0;

	for (i = 0; i <= size; i++)
	{
		if ((tmp = fabsf (f[i])) > max)
			max = tmp;
		all += fabsf (f[i]);
	}

	if ((all / size) < AGC_THRESHOLD)
	{
		g_debug ("amplification o %f %f", all / size, max);
		return (1.0);
	}

	if (fabsf (AGC_MAX - max) < AGC_DEAD_ZONE)
	{
		g_debug ("amplification - %f %f", all / size, max);
		return (amp_factor);
	}

	if (max > AGC_MAX)
	{
		g_debug ("amplification ^ %f %f", all / size, max);
		return (amp_factor + (max - amp_factor) / AGC_STEP);
	}
	else
	{
		g_debug ("amplification v %f %f", all / size, max);
		return (amp_factor - (max - amp_factor) / AGC_STEP);
	}
}

struct audio_filter *
audio_init_filter (guint type, gfloat dbGain, gfloat bandwidth, gfloat freq,
			    gfloat srate)
{
	gfloat A, omega, sn, cs, alpha, beta = 0.0;
	gfloat a0, a1, a2, b0, b1, b2 = 0.0;
	struct audio_filter *audio_filter = NULL;

	// get memory for the filter data
	if ((audio_filter = g_malloc0 (sizeof (struct audio_filter))) == NULL)
	{
		g_critical ("cannot malloc for audio_filter (%bytes)",
				  sizeof (audio_filter));
	}

	/* setup variables */
	A = pow (10, dbGain / 40);
	omega = 2 * M_PI * freq / srate;
	sn = sin (omega);
	cs = cos (omega);
	alpha = sn * sinh (M_LN2 / 2 * bandwidth * omega / sn);
	beta = sqrt (A + A);

	switch (type)
	{
	case LPF:
		b0 = (1 - cs) / 2;
		b1 = 1 - cs;
		b2 = (1 - cs) / 2;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case HPF:
		b0 = (1 + cs) / 2;
		b1 = -(1 + cs);
		b2 = (1 + cs) / 2;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case BPF:
		b0 = alpha;
		b1 = 0;
		b2 = -alpha;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case NOTCH:
		b0 = 1;
		b1 = -2 * cs;
		b2 = 1;
		a0 = 1 + alpha;
		a1 = -2 * cs;
		a2 = 1 - alpha;
		break;
	case PEQ:
		b0 = 1 + (alpha * A);
		b1 = -2 * cs;
		b2 = 1 - (alpha * A);
		a0 = 1 + (alpha / A);
		a1 = -2 * cs;
		a2 = 1 - (alpha / A);
		break;
	case LSH:
		b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
		b1 = 2 * A * ((A - 1) - (A + 1) * cs);
		b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);
		a0 = (A + 1) + (A - 1) * cs + beta * sn;
		a1 = -2 * ((A - 1) + (A + 1) * cs);
		a2 = (A + 1) + (A - 1) * cs - beta * sn;
		break;
	case HSH:
		b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
		b1 = -2 * A * ((A - 1) + (A + 1) * cs);
		b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);
		a0 = (A + 1) - (A - 1) * cs + beta * sn;
		a1 = 2 * ((A - 1) - (A + 1) * cs);
		a2 = (A + 1) - (A - 1) * cs - beta * sn;
		break;
	default:
		return (NULL);
	}

	/* precompute the coefficients */
	audio_filter->a0 = b0 / a0;
	audio_filter->a1 = b1 / a0;
	audio_filter->a2 = b2 / a0;
	audio_filter->a3 = a1 / a0;
	audio_filter->a4 = a2 / a0;

	return (audio_filter);
}

void
audio_fir_filter (struct audio_filter *audio_filter, iframe * f, guint size)
{
	guint i = 0;
	iframe result = 0.0;

	for (i = 0; i < size; i++)
	{
		result = audio_filter->a0 * f[i] +
			audio_filter->a1 * audio_filter->x[0] +
			audio_filter->a2 * audio_filter->x[1] -
			audio_filter->a3 * audio_filter->y[0] -
			audio_filter->a4 * audio_filter->y[1];

		audio_filter->x[1] = audio_filter->x[0];
		audio_filter->x[0] = f[i];

		f[i] = result;

		audio_filter->y[1] = audio_filter->y[0];
		audio_filter->y[0] = f[i];
	}
}

/****************************************************************************
 *  private functions
 ****************************************************************************/
