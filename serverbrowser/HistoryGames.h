//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef HISTORYGAMES_H
#define HISTORYGAMES_H
#ifdef _WIN32
#pragma once
#endif

struct historyserver_t
{
	char m_szServerName[64];
	char m_szMap[64];
	uint64 m_ullTimeLastPlayed;
	netadr_t m_NetAdr;
};

//-----------------------------------------------------------------------------
// Purpose: History of all the servers joined
//-----------------------------------------------------------------------------
class CHistoryGames : public CBaseGamesPage
{
	DECLARE_CLASS_SIMPLE( CHistoryGames, CBaseGamesPage );

public:
	CHistoryGames(vgui::Panel *parent);
	~CHistoryGames();

	// history list, loads/saves into keyvalues
	void LoadHistoryList();
	void SaveHistoryList();
	void AddServerToHistory( newgameserver_t &server );

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem(InterfaceItem_e item);

	// called when the current refresh list is complete
	virtual void RefreshComplete( NServerResponse response );

	// address list the base page queries for this tab
	virtual void GetServerAddressList( CUtlVector<netadr_t> &list ) OVERRIDE;

	void SetRefreshOnReload() { m_bRefreshOnListReload = true; }

private:
	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnRemoveFromHistory, "RemoveFromHistory" );

	bool m_bRefreshOnListReload;

	CUtlVector<historyserver_t> m_History;
};


#endif // HISTORYGAMES_H
