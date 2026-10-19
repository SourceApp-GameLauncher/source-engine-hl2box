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

	// HL2SB: MOUSE input has to stay enabled on this panel.
	//
	// vgui2/vgui_controls/Panel.cpp::IsWithinTraverse() refuses to recurse into
	// the children of a panel that does not take mouse input:
	//
	//     // also if it doesn't want mouse input its children can't get it either
	//     if (!IsVisible() || !IsMouseInputEnabled()) return NULL;
	//
	// This panel is the parent of every scripted Lua panel.  Measured in game,
	// the ancestor chain of a vgui.Create( "DFrame" ) is
	//
	//     DFrame -> CScriptedClientLuaPanel
	//
	// so with mouse input off -- what this constructor used to do -- every Derma
	// panel drew perfectly and yet nothing inside it could be clicked: the player
	// model list, its Apply button and every GMod addon panel were dead to the
	// mouse.
	//
	// DisableMouseInputForThisPanel( true ) keeps this panel from ever becoming
	// the mouse-over target itself, so it stays a pure pass-through container.
	SetMouseInputEnabled( true );
	DisableMouseInputForThisPanel( true );

	SetProportional( true );
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
	// force ourselves to be proportional - when we set our parent above, if our new
	// parent happened to be non-proportional (such as the vgui root panel), we got
	// slammed to be nonproportional
	EditablePanel::SetProportional( true );
}

void CScriptedClientLuaPanel::Paint()
{
}
