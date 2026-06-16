//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: medal.h
//
// Description: Contains medal structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __MEDAL_H__
#define __MEDAL_H__

#include <vector>
#include "doomdef.h"
#include "info.h"
#include "v_font.h"

#include "medal_enums.h"

//*****************************************************************************
//	DEFINES

#define	MEDAL_ICON_DURATION			( 3 * TICRATE )

enum
{
	SPRITE_CHAT,
	SPRITE_VOICECHAT,
	SPRITE_INCONSOLE,
	SPRITE_INMENU,
	SPRITE_ALLY,
	SPRITE_ENEMY,
	SPRITE_LAG,
	SPRITE_WHITEFLAG,
	SPRITE_TERMINATORARTIFACT,
	SPRITE_POSSESSIONARTIFACT,
	SPRITE_TEAMITEM,
	NUM_SPRITES
};

//*****************************************************************************
enum
{
	// Never show ally/enemy icons.
	SHOW_ICON_NEVER,

	// Only show ally/enemy icons in team-based game modes.
	SHOW_ICON_TEAMS_ONLY,

	// Show ally/enemy icons in all game modes.
	SHOW_ICON_ALWAYS,
};

//*****************************************************************************
//	STRUCTURES

struct MEDAL_t
{
	// [AK] A name used to identify the medal.
	const FName		name;

	// Icon that displays on the screen when this medal is received.
	FTextureID		icon;

	// [AK] Icon that displays on the scoreboard, if defined.
	FTextureID		scoreboardIcon;

	// [AK] The floaty icon class to spawn above the player's head.
	const PClass	*iconClass;

	// State that the floaty icon above the player's head is set to.
	FState			*iconState;

	// Text that appears below the medal icon when received.
	FString			text;

	// Color that text is displayed in.
	EColorRange		textColor;

	// [AK] Color that the quantity of the medal is displayed in.
	FString			quantityColor;

	// Announcer entry that's played when this medal is triggered.
	FString			announcerEntry;

	// [RC] The "lower" medal that this overrides.
	MEDAL_t			*lowerMedal;

	// Name of sound to play when this medal type is triggered.
	FSoundID		sound;

	// [AK] Which flags are enabled for this medal.
	unsigned int	flags;

	// [AK] How much of this medal that each player currently has.
	unsigned int	awardedCount[MAXPLAYERS];

	MEDAL_t( FName name ) :
		name( name ),
		iconClass( nullptr ),
		iconState( nullptr ),
		textColor( CR_UNTRANSLATED ),
		lowerMedal( nullptr ),
		flags( 0 ),
		awardedCount{ 0 }
	{
		icon.SetInvalid( );
		scoreboardIcon.SetInvalid( );
	}
};

//*****************************************************************************
struct MEDALQUEUE_t
{
	// The medals in this queue.
	std::vector<MEDAL_t *>	medals;

	// Amount of time before the medal display in this queue expires.
	unsigned int			ticks;

	MEDALQUEUE_t( void ) : ticks( 0 ) { }
};

//*****************************************************************************
//	PROTOTYPES

// Standard API.
void		MEDAL_Construct( void );
void		MEDAL_Tick( void );
void		MEDAL_Render( void );

bool		MEDAL_GiveMedal( const ULONG player, const ULONG medalIndex, const bool silent = false );
bool		MEDAL_GiveMedal( const ULONG player, const FName medalName, const bool silent = false );
void		MEDAL_SetMedalAwardedCount( const unsigned int player, const unsigned int medalIndex, const unsigned int count );
int			MEDAL_GetMedalIndex( const FName medalName );
MEDAL_t		*MEDAL_GetMedal( const FName medalName );
MEDAL_t		*MEDAL_GetDisplayedMedal( const ULONG player );
void		MEDAL_RetrieveAwardedMedals( const unsigned int player, TArray<MEDAL_t *> &list );
void		MEDAL_ResetPlayerMedals( const ULONG player, const bool resetAll );
void		MEDAL_PlayerDied( ULONG ulPlayer, ULONG ulSourcePlayer );
void		MEDAL_ResetFirstFragAwarded( void );

#endif
