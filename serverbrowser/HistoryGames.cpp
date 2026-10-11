//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

// The nosteam build keeps the played-server history in a local file (the
// reference browser stores it through the Steam matchmaking API).
#define HISTORY_SAVE_FILE "ServerBrowserHistory.vdf"

// How many entries the history keeps before dropping the oldest
#define HISTORY_MAX_ENTRIES 64

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHistoryGames::CHistoryGames(vgui::Panel *parent) : 
	CBaseGamesPage(parent, "HistoryGames", eHistoryServer )
{
	m_bRefreshOnListReload = false;
	m_pGameList->AddColumnHeader(10, "LastPlayed", "#ServerBrowser_LastPlayed", 100);
	m_pGameList->SetSortFunc(10, LastPlayedCompare);
	m_pGameList->SetSortColumn(10);

	LoadHistoryList();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHistoryGames::~CHistoryGames()
{
}		

//-----------------------------------------------------------------------------
// Purpose: loads history list from disk
//-----------------------------------------------------------------------------
void CHistoryGames::LoadHistoryList()
{
	m_History.RemoveAll();

	KeyValues *kv = new KeyValues( "history" );
	if ( kv->LoadFromFile( g_pFullFileSystem, HISTORY_SAVE_FILE, "CONFIG" ) )
	{
		for ( KeyValues *pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
		{
			historyserver_t entry;
			memset( &entry, 0, sizeof( entry ) );
			V_strncpy( entry.m_szServerName, pData->GetString( "name" ), sizeof( entry.m_szServerName ) );
			V_strncpy( entry.m_szMap, pData->GetString( "map" ), sizeof( entry.m_szMap ) );
			entry.m_NetAdr.SetFromString( pData->GetString( "addr" ) );
			entry.m_ullTimeLastPlayed = pData->GetUint64( "date" );
			m_History.AddToTail( entry );
		}
	}
	kv->deleteThis();

	if ( m_bRefreshOnListReload )
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: saves history list to disk
//-----------------------------------------------------------------------------
void CHistoryGames::SaveHistoryList()
{
	KeyValues *kv = new KeyValues( "history" );

	for ( int i = 0; i < m_History.Count(); i++ )
	{
		KeyValues *pSubKey = new KeyValues( "server" );
		pSubKey->SetString( "name", m_History[i].m_szServerName );
		pSubKey->SetString( "map", m_History[i].m_szMap );
		pSubKey->SetUint64( "date", m_History[i].m_ullTimeLastPlayed );
		pSubKey->SetString( "addr", m_History[i].m_NetAdr.ToString() );
		kv->AddSubKey( pSubKey );
	}

	kv->SaveToFile( g_pFullFileSystem, HISTORY_SAVE_FILE, "CONFIG" );
	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: records a server in the history (called when connecting)
//-----------------------------------------------------------------------------
void CHistoryGames::AddServerToHistory( newgameserver_t &server )
{
	// re-connecting to a server moves it to the head of the list
	for ( int i = 0; i < m_History.Count(); i++ )
	{
		if ( m_History[i].m_NetAdr.CompareAdr( server.m_NetAdr ) )
		{
			m_History.Remove( i );
			break;
		}
	}

	historyserver_t entry;
	memset( &entry, 0, sizeof( entry ) );
	V_strncpy( entry.m_szServerName, server.m_szServerName, sizeof( entry.m_szServerName ) );
	V_strncpy( entry.m_szMap, server.m_szMap, sizeof( entry.m_szMap ) );

	time_t today;
	time( &today );
	entry.m_ullTimeLastPlayed = (uint64)today;
	entry.m_NetAdr = server.m_NetAdr;

	m_History.AddToHead( entry );

	// keep the list bounded
	while ( m_History.Count() > HISTORY_MAX_ENTRIES )
	{
		m_History.Remove( m_History.Count() - 1 );
	}

	SaveHistoryList();

	m_bRefreshOnListReload = true;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CHistoryGames::SupportsItem(InterfaceItem_e item)
{
	switch (item)
	{
	case FILTERS:
		return true;
	
	case ADDSERVER:
	case GETNEWLIST:
	default:
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fills the address list this page queries
//-----------------------------------------------------------------------------
void CHistoryGames::GetServerAddressList( CUtlVector<netadr_t> &list )
{
	for ( int i = 0; i < m_History.Count(); i++ )
	{
		list.AddToTail( m_History[i].m_NetAdr );
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CHistoryGames::RefreshComplete( NServerResponse response )
{
	SetRefreshing(false);
	m_pGameList->SetEmptyListText("#ServerBrowser_NoServersPlayed");
	m_pGameList->SortList();

	BaseClass::RefreshComplete( response );
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CHistoryGames::OnOpenContextMenu(int itemID)
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(GetActiveList());

	// get the server
	int serverID = GetSelectedServerID();

	if(  serverID != -1 )
	{
		// Activate context menu
		menu->ShowMenu(this, serverID, true, true, true, true);
		menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromHistory", new KeyValues("RemoveFromHistory"), this);
	}
	else
	{
		// no selected rows, so don't display default stuff in menu
		menu->ShowMenu(this, (uint32)-1, false, false, false, false);
	}
}


//-----------------------------------------------------------------------------
// Purpose: removes a server from the history
//-----------------------------------------------------------------------------
void CHistoryGames::OnRemoveFromHistory()
{
	// iterate the selection
	for ( int iGame = m_pGameList->GetSelectedItemsCount() - 1; iGame >= 0; iGame-- )
	{
		int itemID = m_pGameList->GetSelectedItem( iGame );
		int serverID = m_pGameList->GetItemData(itemID)->userData;

		newgameserver_t *pServer = GetServer( serverID );
		if ( pServer )
		{
			for ( int i = 0; i < m_History.Count(); i++ )
			{
				if ( m_History[i].m_NetAdr.CompareAdr( pServer->m_NetAdr ) )
				{
					m_History.Remove( i );
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

	SaveHistoryList();

	UpdateStatus();	
	InvalidateLayout();
	Repaint();
}
