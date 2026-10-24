
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

#ifndef _audio_h
#define _audio_h

/* filter coefficients */
struct audio_filter
{
	float a0;
	float a1;
	float a2;
	float a3;
	float a4;
	iframe x[2];
	iframe y[2];
};

/* filter enums */
enum
{
	LPF,					  /* low pass filter */
	HPF,					  /* High pass filter */
	BPF,					  /* band pass filter */
	NOTCH,				  /* Notch Filter */
	PEQ,					  /* Peaking band EQ filter */
	LSH,					  /* Low shelf filter */
	HSH					  /* High shelf filter */
} audio_dsp_filter_enum;

/* public prototypes */
iframe *audio_frame2iframe (frame * f, guint size);
frame *audio_iframe2frame (iframe * f, guint size);
iframe *audio_generate_iframe (guint size);
frame *audio_generate_frame (guint size);
void audio_generate_noise (gfloat level, iframe * f, guint suize);
iframe *audio_generate_noise_iframe (gfloat level, guint size);
frame *audio_generate_noise_frame (gfloat level, guint size);
void audio_dsp_crusher (gfloat frq, guint bits, iframe * f, guint size);
void audio_distortion (gfloat threshold, iframe * f, guint size);
void audio_amp (gfloat level, iframe * f, guint size);
gfloat audio_calculate_amp_factor (gfloat amp_factor, iframe * f, guint size);
struct audio_filter *audio_init_filter (guint type, gfloat dbGain,
								gfloat bandwidth, gfloat freq,
								gfloat srate);
void audio_fir_filter (struct audio_filter *audio_filter, iframe * f,
				   guint size);

/* private prototypes */

#endif
