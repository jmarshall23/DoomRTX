#pragma once

#include "math/prey_math.h"
#include "math/prey_interpolate.h"

#include "MsgQueue.h"
#include "containers/PreyStack.h"

// Profiling not enabled, compile it out
#define PROFILE_START(n, m)		
#define PROFILE_STOP(n, m)		
#define PROFILE_SCOPE(n, m)		

#define PROFILE_START_EXPENSIVE(n, m)
#define PROFILE_STOP_EXPENSIVE(n, m)
#define PROFILE_SCOPE_EXPENSIVE(n, m)

#define	SINGLE_MAP_BUILD				1		// For single map external builds
#define PARTICLE_BOUNDS					1		// rdr - New type of particle bounds calc
#define SOUND_TOOLS_BUILD				1		// Turn on for making builds for Ed to change reverbs, should be 0 in gold build
#define GUIS_IN_DEMOS					1		// Include guis in demo streams
#define	MUSICAL_LEVELLOADS				1		// Allow music playback during level loads
#define GAMEPORTAL_PVS					0		// Allow PVS to flow through game portals
#define GAMEPORTAL_SOUND				0		// Allow sound to flow through game portals (requires GAMEPORTAL_PVS)
#define	GAME_PLAYERDEFNAME				"player_tommy"
#define GAME_PLAYERDEFNAME_MP			"player_tommy_mp"
#define AUTOMAP							0
#define EDITOR_HELP_LOCATOR				"http://www.3drealms.com/prey/wiki"
#define PREY_SITE_LOCATOR				"http://www.prey.com"
#define CREATIVE_DRIVER_LOCATOR			"http://www.creative.com/language.asp?sDestUrl=/support/downloads"
#define TRITON_LOCATOR					"http://www.playtriton.com/prey"
#define NVIDIA_DRIVER_LOCATOR			"www.nvidia.com"
#define ATI_DRIVER_LOCATOR				"www.ati.com"
#define _HH_RENDERDEMO_HACKS			0		//rww - if 1 enables hacks to make renderdemos work through whatever nefarious means necessary
#define _HH_CLIP_FASTSECTORS			1		//rww - much faster method for clip sector checking
#define NEW_MESH_TRANSFORM				1		//bjk - SSE new vert transform
#define SIMD_SHADOW						0		//bjk - simd shadow calculations
#define MULTICORE						0		// Multicore optimizations
#define DEBUG_SOUND_LOG					0		// Write out a debug log, remove from final build
#ifdef _USE_SECUROM_ // mdl: Only enable securom for certain builds
#define _HH_SECUROM						1		//rww - enables securom api hooks
#else
#define _HH_SECUROM						0
#endif
#define _HH_INLINED_PROC_CLIPMODELS		0		//rww - enables crazy last-minute proc geometry clipmodel support

#ifdef ID_DEDICATED
#define _HH_MYGAMES_SAVES				0
#else
#define _HH_MYGAMES_SAVES				1		//HUMANHEAD PCF rww 05/10/06 - use My Games for saves
#endif

#ifdef ID_DEMO_BUILD
#define INGAME_DEBUGGER_ENABLED			0
#define INGAME_PROFILER_ENABLED			0
#else
#define INGAME_DEBUGGER_ENABLED			0
#define INGAME_PROFILER_ENABLED			0
#endif

#ifndef _NODONGLE_
#define __HH_DONGLE__					0		// always require a dongle somewhere on the net
#endif
#ifdef _GERMAN_BUILD_
#	define GERMAN_VERSION					1
#else
#	define GERMAN_VERSION					0		// Set to 1 to disable gore
#endif


#define	FLOAT_IS_INVALID(x)		(FLOAT_IS_NAN(x) || FLOAT_IS_DENORMAL(x))
#define FLOAT_SET_NAN( x )		(*(unsigned long *)&x) |= 0x7f800000

const float	USERCMD_ONE_OVER_HZ = (1.0f / USERCMD_HZ); // HUMANHEAD JRM

#ifndef ID_VERSIONTAG
#define ID_VERSIONTAG ""
//#define ID_VERSIONTAG ".MP"
#endif

#include "../framework/DeclPreyBeam.h"