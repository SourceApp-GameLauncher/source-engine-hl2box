//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "hl2mp_gameinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameClients implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	// Reference behavior: GMod reports min 1, max 128, default 128 - the start
	// menu offers up to 128 players and a server launched without an explicit
	// maxplayers opens with 128 slots.  MAX_PLAYERS carries the +1 proxy slot
	// the engine adds for replay / SourceTV on top of the 128 observable limit.
	minplayers = 1;
	defaultMaxPlayers = MAX_PLAYERS - 1;
	maxplayers = MAX_PLAYERS - 1;
}

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
}

