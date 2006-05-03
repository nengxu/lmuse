// vam.h
//
//  (C) Copyright 2002 Jotsif Lindman H�rnlund (jotsif@linux.nu)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA or point your web browser to http://www.gnu.org.


#ifndef __VAM_H
#define __VAM_H

enum {
	DCO1_PITCHMOD, DCO1_WAVEFORM, DCO1_FM, DCO1_PWM,
	DCO1_ATTACK, DCO1_DECAY, DCO1_SUSTAIN, DCO1_RELEASE,
	DCO2_PITCHMOD, DCO2_WAVEFORM, DCO2_FM, DCO2_PWM,
	DCO2_ATTACK, DCO2_DECAY, DCO2_SUSTAIN, DCO2_RELEASE,
	LFO_FREQ, LFO_WAVEFORM, FILT_ENV_MOD, FILT_KEYTRACK,
	FILT_RES, FILT_ATTACK, FILT_DECAY, FILT_SUSTAIN,
	FILT_RELEASE, DCO2ON, FILT_INVERT, FILT_CUTOFF,
	DCO1_DETUNE, DCO2_DETUNE, DCO1_PW, DCO2_PW
};

#define NUM_CONTROLLER		32

#endif /* __VAM_H */
