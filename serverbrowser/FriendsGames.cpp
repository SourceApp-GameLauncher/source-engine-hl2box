//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFriendsGames::CFriendsGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "FriendsGames",  eFriendsServer )
{
	m_iServerRefreshCount = 0;

	// The friends list is fed by the Steam friends graph, which a nosteam
	// build does not have; the tab stays present and answers with the
	// "no friends playing" notice like the reference does when none of your
	// friends are in a game.
	m_pGameList->SetEmptyListText("#ServerBrowser_NoFriendsServers");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFriendsGames::~CFriendsGames()
{
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CFriendsGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
		return true;

	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CFriendsGames::RefreshComplete( NServerResponse response )
{
	SetRefreshing(false);
	m_pGameList->SortList();
	m_iServerRefreshCount = 0;

	// set empty message
	m_pGameList->SetEmptyListText("#ServerBrowser_NoFriendsServers");

	BaseClass::RefreshComplete( response );
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CFriendsGames::OnOpenContextMenu(int itemID)
{
	// get the server
	int serverID = GetSelectedServerID();

	if ( serverID == -1 )
		return;

	// Activate context menu
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());
	menu->ShowMenu(this, serverID, true, true, true, true);
}
