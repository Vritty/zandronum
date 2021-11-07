//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2008 Benjamin Berkels
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
// Filename: st_hud.cpp
//
// Description: Contains extensions to the HUD code.
//
//-----------------------------------------------------------------------------

#include "st_hud.h"
#include "c_console.h"
#include "chat.h"
#include "v_video.h"
#include "v_text.h"
#include "gamemode.h"
#include "g_level.h"
#include "doomstat.h"
#include "team.h"
#include "gi.h"
#include "c_bind.h"
#include "cl_demo.h"
#include "g_game.h"
#include "callvote.h"
#include "scoreboard.h"
#include "joinqueue.h"
#include "deathmatch.h"
#include "duel.h"
#include "cooperative.h"
#include "invasion.h"
#include "domination.h"
#include "sbar.h"

//*****************************************************************************
//	VARIABLES

// How many players are currently in the game?
static	ULONG	g_ulNumPlayers = 0;

// [AK] How many true spectators are currently in the game?
static	ULONG	g_ulNumSpectators = 0;

// What is our current rank?
static	ULONG	g_ulRank = 0;

// What is the spread between us and the person in 1st/2nd?
static	LONG	g_lSpread = 0;

// Is this player tied with another?
static	bool	g_bIsTied = false;

// [AK] Does this player's team have other players besides themselves?
static	bool	g_bHasAllies = false;

// How many opponents are left standing in LMS?
static	LONG	g_lNumOpponentsLeft = 0;

// [RC] How many allies are alive in Survival, or Team LMS?
static	LONG	g_lNumAlliesLeft = 0;

// [AK] Who has the terminator sphere, hellstone, or white flag?
static	player_t	*g_pArtifactCarrier = NULL;

// [AK] Who are the two duelers?
static	player_t	*g_pDuelers[2];

//*****************************************************************************
//	PROTOTYPES

static	void	HUD_DrawBottomString( ULONG ulDisplayPlayer );
static	void	HUD_RenderHolders( void );
static	void	HUD_RenderTeamScores( void );
static	void	HUD_RenderRankAndSpread( void );
static	void	HUD_RenderInvasionStats( void );
static	void	HUD_RenderCountdown( ULONG ulTimeLeft );

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, cl_drawcoopinfo, true, CVAR_ARCHIVE )
CVAR( Bool, r_drawspectatingstring, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
EXTERN_CVAR( Int, con_notifylines )
EXTERN_CVAR( Bool, cl_stfullscreenhud )
EXTERN_CVAR( Int, screenblocks )
EXTERN_CVAR( Bool, st_scale )

//*****************************************************************************
//	FUNCTIONS

int HUD_GetWidth( void )
{
	return ( g_bScale ? con_virtualwidth : SCREENWIDTH );
}

int HUD_GetHeight( void )
{
	return ( g_bScale ? con_virtualheight : SCREENHEIGHT );
}

//*****************************************************************************
//
void HUD_DrawTexture( FTexture *Img, int X, int Y, const bool Scale )
{
	screen->DrawTexture( Img, X, Y, DTA_UseVirtualScreen, Scale, TAG_DONE );
}

void HUD_DrawTexture( FTexture *Img, int X, int Y )
{
	HUD_DrawTexture( Img, X, Y, g_bScale );
}

//*****************************************************************************
//
void HUD_DrawText( FFont* Font, int Normalcolor, int X, int Y, const char *String, const bool Scale )
{
	screen->DrawText( Font, Normalcolor, X, Y, String, DTA_UseVirtualScreen, Scale, TAG_DONE );
}

void HUD_DrawText( FFont* Font, int Normalcolor, int X, int Y, const char *String )
{
	HUD_DrawText( Font, Normalcolor, X, Y, String, g_bScale );
}

void HUD_DrawText( int Normalcolor, int X, int Y, const char *String, const bool Scale )
{
	HUD_DrawText( SmallFont, Normalcolor, X, Y, String, Scale );
}

void HUD_DrawTextAligned( int Normalcolor, int Y, const char *String, bool AlignLeft, const bool Scale )
{
	int screenWidthSacled = Scale ? con_virtualwidth : SCREENWIDTH;
	HUD_DrawText ( Normalcolor, AlignLeft ? 0 : ( screenWidthSacled - SmallFont->StringWidth ( String ) ) , Y, String, Scale );
}

// [AK]
void HUD_DrawTextCentered( FFont* Font, int Normalcolor, int Y, const char *String, const bool Scale )
{
	int X = (( Scale ? con_virtualwidth : SCREENWIDTH ) - Font->StringWidth( String )) >> 1;
	HUD_DrawText( Font, Normalcolor, X, Y, String, Scale );
}

//*****************************************************************************
// [AK]
void HUD_DrawTextClean( FFont* Font, int Normalcolor, int X, int Y, const char *String )
{
	screen->DrawText( Font, Normalcolor, X, Y, String, DTA_Clean, true, TAG_DONE );
}

void HUD_DrawTextCleanCentered( FFont *Font, int Normalcolor, int Y, const char *String )
{
	HUD_DrawTextClean( Font, Normalcolor, 160 - ( Font->StringWidth( String ) >> 1 ), Y, String );
}

//*****************************************************************************
//
bool HUD_IsUsingNewHud( void )
{
	return (( cl_stfullscreenhud ) && ( gameinfo.gametype & GAME_DoomChex ));
}

bool HUD_IsVisible( void )
{
	return ( screenblocks < 12 );
}

bool HUD_IsFullscreen( void )
{
	return ( viewheight == SCREENHEIGHT );
}

//*****************************************************************************
//*****************************************************************************
// Renders some HUD strings, and the main board if the player is pushing the keys.
//
void HUD_Render( ULONG ulDisplayPlayer )
{
	// Make sure the display player is valid.
	if ( ulDisplayPlayer >= MAXPLAYERS )
		return;

	// Draw the main scoreboard.
	if (SCOREBOARD_ShouldDrawBoard( ulDisplayPlayer ))
		SCOREBOARD_Render( ulDisplayPlayer );

	if ( CALLVOTE_ShouldShowVoteScreen( ))
	{		
		// [RC] Display either the fullscreen or minimized vote screen.
		if ( cl_showfullscreenvote )
			CALLVOTE_RenderClassic( );
		else
			CALLVOTE_Render( );
	}

	// [AK] Render the countdown screen when we're in the countdown.
	if ( GAMEMODE_GetState( ) == GAMESTATE_COUNTDOWN )
		HUD_RenderCountdown( GAMEMODE_GetCountdownTicks( ) + TICRATE );
	// [AK] Render the invasion stats while the game is in progress.
	else if (( invasion) && ( GAMEMODE_GetState( ) == GAMESTATE_INPROGRESS ))
		HUD_RenderInvasionStats( );

	if ( HUD_IsVisible( ))
	{
		// Draw the item holders (hellstone, flags, skulls, etc).
		HUD_RenderHolders( );

		if (( HUD_IsUsingNewHud( ) && HUD_IsFullscreen( )) == false )
		{
			// Are we in a team game? Draw scores.
			if( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
				HUD_RenderTeamScores( );

			if ( !players[ulDisplayPlayer].bSpectating )
			{
				// Draw the player's rank and spread in FFA modes.
				if ((( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) == false ) && ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNFRAGS ))
					HUD_RenderRankAndSpread( );

				// [BB] Draw number of lives left.
				if ( GAMEMODE_AreLivesLimited( ))
				{
					FString text;
					text.Format( "Lives: %d / %d", static_cast<unsigned int>( players[ulDisplayPlayer].ulLivesLeft + 1 ), GAMEMODE_GetMaxLives( ));
					HUD_DrawText( SmallFont, CR_RED, 0, static_cast<int>( g_rYScale * ( ST_Y - g_ulTextHeight + 1 )), text );
				}
			}

		}
	}
	
	// Display the bottom message.
	HUD_DrawBottomString( ulDisplayPlayer );
}

//*****************************************************************************
//
void HUD_Refresh( void )
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

	player_t *player = &players[HUD_GetViewPlayer( )];
	ULONG ulPlayer = player - players;

	g_ulRank = PLAYER_CalcRank( ulPlayer );
	g_lSpread = PLAYER_CalcSpread( ulPlayer );
	g_bIsTied = HUD_IsTied( ulPlayer );

	// [AK] Count how many players are in the game.
	g_ulNumPlayers = SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS );
	g_ulNumSpectators = SERVER_CountPlayers( true ) - g_ulNumPlayers;

	// "x opponents left", "x allies alive", etc
	if ( GAMEMODE_GetCurrentFlags( ) & GMF_DEADSPECTATORS )
	{
		// Survival, Survival Invasion, etc
		if ( GAMEMODE_GetCurrentFlags( ) & GMF_COOPERATIVE )
		{
			g_bHasAllies = g_ulNumPlayers > 1;
			g_lNumAlliesLeft = GAME_CountLivingAndRespawnablePlayers( ) - PLAYER_IsAliveOrCanRespawn( player );
		}

		// Last Man Standing, TLMS, etc
		if ( GAMEMODE_GetCurrentFlags( ) & GMF_DEATHMATCH )
		{
			if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS )
			{
				g_bHasAllies = TEAM_CountPlayers( player->Team ) > 1;

				unsigned livingAndRespawnableTeammates = TEAM_CountLivingAndRespawnablePlayers( player->Team );
				g_lNumOpponentsLeft = GAME_CountLivingAndRespawnablePlayers( ) - livingAndRespawnableTeammates;
				g_lNumAlliesLeft = livingAndRespawnableTeammates - PLAYER_IsAliveOrCanRespawn( player );
			}
			else
			{
				g_lNumOpponentsLeft = GAME_CountLivingAndRespawnablePlayers( ) - PLAYER_IsAliveOrCanRespawn( player );
			}
		}
	}
}

//*****************************************************************************
//
void HUD_DrawCoopInfo( void )
{
	// [BB] Only draw the info if the user wishes to see it (cl_drawcoopinfo)
	if (( cl_drawcoopinfo == false ) || ( zadmflags & ZADF_NO_COOP_INFO ))
		return;

	// [BB] Only draw the info if this is a cooperative or team based game mode. Further don't draw this in single player.
	if ( !( GAMEMODE_GetCurrentFlags() & ( GMF_COOPERATIVE | GMF_PLAYERSONTEAMS )) || ( NETWORK_GetState() == NETSTATE_SINGLE ))
		return;

	FString drawString;

	// [BB] We may not draw in the first 4 lines, this is reserved for chat messages.
	// Leave free another line to prevent the keys from being drawn over in ST's
	// fullscreen HUD.
	// [Dusk] Said message field can now have an arbitrary amount of lines, so
	// we cannot assume the default 4.
	const int yOffset = ( 1 + con_notifylines ) * SmallFont->GetHeight( );
	int playersDrawn = 0;

	for ( int i = 0; i < MAXPLAYERS; i++ )
	{
		// [BB] Only draw the info of players who are actually in the game.
		if ( (playeringame[i] == false) || ( players[i].bSpectating ) || (players[i].mo == NULL) )
			continue;

		// [BB] No need to draw the info of the player who's eyes we are looking through.
		if ( players[i].mo->CheckLocalView( consoleplayer ) )
			continue;

		// [BB] Only display team mates (in coop all players are team mates). Spectators see everybody.
		if ( players[consoleplayer].camera && !players[consoleplayer].camera->IsTeammate ( players[i].mo )
			&& !( players[consoleplayer].camera->player && players[consoleplayer].camera->player->bSpectating ) )
			continue;

		// [BB] We need more spacing if there is SECTINFO.
		int curYPos = yOffset + (playersDrawn/2) * ( ( 4 + ( level.info->SectorInfo.Names.Size() > 0 ) ) * SmallFont->GetHeight( ) + 3 ) ;

		const bool drawLeft = ( playersDrawn % 2 == 0 );

		// [BB] Draw player name.
		drawString = players[i].userinfo.GetName();
		V_ColorizeString( drawString );
		EColorRange nameColor = CR_GREY;
		// [BB] If the player is on a team, use the team's text color.
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			nameColor = static_cast<EColorRange> ( TEAM_GetTextColor ( players[i].Team ) );
		HUD_DrawTextAligned ( nameColor, curYPos, drawString.GetChars(), drawLeft, g_bScale );
		curYPos += SmallFont->GetHeight( ) + 1;

		// [BL] Draw the player's location, [BB] but only if the map has any SectorInfo.
		if ( level.info->SectorInfo.Names.Size() > 0 )
		{
			drawString = SECTINFO_GetPlayerLocation( i );
			V_ColorizeString( drawString );
			HUD_DrawTextAligned ( CR_GREY, curYPos, drawString.GetChars(), drawLeft, g_bScale );
			curYPos += SmallFont->GetHeight( ) + 1;
		}

		// [BB] Draw player health (color coded) and armor.
		EColorRange healthColor = CR_RED;
		// [BB] Player is alive.
		if ( players[i].mo->health <= 0 )
			drawString = "dead";
		else if ( SERVER_IsPlayerAllowedToKnowHealth ( consoleplayer, i ) )
		{
			int healthPercentage = ( 100 * players[i].mo->health ) / players[i].mo->GetMaxHealth();

			AInventory* pArmor = players[i].mo->FindInventory(RUNTIME_CLASS(ABasicArmor));
			drawString.Format( "%d \\cD/ %d", players[i].mo->health, pArmor ? pArmor->Amount : 0 );
			V_ColorizeString( drawString );
			if ( healthPercentage > 75 )
				healthColor = CR_GREEN;
			else if ( healthPercentage > 50 )
				healthColor = CR_GOLD;
			else if ( healthPercentage > 25 )
				healthColor = CR_ORANGE;
		}
		else
			drawString = "??? / ???";
		HUD_DrawTextAligned ( healthColor, curYPos, drawString.GetChars(), drawLeft, g_bScale );
		curYPos += SmallFont->GetHeight( ) + 1;

		// [BB] Draw player weapon and Ammo1/Ammo2, but only if the player is alive.
		// [Spleen] And don't draw ammo if sv_infiniteammo is enabled.
		if ( players[i].ReadyWeapon && players[i].mo->health > 0 )
		{
			drawString = players[i].ReadyWeapon->GetTag();
			if ( players[i].ReadyWeapon->Ammo1 && ( ( dmflags & DF_INFINITE_AMMO ) == false ) )
				drawString.AppendFormat( " \\cf%d", players[i].ReadyWeapon->Ammo1->Amount );
			else
				drawString += " \\cg-";
			if ( players[i].ReadyWeapon->Ammo2 && ( ( dmflags & DF_INFINITE_AMMO ) == false ) )
				drawString.AppendFormat( " \\cf%d", players[i].ReadyWeapon->Ammo2->Amount );
			V_ColorizeString( drawString );
			HUD_DrawTextAligned ( CR_GREEN, curYPos, drawString.GetChars(), drawLeft, g_bScale );
		}

		playersDrawn++;
	}
}

//*****************************************************************************
//
static void HUD_DrawBottomString( ULONG ulDisplayPlayer )
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
			color += TEAM_GetTextColorName( players[ulDisplayPlayer].Team );
		}

		bottomString.AppendFormat( "%sFollowing - %s%s", color.GetChars( ), players[ulDisplayPlayer].userinfo.GetName( ), color.GetChars( ));
	}

	// [AK] Draw the "waiting for players" or "x allies/opponents left" messages when viewing through a non-spectating player.
	// Only do this if GMF_DONTPRINTPLAYERSLEFT isn't enabled in the current game mode.
	if (( players[ulDisplayPlayer].bSpectating == false ) && (( GAMEMODE_GetCurrentFlags( ) & GMF_DONTPRINTPLAYERSLEFT ) == false ))
	{
		GAMESTATE_e gamestate = GAMEMODE_GetState( );

		// [AK] Draw a message showing that we're waiting for players if we are.
		if ( gamestate == GAMESTATE_WAITFORPLAYERS )
		{
			if ( ulDisplayPlayer != static_cast<ULONG>( consoleplayer ))
				bottomString += " - ";

			bottomString += TEXTCOLOR_RED "Waiting for players";
		}
		// Print the totals for living and dead allies/enemies.
		else if (( gamestate == GAMESTATE_INPROGRESS ) && ( GAMEMODE_GetCurrentFlags( ) & GMF_DEADSPECTATORS ))
		{
			if ( ulDisplayPlayer != static_cast<ULONG>( consoleplayer ))
				bottomString += " - ";

			// Survival, Survival Invasion, etc
			// [AK] Only print how many allies are left if we had any to begin with.
			if ( GAMEMODE_GetCurrentFlags( ) & GMF_COOPERATIVE )
			{
				if ( g_bHasAllies )
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
			}
			// Last Man Standing, TLMS, etc
			else
			{
				bottomString.AppendFormat( TEXTCOLOR_GRAY "%d ", static_cast<int>( g_lNumOpponentsLeft ));
				bottomString.AppendFormat( TEXTCOLOR_RED "opponent%s", g_lNumOpponentsLeft != 1 ? "s" : "" );

				// [AK] Only print how many teammates are left if we actually have any.
				if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) && ( g_bHasAllies ))
				{
					if ( g_lNumAlliesLeft < 1 )
					{
						bottomString += " left - allies dead";
					}
					else
					{
						bottomString.AppendFormat( ", " TEXTCOLOR_GRAY "%d ", static_cast<int>( g_lNumAlliesLeft ));
						bottomString.AppendFormat( TEXTCOLOR_RED "all%s left", g_lNumAlliesLeft != 1 ? "ies" : "y" );
					}
				}
				else
				{
					bottomString += " left";
				}
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
			bottomString.AppendFormat( "Waiting to play - %s in line", HUD_SpellOrdinal( lPosition ).GetChars() );
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

	// [RC] Draw the centered bottom message (spectating, following, waiting, etc).
	if ( bottomString.Len( ) > 0 )
	{
		DHUDMessageFadeOut *pMsg = new DHUDMessageFadeOut( SmallFont, bottomString, 1.5f, 1.0f, 0, 0, CR_WHITE, 0.20f, 0.15f );
		StatusBar->AttachMessage( pMsg, MAKE_ID( 'W', 'A', 'I', 'T' ));
	}
}

//*****************************************************************************
//
static void HUD_RenderHolders( void )
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
				color = CR_GRAY;
				text = "-";
			}
		
			text.AppendFormat( ": " TEXTCOLOR_GRAY "%s", level.info->SectorInfo.PointNames[i]->GetChars( ));
			HUD_DrawTextAligned( color, static_cast<int>( ST_Y * g_rYScale ) - ( numPoints - i ) * SmallFont->GetHeight( ), text, false, g_bScale );
		}
	}
}

//*****************************************************************************
//
static void HUD_RenderTeamScores( void )
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
static void HUD_RenderRankAndSpread( void )
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
static void HUD_RenderInvasionStats( void )
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
static void HUD_RenderCountdown( ULONG ulTimeLeft )
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
		HUD_DrawTextCleanCentered( BigFont, ulTitleColor, ulYPos, "vs." );

		// [AK] Next, draw the names of the two duelers.
		HUD_DrawTextCleanCentered( BigFont, ulTitleColor, ulYPos - 16, g_pDuelers[0]->userinfo.GetName( ));
		HUD_DrawTextCleanCentered( BigFont, ulTitleColor, ulYPos + 16, g_pDuelers[1]->userinfo.GetName( ));
		ulYPos += 40;
	}
	else
	{
		// [AK] TLMS and team possession should still keep "team" in the title for consistency.
		text = invasion ? INVASION_GetCurrentWaveString( ) : GAMEMODE_GetCurrentName( );

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
void HUD_DrawFragMessage( player_t *pPlayer, bool bFraggedBy )
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
	message = HUD_BuildPlaceString( consoleplayer );

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
// [TP]
//
bool HUD_ShouldDrawRank( ULONG ulPlayer )
{
	if ( PLAYER_IsTrueSpectator( &players[ulPlayer] ))
		return false;

	// [AK] Don't draw the rank if we're also on the lobbby map.
	if (( deathmatch == false ) || ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) || ( GAMEMODE_IsLobbyMap( )))
		return false;

	return true;
}

//*****************************************************************************
//
bool HUD_IsTied( ULONG ulPlayerNum )
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
bool HUD_IsTied( void )
{
	return ( g_bIsTied );
}

//*****************************************************************************
// Returns either consoleplayer, or (if using F12), the player we're spying.
//
ULONG HUD_GetViewPlayer( void )
{
	if (( players[consoleplayer].camera ) && ( players[consoleplayer].camera->player ))
	{
		return ( players[consoleplayer].camera->player - players );
	}

	return ( consoleplayer );
}

//*****************************************************************************
//
ULONG HUD_GetNumPlayers( void )
{
	return ( g_ulNumPlayers );
}

//*****************************************************************************
//
ULONG HUD_GetNumSpectators( void )
{
	return ( g_ulNumSpectators );
}

//*****************************************************************************
//
ULONG HUD_GetRank( void )
{
	return ( g_ulRank );
}

//*****************************************************************************
//
LONG HUD_GetSpread( void )
{
	return ( g_lSpread );
}

//*****************************************************************************
//
// [TP] Now in a function
//
FString HUD_SpellOrdinal( int ranknum, bool bColored )
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
FString HUD_BuildPointString( void )
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
			teamName.AppendFormat( "%s%s", TEAM_GetTextColorName( ulTeam ), TEAM_GetName( ulTeam ));
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
			lastTeamName.AppendFormat( "%s%s", TEAM_GetTextColorName( ulTeam ), TEAM_GetName( ulTeam ));
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
		text.Format( "Teams are tied at %d %s", static_cast<int>( lHighestScore ), scoreName.GetChars() );
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
FString HUD_BuildPlaceString( ULONG ulPlayer )
{
	FString text;

	// [AK] Only build the string in game modes for which we can earn frags, points, or wins in.
	if ( GAMEMODE_GetCurrentFlags( ) & ( GMF_PLAYERSEARNFRAGS | GMF_PLAYERSEARNPOINTS | GMF_PLAYERSEARNWINS ))
	{
		if ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS )
 		{
			// [AK] Show which team(s) have the highest score and how much.
			text = HUD_BuildPointString( );
		}
		else
		{
			// If the player is tied with someone else, add a "tied for" to their string.
			if ( HUD_IsTied( ulPlayer ))
				text = "Tied for ";

			// [AK] Get the rank of this player, though it isn't always equivalent to g_ulRank. Particularly,
			// when we (the local player) get a frag or get fragged while spying on another player.
			ULONG ulRank = ( ulPlayer == HUD_GetViewPlayer( )) ? g_ulRank : PLAYER_CalcRank( ulPlayer );
			text.AppendFormat( "%s" TEXTCOLOR_NORMAL " place with ", HUD_SpellOrdinal( ulRank, true ).GetChars() );

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
