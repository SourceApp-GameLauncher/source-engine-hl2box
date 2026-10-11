//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

// The nosteam build keeps favorites in a local file (the reference browser
// stores them through the Steam matchmaking API, which doesn't exist here).
#define FAVORITES_SAVE_FILE "ServerBrowserFavorites.vdf"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFavoriteGames::CFavoriteGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "FavoriteGames", eFavoritesServer )
{
	m_bRefreshOnListReload = false;

	LoadFavoritesList();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFavoriteGames::~CFavoriteGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: loads favorites list from disk
//-----------------------------------------------------------------------------
void CFavoriteGames::LoadFavoritesList()
{
	m_Favorites.RemoveAll();

	KeyValues *kv = new KeyValues( "favorites" );
	if ( kv->LoadFromFile( g_pFullFileSystem, FAVORITES_SAVE_FILE, "CONFIG" ) )
	{
		for ( KeyValues *pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
		{
			favoriteserver_t entry;
			V_strncpy( entry.m_szServerName, pData->GetString( "name" ), sizeof( entry.m_szServerName ) );
			entry.m_NetAdr.SetFromString( pData->GetString( "addr" ) );
			entry.m_ullTimeFavorited = pData->GetUint64( "date" );
			m_Favorites.AddToTail( entry );
		}
	}
	kv->deleteThis();

	m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");

	if ( m_bRefreshOnListReload )
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: saves favorites list to disk
//-----------------------------------------------------------------------------
void CFavoriteGames::SaveFavoritesList()
{
	KeyValues *kv = new KeyValues( "favorites" );

	for ( int i = 0; i < m_Favorites.Count(); i++ )
	{
		KeyValues *pSubKey = new KeyValues( "server" );
		pSubKey->SetString( "name", m_Favorites[i].m_szServerName );
		pSubKey->SetUint64( "date", m_Favorites[i].m_ullTimeFavorited );
		pSubKey->SetString( "addr", m_Favorites[i].m_NetAdr.ToString() );
		kv->AddSubKey( pSubKey );
	}

	kv->SaveToFile( g_pFullFileSystem, FAVORITES_SAVE_FILE, "CONFIG" );
	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: adds a server to the favorites (called by the context menu and the
//			add-server dialog; persists immediately)
//-----------------------------------------------------------------------------
void CFavoriteGames::AddServerToFavorites( newgameserver_t &server )
{
	// Make sure we don't already have this IP in the list somewhere
	netadr_t netAdr = server.m_NetAdr;

	for ( int i = 0; i < m_Favorites.Count(); i++ )
	{
		if ( m_Favorites[i].m_NetAdr.CompareAdr( netAdr ) )
		{
			return;
		}
	}

	int iIdx = m_Favorites.AddToTail();
	V_strncpy( m_Favorites[iIdx].m_szServerName, server.m_szServerName, sizeof( m_Favorites[iIdx].m_szServerName ) );

	time_t today;
	time( &today );
	m_Favorites[iIdx].m_ullTimeFavorited = (uint64)today;
	m_Favorites[iIdx].m_NetAdr = netAdr;

	SaveFavoritesList();

	m_bRefreshOnListReload = true;
	LoadFavoritesList();
}

//-----------------------------------------------------------------------------
// Purpose: Fills the address list this page queries
//-----------------------------------------------------------------------------
void CFavoriteGames::GetServerAddressList( CUtlVector<netadr_t> &list )
{
	for ( int i = 0; i < m_Favorites.Count(); i++ )
	{
		list.AddToTail( m_Favorites[i].m_NetAdr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CFavoriteGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
	case ADDSERVER:
		return true;

	case ADDCURRENTSERVER:
		return !IsSteam() && BFiltersVisible();
	
	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CFavoriteGames::RefreshComplete( NServerResponse response )
{
	SetRefreshing(false);
	if ( m_Favorites.Count() == 0 )
	{
		// set empty message
		m_pGameList->SetEmptyListText("#ServerBrowser_NoFavoriteServers");
	}
	else
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

	}
	m_pGameList->SortList();

	BaseClass::RefreshComplete( response );
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CFavoriteGames::OnOpenContextMenu(int itemID)
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());

	// get the server
	int serverID = GetSelectedServerID();

	if ( serverID != -1 )
	{
		// Activate context menu
		menu->ShowMenu(this, serverID, true, true, true, false);
		menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromFavorites", new KeyValues("RemoveFromFavorites"), this);
	}
	else
	{
		// no selected rows, so don't display default stuff in menu
		menu->ShowMenu( this,(uint32)-1, false, false, false, false );
	}
	
	menu->AddMenuItem("AddServerByName", "#ServerBrowser_AddServerByIP", new KeyValues("AddServerByName"), this);
}


//-----------------------------------------------------------------------------
// Purpose: removes a server from the favorites
//-----------------------------------------------------------------------------
void CFavoriteGames::OnRemoveFromFavorites()
{
	// iterate the selection bottom-up so removing rows stays valid
	for ( int iGame = m_pGameList->GetSelectedItemsCount() - 1; iGame >= 0; iGame-- )
	{
		int itemID = m_pGameList->GetSelectedItem( iGame );
		int serverID = m_pGameList->GetItemData(itemID)->userData;

		newgameserver_t *pServer = GetServer( serverID );
		if ( pServer )
		{
			for ( int i = 0; i < m_Favorites.Count(); i++ )
			{
				if ( m_Favorites[i].m_NetAdr.CompareAdr( pServer->m_NetAdr ) )
				{
					m_Favorites.Remove( i );
					break;
				}
			}

			// the row leaves the list right away (reference behavior), and
			// the dead mark keeps any in-flight reply from re-adding it
			int iMap = m_mapServers.Find( serverID );
			if ( iMap != m_mapServers.InvalidIndex() )
			{
				if ( m_pGameList->IsValidItemID( m_mapServers[iMap].m_iListID ) )
					m_pGameList->RemoveItem( m_mapServers[iMap].m_iListID );
				m_mapServers.RemoveAt( iMap );
			}
			m_serversInfo[serverID].m_bDoNotRefresh = true;
		}
	}

	SaveFavoritesList();

	UpdateStatus();
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a server by IP address
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddServerByName()
{
	// open the add server dialog
	CDialogAddServer *dlg = new CDialogAddServer( &ServerBrowserDialog(), this );
	dlg->MoveToCenterOfScreen();
	dlg->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: Adds the currently connected server to the list
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddCurrentServer()
{
	newgameserver_t *pConnected = ServerBrowserDialog().GetCurrentConnectedServer();

	if ( pConnected && pConnected->m_NetAdr.GetPort() )
	{
		AddServerToFavorites( *pConnected );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse posted messages
//			 
//-----------------------------------------------------------------------------
void CFavoriteGames::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "AddServerByName"))
	{
		OnAddServerByName();
	}
	else if (!Q_stricmp(command, "AddCurrentServer" ))
	{
		OnAddCurrentServer();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: enables adding server
//-----------------------------------------------------------------------------
void CFavoriteGames::OnConnectToGame()
{
	m_pAddCurrentServer->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: disables adding current server
//-----------------------------------------------------------------------------
void CFavoriteGames::OnDisconnectFromGame( void )
{
	m_pAddCurrentServer->SetEnabled( false );
}
