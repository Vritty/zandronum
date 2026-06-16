/*
** music_fluidsynth_mididevice.cpp
** Provides access to FluidSynth as a generic MIDI device.
**
**---------------------------------------------------------------------------
** Copyright 2010 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifdef HAVE_FLUIDSYNTH

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"
#include "v_text.h"
#include "cmdlib.h"

// MACROS ------------------------------------------------------------------

// Defaults for chorus type; FluidSynth 2.x headers provide the MOD_* enums but
// not FLUID_CHORUS_DEFAULT_TYPE.
#ifndef FLUID_CHORUS_MOD_SINE
#define FLUID_CHORUS_MOD_SINE		0
#define FLUID_CHORUS_MOD_TRIANGLE	1
#endif

#ifndef FLUID_CHORUS_DEFAULT_TYPE
#define FLUID_CHORUS_DEFAULT_TYPE FLUID_CHORUS_MOD_SINE
#endif

#ifdef DYN_FLUIDSYNTH

#ifdef _WIN32
#ifndef _M_X64
#define FLUIDSYNTHLIB1	"fluidsynth.dll"
#define FLUIDSYNTHLIB2	"libfluidsynth.dll"
#else
#define FLUIDSYNTHLIB1	"fluidsynth64.dll"
#define FLUIDSYNTHLIB2	"libfluidsynth64.dll"
#endif
#else
#include <dlfcn.h>

#if defined(__APPLE__)
#define FLUIDSYNTHLIB	"libfluidsynth.dylib"
#else
#define FLUIDSYNTHLIB	"libfluidsynth.so.1"
#endif

#if defined(__APPLE__)
static const char *const FluidSynthLibPaths[] =
{
	FLUIDSYNTHLIB,
	"/opt/homebrew/lib/libfluidsynth.dylib",
	"/usr/local/lib/libfluidsynth.dylib",
	"@executable_path/../Frameworks/libfluidsynth.dylib",
};
#endif

#endif

#define FLUID_REVERB_DEFAULT_ROOMSIZE 0.2f
#define FLUID_REVERB_DEFAULT_DAMP 0.0f
#define FLUID_REVERB_DEFAULT_WIDTH 0.5f
#define FLUID_REVERB_DEFAULT_LEVEL 0.9f

#define FLUID_CHORUS_DEFAULT_N 3
#define FLUID_CHORUS_DEFAULT_LEVEL 2.0f
#define FLUID_CHORUS_DEFAULT_SPEED 0.3f
#define FLUID_CHORUS_DEFAULT_DEPTH 8.0f

#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR(String, fluid_patchset, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Float, fluid_gain, 0.5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 10)
		self = 10;
	else if (currSong != NULL)
		currSong->FluidSettingNum("synth.gain", self);
}

CUSTOM_CVAR(Bool, fluid_reverb, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (currSong != NULL)
		currSong->FluidSettingInt("synth.reverb.active", self);
}

CUSTOM_CVAR(Bool, fluid_chorus, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (currSong != NULL)
		currSong->FluidSettingInt("synth.chorus.active", self);
}

//*SEB Plyphony: 32 is enough, 128 is overkill for Pandora.
CUSTOM_CVAR(Int, fluid_voices, 32, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 16)
		self = 16;
	else if (self > 64)
		self = 64;
	else if (currSong != NULL)
		currSong->FluidSettingInt("synth.polyphony", self);
}

CUSTOM_CVAR(Int, fluid_interp, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	// Values are: 0 = FLUID_INTERP_NONE
	//             1 = FLUID_INTERP_LINEAR
	//             4 = FLUID_INTERP_4THORDER (the FluidSynth default)
	//             7 = FLUID_INTERP_7THORDER
	// (And here I thought it was just a linear list.)
	// Round undefined values to the nearest valid one.
	if (self < 0)
		self = 0;
	else if (self == 2)
		self = 1;
	else if (self == 3 || self == 5)
		self = 4;
	else if (self == 6 || self > 7)
		self = 7;
	else if (currSong != NULL)
		currSong->FluidSettingInt("synth.interpolation", self);
}

CVAR(Int, fluid_samplerate, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// I don't know if this setting even matters for us, since we aren't letting
// FluidSynth drives its own output.
CUSTOM_CVAR(Int, fluid_threads, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 256)
		self = 256;
}

CUSTOM_CVAR(Float, fluid_reverb_roomsize, 0.61f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1.2f)
		self = 1.2f;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Float, fluid_reverb_damping, 0.23f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Float, fluid_reverb_width, 0.76f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 100)
		self = 100;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Float, fluid_reverb_level, 0.57f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Int, fluid_chorus_voices, 3, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 99)
		self = 99;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

CUSTOM_CVAR(Float, fluid_chorus_level, 1.2f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

CUSTOM_CVAR(Float, fluid_chorus_speed, 0.3f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.29f)
		self = 0.29f;
	else if (self > 5)
		self = 5;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

// depth is in ms and actual maximum depends on the sample rate
CUSTOM_CVAR(Float, fluid_chorus_depth, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 21)
		self = 21;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

CUSTOM_CVAR(Int, fluid_chorus_type, FLUID_CHORUS_DEFAULT_TYPE, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self != FLUID_CHORUS_MOD_SINE && self != FLUID_CHORUS_MOD_TRIANGLE)
		self = FLUID_CHORUS_DEFAULT_TYPE;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

#ifndef DYN_FLUIDSYNTH
static void FluidSynth_ApplyReverb(fluid_synth_t *synth)
{
	fluid_synth_set_reverb_group_roomsize(synth, 0, fluid_reverb_roomsize);
	fluid_synth_set_reverb_group_damp(synth, 0, fluid_reverb_damping);
	fluid_synth_set_reverb_group_width(synth, 0, fluid_reverb_width);
	fluid_synth_set_reverb_group_level(synth, 0, fluid_reverb_level);
}

static void FluidSynth_ApplyChorus(fluid_synth_t *synth)
{
	fluid_synth_set_chorus_group_nr(synth, 0, fluid_chorus_voices);
	fluid_synth_set_chorus_group_level(synth, 0, fluid_chorus_level);
	fluid_synth_set_chorus_group_speed(synth, 0, fluid_chorus_speed);
	fluid_synth_set_chorus_group_depth(synth, 0, fluid_chorus_depth);
	fluid_synth_set_chorus_group_type(synth, 0, fluid_chorus_type);
}

static void FluidSynth_SetReverbOn(fluid_synth_t *synth, int on)
{
	fluid_synth_reverb_on(synth, -1, on);
}

static void FluidSynth_SetChorusOn(fluid_synth_t *synth, int on)
{
	fluid_synth_chorus_on(synth, -1, on);
}
#else
#define FluidSynth_ApplyReverb(synth) \
	fluid_synth_set_reverb((synth), fluid_reverb_roomsize, fluid_reverb_damping, \
		fluid_reverb_width, fluid_reverb_level)
#define FluidSynth_ApplyChorus(synth) \
	fluid_synth_set_chorus((synth), fluid_chorus_voices, fluid_chorus_level, \
		fluid_chorus_speed, fluid_chorus_depth, fluid_chorus_type)
#define FluidSynth_SetReverbOn(synth, on) fluid_synth_set_reverb_on((synth), (on))
#define FluidSynth_SetChorusOn(synth, on) fluid_synth_set_chorus_on((synth), (on))
#endif

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FluidSynthMIDIDevice Constructor
//
//==========================================================================

FluidSynthMIDIDevice::FluidSynthMIDIDevice()
{
	FluidSynth = NULL;
	FluidSettings = NULL;
#ifdef DYN_FLUIDSYNTH
	if (!LoadFluidSynth())
	{
		return;
	}
#endif
	FluidSettings = new_fluid_settings();
	if (FluidSettings == NULL)
	{
		printf("Failed to create FluidSettings.\n");
		return;
	}
	SampleRate = fluid_samplerate;
	if (SampleRate < 22050 || SampleRate > 96000)
	{ // Match sample rate to SFX rate
		SampleRate = clamp((int)GSnd->GetOutputRate(), 22050, 96000);
	}
	fluid_settings_setnum(FluidSettings, "synth.sample-rate", SampleRate);
	fluid_settings_setnum(FluidSettings, "synth.gain", fluid_gain);
	fluid_settings_setint(FluidSettings, "synth.reverb.active", fluid_reverb);
	fluid_settings_setint(FluidSettings, "synth.chorus.active", fluid_chorus);
	fluid_settings_setint(FluidSettings, "synth.polyphony", fluid_voices);
	fluid_settings_setint(FluidSettings, "synth.cpu-cores", fluid_threads);
	FluidSynth = new_fluid_synth(FluidSettings);
	if (FluidSynth == NULL)
	{
		Printf("Failed to create FluidSynth.\n");
		return;
	}
	fluid_synth_set_interp_method(FluidSynth, -1, fluid_interp);
	FluidSynth_ApplyReverb(FluidSynth);
	FluidSynth_ApplyChorus(FluidSynth);
	if (0 == LoadPatchSets(fluid_patchset))
	{
#ifdef __unix__
		// This is the standard location on ZDoom for Pandora :).
		if (0 == LoadPatchSets("/mnt/utmp/zdoom/share/FluidR3_GS.sf2:/mnt/utmp/zdoom/share/TimGM6mb.sf2"))
		{
#endif
#ifdef _WIN32
		// On Windows, look for the 4 megabyte patch set installed by Creative's drivers as a default.
		char sysdir[MAX_PATH+sizeof("\\CT4MGM.SF2")];
		UINT filepart;
		if (0 != (filepart = GetSystemDirectoryA(sysdir, MAX_PATH)))
		{
			strcat(sysdir, "\\CT4MGM.SF2");
			if (0 == LoadPatchSets(sysdir))
			{
				// Try again with CT2MGM.SF2
				sysdir[filepart + 3] = '2';
				if (0 == LoadPatchSets(sysdir))
				{
#endif
					Printf("Failed to load any MIDI patches.\n");
					delete_fluid_synth(FluidSynth);
					FluidSynth = NULL;
#ifdef _WIN32
				}
			}
		}
#endif
#ifdef __unix__
		}
#endif
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice Destructor
//
//==========================================================================

FluidSynthMIDIDevice::~FluidSynthMIDIDevice()
{
	Close();
	if (FluidSynth != NULL)
	{
		delete_fluid_synth(FluidSynth);
	}
	if (FluidSettings != NULL)
	{
		delete_fluid_settings(FluidSettings);
	}
#ifdef DYN_FLUIDSYNTH
	UnloadFluidSynth();
#endif
}

//==========================================================================
//
// FluidSynthMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int FluidSynthMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	if (FluidSynth == NULL)
	{
		return 2;
	}
	int ret = OpenStream(4, 0, callback, userdata);
	if (ret == 0)
	{
		fluid_synth_system_reset(FluidSynth);
	}
	return ret;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: HandleEvent
//
// Translates a MIDI event into FluidSynth calls.
//
//==========================================================================

void FluidSynthMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int channel = status & 0x0F;
	
	switch (command)
	{
	case MIDI_NOTEOFF:
		fluid_synth_noteoff(FluidSynth, channel, parm1);
		break;

	case MIDI_NOTEON:
		fluid_synth_noteon(FluidSynth, channel, parm1, parm2);
		break;

	case MIDI_POLYPRESS:
		break;

	case MIDI_CTRLCHANGE:
		fluid_synth_cc(FluidSynth, channel, parm1, parm2);
		break;

	case MIDI_PRGMCHANGE:
		fluid_synth_program_change(FluidSynth, channel, parm1);
		break;

	case MIDI_CHANPRESS:
		fluid_synth_channel_pressure(FluidSynth, channel, parm1);
		break;

	case MIDI_PITCHBEND:
		fluid_synth_pitch_bend(FluidSynth, channel, (parm1 & 0x7f) | ((parm2 & 0x7f) << 7));
		break;
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: HandleLongEvent
//
// Handle SysEx messages.
//
//==========================================================================

void FluidSynthMIDIDevice::HandleLongEvent(const BYTE *data, int len)
{
	if (len > 1 && (data[0] == 0xF0 || data[0] == 0xF7))
	{
		fluid_synth_sysex(FluidSynth, (const char *)data + 1, len - 1, NULL, NULL, NULL, 0);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: ComputeOutput
//
//==========================================================================

void FluidSynthMIDIDevice::ComputeOutput(float *buffer, int len)
{
	fluid_synth_write_float(FluidSynth, len,
		buffer, 0, 2,
		buffer, 1, 2);
}

//==========================================================================
//
// FluidSynthMIDIDevice :: LoadPatchSets
//
// Loads a delimiter-separated list of patch sets. This delimiter matches
// that of the PATH environment variable. On Windows, it is ';'. On other
// systems, it is ':'. Returns the number of patch sets loaded.
//
//==========================================================================

int FluidSynthMIDIDevice::LoadPatchSets(const char *patches)
{
	int count;
	char *wpatches = strdup(patches);
	char *tok;
#ifdef _WIN32
	const char *const delim = ";";
#else
	const char *const delim = ":";
#endif

	if (wpatches == NULL)
	{
		return 0;
	}
	tok = strtok(wpatches, delim);
	count = 0;
	while (tok != NULL)
	{
		FString path;
#ifdef _WIN32
		// If the path does not contain any path separators, automatically
		// prepend $PROGDIR to the path.
		if (strcspn(tok, ":/\\") == strlen(tok))
		{
			path << "$PROGDIR/" << tok;
			path = NicePath(path);
		}
		else
#endif
		{
			path = NicePath(tok);
		}
		if (FLUID_FAILED != fluid_synth_sfload(FluidSynth, path, count == 0))
		{
			DPrintf("Loaded patch set %s.\n", tok);
			count++;
		}
		else
		{
			DPrintf("Failed to load patch set %s.\n", tok);
		}
		tok = strtok(NULL, delim);
	}
	free(wpatches);
	return count;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FluidSettingInt
//
// Changes an integer setting.
//
//==========================================================================

void FluidSynthMIDIDevice::FluidSettingInt(const char *setting, int value)
{
	if (FluidSynth == NULL || FluidSettings == NULL)
	{
		return;
	}

	if (strcmp(setting, "synth.interpolation") == 0)
	{
		if (FLUID_OK != fluid_synth_set_interp_method(FluidSynth, -1, value))
		{
			Printf("Setting interpolation method %d failed.\n", value);
		}
	}
	else if (strcmp(setting, "synth.polyphony") == 0)
	{
		if (FLUID_OK != fluid_synth_set_polyphony(FluidSynth, value))
		{
			Printf("Setting polyphony to %d failed.\n", value);
		}
	}
	else if (strcmp(setting, "z.reverb-changed") == 0)
	{
		FluidSynth_ApplyReverb(FluidSynth);
	}
	else if (strcmp(setting, "z.chorus-changed") == 0)
	{
		FluidSynth_ApplyChorus(FluidSynth);
	}
	else if (0 == fluid_settings_setint(FluidSettings, setting, value))
	{
		Printf("Failed to set %s to %d.\n", setting, value);
	}
	// fluid_settings_setint succeeded; update these settings in the running synth, too
	else if (strcmp(setting, "synth.reverb.active") == 0)
	{
		FluidSynth_SetReverbOn(FluidSynth, value);
	}
	else if (strcmp(setting, "synth.chorus.active") == 0)
	{
		FluidSynth_SetChorusOn(FluidSynth, value);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FluidSettingNum
//
// Changes a numeric setting.
//
//==========================================================================

void FluidSynthMIDIDevice::FluidSettingNum(const char *setting, double value)
{
	if (FluidSettings != NULL)
	{
		if (0 == fluid_settings_setnum(FluidSettings, setting, value))
		{
			Printf("Failed to set %s to %g.\n", setting, value);
		}
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FluidSettingStr
//
// Changes a string setting.
//
//==========================================================================

void FluidSynthMIDIDevice::FluidSettingStr(const char *setting, const char *value)
{
	if (FluidSettings != NULL)
	{
		if (0 == fluid_settings_setstr(FluidSettings, setting, value))
		{
			Printf("Failed to set %s to %s.\n", setting, value);
		}
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: GetStats
//
//==========================================================================

FString FluidSynthMIDIDevice::GetStats()
{
	if (FluidSynth == NULL || FluidSettings == NULL)
	{
		return "FluidSynth is invalid";
	}
	FString out;

	CritSec.Enter();
	int polyphony = fluid_synth_get_polyphony(FluidSynth);
	int voices = fluid_synth_get_active_voice_count(FluidSynth);
	double load = fluid_synth_get_cpu_load(FluidSynth);
	int chorus = 0, reverb = 0;
	fluid_settings_getint(FluidSettings, "synth.chorus.active", &chorus);
	fluid_settings_getint(FluidSettings, "synth.reverb.active", &reverb);
	int maxpoly;
	fluid_settings_getint(FluidSettings, "synth.polyphony", &maxpoly);
	CritSec.Leave();

	out.Format("Voices: " TEXTCOLOR_YELLOW "%3d" TEXTCOLOR_NORMAL "/" TEXTCOLOR_ORANGE "%3d" TEXTCOLOR_NORMAL "(" TEXTCOLOR_RED "%3d" TEXTCOLOR_NORMAL ")"
			   TEXTCOLOR_YELLOW "%6.2f" TEXTCOLOR_NORMAL "%% CPU   "
			   "Reverb: " TEXTCOLOR_YELLOW "%3s" TEXTCOLOR_NORMAL
			   " Chorus: " TEXTCOLOR_YELLOW "%3s",
		voices, polyphony, maxpoly, load, reverb ? "yes" : "no", chorus ? "yes" : "no");
	return out;
}

#ifdef DYN_FLUIDSYNTH

struct LibFunc
{
	void **FuncPointer;
	const char *FuncName;
};

//==========================================================================
//
// FluidSynthMIDIDevice :: LoadFluidSynth
//
// Returns true if the FluidSynth library was successfully loaded.
//
//==========================================================================

bool FluidSynthMIDIDevice::LoadFluidSynth()
{
	LibFunc imports[] =
	{
		{ (void **)&new_fluid_settings,					"new_fluid_settings" },
		{ (void **)&new_fluid_synth,					"new_fluid_synth" },
		{ (void **)&delete_fluid_synth,					"delete_fluid_synth" },
		{ (void **)&delete_fluid_settings,				"delete_fluid_settings" },
		{ (void **)&fluid_settings_setnum,				"fluid_settings_setnum" },
		{ (void **)&fluid_settings_setstr,				"fluid_settings_setstr" },
		{ (void **)&fluid_settings_setint,				"fluid_settings_setint" },
		{ (void **)&fluid_settings_getint,				"fluid_settings_getint" },
		{ (void **)&fluid_synth_set_reverb_on,			"fluid_synth_set_reverb_on" },
		{ (void **)&fluid_synth_set_chorus_on,			"fluid_synth_set_chorus_on" },
		{ (void **)&fluid_synth_set_interp_method,		"fluid_synth_set_interp_method" },
		{ (void **)&fluid_synth_set_polyphony,			"fluid_synth_set_polyphony" },
		{ (void **)&fluid_synth_get_polyphony,			"fluid_synth_get_polyphony" },
		{ (void **)&fluid_synth_get_active_voice_count,	"fluid_synth_get_active_voice_count" },
		{ (void **)&fluid_synth_get_cpu_load,			"fluid_synth_get_cpu_load" },
		{ (void **)&fluid_synth_system_reset,			"fluid_synth_system_reset" },
		{ (void **)&fluid_synth_noteon,					"fluid_synth_noteon" },
		{ (void **)&fluid_synth_noteoff,				"fluid_synth_noteoff" },
		{ (void **)&fluid_synth_cc,						"fluid_synth_cc" },
		{ (void **)&fluid_synth_program_change,			"fluid_synth_program_change" },
		{ (void **)&fluid_synth_channel_pressure,		"fluid_synth_channel_pressure" },
		{ (void **)&fluid_synth_pitch_bend,				"fluid_synth_pitch_bend" },
		{ (void **)&fluid_synth_write_float,			"fluid_synth_write_float" },
		{ (void **)&fluid_synth_sfload,					"fluid_synth_sfload" },
		{ (void **)&fluid_synth_set_reverb,				"fluid_synth_set_reverb" },
		{ (void **)&fluid_synth_set_chorus,				"fluid_synth_set_chorus" },
		{ (void **)&fluid_synth_sysex,					"fluid_synth_sysex" },
	};
	int fail = 0;
	const char *libname;

#ifdef _WIN32
	FluidSynthDLL = LoadLibrary((libname = FLUIDSYNTHLIB1));
	if (FluidSynthDLL == NULL)
	{
		FluidSynthDLL = LoadLibrary((libname = FLUIDSYNTHLIB2));
		if (FluidSynthDLL == NULL)
		{
			Printf(TEXTCOLOR_RED"Could not load " FLUIDSYNTHLIB1 " or " FLUIDSYNTHLIB2 "\n");
			return false;
		}
	}
#else
#if defined(__APPLE__)
	FluidSynthSO = NULL;
	for (size_t i = 0; i < countof(FluidSynthLibPaths); ++i)
	{
		FluidSynthSO = dlopen((libname = FluidSynthLibPaths[i]), RTLD_LAZY);
		if (FluidSynthSO != NULL)
			break;
	}
	if (FluidSynthSO == NULL)
	{
		Printf(TEXTCOLOR_RED"Could not load " FLUIDSYNTHLIB ": %s\n", dlerror());
		return false;
	}
#else
	FluidSynthSO = dlopen((libname = FLUIDSYNTHLIB), RTLD_LAZY);
	if (FluidSynthSO == NULL)
	{
		Printf(TEXTCOLOR_RED"Could not load " FLUIDSYNTHLIB ": %s\n", dlerror());
		return false;
	}
#endif
#endif

	for (size_t i = 0; i < countof(imports); ++i)
	{
#ifdef _WIN32
		FARPROC proc = GetProcAddress(FluidSynthDLL, imports[i].FuncName);
#else
		void *proc = dlsym(FluidSynthSO, imports[i].FuncName);
#endif
		if (proc == NULL)
		{
			Printf(TEXTCOLOR_RED"Failed to find %s in %s\n", imports[i].FuncName, libname);
			fail++;
		}
		*imports[i].FuncPointer = (void *)proc;
	}
	if (fail == 0)
	{
		return true;
	}
	else
	{
#ifdef _WIN32
		FreeLibrary(FluidSynthDLL);
		FluidSynthDLL = NULL;
#else
		dlclose(FluidSynthSO);
		FluidSynthSO = NULL;
#endif
		return false;
	}

}

//==========================================================================
//
// FluidSynthMIDIDevice :: UnloadFluidSynth
//
//==========================================================================

void FluidSynthMIDIDevice::UnloadFluidSynth()
{
#ifdef _WIN32
	if (FluidSynthDLL != NULL)
	{
		FreeLibrary(FluidSynthDLL);
		FluidSynthDLL = NULL;
	}
#else
	if (FluidSynthSO != NULL)
	{
		dlclose(FluidSynthSO);
		FluidSynthSO = NULL;
	}
#endif
}

#endif

#endif

