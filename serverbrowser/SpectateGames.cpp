//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"


CSpectateGames::CSpectateGames( vgui::Panel *parent )
	: CInternetGames( parent, "SpectateGames", eSpectatorServer )
{
}

void CSpectateGames::GetNewServerList()
{
	// The reference browser drops its "proxy" matchmaking filter here; the
	// nosteam list comes straight from the master server, so the SourceTV
	// selection happens in CheckTagFilter instead.
	BaseClass::GetNewServerList();
}

void CSpectateGames::OnPageShow()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSpectateGames::CheckTagFilter( newgameserver_t &server )
{
	// Spectate lists the servers that are broadcasting through SourceTV
	return ( server.m_iFlags & S2A_EXTRA_DATA_IS_SOURCETV ) != 0;
}

