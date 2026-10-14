//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: GMod-style fullscreen "create server" dialog
//
// $NoKeywords: $
//=============================================================================//

#ifndef CREATEMULTIPLAYERGAMEDIALOG_H
#define CREATEMULTIPLAYERGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CCreateMultiplayerGameServerPage;
class CCreateMultiplayerGameGameplayPage;
class CCreateMultiplayerGameBotPage;
class CPNGImagePanel;
namespace vgui { class PanelListPanel; }

//-----------------------------------------------------------------------------
// Purpose: GMod-style fullscreen dialog for launching a listenserver
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CCreateMultiplayerGameDialog,  vgui::Frame );

public:
	CCreateMultiplayerGameDialog(vgui::Panel *parent);
	~CCreateMultiplayerGameDialog();

	// map card callbacks
	virtual void OnMapSelected( const char *pszMapName );

protected:
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnCommand( const char *command );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	// HL2SB: the max-players combo posts the stock "TextChanged" action
	// signal when a row is picked (registered below with
	// AddActionSignalTarget).  Crossing the single player boundary swaps
	// the whole sidebar shape, GMod-style.
	MESSAGE_FUNC( OnMaxPlayersChanged, "TextChanged" );

private:
	void BuildGameModeList();
	void BuildMapGrid();
	void LoadMapList();
	void LoadMaps( const char *pszPathID );
	void RefreshSelection( void );
	bool IsRandomMapSelected();
	const char *GetMapName();

	const char *GetHostName();
	const char *GetPassword();
	int GetMaxPlayers();
	// HL2SB: the sidebar has two shapes, GMod-style.  Single player keeps the
	// small column (fall damage / max ammo / loadout / god / HEV); every
	// multiplayer row swaps in the sandbox options (LAN, pvp damage, limited
	// physgun and the scrollable sbox_max* limits list).  Recomputed whenever
	// the max-players combo changes ("TextChanged").
	void UpdateSidebarMode();
	// Current value of mp_falldamage (1 = on).  The cvar is registered by the
	// server DLL, so while the main menu is up it is usually not visible and
	// this falls back to the sandbox default (off).
	bool GetFallDamage();
	// HL2SB (2026-09-28): sidebar options, same ConVarRef-first /
	// ServerConfig.vdf-fallback shape as GetFallDamage.  The sbox_* pair is
	// never visible at the menu (Lua creates it inside the running sandbox),
	// so those two always take the vdf path; defaults match GMod's new-game
	// sidebar (9999 / loadout on / god off / HEV off).
	int GetMaxAmmo();	// gmod_maxammo
	bool GetLoadout();	// sbox_weapons
	bool GetGodMode();	// sbox_godmode
	bool GetSuit();		// gmod_suit
	// Writes cfg/listenserver.cfg.  The engine execs that file at
	// SV_ActivateServer, which is the first moment a server-DLL cvar such as
	// mp_falldamage can be set at all - it cannot go on the command line
	// before "map".  The sbox_* pair does NOT go through here (ordering vs
	// Lua gamemode load is not guaranteed); sandbox/init.lua applies those
	// from ServerConfig.vdf instead.
	void WriteListenServerConfig( bool bFallDamage, int nMaxAmmo, bool bSuit );
	void SaveConfig();

	void CreateGame();

	// GMod-style map categories. The left list becomes a clickable category
	// filter; selecting one restricts the map grid to that category.
	//
	// Categorisation is by MOUNT, not by hard-coded map name: a map's full
	// disk path (GetLocalPath) tells us which mounted game it came from
	// (hl2 / hl2mp / custom gmod_maps), so anything mounted by hl2 lands in
	// the "Half-Life 2" bucket regardless of its chapter prefix.
	enum MapCategory_t
	{
		MAPCAT_ALL = 0,
		MAPCAT_HL2,
		MAPCAT_HL2DM,
		MAPCAT_SANDBOX,
		MAPCAT_OTHER,
		MAPCAT_COUNT,
	};
	const char *GetCategoryName( int iCategory );
	// Detects a category for a map from its filesystem mount path.
	int MapNameToCategory( const char *pszMapName );
	// Rebuilds the map grid honouring the selected category.
	void ApplyCategoryFilter();
	int m_iSelectedCategory = MAPCAT_ALL;

	// Left-hand category buttons (owned directly by the dialog so their
	// commands route to our OnCommand without relying on PanelListPanel).
	CUtlVector< vgui::Button * > m_CategoryButtons;

private:
	vgui::PanelListPanel *m_pGameModeList;
	vgui::PanelListPanel *m_pMapList;
	vgui::TextEntry *m_pHostName;
	vgui::TextEntry *m_pPassword;
	vgui::ComboBox *m_pMaxPlayers;
	vgui::Label *m_pSelectedMapLabel;
	vgui::Button *m_pStartButton;
	vgui::Button *m_pBackButton;
	vgui::Label *m_pTitleLabel;
	vgui::Label *m_pHostNameLabel;
	vgui::Label *m_pPasswordLabel;
	vgui::Label *m_pMaxPlayersLabel;
	vgui::CheckButton *m_pFallDamageCheck;
	// HL2SB (2026-09-28): singleplayer sidebar options (GMod's new-game column).
	vgui::Label *m_pMaxAmmoLabel;
	vgui::TextEntry *m_pMaxAmmo;
	vgui::CheckButton *m_pLoadoutCheck;
	vgui::CheckButton *m_pGodModeCheck;
	vgui::CheckButton *m_pSuitCheck;
	// HL2SB (2026-10-15): multiplayer sidebar options, the GMod create-server
	// column.  LAN rides the map command (sv_lan); everything else persists
	// into ServerConfig.vdf and is applied by sandbox's init.lua once the
	// gamemode's convars exist.  The limits list scrolls (24 rows will not fit
	// the column), one row per sbox_max* entry, plus three bone-manipulate
	// checkboxes at its end.
	vgui::CheckButton *m_pLanCheck;
	vgui::CheckButton *m_pPvPCheck;             // sbox_playershurtplayers
	vgui::CheckButton *m_pPhysgunLimitedCheck;  // physgun_limited
	vgui::Label *m_pSandboxHeader;              // "Sandbox limits" section title
	vgui::PanelListPanel *m_pSandboxList;

	// One persisted sandbox option.  Either the checkbox or the text entry is
	// set; pszDefault is the GMod default shown when ServerConfig.vdf has no
	// value for the key yet.
	struct SandboxOption_t
	{
		const char *pszConVar;
		vgui::TextEntry *pEntry;     // numeric row
		vgui::CheckButton *pCheck;   // checkbox row
	};
	CUtlVector< SandboxOption_t > m_SandboxOptions;

	CUtlVector<char*> m_MapNames; // own the strings

	KeyValues *m_pSavedData;

	char m_szSelectedMap[256];

	bool m_bBotsEnabled;
	bool m_bBuilt;
	bool m_bInRefreshSelection = false;
};

#endif // CREATEMULTIPLAYERGAMEDIALOG_H
