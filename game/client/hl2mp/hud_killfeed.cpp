//========= Copyright Valve Corporation, All rights reserved. ============//
// Native pickup delivery for the original gamemode HUD. Death notices are
// produced by the gamemode's server net protocol and drawn by its Lua panel.
#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "c_baseplayer.h"
#include "c_basecombatweapon.h"
#include "clientmode_hl2mpnormal.h"
#include <vgui_controls/Panel.h>
#ifdef LUA_SDK
#include "luamanager.h"
#include "lbaseentity_shared.h"
#endif
#include "tier0/memdbgon.h"

class CHudGModPickups : public CHudElement, public vgui::Panel
{
    DECLARE_CLASS_SIMPLE( CHudGModPickups, vgui::Panel );
public:
    CHudGModPickups( const char *name ) : CHudElement( name ), BaseClass( NULL, "HudGModPickups" )
    {
        SetParent( g_pClientMode->GetViewport() );
        SetHiddenBits( 0 );
        SetPaintBackgroundEnabled( false );
    }
    void Init() { ListenForGameEvent( "item_pickup" ); }
    void VidInit() { m_PendingPickups.Purge(); }
    bool ShouldDraw()
    {
#ifdef LUA_SDK
        FlushPickups();
#endif
        return false;
    }
    void FireGameEvent( IGameEvent *event )
    {
#ifdef LUA_SDK
        if ( !Q_stricmp( event->GetName(), "item_pickup" ) )
            DispatchPickup( event->GetInt( "userid", 0 ), event->GetString( "item", "" ),
                            event->GetInt( "amount", 0 ), event->GetBool( "weapon", false ) );
#endif
    }
private:
    struct PendingPickup_t
    {
        int iUserID;
        char szItem[64];
        int iAmount;
        bool bWeapon;
        float flExpire;
    };
    CUtlVector<PendingPickup_t> m_PendingPickups;
#ifdef LUA_SDK
    void DispatchPickup( int iUserID, const char *pszItem, int iAmount, bool bWeapon );
    void FlushPickups();
    void PushPickupHook( int iUserID, const char *pszItem, int iAmount, bool bWeapon, C_BaseCombatWeapon *pPickupWeapon );
    C_BaseCombatWeapon *FindPickupWeapon( const char *pszItem );
#endif
};
DECLARE_HUDELEMENT( CHudGModPickups );

#ifdef LUA_SDK
C_BaseCombatWeapon *CHudGModPickups::FindPickupWeapon( const char *pszItem )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( pLocal == NULL || pszItem == NULL || pszItem[0] == '\0' )
		return NULL;

	// No prefix gate - SWEPs can register ANY classname; membership in the
	// inventory IS the weapon test (ammo/items never live in m_hMyWeapons).
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pIter = pLocal->GetWeapon( i );
		if ( pIter && !Q_stricmp( pIter->GetClassname(), pszItem ) )
			return pIter;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: One item_pickup: dispatch now when the join window is over, queue
//			otherwise (local player missing/dead, or the weapon entity is not
//			networked yet).  Duplicate queue entries keep the EARLIEST expiry.
//-----------------------------------------------------------------------------
void CHudGModPickups::DispatchPickup( int iUserID, const char *pszItem, int iAmount, bool bWeapon )
{
	if ( pszItem == NULL )
		pszItem = "";

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	bool bReady = ( pLocal != NULL && pLocal->IsAlive() );
	C_BaseCombatWeapon *pWeapon = ( bReady && bWeapon ) ? FindPickupWeapon( pszItem ) : NULL;

	if ( !bReady || ( bWeapon && pWeapon == NULL ) )
	{
		for ( int i = 0; i < m_PendingPickups.Count(); i++ )
		{
			if ( m_PendingPickups[i].iUserID == iUserID &&
				 !Q_stricmp( m_PendingPickups[i].szItem, pszItem ) )
				return;		// already queued, expiry untouched
		}

		if ( m_PendingPickups.Count() < 32 )
		{
			PendingPickup_t p;
			p.iUserID = iUserID;
			Q_strncpy( p.szItem, pszItem, sizeof( p.szItem ) );
			p.iAmount = iAmount;
			p.bWeapon = bWeapon;
			p.flExpire = gpGlobals->curtime + 5.0f;	// GMod's own window
			m_PendingPickups.AddToTail( p );
		}
		return;
	}

	PushPickupHook( iUserID, pszItem, iAmount, bWeapon, pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: Flush queued pickups - called from ShouldDraw (every frame).  A
//			weapon entry waits for its entity (GMod's EHANDLE retry) until the
//			5s expiry; a ready local player flushes everything else at once.
//-----------------------------------------------------------------------------
void CHudGModPickups::FlushPickups( void )
{
	if ( m_PendingPickups.Count() == 0 )
		return;

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	bool bReady = ( pLocal != NULL && pLocal->IsAlive() );
	float flNow = gpGlobals->curtime;

	for ( int i = 0; i < m_PendingPickups.Count(); )
	{
		PendingPickup_t p = m_PendingPickups[i];
		bool bExpired = ( p.flExpire <= flNow );

		if ( !bReady )
		{
			// Not even a local player after the whole window - GMod's queued
			// weapon handle expires the same way: give up rather than pile up.
			if ( bExpired )
				m_PendingPickups.Remove( i );
			else
				++i;
			continue;
		}

		C_BaseCombatWeapon *pWeapon = p.bWeapon ? FindPickupWeapon( p.szItem ) : NULL;

		if ( p.bWeapon && pWeapon == NULL && !bExpired )
		{
			++i;		// snapshot still coming - retry next frame
			continue;
		}

		m_PendingPickups.Remove( i );
		if ( p.bWeapon && pWeapon == NULL )
			continue;
		PushPickupHook( p.iUserID, p.szItem, p.iAmount, p.bWeapon, pWeapon );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Deliver the original pickup hooks with their native arguments.
//-----------------------------------------------------------------------------
void CHudGModPickups::PushPickupHook( int iUserID, const char *pszItem, int iAmount, bool bWeapon, C_BaseCombatWeapon *pPickupWeapon )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( pLocal == NULL || engine->GetPlayerForUserID( iUserID ) != pLocal->entindex() )
		return;

	if ( bWeapon )
	{
		if ( pPickupWeapon == NULL )
			return;
		BEGIN_LUA_CALL_HOOK( "HUDWeaponPickedUp" );
			lua_pushentity( L, pPickupWeapon );
		END_LUA_CALL_HOOK( 1, 0 );
		return;
	}

	const int nLength = Q_strlen( pszItem );
	if ( nLength > 5 && !Q_stricmp( pszItem + nLength - 5, "_ammo" ) )
	{
		BEGIN_LUA_CALL_HOOK( "HUDAmmoPickedUp" );
			lua_pushlstring( L, pszItem, nLength - 5 );
			lua_pushinteger( L, iAmount );
		END_LUA_CALL_HOOK( 2, 0 );
		return;
	}

	BEGIN_LUA_CALL_HOOK( "HUDItemPickedUp" );
		lua_pushstring( L, pszItem );
	END_LUA_CALL_HOOK( 1, 0 );
}


#endif
