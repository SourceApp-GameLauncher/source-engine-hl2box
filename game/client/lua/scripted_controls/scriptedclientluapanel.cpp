//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

// our definition
#include "scriptedclientluapanel.h"

// lua hooks
#ifdef LUA_SDK
#include "luamanager.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//================================================================
CScriptedClientLuaPanel::CScriptedClientLuaPanel() : vgui::EditablePanel( NULL, "CScriptedClientLuaPanel")
{
	// HL2SB (2026-10-19): keyboard input must stay ENABLED on this panel too.
	//
	// This is the root of EVERY Lua panel, so a disabled keyboard here silently
	// turned off text entry for the whole Derma tree: a DTextEntry could be
	// clicked and its caret drawn, but no keystroke ever reached it (the
	// Minecraft sign's text dialog was the visible case).  Mouse keeps its
	// "disable for this panel only" treatment below, which is about hit testing;
	// keyboard enablement is what makes children focusable and lets the focused
	// child receive key events.
	SetKeyBoardInputEnabled( true );

	// Child controls receive input first. Uncovered space belongs to the
	// world panel so screen-click tools can select a point in the scene.
	SetMouseInputEnabled( true );
	DisableMouseInputForThisPanel( false );

	SetProportional( true );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
	SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
}

void CScriptedClientLuaPanel::CreateDefaultPanels( void )
{
	// Was a nice idea, but is called on game init and not level init
#if 0
	BEGIN_LUA_CALL_HOOK( "CreateDefaultPanels" );
	END_LUA_CALL_HOOK( 0, 0 );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: called when the VGUI subsystem starts up
//			Creates the sub panels and initialises them
//-----------------------------------------------------------------------------
void CScriptedClientLuaPanel::Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager )
{
	CreateDefaultPanels();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the parent for each panel to use
//-----------------------------------------------------------------------------
void CScriptedClientLuaPanel::SetParent(vgui::VPANEL parent)
{
	EditablePanel::SetParent( parent );
	// Parenting inherits input flags, but this root must keep its children interactive.
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	DisableMouseInputForThisPanel( false );
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
	// force ourselves to be proportional - when we set our parent above, if our new
	// parent happened to be non-proportional (such as the vgui root panel), we got
	// slammed to be nonproportional
	EditablePanel::SetProportional( true );
}

void CScriptedClientLuaPanel::Paint()
{
}
