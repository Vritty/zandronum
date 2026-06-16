//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
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
// Filename: maprotation.cpp
//
// Description: The server's list of maps to play.
//
//-----------------------------------------------------------------------------

#include <string.h>
#include <vector>
#include "c_cvars.h"
#include "maprotation.h"
#include "joinqueue.h"
#include "sv_commands.h"

//*****************************************************************************
//	VARIABLES

std::vector<MapRotationEntry>	g_MapRotationEntries;

static	unsigned int			g_CurMapInList;
static	unsigned int			g_NextMapInList;

// [AK] This is true when the next map should ignore its player limits.
static	bool					g_NextMapIgnoresLimits;

//*****************************************************************************
//	FUNCTIONS

void MAPROTATION_Construct( void )
{
	g_MapRotationEntries.clear( );
	g_CurMapInList = g_NextMapInList = 0;
	g_NextMapIgnoresLimits = false;

	// [AK] If we're the server, tell the clients to clear their map lists too.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DelFromMapRotation( nullptr, true );
}

//*****************************************************************************
//
void MAPROTATION_StartNewGame( void )
{
	unsigned int position = 0;

	// [K6] Start with a random map if we are using sv_randommaprotation.
	// [AK] The player limits assigned to each map entry must be respected, so
	// if a random map should be picked, or if the first entry can't be entered,
	// pick one that can.
	// Note: the next map position should always start at zero here.
	if (( sv_randommaprotation ) || ( MAPROTATION_CanEnterMap( position, MAPROTATION_CountEligiblePlayers( )) == false ))
	{
		MAPROTATION_CalcNextMap( false );
		position = MAPROTATION_GetNextPosition( );
	}

	// [BB] G_InitNew seems to alter the contents of the first argument, which it
	// shouldn't. This causes the "Frags" bug. The following is just a workaround,
	// the behavior of G_InitNew should be fixed.
	char levelname[10];
	sprintf( levelname, "%s", MAPROTATION_GetMap( position )->mapname );

	MAPROTATION_SetPositionToMap( levelname, true );
	G_InitNew( levelname, false );
}

//*****************************************************************************
//
unsigned int MAPROTATION_CountEligiblePlayers( void )
{
	unsigned int playerCount = 0;

	// [AK] Count players who are already playing or are in the join queue.
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if (( playeringame[i] ) && (( PLAYER_IsTrueSpectator( &players[i] ) == false ) || ( JOINQUEUE_GetPositionInLine( i ) != -1 )))
			playerCount++;
	}

	return playerCount;
}

//*****************************************************************************
//
unsigned int MAPROTATION_GetNumEntries( void )
{
	return g_MapRotationEntries.size( );
}

//*****************************************************************************
//
unsigned int MAPROTATION_GetCurrentPosition( void )
{
	return g_CurMapInList;
}

//*****************************************************************************
//
unsigned int MAPROTATION_GetNextPosition( void )
{
	return g_NextMapInList;
}

//*****************************************************************************
//
void MAPROTATION_SetCurrentPosition( unsigned int position )
{
	if ( position >= g_MapRotationEntries.size( ))
		return;

	g_CurMapInList = position;
}

//*****************************************************************************
//
void MAPROTATION_SetNextPosition( unsigned int position, const bool ignoreLimits )
{
	if ( position >= g_MapRotationEntries.size( ))
		return;

	g_NextMapInList = position;
	g_NextMapIgnoresLimits = ignoreLimits;
}

//*****************************************************************************
//
bool MAPROTATION_ShouldNextMapIgnoreLimits( void )
{
	return g_NextMapIgnoresLimits;
}

//*****************************************************************************
//
bool MAPROTATION_CanEnterMap( unsigned int position, unsigned int playerCount )
{
	if ( position >= g_MapRotationEntries.size( ))
		return false;

	// [AK] If this is the next map in the rotation and it should ignore its
	// player limits because of the SetNextMapPosition ACS function, then it can
	// be entered regardless of whether or not the player count is admissable.
	if (( position == g_NextMapInList ) && ( g_NextMapIgnoresLimits ))
		return true;

	return (( g_MapRotationEntries[position].minPlayers <= playerCount ) && ( g_MapRotationEntries[position].maxPlayers >= playerCount ));
}

//*****************************************************************************
//
static bool MAPROTATION_MapHasLowestOrHighestLimit( unsigned int position, unsigned int lowest, unsigned int highest, bool useMax )
{
	if ( position >= g_MapRotationEntries.size( ))
		return false;

	if ( useMax )
		return ( g_MapRotationEntries[position].maxPlayers == highest );
	else
		return ( g_MapRotationEntries[position].minPlayers == lowest );
}

//*****************************************************************************
//
static bool MAPROTATION_GetLowestAndHighestLimits( unsigned int playerCount, unsigned int &lowest, unsigned int &highest )
{
	bool useMaxLimit = false;
	lowest = MAXPLAYERS;
	highest = 1;

	// [AK] Get the lowest min player limit and highest max player limit from the list.
	for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
	{
		if ( g_MapRotationEntries[i].minPlayers < lowest )
			lowest = g_MapRotationEntries[i].minPlayers;

		if ( g_MapRotationEntries[i].maxPlayers > highest )
			highest = g_MapRotationEntries[i].maxPlayers;

		// [AK] If there's any map where the player count exceeds the min limit, then use the max limit.
		if ( playerCount >= g_MapRotationEntries[i].minPlayers )
			useMaxLimit = true;
	}

	return useMaxLimit;
}

//*****************************************************************************
//
void MAPROTATION_CalcNextMap( const bool updateClients )
{
	if ( g_MapRotationEntries.empty( ))
		return;

	const unsigned int playerCount = MAPROTATION_CountEligiblePlayers( );
	unsigned int lowestLimit;
	unsigned int highestLimit;
	bool useMaxLimit;

	// [AK] Before determining the next map, make sure it won't ignore its limits.
	g_NextMapIgnoresLimits = false;

	// If all the maps have been played, make them all available again.
	{
		bool allMapsPlayed = true;

		for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
		{
			// [AK] Ignore rotation entries that we can't select due to player limits.
			if ( MAPROTATION_CanEnterMap( i, playerCount ) == false )
				continue;

			if ( !g_MapRotationEntries[i].isUsed )
			{
				allMapsPlayed = false;
				break;
			}
		}

		if ( allMapsPlayed )
		{
			for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
				g_MapRotationEntries[i].isUsed = false;

			// [AK] If we're the server, tell the clients to reset their map lists too.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_ResetMapRotation( );
		}
	}

	// [BB] The random selection is only necessary if there is more than one map.
	if ( sv_randommaprotation && ( g_MapRotationEntries.size( ) > 1 ))
	{
		// Select a new map.
		std::vector<unsigned int> unusedEntries;

		for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
		{
			// [AK] Only select maps that we can enter with the current number of players.
			if (( g_MapRotationEntries[i].isUsed == false ) && ( MAPROTATION_CanEnterMap( i, playerCount )))
				unusedEntries.push_back( i );
		}

		// [AK] If we can't select any maps because the player count exceeds all limits, we'll just select the map with the lowest
		// lowest min player or highest max player limit, based on if there's too few or too many players.
		if ( unusedEntries.empty( ))
		{
			useMaxLimit = MAPROTATION_GetLowestAndHighestLimits( playerCount, lowestLimit, highestLimit );

			for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
			{
				if ( MAPROTATION_MapHasLowestOrHighestLimit( i, lowestLimit, highestLimit, useMaxLimit ))
					unusedEntries.push_back( i );
			}
		}

		g_NextMapInList = unusedEntries[M_Random( unusedEntries.size( ))];
	}
	else
	{
		g_NextMapInList = ( g_CurMapInList + 1 ) % MAPROTATION_GetNumEntries( );

		// [AK] Check if the next map in the list can be entered with the current number of players.
		if (( g_MapRotationEntries.size( ) > 1 ) && ( MAPROTATION_CanEnterMap( g_NextMapInList, playerCount ) == false ))
		{
			unsigned int oldMapInList = g_NextMapInList;
			bool nothingFound = false;

			do
			{
				// [AK] Cycle through the entire list until we find a map that can be entered.
				g_NextMapInList = ( g_NextMapInList + 1 ) % g_MapRotationEntries.size( );

				// [AK] We went through the entire list and couldn't find a valid map.
				if ( g_NextMapInList == oldMapInList )
				{
					nothingFound = true;
					break;
				}
			}
			while ( MAPROTATION_CanEnterMap( g_NextMapInList, playerCount ) == false );

			if ( nothingFound )
			{
				useMaxLimit = MAPROTATION_GetLowestAndHighestLimits( playerCount, lowestLimit, highestLimit );
				g_NextMapInList = oldMapInList;

				// [AK] Find the next map in the list with the lowest min player or highest max player limit.
				while ( MAPROTATION_MapHasLowestOrHighestLimit( g_NextMapInList, lowestLimit, highestLimit, useMaxLimit ) == false )
				{
					g_NextMapInList = ( g_NextMapInList + 1 ) % g_MapRotationEntries.size( );

					if ( g_NextMapInList == oldMapInList )
						break;
				}
			}
		}
	}

	// [AK] If we're the server, tell the clients what the next map is.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( updateClients ))
		SERVERCOMMANDS_SetNextMapPosition( );
}

//*****************************************************************************
//
level_info_t *MAPROTATION_GetNextMap( void )
{
	// [BB] If we don't want to use the rotation, there is no scheduled next map.
	if (( sv_maprotation == false ) || ( g_MapRotationEntries.empty( )))
		return nullptr;

	return ( g_MapRotationEntries[g_NextMapInList].map );
}

//*****************************************************************************
//
level_info_t *MAPROTATION_GetMap( unsigned int position )
{
	if ( position >= g_MapRotationEntries.size( ))
		return nullptr;

	return g_MapRotationEntries[position].map;
}

//*****************************************************************************
//
unsigned int MAPROTATION_GetPlayerLimits( unsigned int position, bool getMaxPlayers )
{
	if ( position >= g_MapRotationEntries.size( ))
		return 0;

	return ( getMaxPlayers ? g_MapRotationEntries[position].maxPlayers : g_MapRotationEntries[position].minPlayers );
}

//*****************************************************************************
//
void MAPROTATION_SetPositionToMap( const char *mapName, const bool setNextMap )
{
	for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
	{
		if ( stricmp( g_MapRotationEntries[i].map->mapname, mapName ) == 0 )
		{
			g_CurMapInList = i;
			g_MapRotationEntries[g_CurMapInList].isUsed = true;
			break;
		}
	}

	// [AK] Set the next map position to the current position, if desired.
	if ( setNextMap )
		MAPROTATION_SetNextPosition( g_CurMapInList, false );
}

//*****************************************************************************
//
bool MAPROTATION_IsMapInRotation( const char *mapName )
{
	for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
	{
		if ( stricmp( g_MapRotationEntries[i].map->mapname, mapName ) == 0 )
			return true;
	}

	return false;
}

//*****************************************************************************
//
bool MAPROTATION_IsUsed( unsigned int position )
{
	if ( position >= g_MapRotationEntries.size( ))
		return false;

	return g_MapRotationEntries[position].isUsed;
}

//*****************************************************************************
//
void MAPROTATION_SetUsed( unsigned int position, bool used )
{
	if ( position >= g_MapRotationEntries.size( ))
		return;

	g_MapRotationEntries[position].isUsed = used;
}

//*****************************************************************************
//
void MAPROTATION_AddMap( FCommandLine &argv, bool silent, bool insert )
{
	int position = insert ? atoi( argv[2] ) : 0;
	int limitArg = insert ? 3 : 2;

	// [AK] Get the minimum and maximum player limits if they've been included.
	unsigned int minPlayers = ( argv.argc( ) > limitArg ) ? atoi( argv[limitArg] ) : 0;
	unsigned int maxPlayers = ( argv.argc( ) > limitArg + 1 ) ? atoi( argv[limitArg + 1] ) : MAXPLAYERS;

	MAPROTATION_AddMap( argv[1], position, minPlayers, maxPlayers, silent );
}

//*****************************************************************************
//
void MAPROTATION_AddMap( const char *mapName, int position, unsigned int minPlayers, unsigned int maxPlayers, bool silent )
{
	// Find the map.
	level_info_t *map = FindLevelByName( mapName );

	if ( map == nullptr )
	{
		Printf( "map %s doesn't exist.\n", mapName );
		return;
	}

	// [AK] Save the position we originally passed into this function.
	int originalPosition = position;

	MapRotationEntry newEntry;
	newEntry.map = map;
	newEntry.isUsed = false;

	// [AK] Add the minimum and maximum player limits the map will use.
	newEntry.minPlayers = clamp<unsigned>( minPlayers, 0, MAXPLAYERS );
	newEntry.maxPlayers = clamp<unsigned>( maxPlayers, 1, MAXPLAYERS );

	// [AK] The minimum limit should never be greater than the maximum limit.
	if ( newEntry.minPlayers > newEntry.maxPlayers )
		swapvalues( newEntry.minPlayers, newEntry.maxPlayers );

	// [Dusk] position of 0 implies the end of the maplist.
	if ( position == 0 )
	{
		// Add it to the queue.
		g_MapRotationEntries.push_back( newEntry );

		// [Dusk] note down the position for output
		position = g_MapRotationEntries.end( ) - g_MapRotationEntries.begin( );
	}
	else
	{
		// [Dusk] insert the map into a certain position
		std::vector<MapRotationEntry>::iterator itPosition = g_MapRotationEntries.begin( ) + position - 1;

		// sanity check.
		if ( itPosition < g_MapRotationEntries.begin( ) || itPosition > g_MapRotationEntries.end( ))
		{
			Printf( "Bad index specified!\n" );
			return;
		}

		g_MapRotationEntries.insert( itPosition, 1, newEntry );
	}

	// [AK] Set the current entry in the map rotation to the current level, but
	// only set the next entry if it's the only one in the rotation.
	MAPROTATION_SetPositionToMap( level.mapname, g_MapRotationEntries.size( ) == 1 );

	// [AK] If there's more than one entry in the map rotation now, and the
	// current and next entries are the same, calculate a new next map.
	if (( g_MapRotationEntries.size( ) > 1 ) && ( g_CurMapInList == g_NextMapInList ))
		MAPROTATION_CalcNextMap( true );

	if ( !silent )
	{
		FString message;
		message.Format( "%s (%s) added to map rotation list at position %d", map->mapname, map->LookupLevelName( ).GetChars( ), position );

		if (( newEntry.minPlayers > 0 ) || ( newEntry.maxPlayers < MAXPLAYERS ))
		{
			message += " (";

			if ( newEntry.minPlayers > 0 )
				message.AppendFormat( "min = %u", newEntry.minPlayers );

			if ( newEntry.maxPlayers < MAXPLAYERS )
			{
				if ( newEntry.minPlayers > 0 )
					message += ", ";

				message.AppendFormat( "max = %u", newEntry.maxPlayers );
			}

			message += ')';
		}

		Printf( "%s.\n", message.GetChars( ));
	}

	// [AK] If we're the server, tell the clients to add the map on their end.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_AddToMapRotation( map->mapname, originalPosition, newEntry.minPlayers, newEntry.maxPlayers );
}

//*****************************************************************************
// [Dusk] Removes a map from map rotation
void MAPROTATION_DelMap( const char *mapName, bool silent )
{
	// look up the map
	level_info_t *map = FindLevelByName( mapName );

	if ( map == nullptr )
	{
		Printf( "map %s doesn't exist.\n", mapName );
		return;
	}

	std::vector<MapRotationEntry>::iterator iterator;
	bool gotcha = false;

	// search the map in the map rotation and throw it to trash
	for ( iterator = g_MapRotationEntries.begin( ); iterator < g_MapRotationEntries.end( ); iterator++ )
	{
		level_info_t *entry = iterator->map;

		if ( !stricmp( entry->mapname, mapName ))
		{
			level_info_t *nextEntry = MAPROTATION_GetNextMap( );

			g_MapRotationEntries.erase( iterator );
			gotcha = true;

			// [AK] If the deleted map was the next entry, calculate a new one.
			if (( g_MapRotationEntries.size( ) > 0 ) && ( entry == nextEntry ))
				MAPROTATION_CalcNextMap( true );

			break;
		}
	}

	if ( gotcha )
	{
		if ( !silent )
			Printf( "%s (%s) has been removed from map rotation list.\n", map->mapname, map->LookupLevelName( ).GetChars( ));

		// [AK] If we're the server, tell the clients to remove the map on their end.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DelFromMapRotation( mapName );
	}
	else
	{
		Printf( "Map %s is not in rotation.\n", mapName );
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( addmap )
{
	if ( argv.argc( ) > 1 )
		MAPROTATION_AddMap( argv, false );
	else
		Printf( "addmap <lumpname> [minplayers] [maxplayers]: Adds a map to the map rotation list.\n" );
}

CCMD( addmapsilent ) // Backwards API needed for server console, RCON.
{
	if ( argv.argc( ) > 1 )
		MAPROTATION_AddMap( argv, true );
	else
		Printf( "addmapsilent <lumpname> [minplayers] [maxplayers]: Silently adds a map to the map rotation list.\n" );
}

//*****************************************************************************
//
CCMD( maplist )
{
	if ( g_MapRotationEntries.size( ) == 0 )
	{
		Printf( "The map rotation list is empty.\n" );
	}
	else
	{
		const unsigned int playerCount = MAPROTATION_CountEligiblePlayers( );
		FString message;

		Printf( "Map rotation list: \n" );
		for ( unsigned int i = 0; i < g_MapRotationEntries.size( ); i++ )
		{
			const bool canEnter = MAPROTATION_CanEnterMap( i, playerCount );
			message.Format( "%u. ", i + 1 );

			// [AK] Highlight the current position in the map rotation in green, but only if we're actually playing on that map.
			if (( g_CurMapInList == i ) && ( stricmp( level.mapname, g_MapRotationEntries[g_CurMapInList].map->mapname ) == 0 ))
			{
				message += "(Current";

				// [AK] If the current and next positions are the same, use cyan or turquoise instead.
				if ( g_NextMapInList == i )
				{
					message.Insert( 0, canEnter ? TEXTCOLOR_CYAN : "\034[Turquoise]" );
					message += " and next";
				}
				else
				{
					message.Insert( 0, canEnter ? TEXTCOLOR_GREEN : TEXTCOLOR_DARKGREEN );
				}

				message += ") ";
			}
			// [AK] Highlight the next position in the map rotation in blue.
			else if ( g_NextMapInList == i )
			{
				message.Insert( 0, canEnter ? TEXTCOLOR_LIGHTBLUE : TEXTCOLOR_BLUE );
				message += "(Next) ";
			}
			// [AK] Highlight maps that have already been played in red.
			else if ( g_MapRotationEntries[i].isUsed )
			{
				message.Insert( 0, canEnter ? TEXTCOLOR_RED : TEXTCOLOR_DARKRED );
				message += "(Used) ";
			}
			// [AK] Maps that can't be entered are displayed in dark grey.
			else if ( canEnter == false )
			{
				message.Insert( 0, TEXTCOLOR_DARKGRAY );
			}

			message.AppendFormat( "%s - %s", g_MapRotationEntries[i].map->mapname, g_MapRotationEntries[i].map->LookupLevelName( ).GetChars( ));

			// [AK] Also print the min and max player limits if they're different from the default values.
			if (( g_MapRotationEntries[i].minPlayers > 0 ) || ( g_MapRotationEntries[i].maxPlayers < MAXPLAYERS ))
			{
				message += " (";

				if ( g_MapRotationEntries[i].minPlayers > 0 )
					message.AppendFormat( "min = %u", g_MapRotationEntries[i].minPlayers );

				if ( g_MapRotationEntries[i].maxPlayers < MAXPLAYERS )
				{
					if ( g_MapRotationEntries[i].minPlayers > 0 )
						message += ", ";

					message.AppendFormat( "max = %u", g_MapRotationEntries[i].maxPlayers );
				}

				message += ')';
			}

			Printf( "%s\n", message.GetChars( ));
		}
	}
}

//*****************************************************************************
//
CCMD( clearmaplist )
{
	// [AK] Don't let clients clear the map rotation list for themselves.
	if ( NETWORK_InClientMode( ))
		return;

	// Reset the map list.
	MAPROTATION_Construct( );

	Printf( "Map rotation list cleared.\n" );
}

// [Dusk] delmap
CCMD (delmap) {
	if (argv.argc() > 1)
		MAPROTATION_DelMap (argv[1], false);
	else
		Printf ("delmap <lumpname>: Removes a map from the map rotation list.\n");
}

CCMD (delmapsilent) {
	if (argv.argc() > 1)
		MAPROTATION_DelMap (argv[1], true);
	else
		Printf ("delmapsilent <lumpname>: Silently removes a map from the map rotation list.\n");
}

CCMD (delmap_idx) {
	if (argv.argc() <= 1)
	{
		Printf ("delmap_idx <idx>: Removes a map from the map rotation list based in index number.\nUse maplist to list the rotation with index numbers.\n");
		return;
	}

	unsigned int idx = static_cast<unsigned int> ( ( atoi(argv[1]) - 1 ) );
	if ( idx >= g_MapRotationEntries.size() )
	{
		Printf ("No such map!\n");
		return;
	}

	Printf ("%s (%s) has been removed from map rotation list.\n",	g_MapRotationEntries[idx].map->mapname, g_MapRotationEntries[idx].map->LookupLevelName().GetChars());
	g_MapRotationEntries.erase (g_MapRotationEntries.begin()+idx);
}

//*****************************************************************************
// [Dusk] insertmap
CCMD (insertmap) {
	if ( argv.argc( ) > 2 )
		MAPROTATION_AddMap( argv, false, true );
	else
	{
		Printf( "insertmap <lumpname> <position> [minplayers] [maxplayers]: Inserts a map to the map rotation list, after <position>.\n"
			"Use maplist to list the rotation with index numbers.\n" );
	}
}

CCMD (insertmapsilent) {
	if ( argv.argc( ) > 2 )
		MAPROTATION_AddMap( argv, true, true );
	else
	{
		Printf( "insertmapsilent <lumpname> <position> [minplayers] [maxplayers]: Silently inserts a map to the map rotation list, after <position>.\n"
			"Use maplist to list the rotation with index numbers.\n" );
	}
}

//*****************************************************************************
//	CONSOLE VARIABLES

CVAR( Bool, sv_maprotation, true, CVAR_ARCHIVE | CVAR_GAMEPLAYSETTING );
CVAR( Bool, sv_randommaprotation, false, CVAR_ARCHIVE | CVAR_GAMEPLAYSETTING );
