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
// Filename: scoreboard.cpp
//
// Description: Contains scoreboard routines and globals
//
//-----------------------------------------------------------------------------

#include "a_pickups.h"
#include "c_dispatch.h"
#include "callvote.h"
#include "chat.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "doomtype.h"
#include "d_player.h"
#include "g_game.h"
#include "gamemode.h"
#include "gi.h"
#include "invasion.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "network.h"
#include "possession.h"
#include "sbar.h"
#include "scoreboard.h"
#include "st_stuff.h"
#include "survival.h"
#include "team.h"
#include "templates.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "c_bind.h"	// [RC] To tell user what key to press to vote.
#include "domination.h"
#include "st_hud.h"

//*****************************************************************************
//	VARIABLES

// Player list according to rank.
static	int		g_iSortedPlayers[MAXPLAYERS];

// How many players are currently in the game?
static	ULONG	g_ulNumPlayers = 0;

// What is our current rank?
static	ULONG	g_ulRank = 0;

// What is the spread between us and the person in 1st/2nd?
static	LONG	g_lSpread = 0;

// Is this player tied with another?
static	bool	g_bIsTied = false;

// How many opponents are left standing in LMS?
static	LONG	g_lNumOpponentsLeft = 0;

// [RC] How many allies are alive in Survival, or Team LMS?
static	LONG	g_lNumAlliesLeft = 0;

// [AK] Who has the terminator sphere, hellstone, or white flag?
static	player_t	*g_pArtifactCarrier = NULL;

// [AK] Who are the two duelers?
static	player_t	*g_pDuelers[2];

// Current position of our "pen".
static	ULONG		g_ulCurYPos;

// Is text scaling enabled?
static	bool		g_bScale;

// What are the virtual dimensions of our screen?
static	UCVarValue	g_ValWidth;
static	UCVarValue	g_ValHeight;

// How much bigger is the virtual screen than the base 320x200 screen?
static	float		g_fXScale;
static	float		g_fYScale;

// How much bigger or smaller is the virtual screen vs. the actual resolution?
static	float		g_rXScale;
static	float		g_rYScale;

// How tall is the smallfont, scaling considered?
static	ULONG		g_ulTextHeight;

// How many columns are we using in our scoreboard display?
static	ULONG		g_ulNumColumnsUsed = 0;

// Array that has the type of each column.
static	ULONG		g_aulColumnType[MAX_COLUMNS];

// X position of each column.
static	ULONG		g_aulColumnX[MAX_COLUMNS];

// What font are the column headers using?
static	FFont		*g_pColumnHeaderFont = NULL;

// This is the header for each column type.
static	const char	*g_pszColumnHeaders[NUM_COLUMN_TYPES] =
{
	"",
	"NAME",
	"TIME",
	"PING",
	"FRAGS",
	"POINTS",
	"DEATHS",
	"WINS",
	"KILLS",
	"SCORE",
	"SECRETS",
	"MEDALS",
};

//*****************************************************************************
//	PROTOTYPES

static	void			scoreboard_SortPlayers( ULONG ulSortType );
static	int	STACK_ARGS	scoreboard_FragCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 );
static	void			scoreboard_RenderIndividualPlayer( ULONG ulDisplayPlayer, ULONG ulPlayer );
static	void			scoreboard_DrawHeader( ULONG ulPlayer );
static	void			scoreboard_ClearColumns( void );
static	void			scoreboard_Prepare5ColumnDisplay( void );
static	void			scoreboard_Prepare4ColumnDisplay( void );
static	void			scoreboard_Prepare3ColumnDisplay( void );
static	void			scoreboard_DoRankingListPass( ULONG ulPlayer, LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam );
static	void			scoreboard_DrawRankings( ULONG ulPlayer );
static	void			scoreboard_DrawBottomString( ULONG ulPlayer );
static	void			scoreboard_RenderCountdown( ULONG ulTimeLeft );
static	void			scoreboard_DrawText( const char *pszString, EColorRange Color, ULONG &ulXPos, ULONG ulOffset, bool bOffsetRight = false );
static	void			scoreboard_DrawIcon( const char *pszPatchName, ULONG &ulXPos, ULONG ulYPos, ULONG ulOffset, bool bOffsetRight = false );

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR (Bool, r_drawspectatingstring, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG );
EXTERN_CVAR( Int, screenblocks );
EXTERN_CVAR( Bool, st_scale );


//*****************************************************************************
//	FUNCTIONS

static void scoreboard_DrawBottomString( ULONG ulDisplayPlayer )
{
	FString bottomString;

	// [BB] Draw a message to show that the free spectate mode is active.
	if ( CLIENTDEMO_IsInFreeSpectateMode( ))
		bottomString.AppendFormat( "Free Spectate Mode" );
	// If the console player is looking through someone else's eyes, draw the following message.
	else if ( ulDisplayPlayer != static_cast<ULONG>( consoleplayer ))
	{
		FString color = TEXTCOLOR_RED;

		// [RC] Or draw this in their team's color.
		if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS )
		{
			color = TEXTCOLOR_ESCAPE;
			color += V_GetColorChar( TEAM_GetTextColor( players[ulDisplayPlayer].Team ));
		}

		bottomString.AppendFormat( "%sFollowing - %s%s", color.GetChars( ), players[ulDisplayPlayer].userinfo.GetName( ), color.GetChars( ));
	}

	// Print the totals for living and dead allies/enemies.
	if (( players[ulDisplayPlayer].bSpectating == false ) && ( GAMEMODE_GetCurrentFlags( ) & GMF_DEADSPECTATORS ) && ( GAMEMODE_GetState( ) == GAMESTATE_INPROGRESS ))
	{
		if ( ulDisplayPlayer != static_cast<ULONG>( consoleplayer ))
			bottomString += " - ";

		// Survival, Survival Invasion, etc
		if ( GAMEMODE_GetCurrentFlags( ) & GMF_COOPERATIVE )
		{
			if ( g_lNumAlliesLeft < 1 )
			{
				bottomString += TEXTCOLOR_RED "Last Player Alive"; // Uh-oh.
			}
			else
			{
				bottomString.AppendFormat( TEXTCOLOR_GRAY "%d ", static_cast<int>( g_lNumAlliesLeft ));
				bottomString.AppendFormat( TEXTCOLOR_RED "all%s left", g_lNumAlliesLeft != 1 ? "ies" : "y" );
			}
		}
		// Last Man Standing, TLMS, etc
		else
		{
			bottomString.AppendFormat( TEXTCOLOR_GRAY "%d ", static_cast<int>( g_lNumOpponentsLeft ));
			bottomString.AppendFormat( TEXTCOLOR_RED "opponent%s", g_lNumOpponentsLeft != 1 ? "s" : "" );

			if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS )
			{
				if ( g_lNumAlliesLeft < 1 )
				{
					bottomString += " left - allies dead";
				}
				else
				{
					bottomString.AppendFormat( ", " TEXTCOLOR_GRAY " %d ", static_cast<int>( g_lNumAlliesLeft ));
					bottomString.AppendFormat( TEXTCOLOR_RED "all%s left", g_lNumAlliesLeft != 1 ? "ies" : "y" );
				}
			}
			else
			{
				bottomString += " left";
			}
		}
	}

	// If the console player is spectating, draw the spectator message.
	// [BB] Only when not in free spectate mode.
	if (( r_drawspectatingstring ) && ( players[consoleplayer].bSpectating ) && ( CLIENTDEMO_IsInFreeSpectateMode( ) == false ))
	{
		LONG lPosition = JOINQUEUE_GetPositionInLine( consoleplayer );
		bottomString += "\n" TEXTCOLOR_GREEN;

		if ( players[consoleplayer].bDeadSpectator )
			bottomString += "Spectating - Waiting to respawn";
		else if ( lPosition != -1 )
			bottomString.AppendFormat( "Waiting to play - %s in line", SCOREBOARD_SpellOrdinal( lPosition ));
		else
		{
			int key1 = 0, key2 = 0;

			Bindings.GetKeysForCommand( "menu_join", &key1, &key2 );
			bottomString += "Spectating - press '";

			if ( key2 )
				bottomString.AppendFormat( "%s' or '%s'", KeyNames[key1], KeyNames[key2] );
			else if ( key1 )
				bottomString += KeyNames[key1];
			else
				bottomString += G_DescribeJoinMenuKey( );

			bottomString += "' to join";
		}
	}

	// [AK] Draw a message showing that we're waiting for players if we are.
	if (( GAMEMODE_GetState( ) == GAMESTATE_WAITFORPLAYERS ) && ( players[ulDisplayPlayer].bSpectating == false ))
	{
		if ( ulDisplayPlayer != static_cast<ULONG>( consoleplayer ))
			bottomString += '\n';

		bottomString += TEXTCOLOR_RED "Waiting for players";
	}

	// [RC] Draw the centered bottom message (spectating, following, waiting, etc).
	if ( bottomString.Len( ) > 0 )
	{
		DHUDMessageFadeOut *pMsg = new DHUDMessageFadeOut( SmallFont, bottomString, 1.5f, 1.0f, 0, 0, CR_WHITE, 0.10f, 0.15f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'W', 'A', 'I', 'T' ));
	}
}

//*****************************************************************************
// Checks if the user wants to see the scoreboard and is allowed to.
//
bool SCOREBOARD_ShouldDrawBoard( ULONG ulDisplayPlayer )
{
	// [AK] If the user isn't pressing their scoreboard key or if the current game mode
	// doesn't use the scoreboard then return false.
	if (( Button_ShowScores.bDown == false ) || ( GAMEMODE_GetCurrentFlags( ) & GMF_DONTUSESCOREBOARD ))
		return false;

	// [AK] We generally don't want to draw the scoreboard in singleplayer games unless we're
	// watching a demo. However, we still want to draw it in deathmatch, teamgame, or invasion.
	if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) && ( CLIENTDEMO_IsPlaying( ) == false ) && (( deathmatch || teamgame || invasion ) == false ))
		return false;

	return true;
}

//*****************************************************************************
//
// [TP]
//
bool SCOREBOARD_ShouldDrawRank( ULONG ulPlayer )
{
	return (( deathmatch ) && (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) == 0 ) && ( PLAYER_IsTrueSpectator( &players[ulPlayer] ) == false ));
}

//*****************************************************************************
// Renders some HUD strings, and the main board if the player is pushing the keys.
//
void SCOREBOARD_Render( ULONG ulDisplayPlayer )
{
	// Make sure the display player is valid.
	if ( ulDisplayPlayer >= MAXPLAYERS )
		return;

	// Initialization for text scaling.
	g_ValWidth	= con_virtualwidth.GetGenericRep( CVAR_Int );
	g_ValHeight	= con_virtualheight.GetGenericRep( CVAR_Int );

	g_bScale = HUD_IsScaled( );
	g_ulTextHeight = SmallFont->GetHeight( ) + 1;

	if ( g_bScale )
	{
		g_fXScale = static_cast<float>( g_ValWidth.Int ) / 320.0f;
		g_fYScale = static_cast<float>( g_ValHeight.Int ) / 200.0f;
		g_rXScale = static_cast<float>( g_ValWidth.Int ) / SCREENWIDTH;
		g_rYScale = static_cast<float>( g_ValHeight.Int ) / SCREENHEIGHT;
		g_ulTextHeight = Scale( SCREENHEIGHT, g_ulTextHeight, con_virtualheight );
	}
	else
	{
		g_fXScale = static_cast<float>( SCREENWIDTH ) / 320.0f;
		g_fYScale = static_cast<float>( SCREENHEIGHT ) / 200.0f;
		g_rXScale = g_rYScale = 1.0f;
	}

	// Draw the main scoreboard.
	if (SCOREBOARD_ShouldDrawBoard( ulDisplayPlayer ))
		SCOREBOARD_RenderBoard( ulDisplayPlayer );

	if ( CALLVOTE_ShouldShowVoteScreen( ))
	{		
		// [RC] Display either the fullscreen or minimized vote screen.
		if ( cl_showfullscreenvote )
			SCOREBOARD_RenderInVoteClassic( );
		else
			SCOREBOARD_RenderInVote( );
	}

	// [AK] Render the countdown screen when we're in the countdown.
	if ( GAMEMODE_GetState( ) == GAMESTATE_COUNTDOWN )
		scoreboard_RenderCountdown( GAMEMODE_GetCountdownTicks( ) + TICRATE );
	// [AK] Render the invasion stats while the game is in progress.
	else if (( invasion) && ( GAMEMODE_GetState( ) == GAMESTATE_INPROGRESS ))
		SCOREBOARD_RenderInvasionStats( );

	if ( HUD_IsVisible( ))
	{
		// Draw the item holders (hellstone, flags, skulls, etc).
		SCOREBOARD_RenderStats_Holders( );

		if (( HUD_IsUsingNewHud( ) && HUD_IsFullscreen( )) == false )
		{
			// Are we in a team game? Draw scores.
			if( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
				SCOREBOARD_RenderStats_TeamScores( );

			if ( !players[ulDisplayPlayer].bSpectating )
			{
				// Draw the player's rank and spread in FFA modes.
				if ((( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) == false ) && ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNFRAGS ))
					SCOREBOARD_RenderStats_RankSpread( );

				// [BB] Draw number of lives left.
				if ( GAMEMODE_AreLivesLimited ( ) )
				{
					FString text;
					text.Format( "Lives: %d / %d", static_cast<unsigned int>( players[ulDisplayPlayer].ulLivesLeft + 1 ), GAMEMODE_GetMaxLives( ));
					HUD_DrawText ( SmallFont, CR_RED, 0, static_cast<int> ( g_rYScale * ( ST_Y - g_ulTextHeight + 1 ) ), text );
				}
			}

		}
	}
	
	// Display the bottom message.
	scoreboard_DrawBottomString( ulDisplayPlayer );
}

//*****************************************************************************
//
void SCOREBOARD_RenderBoard( ULONG ulDisplayPlayer )
{
	ULONG	ulNumIdealColumns;

	// Make sure the display player is valid.
	if ( ulDisplayPlayer >= MAXPLAYERS )
		return;

	// [AK] Draw the scoreboard header at the top.
	scoreboard_DrawHeader( ulDisplayPlayer );

	// Draw the player list and its data.
	// First, determine how many columns we can use, based on our screen resolution.
	ulNumIdealColumns = 3;
	if ( g_bScale )
	{
		if ( g_ValWidth.Int >= 480 )
			ulNumIdealColumns = 4;
		if ( g_ValWidth.Int >= 600 )
			ulNumIdealColumns = 5;
	}
	else
	{
		if ( SCREENWIDTH >= 480 )
			ulNumIdealColumns = 4;
		if ( SCREENWIDTH >= 600 )
			ulNumIdealColumns = 5;
	}

	// The 5 column display is only availible for modes that support it.
	if (( ulNumIdealColumns == 5 ) && !( GAMEMODE_GetCurrentFlags() & (GMF_PLAYERSEARNPOINTS|GMF_PLAYERSEARNWINS) ))
		ulNumIdealColumns = 4;

	if ( ulNumIdealColumns == 5 )
		scoreboard_Prepare5ColumnDisplay( );
	else if( ulNumIdealColumns == 4 )
		scoreboard_Prepare4ColumnDisplay( );
	else
		scoreboard_Prepare3ColumnDisplay( );

	// Draw the headers, list, entries, everything.
	scoreboard_DrawRankings( ulDisplayPlayer );
}

//*****************************************************************************
//
void SCOREBOARD_RenderStats_Holders( void )
{
	ULONG ulYPos;
	ULONG color = CR_GRAY;
	FString patchName;
	FString text;

	// Draw the carrier information for ONE object (POS, TERM, OFCTF).
	if ( oneflagctf || terminator || possession || teampossession)
	{
		// Decide what text and object needs to be drawn.
		if ( oneflagctf )
		{
			patchName = "STFLA3";

			if ( g_pArtifactCarrier )
				text.AppendFormat( "%s" TEXTCOLOR_NORMAL ": ", g_pArtifactCarrier->userinfo.GetName( ));
			else
				text.AppendFormat( "%s: ", TEAM_GetReturnTicks( teams.Size( )) ? "?" : "-" );
		}
		else
		{
			// [AK] Draw the terminator sphere or hellstone icons in their respective gamemodes.
			color = CR_RED;
			patchName = terminator ? "TERMINAT" : "HELLSTON";
			text.Format( "%s" TEXTCOLOR_NORMAL ": ", g_pArtifactCarrier ? g_pArtifactCarrier->userinfo.GetName( ) : "-" );

			// [AK] Use the carrier's team colors in the string if applicable.
			if (( g_pArtifactCarrier ) && ( teampossession ) && ( TEAM_CheckIfValid( g_pArtifactCarrier->Team )))
			{
				color = TEAM_GetTextColor( g_pArtifactCarrier->Team );
				text.Format( "%s" TEXTCOLOR_NORMAL ": ", g_pArtifactCarrier->userinfo.GetName( ));
			}
		}

		// Now, draw it.
		ULONG ulXPos = HUD_GetWidth( ) - TexMan[patchName]->GetWidth( );
		ulYPos = ST_Y - g_ulTextHeight * 3 + 1;

		HUD_DrawTexture( TexMan[patchName], ulXPos, static_cast<int>( ulYPos * g_rYScale ), g_bScale );
		HUD_DrawText( SmallFont, color, ulXPos - SmallFont->StringWidth( text ), static_cast<int>( ulYPos * g_rYScale ), text, g_bScale );
	}

	// Draw the carrier information for TWO objects (ST, CTF).
	else if ( ctf || skulltag )
	{
		ulYPos = ST_Y - g_ulTextHeight * 3 + 1;

		for ( LONG lTeam = teams.Size( ) - 1; lTeam >= 0; lTeam-- )
		{
			if ( TEAM_ShouldUseTeam( lTeam ) == false )
				continue;

			// [AK] Get the player carrying this team's flag or skull.
			player_t *carrier = TEAM_GetCarrier( lTeam );
			patchName = TEAM_GetSmallHUDIcon( lTeam );
			color = TEAM_GetTextColor( lTeam );

			if ( carrier )
				text.Format( "%s", carrier->userinfo.GetName( ));
			else
				text.Format( TEXTCOLOR_GRAY "%s", TEAM_GetReturnTicks( lTeam ) ? "?" : "-" );

			text += TEXTCOLOR_NORMAL ": ";

			// Now, draw it.
			ULONG ulXPos = HUD_GetWidth( ) - TexMan[patchName]->GetWidth( );
			HUD_DrawTexture( TexMan[patchName], ulXPos, static_cast<int>( ulYPos * g_rYScale ), g_bScale );

			HUD_DrawText( SmallFont, color, ulXPos - SmallFont->StringWidth( text ), static_cast<int>( ulYPos * g_rYScale ), text, g_bScale );
			ulYPos -= g_ulTextHeight;
		}
	}
	//Domination can have an indefinite amount
	else if ( domination )
	{
		int numPoints = DOMINATION_NumPoints( );
		unsigned int *pointOwners = DOMINATION_PointOwners( );

		for ( int i = numPoints - 1; i >= 0; i-- )
		{
			if ( TEAM_CheckIfValid( pointOwners[i] ))
			{
				color = TEAM_GetTextColor( pointOwners[i] );
				text = TEAM_GetName( pointOwners[i] );
			}
			else
			{
				text = "-";
			}
		
			text.AppendFormat( ": " TEXTCOLOR_GRAY "%s", level.info->SectorInfo.PointNames[i]->GetChars( ));
			HUD_DrawTextAligned( color, static_cast<int>( ST_Y * g_rYScale ) - ( numPoints - i ) * SmallFont->GetHeight( ), text, false, g_bScale );
		}
	}
}

//*****************************************************************************
//

void SCOREBOARD_RenderStats_TeamScores( void )
{	
	ULONG ulFlags = GAMEMODE_GetCurrentFlags( );
	LONG lTeamScore = 0;

	// [AK] Don't render anything if there's no teams.
	if (( ulFlags & GMF_PLAYERSONTEAMS ) == false )
		return;

	// [AK] Don't render anything if we can't earn frags, points, or wins.
	if (( ulFlags & ( GMF_PLAYERSEARNFRAGS | GMF_PLAYERSEARNPOINTS | GMF_PLAYERSEARNWINS )) == false )
		return;

	if ( HUD_IsFullscreen( ))
	{
		// The classic sbar HUD for Doom, Heretic, and Hexen has its own display for CTF and Skulltag scores.
		if ((( gameinfo.gametype == GAME_Doom ) || ( gameinfo.gametype == GAME_Raven )) && ( ctf || oneflagctf || skulltag ))
			return;
	}

	ULONG ulYPos = ST_Y - ( g_ulTextHeight * 2 ) + 1;
	FString text;

	for ( ULONG ulTeam = 0; ulTeam < teams.Size( ); ulTeam++ )
	{
		if (( TEAM_ShouldUseTeam( ulTeam ) == false ) || ( TEAM_CountPlayers( ulTeam ) < 1 ))
			continue;

		// [AK] Get this team's win, point, or frag count.
		if ( ulFlags & GMF_PLAYERSEARNWINS )
			lTeamScore = TEAM_GetWinCount( ulTeam );
		else if ( ulFlags & GMF_PLAYERSEARNPOINTS )
			lTeamScore = TEAM_GetScore( ulTeam );
		else
			lTeamScore = TEAM_GetFragCount( ulTeam );

		// Now, draw it.
		text.Format( "%s: " TEXTCOLOR_GRAY "%d", TEAM_GetName( ulTeam ), static_cast<int>( lTeamScore ));
		HUD_DrawText( SmallFont, TEAM_GetTextColor( ulTeam ), 0, static_cast<int>( ulYPos * g_rYScale ), text, g_bScale );
		ulYPos -= g_ulTextHeight;
	}
}
//*****************************************************************************
//
void SCOREBOARD_RenderStats_RankSpread( void )
{
	// [RC] Don't draw this if there aren't any competitors.
	if ( g_ulNumPlayers <= 1 )
		return;

	ULONG ulYPos = ST_Y - g_ulTextHeight * 2 + 1;
	FString text;

	// [RC] Move this up to make room for armor on the fullscreen, classic display.
	if (( st_scale == false ) && ( screenblocks > 10 ))
		ulYPos -= g_ulTextHeight * 2;

	// [AK] Draw this player's rank.
	text.Format( "Rank: " TEXTCOLOR_GRAY "%d/%s%d", static_cast<unsigned int>( g_ulRank + 1 ), g_bIsTied ? TEXTCOLOR_RED : "", static_cast<unsigned int>( g_ulNumPlayers ));
	HUD_DrawText( SmallFont, CR_RED, 0, static_cast<int>( ulYPos * g_rYScale ), text, g_bScale );

	ulYPos += g_ulTextHeight;

	// [AK] Draw this player's spread.
	text.Format( "Spread: %s%d", g_lSpread > 0 ? TEXTCOLOR_BOLD : TEXTCOLOR_GRAY, static_cast<int>( g_lSpread ));
	HUD_DrawText( SmallFont, CR_RED, 0, static_cast<int>( ulYPos * g_rYScale ), text, g_bScale );

	// 'Wins' isn't an entry on the statusbar, so we have to draw this here.
	unsigned int viewplayerwins = static_cast<unsigned int>( players[HUD_GetViewPlayer( )].ulWins );
	if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNWINS ) && ( viewplayerwins > 0 ))
	{
		text.Format( "Wins: " TEXTCOLOR_GRAY "%u", viewplayerwins );
		HUD_DrawText( SmallFont, CR_RED, HUD_GetWidth( ) - SmallFont->StringWidth( text ), static_cast<int>( ulYPos * g_rYScale ), text, g_bScale );
	}
}

//*****************************************************************************
//

void SCOREBOARD_RenderInvasionStats( void )
{
	if (( HUD_IsUsingNewHud( )) && ( HUD_IsFullscreen( )))
		return;

	if ( HUD_IsVisible( ))
	{
		FString text;
		text.Format( "Wave: %d  Monsters: %d  Arch-Viles: %d", static_cast<unsigned int>( INVASION_GetCurrentWave( )), static_cast<unsigned int>( INVASION_GetNumMonstersLeft( )), static_cast<unsigned int>( INVASION_GetNumArchVilesLeft( )));

		DHUDMessage *pMsg = new DHUDMessage( SmallFont, text, 0.5f, 0.075f, 0, 0, CR_RED, 0.1f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'I', 'N', 'V', 'S' ));
	}
}

//*****************************************************************************
//
void SCOREBOARD_RenderInVoteClassic( void )
{
	ULONG ulNumYes = CALLVOTE_CountPlayersWhoVotedYes( );
	ULONG ulNumNo = CALLVOTE_CountPlayersWhoVotedNo( );
	ULONG *pulPlayersWhoVotedYes = CALLVOTE_GetPlayersWhoVotedYes( );
	ULONG *pulPlayersWhoVotedNo = CALLVOTE_GetPlayersWhoVotedNo( );
	ULONG ulMaxYesOrNoVoters = ( MAXPLAYERS / 2 ) + 1;
	FString text;

	// Start with the "VOTE NOW!" title.
	ULONG ulYPos = 16;
	HUD_DrawTextCleanCentered( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED, ulYPos, "VOTE NOW!" );

	// Render who called the vote.
	ulYPos += 24;
	text.Format( "Vote called by: %s", players[CALLVOTE_GetVoteCaller( )].userinfo.GetName( ));
	HUD_DrawTextCleanCentered( SmallFont, CR_UNTRANSLATED, ulYPos, text );

	// Render the command being voted on.
	ulYPos += 16;
	HUD_DrawTextCleanCentered( SmallFont, CR_WHITE, ulYPos, CALLVOTE_GetVoteMessage( ));

	// Render the reason for the vote being voted on.
	if ( strlen( CALLVOTE_GetReason( )) > 0 )
	{
		ulYPos += 16;
		text.Format( "Reason: %s", CALLVOTE_GetReason( ));
		HUD_DrawTextCleanCentered( SmallFont, CR_ORANGE, ulYPos, text );
	}

	// Render how much time is left to vote.
	ulYPos += 16;
	text.Format( "Vote ends in: %d", static_cast<unsigned int>(( CALLVOTE_GetCountdownTicks( ) + TICRATE ) / TICRATE ));
	HUD_DrawTextCleanCentered( SmallFont, CR_RED, ulYPos, text );

	// Display how many have voted for "Yes" and "No".
	ulYPos += 16;
	text.Format( "Yes: %d", static_cast<unsigned int>( ulNumYes ));
	HUD_DrawTextClean( SmallFont, CR_UNTRANSLATED, 32, ulYPos, text );

	text.Format( "No: %d", static_cast<unsigned int>( ulNumNo ));
	HUD_DrawTextClean( SmallFont, CR_UNTRANSLATED, 320 - 32 - SmallFont->StringWidth( text ), ulYPos, text );

	ulYPos += 8;
	ULONG ulOldYPos = ulYPos;

	// [AK] Show a list of all the players who voted yes.
	for ( ULONG ulIdx = 0; ulIdx < ulMaxYesOrNoVoters; ulIdx++ )
	{
		if ( pulPlayersWhoVotedYes[ulIdx] != MAXPLAYERS )
		{
			ulYPos += 8;
			text.Format( "%s", players[ulIdx].userinfo.GetName( ));
			HUD_DrawTextClean( SmallFont, CR_UNTRANSLATED, 32, ulYPos, text );
		}
	}

	ulYPos = ulOldYPos;

	// [AK] Next, show another list with all the players who voted no.
	for ( ULONG ulIdx = 0; ulIdx < ulMaxYesOrNoVoters; ulIdx++ )
	{
		if ( pulPlayersWhoVotedNo[ulIdx] != MAXPLAYERS )
		{
			ulYPos += 8;
			text.Format( "%s", players[ulIdx].userinfo.GetName( ));
			HUD_DrawTextClean( SmallFont, CR_UNTRANSLATED, 320 - 32 - SmallFont->StringWidth( text ), ulYPos, text );
		}
	}
}

//*****************************************************************************
//
// [RC] New compact version; RenderInVoteClassic is the fullscreen version
//
void SCOREBOARD_RenderInVote( void )
{
	ULONG ulNumYes = CALLVOTE_CountPlayersWhoVotedYes( );
	ULONG ulNumNo = CALLVOTE_CountPlayersWhoVotedNo( );
	ULONG ulVoteChoice = CALLVOTE_GetPlayerVoteChoice( consoleplayer );
	FString text;

	// Render the title and time left.
	ULONG ulYPos = 8;
	text.Format( "VOTE NOW! ( %d )", static_cast<unsigned int>(( CALLVOTE_GetCountdownTicks( ) + TICRATE ) / TICRATE ));
	HUD_DrawTextCentered( BigFont, gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED, ulYPos, text, g_bScale );

	// Render the command being voted on.
	ulYPos += 14;
	HUD_DrawTextCentered( SmallFont, CR_WHITE, ulYPos, CALLVOTE_GetVoteMessage( ), g_bScale );

	ulYPos += 4;

	// Render the reason of the vote being voted on.
	if ( strlen( CALLVOTE_GetReason( )) > 0 )
	{
		ulYPos += 8;
		text.Format( "Reason: %s", CALLVOTE_GetReason( ));
		HUD_DrawTextCentered( SmallFont, CR_ORANGE, ulYPos, text, g_bScale );
	}

	ulYPos += 8;

	// Render the number of votes.
	text.Format( "%sYes: %d", ulVoteChoice == VOTE_YES ? TEXTCOLOR_YELLOW : "", static_cast<unsigned int>( ulNumYes ));
	text.AppendFormat( TEXTCOLOR_NORMAL ", %sNo: %d", ulVoteChoice == VOTE_NO ? TEXTCOLOR_YELLOW : "", static_cast<unsigned int>( ulNumNo ));
	HUD_DrawTextCentered( SmallFont, CR_DARKBROWN, ulYPos, text, g_bScale );

	// Render the explanation of keys.
	if ( ulVoteChoice == VOTE_UNDECIDED )
	{
		char keyVoteYes[16];
		C_FindBind( "vote_yes", keyVoteYes );

		char keyVoteNo[16];
		C_FindBind( "vote_no", keyVoteNo );

		ulYPos += 8;
		text.Format( "%s | %s", keyVoteYes, keyVoteNo );
		HUD_DrawTextCentered( SmallFont, CR_BLACK, ulYPos, text, g_bScale );
	}
}

//*****************************************************************************
//
static void scoreboard_RenderCountdown( ULONG ulTimeLeft )
{
	// [AK] Don't draw anything if we're on the intermission screen.
	if ( gamestate != GS_LEVEL )
		return;

	ULONG ulTitleColor = gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED;
	ULONG ulYPos = 32;
	FString text;

	if ( duel )
	{
		// This really should not happen, because if we can't find two duelers, we're shouldn't be
		// in the countdown phase.
		if (( g_pDuelers[0] == NULL ) || ( g_pDuelers[1] == NULL ))
			return;

		// [AK] Draw the versus message that appears between the two names.
		HUD_DrawTextCleanCentered( BigFont, CR_UNTRANSLATED, ulYPos, "vs." );

		// [AK] Next, draw the names of the two duelers.
		HUD_DrawTextCleanCentered( BigFont, ulTitleColor, ulYPos - 16, g_pDuelers[0]->userinfo.GetName( ));
		HUD_DrawTextCleanCentered( BigFont, ulTitleColor, ulYPos + 16, g_pDuelers[1]->userinfo.GetName( ));
		ulYPos += 40;
	}
	else
	{
		// [AK] TLMS and team possession should still keep "team" in the title for consistency.
		text = invasion ? INVASION_GetCurrentWaveString( ) : GAMEMODE_GetName( GAMEMODE_GetCurrentMode( ));

		// [AK] Append "co-op" to the end of "survival".
		if ( survival )
			text += " Co-op";

		HUD_DrawTextCleanCentered( BigFont, ulTitleColor, ulYPos, text );
		ulYPos += 24;
	}

	// [AK] Draw the actual countdown message.
	if ( invasion )
		text = INVASION_GetState( ) == IS_FIRSTCOUNTDOWN ? "First wave begins" : "Begins";
	else
		text = "Match begins";

	text.AppendFormat( " in: %d", static_cast<unsigned int>( ulTimeLeft / TICRATE ));
	HUD_DrawTextCleanCentered( SmallFont, CR_UNTRANSLATED, ulYPos, text );
}

//*****************************************************************************
//
LONG SCOREBOARD_CalcSpread( ULONG ulPlayerNum )
{
	ULONG ulFlags = GAMEMODE_GetCurrentFlags( );
	LONG lHighestScore = 0;
	bool bInit = true;

	// First, find the highest fragcount that isn't ours.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( ulPlayerNum == ulIdx ) || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if (( ulFlags & GMF_PLAYERSEARNWINS ) && (( bInit ) || ( players[ulIdx].ulWins > static_cast<ULONG>( lHighestScore ))))
		{
			lHighestScore = players[ulIdx].ulWins;
			bInit = false;
		}
		else if (( ulFlags & GMF_PLAYERSEARNPOINTS ) && (( bInit ) || ( players[ulIdx].lPointCount > lHighestScore )))
		{
			lHighestScore = players[ulIdx].lPointCount;
			bInit = false;
		}
		else if (( ulFlags & GMF_PLAYERSEARNFRAGS ) && (( bInit ) || ( players[ulIdx].fragcount > lHighestScore )))
		{
			lHighestScore = players[ulIdx].fragcount;
			bInit = false;
		}
	}

	// [AK] Return the difference between our score and the highest score.
	if ( bInit == false )
	{
		if ( ulFlags & GMF_PLAYERSEARNWINS )
			return ( players[ulPlayerNum].ulWins - lHighestScore );
		else if ( ulFlags & GMF_PLAYERSEARNPOINTS )
			return ( players[ulPlayerNum].lPointCount - lHighestScore );
		else
			return ( players[ulPlayerNum].fragcount - lHighestScore );
	}

	// [AK] If we're the only person in the game just return zero.
	return ( 0 );
}

//*****************************************************************************
//
ULONG SCOREBOARD_CalcRank( ULONG ulPlayerNum )
{
	ULONG ulFlags = GAMEMODE_GetCurrentFlags( );
	ULONG ulRank = 0;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( ulIdx == ulPlayerNum ) || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if (( ulFlags & GMF_PLAYERSEARNWINS ) && ( players[ulIdx].ulWins > players[ulPlayerNum].ulWins ))
			ulRank++;
		else if (( ulFlags & GMF_PLAYERSEARNPOINTS ) && ( players[ulIdx].lPointCount > players[ulPlayerNum].lPointCount ))
			ulRank++;
		else if (( ulFlags & GMF_PLAYERSEARNFRAGS ) && ( players[ulIdx].fragcount > players[ulPlayerNum].fragcount ))
			ulRank++;
	}

	return ( ulRank );
}

//*****************************************************************************
//
bool SCOREBOARD_IsTied( ULONG ulPlayerNum )
{
	ULONG ulFlags = GAMEMODE_GetCurrentFlags( );

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( ulIdx == ulPlayerNum ) || ( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )))
			continue;

		if (( ulFlags & GMF_PLAYERSEARNWINS ) && ( players[ulIdx].ulWins == players[ulPlayerNum].ulWins ))
			return ( true );
		else if (( ulFlags & GMF_PLAYERSEARNPOINTS ) && ( players[ulIdx].lPointCount == players[ulPlayerNum].lPointCount ))
			return ( true );
		else if (( ulFlags & GMF_PLAYERSEARNFRAGS ) && ( players[ulIdx].fragcount == players[ulPlayerNum].fragcount ))
			return ( true );
	}

	return ( false );
}

//*****************************************************************************
//
FString SCOREBOARD_BuildPointString( void )
{
	ULONG ulFlags = GAMEMODE_GetCurrentFlags( );
	ULONG ulNumAvailableTeams = 0;
	ULONG ulNumTeamsWithHighestScore = 0;
	LONG lHighestScore = LONG_MIN;
	LONG lLowestScore = LONG_MAX;

	FString scoreName, teamName;
	FString lastTeamName;
	LONG (*scoreFunction)( ULONG );

	// [AK] Determine what kind of score we are interested in (wins, points, frags).
	if ( ulFlags & GMF_PLAYERSEARNWINS )
	{
		scoreName = "win";
		scoreFunction = &TEAM_GetWinCount;
	}
	else if ( ulFlags & GMF_PLAYERSEARNPOINTS )
	{
		scoreName = "point";
		scoreFunction = &TEAM_GetScore;
	}
	else if ( ulFlags & GMF_PLAYERSEARNFRAGS )
	{
		scoreName = "frag";
		scoreFunction = &TEAM_GetFragCount;
	}

	// [AK] Get the score of any available teams;
	for ( ULONG ulTeam = 0; ulTeam < teams.Size( ); ulTeam++ )
	{
		if ( TEAM_ShouldUseTeam( ulTeam ) == false )
			continue;

		// [AK] Get this team's score and keep track of how many teams are actually available.
		// Simply returning TEAM_GetNumAvailableTeams doesn't always work in this case.
		LONG lTeamScore = scoreFunction( ulTeam );
		ulNumAvailableTeams++;

		// [AK] Is this team's score greater than the highest score we got?
		if ( lTeamScore > lHighestScore )
		{
			lHighestScore = lTeamScore;

			// [AK] Reset the list of team names, starting with this team.
			teamName = TEXTCOLOR_ESCAPE;
			teamName.AppendFormat( "%c%s", V_GetColorChar( TEAM_GetTextColor( ulTeam )), TEAM_GetName( ulTeam ));
			ulNumTeamsWithHighestScore = 1;
		}
		// [AK] If this team's score is equal to the current highest score, add their name to the end of the list.
		else if (( lTeamScore == lHighestScore ) && ( ulNumTeamsWithHighestScore > 0 ))
		{
			// [AK] If there's more than two teams with the highest score, add a comma and the
			// name of the team we got last.
			if (( ulNumTeamsWithHighestScore >= 2 ) && ( lastTeamName.IsNotEmpty( )))
				teamName.AppendFormat( TEXTCOLOR_NORMAL ", %s", lastTeamName.GetChars( ));

			// [AK] Store this team's name and text color into a string, we'll need it later.
			lastTeamName = TEXTCOLOR_ESCAPE;
			lastTeamName.AppendFormat( "%c%s", V_GetColorChar( TEAM_GetTextColor( ulTeam )), TEAM_GetName( ulTeam ));
			ulNumTeamsWithHighestScore++;
		}
		
		// [AK] Is this team's score less than the lowest score we got?
		if ( lTeamScore < lLowestScore )
			lLowestScore = lTeamScore;
	}

	FString text;
	scoreName.AppendFormat( "%s", (( ulNumAvailableTeams == 2 ) || ( lHighestScore != 1 )) ? "s" : "" );

	// Build the score message.
	if ( ulNumAvailableTeams == ulNumTeamsWithHighestScore )
	{
		text.Format( "Teams are tied at %d %s", static_cast<int>( lHighestScore ), scoreName );
	}
	else
	{
		if ( ulNumAvailableTeams > 2 )
		{
			if ( ulNumTeamsWithHighestScore == 1 )
			{
				// [AK] Show the team with the highest score and how much they have.
				text.Format( "%s" TEXTCOLOR_NORMAL " %s with ", teamName.GetChars( ), gamestate == GS_LEVEL ? "leads" : "has won" );
				text.AppendFormat( "%d %s", static_cast<int>( lHighestScore ), scoreName.GetChars( ));
			}
			else
			{
				// [AK] Add the word "and" before the name of the last team on the list.
				if ( lastTeamName.IsNotEmpty( ))
					teamName.AppendFormat( TEXTCOLOR_NORMAL "%s and %s", ulNumTeamsWithHighestScore > 2 ? "," : "", lastTeamName.GetChars( ));

				// [AK] Show a list of all teams who currently have the highest score and how much they have.
				text.Format( "Teams %s with ", gamestate == GS_LEVEL ? "leading" : "that won" );
				text.AppendFormat( "%d %s: %s", static_cast<int>( lHighestScore ), scoreName.GetChars( ), teamName.GetChars( ));
			}
		}
		else
		{
			// [AK] Also indicate the type of score we're comparing in this string (frags, points, wins).
			text.Format( "%s" TEXTCOLOR_NORMAL " %s ", teamName.GetChars( ), gamestate == GS_LEVEL ? "leads" : "has won" );
			text.AppendFormat( "%d to %d in %s", static_cast<int>( lHighestScore ), static_cast<int>( lLowestScore ), scoreName.GetChars( ));
		}
	}

	return text;
}

//*****************************************************************************
//
// [TP] Now in a function
//
FString SCOREBOARD_SpellOrdinal( int ranknum, bool bColored )
{
	FString result;

	// Determine  what color and number to print for their rank.
	if ( bColored )
	{
		switch ( ranknum )
		{
			case 0:
				result = TEXTCOLOR_BLUE;
				break;

			case 1:
				result = TEXTCOLOR_RED;
				break;

			case 2:
				result = TEXTCOLOR_GREEN;
				break;
		}
	}

	result.AppendFormat( "%d", ranknum + 1 );

	//[ES] This way all ordinals are correctly written.
	if ( ranknum % 100 / 10 != 1 )
	{
		switch ( ranknum % 10 )
		{
			case 0:
				result += "st";
				break;

			case 1:
				result += "nd";
				break;

			case 2:
				result += "rd";
				break;

			default:
				result += "th";
				break;
		}
	}
	else
		result += "th";

	return result;
}

//*****************************************************************************
//
FString SCOREBOARD_BuildPlaceString( ULONG ulPlayer )
{
	FString text;

	// [AK] Only build the string in game modes for which we can earn frags, points, or wins in.
	if ( GAMEMODE_GetCurrentFlags( ) & ( GMF_PLAYERSEARNFRAGS | GMF_PLAYERSEARNPOINTS | GMF_PLAYERSEARNWINS ))
	{
		if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS )
 		{
			// [AK] Show which team(s) have the highest score and how much.
			text = SCOREBOARD_BuildPointString( );
		}
		else
		{
			// If the player is tied with someone else, add a "tied for" to their string.
			if ( SCOREBOARD_IsTied( ulPlayer ))
				text = "Tied for ";

			// [AK] Get the rank of this player, though it isn't always equivalent to g_ulRank. Particularly,
			// when we (the local player) get a frag or get fragged while spying on another player.
			ULONG ulRank = ( ulPlayer == HUD_GetViewPlayer( )) ? g_ulRank : SCOREBOARD_CalcRank( ulPlayer );
			text.AppendFormat( "%s" TEXTCOLOR_NORMAL " place with ", SCOREBOARD_SpellOrdinal( ulRank, true ));

			// Tack on the rest of the string.
			if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNWINS )
				text.AppendFormat( "%d win%s", static_cast<unsigned int>( players[ulPlayer].ulWins ), players[ulPlayer].ulWins != 1 ? "s" : "" );
			else if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNPOINTS )
				text.AppendFormat( "%d point%s", static_cast<int>( players[ulPlayer].lPointCount ), players[ulPlayer].lPointCount != 1 ? "s" : "" );
			else
				text.AppendFormat( "%d frag%s", players[ulPlayer].fragcount, players[ulPlayer].fragcount != 1 ? "s" : "" );
		}
	}

	return text;
}

//*****************************************************************************
//
void SCOREBOARD_DrawFragMessage( player_t *pPlayer, bool bFraggedBy )
{
	FString message = GStrings( bFraggedBy ? "GM_YOUWEREFRAGGED" : "GM_YOUFRAGGED" );
	message.StripLeftRight( );

	// [AK] Don't print the message if the string is empty.
	if ( message.Len( ) == 0 )
		return;

	// [AK] Substitute the fragged/fragging player's name into the message if we can.
	message.Substitute( "%s", pPlayer->userinfo.GetName( ));

	// Print the frag message out in the console.
	Printf( "%s\n", message.GetChars( ));

	DHUDMessageFadeOut *pMsg = new DHUDMessageFadeOut( BigFont, message.GetChars( ), 1.5f, 0.325f, 0, 0, CR_RED, 2.5f, 0.5f );
	StatusBar->AttachMessage( pMsg, MAKE_ID( 'F', 'R', 'A', 'G'));

	// [AK] Build the place string.
	message = SCOREBOARD_BuildPlaceString( consoleplayer );

	if ( bFraggedBy == false )
	{
		ULONG ulMenLeftStanding = 0;

		// [AK] Count how many opponents are currently left.
		if ( lastmanstanding )
		{
			ulMenLeftStanding = GAME_CountLivingAndRespawnablePlayers( ) - 1;
		}
		else if (( teamlms ) && ( players[consoleplayer].bOnTeam ))
		{
			for ( ULONG ulIdx = 0; ulIdx < teams.Size( ); ulIdx++ )
			{
				if (( TEAM_ShouldUseTeam( ulIdx ) == false ) || ( ulIdx == players[consoleplayer].Team ))
					continue;

				ulMenLeftStanding += TEAM_CountLivingAndRespawnablePlayers( ulIdx );
			}
		}

		// [AK] If there are any opponents left, display that instead of the place string.
		if ( ulMenLeftStanding > 0 )
			message.Format( "%d opponent%s left standing", static_cast<unsigned int>( ulMenLeftStanding ), ulMenLeftStanding != 1 ? "s" : "" );
	}

	// [AK] Changed the subtext color to grey to make it more neutral.
	pMsg = new DHUDMessageFadeOut( SmallFont, message.GetChars( ), 1.5f, 0.375f, 0, 0, CR_GREY, 2.5f, 0.5f );
	StatusBar->AttachMessage( pMsg, MAKE_ID( 'P', 'L', 'A', 'C' ));
}

//*****************************************************************************
//
void SCOREBOARD_RefreshHUD( void )
{
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	// [AK] Reset the dueler pointers.
	ULONG ulNumDuelers = 0;
	g_pDuelers[0] = g_pDuelers[1] = NULL;

	// [AK] Determine which players are currently dueling.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( DUEL_IsDueler( ulIdx ))
		{
			g_pDuelers[ulNumDuelers] = &players[ulIdx];

			// [AK] We only need to check for two duelers.
			if ( ++ulNumDuelers == 2 )
				break;
		}
	}

	// [AK] Determine which player is carrying the terminator sphere, possession hellstone, or white flag.
	g_pArtifactCarrier = GAMEMODE_GetArtifactCarrier( );

	// [AK] Determine which players are carrying a team's item.
	if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS )
	{
		for ( ULONG ulTeam = 0; ulTeam < teams.Size( ); ulTeam++ )
		{
			TEAM_SetCarrier( ulTeam, NULL );

			for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				// [AK] Ignore invalid players.
				if (( playeringame[ulIdx] == false ) || ( PLAYER_IsTrueSpectator( &players[ulIdx] )) || ( players[ulIdx].mo == NULL ))
					continue;

				if ( players[ulIdx].mo->FindInventory( TEAM_GetItem( ulTeam )))
					TEAM_SetCarrier( ulTeam, &players[ulIdx] );
			}
		}
	}

	player_t *player = &players[HUD_GetViewPlayer( )];

	g_ulRank = SCOREBOARD_CalcRank( player - players );
	g_lSpread = SCOREBOARD_CalcSpread( player - players );
	g_bIsTied = SCOREBOARD_IsTied( player - players );

	// [AK] Count how many players are in the game.
	g_ulNumPlayers = SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS );

	// "x opponents left", "x allies alive", etc
	if ( GAMEMODE_GetCurrentFlags() & GMF_DEADSPECTATORS )
	{
		// Survival, Survival Invasion, etc
		if ( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE )
			g_lNumAlliesLeft = GAME_CountLivingAndRespawnablePlayers() - PLAYER_IsAliveOrCanRespawn( player );

		// Last Man Standing, TLMS, etc
		if ( GAMEMODE_GetCurrentFlags() & GMF_DEATHMATCH )
		{
			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			{
				unsigned livingAndRespawnableTeammates = TEAM_CountLivingAndRespawnablePlayers( player->Team );
				g_lNumOpponentsLeft = GAME_CountLivingAndRespawnablePlayers() - livingAndRespawnableTeammates;
				g_lNumAlliesLeft = livingAndRespawnableTeammates - PLAYER_IsAliveOrCanRespawn( player );
			}
			else
			{
				g_lNumOpponentsLeft = GAME_CountLivingAndRespawnablePlayers() - PLAYER_IsAliveOrCanRespawn( player );
			}
		}
	}
}

//*****************************************************************************
//
ULONG SCOREBOARD_GetNumPlayers( void )
{
	return ( g_ulNumPlayers );
}

//*****************************************************************************
//
ULONG SCOREBOARD_GetRank( void )
{
	return ( g_ulRank );
}

//*****************************************************************************
//
LONG SCOREBOARD_GetSpread( void )
{
	return ( g_lSpread );
}

//*****************************************************************************
//
LONG SCOREBOARD_GetLeftToLimit( void )
{
	ULONG	ulIdx;

	// If we're not in a level, then clearly there's no need for this.
	if ( gamestate != GS_LEVEL )
		return ( 0 );

	// KILL-based mode. [BB] This works indepently of any players in game.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		if ( invasion )
			return (LONG) INVASION_GetNumMonstersLeft( );
		else if ( dmflags2 & DF2_KILL_MONSTERS )
		{
			if ( level.total_monsters > 0 )
				return ( 100 * ( level.total_monsters - level.killed_monsters ) / level.total_monsters );
			else
				return 0;
		}
		else
			return ( level.total_monsters - level.killed_monsters );
	}

	// [BB] In a team game with only empty teams or if there are no players at all, just return the appropriate limit.
	if ( ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	     && ( TEAM_TeamsWithPlayersOn() == 0 ) )
		 || ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) == 0 ) )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			return winlimit;
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			return pointlimit;
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
			return fraglimit;
		else
			return 0;
	}

	// FRAG-based mode.
	if ( fraglimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		LONG	lHighestFragcount;
				
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			lHighestFragcount = TEAM_GetHighestFragCount( );		
		else
		{
			lHighestFragcount = INT_MIN;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] && !players[ulIdx].bSpectating && players[ulIdx].fragcount > lHighestFragcount )
					lHighestFragcount = players[ulIdx].fragcount;
			}
		}

		return ( fraglimit - lHighestFragcount );
	}

	// POINT-based mode.
	else if ( pointlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
		if ( teamgame || teampossession )
			return ( pointlimit - TEAM_GetHighestScoreCount( ));		
		else // Must be possession mode.
		{
			LONG lHighestPointCount = INT_MIN;
			for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] && !players[ulIdx].bSpectating && players[ulIdx].lPointCount > lHighestPointCount )
					lHighestPointCount = players[ulIdx].lPointCount;
			}

			return pointlimit - (ULONG) lHighestPointCount;
		}
	}

	// WIN-based mode (LMS).
	else if ( winlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		bool	bFoundPlayer = false;
		LONG	lHighestWincount = 0;

		if ( teamlms )
			lHighestWincount = TEAM_GetHighestWinCount( );
		else
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false || players[ulIdx].bSpectating )
					continue;

				if ( bFoundPlayer == false )
				{
					lHighestWincount = players[ulIdx].ulWins;
					bFoundPlayer = true;
					continue;
				}
				else if ( players[ulIdx].ulWins > (ULONG)lHighestWincount )
					lHighestWincount = players[ulIdx].ulWins;
			}
		}

		return ( winlimit - lHighestWincount );
	}

	// None of the above.
	return ( -1 );
}

//*****************************************************************************
//
bool SCOREBOARD_IsTied( void )
{
	return ( g_bIsTied );
}

//*****************************************************************************
//*****************************************************************************
//
static void scoreboard_SortPlayers( ULONG ulSortType )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_iSortedPlayers[ulIdx] = ulIdx;

	if ( ulSortType == ST_FRAGCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_FragCompareFunc );
	else if ( ulSortType == ST_POINTCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_PointsCompareFunc );
	else if ( ulSortType == ST_WINCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_WinsCompareFunc );
	else
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_KillsCompareFunc );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_FragCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].fragcount - players[*(int *)arg1].fragcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].lPointCount - players[*(int *)arg1].lPointCount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].killcount - players[*(int *)arg1].killcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].ulWins - players[*(int *)arg1].ulWins );
}

//*****************************************************************************
//
static void scoreboard_DrawText( const char *pszString, EColorRange Color, ULONG &ulXPos, ULONG ulOffset, bool bOffsetRight )
{
	ulXPos += SmallFont->StringWidth( pszString ) * ( bOffsetRight ? 1 : -1 );
	HUD_DrawText( SmallFont, Color, ulXPos, g_ulCurYPos, pszString, g_bScale );
	ulXPos += ulOffset * ( bOffsetRight ? 1 : -1 );
}

//*****************************************************************************
//
static void scoreboard_DrawIcon( const char *pszPatchName, ULONG &ulXPos, ULONG ulYPos, ULONG ulOffset, bool bOffsetRight )
{
	ulXPos += TexMan[pszPatchName]->GetWidth( ) * ( bOffsetRight ? 1 : -1 );
	ulYPos -= (( TexMan[pszPatchName]->GetHeight( ) - SmallFont->GetHeight( )) >> 1 );

	HUD_DrawTexture( TexMan[pszPatchName], ulXPos, ulYPos, g_bScale );
	ulXPos += ulOffset * ( bOffsetRight ? 1 : -1 );
}

//*****************************************************************************
//
static void scoreboard_RenderIndividualPlayer( ULONG ulDisplayPlayer, ULONG ulPlayer )
{
	ULONG ulColor = CR_GRAY;
	FString text;

	// [AK] Change the text color if we're carrying a terminator sphere or on a team.
	if (( terminator ) && ( players[ulPlayer].cheats2 & CF2_TERMINATORARTIFACT ))
		ulColor = CR_RED;
	else if ( players[ulPlayer].bOnTeam )
		ulColor = TEAM_GetTextColor( players[ulPlayer].Team );
	else if ( ulDisplayPlayer == ulPlayer )
		ulColor = demoplayback ? CR_GOLD : CR_GREEN;

	// Draw the data for each column.
	for ( ULONG ulColumn = 0; ulColumn < g_ulNumColumnsUsed; ulColumn++ )
	{
		// [AK] Determine the x-position of the text for this column.
		ULONG ulXPos = static_cast<ULONG>( g_aulColumnX[ulColumn] * g_fXScale );

		// [AK] We need to display icons and some extra text in the name column.
		if ( g_aulColumnType[ulColumn] == COLUMN_NAME )
		{
			// Track where we are to draw multiple icons.
			ULONG ulXPosOffset = ulXPos - SmallFont->StringWidth( "  " );

			// [TP] If this player is in the join queue, display the position.
			int position = JOINQUEUE_GetPositionInLine( ulPlayer );
			if ( position != -1 )
			{
				text.Format( "%d.", position + 1 );
				scoreboard_DrawText( text, position == 0 ? CR_RED : CR_GOLD, ulXPosOffset, 4 );
			}

			// Draw the user's handicap, if any.
			int handicap = players[ulPlayer].userinfo.GetHandicap( );
			if ( handicap > 0 )
			{
				if (( lastmanstanding ) || ( teamlms ))
					text.Format( "(%d)", deh.MaxSoulsphere - handicap < 1 ? 1 : deh.MaxArmor - handicap );
				else
					text.Format( "(%d)", deh.StartHealth - handicap < 1 ? 1 : deh.StartHealth - handicap );

				scoreboard_DrawText( text, static_cast<EColorRange>( ulColor ), ulXPosOffset, 4 );
			}

			// Draw an icon if this player is a ready to go on.
			if ( players[ulPlayer].bReadyToGoOn )
				scoreboard_DrawIcon( "RDYTOGO", ulXPosOffset, g_ulCurYPos, 4 );

			// Draw a bot icon if this player is a bot.
			if ( players[ulPlayer].bIsBot )
			{
				FString patchName;
				patchName.Format( "BOTSKIL%d", botskill.GetGenericRep( CVAR_Int ).Int );
				scoreboard_DrawIcon( patchName, ulXPosOffset, g_ulCurYPos, 4 );
			}

			// Draw a chat icon if this player is chatting.
			// [Cata] Also shows who's in the console.
			if (( players[ulPlayer].bChatting ) || ( players[ulPlayer].bInConsole ))
				scoreboard_DrawIcon( players[ulPlayer].bInConsole ? "CONSMINI" : "TLKMINI", ulXPosOffset, g_ulCurYPos, 4 );

			// [AK] Also show an icon if the player is lagging to the server.
			if (( players[ulPlayer].bLagging ) && ( players[ulPlayer].bSpectating == false ) && ( gamestate == GS_LEVEL ))
				scoreboard_DrawIcon( "LAGMINI", ulXPosOffset, g_ulCurYPos, 4 );

			// Draw text if there's a vote on and this player voted.
			if ( CALLVOTE_GetVoteState( ) == VOTESTATE_INVOTE )
			{
				ULONG ulVoteChoice = CALLVOTE_GetPlayerVoteChoice( ulPlayer );

				// [AK] Check if this player either voted yes or no.
				if ( ulVoteChoice != VOTE_UNDECIDED )
				{
					text.Format( "(%s)", ulVoteChoice == VOTE_YES ? "Yes" : "No" );
					scoreboard_DrawText( text, CALLVOTE_GetVoteCaller( ) == ulPlayer ? CR_RED : CR_GOLD, ulXPosOffset, 4 );
				}
			}

			text = players[ulPlayer].userinfo.GetName( );
		}
		else if ( g_aulColumnType[ulColumn] == COLUMN_TIME )
		{
			text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulTime / ( TICRATE * 60 )));
		}
		else if ( g_aulColumnType[ulColumn] == COLUMN_PING )
		{
			text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulPing ));
		}
		else if ( g_aulColumnType[ulColumn] == COLUMN_DEATHS )
		{
			text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulDeathCount ));
		}
		else
		{
			switch ( g_aulColumnType[ulColumn] )
			{
				case COLUMN_FRAGS:
					text.Format( "%d", players[ulPlayer].fragcount );
					break;

				case COLUMN_POINTS:
					text.Format( "%d", static_cast<int>( players[ulPlayer].lPointCount ));
					break;

				case COLUMN_POINTSASSISTS:
					text.Format( "%d / %d", static_cast<int>( players[ulPlayer].lPointCount ), static_cast<unsigned int>( players[ulPlayer].ulMedalCount[14] ));
					break;

				case COLUMN_WINS:
					text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulWins ));
					break;

				case COLUMN_KILLS:
					text.Format( "%d", players[ulPlayer].killcount );
					break;

				case COLUMN_SECRETS:
					text.Format( "%d", players[ulPlayer].secretcount );
					break;
			}

			// If the player isn't really playing, change this.
			if ( PLAYER_IsTrueSpectator( &players[ulPlayer] ))
				text = "Spect";
			else if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) && ( players[ulPlayer].bOnTeam == false ))
				text = "No Team";
			else if (( GAMEMODE_GetCurrentFlags( ) & GMF_DEADSPECTATORS ) && (( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) && ( gamestate != GS_INTERMISSION ))
				text = "Dead";
		}

		HUD_DrawText( SmallFont, ulColor, ulXPos, g_ulCurYPos, text, g_bScale );
	}
}

//*****************************************************************************
//
static void scoreboard_DrawHeader( ULONG ulPlayer )
{
	g_ulCurYPos = 4;

	// Draw the "RANKINGS" text at the top. Don't draw it if we're in the intermission.
	if ( gamestate == GS_LEVEL )
		HUD_DrawTextCentered( BigFont, CR_RED, g_ulCurYPos, "RANKINGS", g_bScale );

	g_ulCurYPos += 22;

	// Draw the time, frags, points, or kills we have left until the level ends.
	if ( gamestate == GS_LEVEL )
	{
		// Generate the limit strings.
		std::list<FString> lines;
		SCOREBOARD_BuildLimitStrings( lines, true );

		// Now, draw them.
		for ( std::list<FString>::iterator i = lines.begin( ); i != lines.end( ); i++ )
		{
			HUD_DrawTextCentered( SmallFont, CR_GREY, g_ulCurYPos, *i, g_bScale );
			g_ulCurYPos += 10;
		}
	}

	// Draw the team scores and their relation (tied, red leads, etc).
	if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS )
	{
		if ( gamestate != GS_LEVEL )
			g_ulCurYPos += 10;

		HUD_DrawTextCentered( SmallFont, CR_GREY, g_ulCurYPos, SCOREBOARD_BuildPointString( ), g_bScale );
		g_ulCurYPos += 10;
	}
	// Draw my rank and my frags, points, etc. Don't draw it if we're in the intermission.
	else if (( gamestate == GS_LEVEL ) && ( SCOREBOARD_ShouldDrawRank( ulPlayer )))
	{
		HUD_DrawTextCentered( SmallFont, CR_GREY, g_ulCurYPos, SCOREBOARD_BuildPlaceString( ulPlayer ), g_bScale );
		g_ulCurYPos += 10;
	}
}

//*****************************************************************************
// [RC] Helper method for SCOREBOARD_BuildLimitStrings. Creates a "x things remaining" message.
//
void scoreboard_AddSingleLimit( std::list<FString> &lines, bool condition, int remaining, const char *pszUnitName )
{
	if ( condition && remaining > 0 )
	{
		char	szString[128];
		sprintf( szString, "%d %s%s remain%s", static_cast<int> (remaining), pszUnitName, ( remaining == 1 ) ? "" : "s", ( remaining == 1 ) ? "s" : "" );
		lines.push_back( szString );
	}
}

//*****************************************************************************
//
// [RC] Builds the series of "x frags left / 3rd match between the two / 15:10 remain" strings. Used here and in serverconsole.cpp
//
void SCOREBOARD_BuildLimitStrings( std::list<FString> &lines, bool bAcceptColors )
{
	char	szString[128];

	if ( gamestate != GS_LEVEL )
		return;

	LONG remaining = SCOREBOARD_GetLeftToLimit( );

	// Build the fraglimit and/or duellimit strings.
	scoreboard_AddSingleLimit( lines, ( fraglimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ), remaining, "frag" );
	// [TL] The number of duels left is the maximum number of duels less the number of duels fought.
	scoreboard_AddSingleLimit( lines, ( duellimit && duel ), duellimit - DUEL_GetNumDuels( ), "duel" );

	// Build the "wins" string.
	if ( duel && duellimit )
	{
		LONG lWinner = -1;
		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if ( playeringame[ulIdx] && players[ulIdx].ulWins )
			{
				lWinner = ulIdx;
				break;
			}
		}

		bool bDraw = true;
		if ( lWinner == -1 )
		{
			if ( DUEL_CountActiveDuelers( ) == 2 )
				sprintf( szString, "First match between the two" );
			else
				bDraw = false;
		}
		else
			sprintf( szString, "Champion is %s \\c-with %d win%s", players[lWinner].userinfo.GetName(), static_cast<unsigned int> (players[lWinner].ulWins), players[lWinner].ulWins == 1 ? "" : "s" );

		if ( bDraw )
		{
			V_ColorizeString( szString );
			if ( !bAcceptColors )
				V_StripColors( szString );
			lines.push_back( szString );
		}
	}

	// Build the pointlimit, winlimit, and/or wavelimit strings.
	scoreboard_AddSingleLimit( lines, ( pointlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS ), remaining, "point" );
	scoreboard_AddSingleLimit( lines, ( winlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS ), remaining, "win" );
	scoreboard_AddSingleLimit( lines, ( invasion && wavelimit ), wavelimit - INVASION_GetCurrentWave( ), "wave" );

	// Render the timelimit string. - [BB] if the gamemode uses it.
	if ( GAMEMODE_IsTimelimitActive() )
	{
		FString TimeLeftString;
		GAMEMODE_GetTimeLeftString ( TimeLeftString );
		const char *szRound = ( lastmanstanding || teamlms ) ? "Round" : "Level";
		sprintf( szString, "%s ends in %s", szRound, TimeLeftString.GetChars() );
		lines.push_back( szString );
	}

	// Render the number of monsters left in coop.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		if ( dmflags2 & DF2_KILL_MONSTERS )
			sprintf( szString, "%d%% remaining", static_cast<int> (remaining) );		
		else
			sprintf( szString, "%d monster%s remaining", static_cast<int> (remaining), remaining == 1 ? "" : "s" );
		lines.push_back( szString );

		// [WS] Show the damage factor.
		if ( sv_coop_damagefactor != 1.0f )
		{
			sprintf( szString, "damage factor %.2f", static_cast<float> (sv_coop_damagefactor) );
			lines.push_back( szString );
		}
	}
}

//*****************************************************************************
//
static void scoreboard_ClearColumns( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_COLUMNS; ulIdx++ )
		g_aulColumnType[ulIdx] = COLUMN_EMPTY;

	g_ulNumColumnsUsed = 0;
}

//*****************************************************************************
//
static void scoreboard_Prepare5ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 5;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 8;
	g_aulColumnX[1] = 56;
	g_aulColumnX[2] = 106;
	g_aulColumnX[3] = 222;
	g_aulColumnX[4] = 286;

	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
		g_aulColumnType[0] = COLUMN_POINTS;
		// [BC] Doesn't look like this is being used right now (at least not properly).
/*
		// Can have assists.
		if ( ctf || skulltag )
			g_aulColumnType[0] = COL_POINTSASSISTS;
*/
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_EMPTY;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}
}

//*****************************************************************************
//
static void scoreboard_SetColumnZeroToKillsAndSortPlayers( void )
{
	if ( zadmflags & ZADF_AWARD_DAMAGE_INSTEAD_KILLS )
	{
		g_aulColumnType[0] = COLUMN_POINTS;

		// Sort players based on their points.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}
	else
	{
		g_aulColumnType[0] = COLUMN_KILLS;

		// Sort players based on their killcount.
		scoreboard_SortPlayers( ST_KILLCOUNT );
	}
}
//*****************************************************************************
//
static void scoreboard_Prepare4ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 4;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 24;
	g_aulColumnX[1] = 84;
	g_aulColumnX[2] = 192;
	g_aulColumnX[3] = 256;
	
	// Build columns for modes in which players try to earn kills.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		scoreboard_SetColumnZeroToKillsAndSortPlayers();
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;
	}

	// Build columns for modes in which players try to earn frags.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTSASSISTS;

		g_aulColumnType[0] = COLUMN_POINTS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_FRAGS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}

}

//*****************************************************************************
//
static void scoreboard_Prepare3ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 3;
	g_pColumnHeaderFont = SmallFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 16;
	g_aulColumnX[1] = 96;
	g_aulColumnX[2] = 272;

	// All boards share these two columns. However, you can still deviant on these columns if you want.
	g_aulColumnType[1] = COLUMN_NAME;
	g_aulColumnType[2] = COLUMN_TIME;
	if ( NETWORK_InClientMode() )
		g_aulColumnType[2] = COLUMN_PING;

	// Build columns for modes in which players try to earn kills.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		scoreboard_SetColumnZeroToKillsAndSortPlayers();
	}

	// Build columns for modes in which players try to earn frags.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTSASSISTS;

		g_aulColumnType[0] = COLUMN_POINTS;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}

}

//*****************************************************************************
//	These parameters are filters.
//	If 1, players with this trait will be skipped.
//	If 2, players *without* this trait will be skipped.
static void scoreboard_DoRankingListPass( ULONG ulPlayer, LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam )
{
	ULONG	ulIdx;
	ULONG	ulNumPlayers;

	ulNumPlayers = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Skip or require players not in the game.
		if (((lNotPlaying == 1) && (playeringame[g_iSortedPlayers[ulIdx]] == false )) ||
			((lNotPlaying == 2) && (!playeringame[g_iSortedPlayers[ulIdx]] == false )))
			continue;

		// Skip or require players not on a team.
		 if(((lNoTeam == 1) && (!players[g_iSortedPlayers[ulIdx]].bOnTeam)) ||
			 ((lNoTeam == 2) && (players[g_iSortedPlayers[ulIdx]].bOnTeam)))
			continue;

		// Skip or require spectators.
		if (((lSpectators == 1) && PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])) ||
			((lSpectators == 2) && !PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])))
			continue;

		// In LMS, skip or require dead players.
		if( gamestate != GS_INTERMISSION ){
			/*(( lastmanstanding ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
			(( survival ) && (( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))*/
			
			// If we don't want to draw dead players, and this player is dead, skip this player.
			if (( lDead == 1 ) &&
				(( players[g_iSortedPlayers[ulIdx]].health <= 0 ) || ( players[g_iSortedPlayers[ulIdx]].bDeadSpectator )))
			{
				continue;
			}

			// If we don't want to draw living players, and this player is alive, skip this player.
			if (( lDead == 2 ) &&
				( players[g_iSortedPlayers[ulIdx]].health > 0 ) &&
				( players[g_iSortedPlayers[ulIdx]].bDeadSpectator == false ))
			{
				continue;
			}
		}

		// Skip or require players that aren't on this team.
		if (((lWrongTeam == 1) && (players[g_iSortedPlayers[ulIdx]].Team != ulDesiredTeam)) ||
			((lWrongTeam == 2) && (players[g_iSortedPlayers[ulIdx]].Team == ulDesiredTeam)))
			continue;

		scoreboard_RenderIndividualPlayer( ulPlayer, g_iSortedPlayers[ulIdx] );
		g_ulCurYPos += 10;
		ulNumPlayers++;
	}

	if ( ulNumPlayers )
		g_ulCurYPos += 10;
}

//*****************************************************************************
//
static void scoreboard_DrawRankings( ULONG ulPlayer )
{
	ULONG	ulIdx;
	ULONG	ulTeamIdx;
	char	szString[16];

	// Nothing to do.
	if ( g_ulNumColumnsUsed < 1 )
		return;

	g_ulCurYPos += 8;

	// Center this a little better in intermission
	if ( gamestate != GS_LEVEL )
		g_ulCurYPos = ( g_bScale == true ) ? (LONG)( 48 * g_fYScale ) : (LONG)( 48 * CleanYfac );

	// Draw the titles for the columns.
	for ( ulIdx = 0; ulIdx < g_ulNumColumnsUsed; ulIdx++ )
	{
		sprintf( szString, "%s", g_pszColumnHeaders[g_aulColumnType[ulIdx]] );
		if ( g_bScale )
		{
			screen->DrawText( g_pColumnHeaderFont, CR_RED,
				(LONG)( g_aulColumnX[ulIdx] * g_fXScale ),
				g_ulCurYPos,
				szString,
				DTA_VirtualWidth, g_ValWidth.Int,
				DTA_VirtualHeight, g_ValHeight.Int,
				TAG_DONE );
		}
		else
		{
			screen->DrawText( g_pColumnHeaderFont, CR_RED,
				(LONG)( g_aulColumnX[ulIdx] / 320.0f * SCREENWIDTH ),
				g_ulCurYPos,
				szString,
				TAG_DONE );
		}
	}

	// Draw the player list.
	g_ulCurYPos += 24;

	// Team-based games: Divide up the teams.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		// Draw players on teams.
		for ( ulTeamIdx = 0; ulTeamIdx < teams.Size( ); ulTeamIdx++ )
		{
			// In team LMS, separate the dead players from the living.
			if (( teamlms ) && ( gamestate != GS_INTERMISSION ) && ( LASTMANSTANDING_GetState( ) != LMSS_COUNTDOWN ) && ( LASTMANSTANDING_GetState( ) != LMSS_WAITINGFORPLAYERS ))
			{
				scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 1, 1, ulTeamIdx ); // Living in this team
				scoreboard_DoRankingListPass( ulPlayer, 1, 2, 1, 1, 1, ulTeamIdx ); // Dead in this team
			}
			// Otherwise, draw all players all in one group.
			else
				scoreboard_DoRankingListPass( ulPlayer, 1, 0, 1, 1, 1, ulTeamIdx ); 

		}

		// Players that aren't on a team.
		scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 2, 0, 0 ); 

		// Spectators are last.
		scoreboard_DoRankingListPass( ulPlayer, 2, 0, 1, 0, 0, 0 );
	}
	// Other modes: Just players and spectators.
	else
	{
		// [WS] Does the gamemode we are in use lives?
		// If so, dead players are drawn after living ones.
		if (( gamestate != GS_INTERMISSION ) && GAMEMODE_AreLivesLimited( ) && GAMEMODE_IsGameInProgress( ) )
		{
			scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 0, 0, 0 ); // Living
			scoreboard_DoRankingListPass( ulPlayer, 1, 2, 1, 0, 0, 0 ); // Dead
		}
		// Othrwise, draw all active players in the game together.
		else
			scoreboard_DoRankingListPass( ulPlayer, 1, 0, 1, 0, 0, 0 );

		// Spectators are last.
		scoreboard_DoRankingListPass( ulPlayer, 2, 0, 1, 0, 0, 0 );
	}

	V_SetBorderNeedRefresh();
}
