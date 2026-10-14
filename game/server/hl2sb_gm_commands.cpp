//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2SB: Garry's Mod's spawn commands, implemented in the engine.
//
// GMod defines these in its sandbox gamemode (gamemodes/sandbox/gamemode/
// commands.lua) and its spawn menu talks to nothing else.  Having them here means
// every client - the Lua spawn menu included - spawns through ONE code path that:
//
//   * hands a WEAPON to the player instead of creating it in the world.  A world
//     weapon entity has no owner, and that is what crashed the client on
//     `ent_create weapon_rpg`.
//   * places whatever it spawns at the player's eye trace, the way GMod does,
//     instead of at a fixed distance.
//   * applies the NPC weapon the player configured (gmod_npcweapon) as a keyvalue
//     BEFORE the entity spawns, which is when the AI reads it.
//   * records every spawn in the undo stack through HL2SB_UndoRecord
//     (game/server/hl2sb_undo.cpp), which is the engine's own recorder.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hl2sb_undo.h"
#include "igameevents.h"
#include "luamanager.h"
#include "lbaseentity_shared.h"
#include "lbaseplayer_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// GMod keeps the NPC weapon in a cvar; it is ARCHIVE so the choice survives a
// restart, which is the one thing a client-side variable cannot do here.
static ConVar gmod_npcweapon( "gmod_npcweapon", "", FCVAR_ARCHIVE,
	"Weapon given to NPCs spawned from the spawn menu (empty = the NPC's own default, \"none\" = unarmed)." );

static CBasePlayer *GM_CommandPlayer( void )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();

	if ( pPlayer == NULL )
		pPlayer = UTIL_GetLocalPlayer();

	return pPlayer;
}

// GMod drops what it spawns at the end of the player's eye trace, a little off the
// surface that was hit, and level with the ground rather than at eye height.
static void GM_PlaceAtEyeTrace( CBasePlayer *pPlayer, CBaseEntity *pEnt )
{
	Vector vecEye = pPlayer->EyePosition();
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward );

	trace_t tr;
	UTIL_TraceLine( vecEye, vecEye + vecForward * 8192.0f, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	// back off along the surface normal so the entity is not inside what was hit
	pEnt->SetAbsOrigin( tr.endpos + tr.plane.normal * 16.0f );

	QAngle angles = pPlayer->EyeAngles();
	angles.x = 0.0f;
	angles.z = 0.0f;
	pEnt->SetAbsAngles( angles );
}

static CBaseEntity *GM_SpawnAtEyeTrace( CBasePlayer *pPlayer, const char *pszClass,
                                        const char *pszModel, const char *pszEquipment,
                                        const char *pszVehicleScript = NULL, const char *pszName = NULL,
                                        const CCommand *pKVArgs = NULL, int iKVStart = 0,
                                        bool bGmodSeatPod = false )
{
	CBaseEntity *pEnt = CreateEntityByName( pszClass );

	if ( pEnt == NULL )
	{
		Warning( "[HL2SB] %s: no such class \"%s\"\n", "gm_spawn", pszClass );
		return NULL;
	}

	if ( pszModel != NULL && pszModel[ 0 ] != '\0' )
		pEnt->SetModel( pszModel );

	// GMod's seat contract: every pod the SPAWN MENU creates is a seat, not a
	// prison cell - GMod's own seat list pins `limitview 0` on all of them, and
	// the pod's per-frame view clamp (C_PropVehiclePrisonerPod::UpdateViewAngles)
	// holds the yaw to +-60 unless the key is set.  A Hammer-placed pod keeps the
	// SDK behaviour (the keyfield default is true); only the gm_spawnvehicle path
	// opts into the seat contract.  Has to ride before DispatchSpawn, like the
	// other keyvalues here.
	if ( bGmodSeatPod )
		pEnt->KeyValue( "limitview", "0" );

	// the vehicle script is a KEYVALUE - it has to be on the entity before
	// DispatchSpawn, which is when prop_vehicle reads it.  A model-less
	// prop_vehicle is a server crash (CFourWheelVehiclePhysics::Initialize),
	// so the model and the script ride together from the spawn menu.
	if ( pszVehicleScript != NULL && pszVehicleScript[ 0 ] != '\0' )
		pEnt->KeyValue( "vehiclescript", pszVehicleScript );

	// the display name rides as the entity's name, and the undo record shows
	// it instead of the class ("Jeep" / "wood_crate001a" instead of
	// "prop_vehicle_jeep" / "prop_physics")
	if ( pszName != NULL && pszName[ 0 ] != '\0' )
		pEnt->SetName( AllocPooledString( pszName ) );

	// registry KeyValues ride as trailing name/value pairs (gm_spawnnpc).  They
	// have to be on the entity before DispatchSpawn, which is when the NPC reads
	// them -- the hutao pack's citizentype = 4 (CT_UNIQUE) is the difference
	// between npc_citizen keeping the reskin model and rewriting the path into
	// models/Humans/Group01/<file> (a file that does not exist -> ERROR model).
	//
	// HL2SB (sbrust): `vehicleclass` is the exception -- skipped here, applied
	// after Spawn below.  It is GMod's sandbox SetVehicleClass(VName) equivalent
	// (gamemodes/sandbox/gamemode/commands.lua:1057, "Store spawnmenu data for
	// addons and stuff"), the seat-pose lookup key of animations.lua
	// HandlePlayerDriving; the datamap FIELD_CHARACTER key-value path would
	// write a single byte into the vehicle's char array.
	extern void HL2SB_SetEntityVehicleClass( CBaseEntity *pEntity, const char *pszClass );
	const char *pszVehicleClass = NULL;
	if ( pKVArgs != NULL )
	{
		for ( int i = iKVStart; i + 1 < pKVArgs->ArgC(); i += 2 )
		{
			if ( !Q_stricmp( pKVArgs->Arg( i ), "vehicleclass" ) )
			{
				pszVehicleClass = pKVArgs->Arg( i + 1 );
				continue;
			}
			pEnt->KeyValue( pKVArgs->Arg( i ), pKVArgs->Arg( i + 1 ) );
		}
	}

	// equipment is a KEYVALUE - it has to be set before DispatchSpawn, which is when
	// the NPC reads it.  "none" is GMod's "unarmed" and is honoured the same way.
	if ( pszEquipment != NULL && pszEquipment[ 0 ] != '\0' )
		pEnt->KeyValue( "additionalequipment", pszEquipment );

	GM_PlaceAtEyeTrace( pPlayer, pEnt );

	DispatchSpawn( pEnt );
	pEnt->Activate();

	// HL2SB (sbrust): the spawnmenu's vehicle table name, post-Spawn like GMod's
	// sandbox does it.  Driveables and chairs both carry the field (free
	// function in game/server/hl2/vehicle_prisoner_pod.cpp).
	if ( pszVehicleClass != NULL && pszVehicleClass[0] != '\0' )
	{
		HL2SB_SetEntityVehicleClass( pEnt, pszVehicleClass );

		// GMod's sandbox also writes the registry entry's Members ONTO the
		// spawned entity (commands.lua:1116, table.Merge( Ent, vehicle.Members ))
		// -- Members.HandleAnimation lands on the entity's instance table, and
		// that field is what every variant of the gamemode's HandlePlayerDriving
		// reads first (the current animations.lua checks the entity field before
		// the registry; the older copy of that function some autorun packs
		// install over the gamemode - e.g. portalgun's animations.lua - knows
		// ONLY the entity field).  Writing it here keeps the seat pose intact
		// no matter which version of HandlePlayerDriving is in place.
		if ( L != NULL )
		{
			// All indices below are relative to iBase: the console-command frame
			// does NOT start on an empty Lua stack (first map after launch had
			// leftovers, a map change did not - the merge only ever worked when
			// iTop happened to be 0).
			const int iBase = lua_gettop( L );

			lua_getglobal( L, "list" );                                      // iBase+1
			if ( lua_istable( L, iBase + 1 ) )
			{
				lua_getfield( L, iBase + 1, "GetEntry" );                    // iBase+2
				if ( lua_isfunction( L, iBase + 2 ) )
				{
					lua_pushstring( L, "Vehicles" );                         // iBase+3
					lua_pushstring( L, pszVehicleClass );                    // iBase+4
					luasrc_pcall( L, 2, 1, 0 );                              // iBase+2 = entry (nil on miss / pcall error)
					if ( lua_istable( L, iBase + 2 ) )
					{
						if ( pEnt->m_nTableReference < 0 )
						{
							lua_newtable( L );
							pEnt->m_nTableReference = luaL_ref( L, LUA_REGISTRYINDEX );
						}
						lua_rawgeti( L, LUA_REGISTRYINDEX, pEnt->m_nTableReference );  // iBase+3 instance table
						if ( lua_istable( L, iBase + 3 ) )
						{
							lua_getfield( L, iBase + 2, "Members" );         // iBase+4
							if ( lua_istable( L, iBase + 4 ) )
							{
								lua_pushnil( L );                            // iBase+5 first key
								while ( lua_next( L, iBase + 4 ) != 0 )      // +5=key +6=value
								{
									lua_pushvalue( L, iBase + 5 );           // +7=key
									lua_pushvalue( L, iBase + 6 );           // +8=value
									lua_settable( L, iBase + 3 );            // instance[key] = value
									lua_pop( L, 1 );                         // drop the copy, keep the key
								}
							}
							lua_pop( L, 2 );                                 // Members, instance
						}
						else
						{
							lua_pop( L, 1 );
						}
					}
					lua_pop( L, 1 );                                         // entry
				}
				else
				{
					lua_pop( L, 2 );                                         // GetEntry, list
				}
			}
			else
			{
				lua_pop( L, 1 );                                             // list
			}

			lua_settop( L, iBase );
		}
	}

	return pEnt;
}

static void GM_Record( CBasePlayer *pPlayer, CBaseEntity *pEnt, const char *pszLabel = NULL )
{
	if ( pPlayer == NULL || pEnt == NULL )
		return;

	HL2SB_UndoRecord( pPlayer, pEnt, pszLabel );
}

//-----------------------------------------------------------------------------
// HL2SB: the sandbox spawn limits live in the gamemode, not here.
//
// GMod's commands.lua asks the gamemode before it spawns and tells it after:
//
//     gamemode.Call( "PlayerSpawnVehicle", ply, model, vname, vtable )  -> false = refuse
//     gamemode.Call( "PlayerSpawnedVehicle", ply, ent )                 -> ply:AddCount
//
// sandbox/gamemode/player.lua answers the first with LimitReachedProcess ->
// ply:CheckLimit (which fires the limit notification), and the second with
// ply:AddCount.  A spawn path that skips the pair neither counts nor limits
// anything -- that is exactly why the menu could spawn unlimited vehicles and
// the limit notification never appeared.  Both hooks also carry the addon
// contract (PlayerSpawnProp etc. are the documented veto points), so the engine
// commands route through them the same way the Lua commands do.
//
// An absent method answers false ("gamemode.Call" in this fork refuses when the
// method does not exist), so these methods must exist in the active gamemode;
// a NULL Lua state (Lua disabled) lets the spawn through.
//-----------------------------------------------------------------------------

// positionally optional string: GMod passes vtable / equipment in the middle of
// the argument list, so a missing one has to be a real nil, not ""
static void GM_PushOptionalString( lua_State *pL, const char *pszValue )
{
	if ( pszValue != NULL && pszValue[ 0 ] != '\0' )
		lua_pushstring( pL, pszValue );
	else
		lua_pushnil( pL );
}

static bool GM_GamemodeAllowsSpawn( const char *pszMethod, CBasePlayer *pPlayer,
                                    const char *pszArg1 = NULL, const char *pszArg2 = NULL )
{
	if ( L == NULL || pPlayer == NULL )
		return true;

	BEGIN_LUA_CALL_HOOK( pszMethod );
		lua_pushplayer( L, pPlayer );
		GM_PushOptionalString( L, pszArg1 );
		GM_PushOptionalString( L, pszArg2 );
	END_LUA_CALL_HOOK( 3, 1 );

	const bool bAllowed = ( lua_toboolean( L, -1 ) != 0 );
	lua_pop( L, 1 );

	return bAllowed;
}

// PlayerSpawnedVehicle / PlayerSpawnedNPC / PlayerSpawnedSENT: ( ply, ent )
static void GM_GamemodeSpawned( const char *pszMethod, CBasePlayer *pPlayer, CBaseEntity *pEnt )
{
	if ( L == NULL || pPlayer == NULL || pEnt == NULL )
		return;

	BEGIN_LUA_CALL_HOOK( pszMethod );
		lua_pushplayer( L, pPlayer );
		lua_pushentity( L, pEnt );
	END_LUA_CALL_HOOK( 2, 0 );
}

// PlayerSpawnedProp / PlayerSpawnedRagdoll / PlayerSpawnedEffect: ( ply, model, ent )
static void GM_GamemodeSpawnedWithModel( const char *pszMethod, CBasePlayer *pPlayer,
                                         const char *pszModel, CBaseEntity *pEnt )
{
	if ( L == NULL || pPlayer == NULL || pEnt == NULL )
		return;

	BEGIN_LUA_CALL_HOOK( pszMethod );
		lua_pushplayer( L, pPlayer );
		GM_PushOptionalString( L, pszModel );
		lua_pushentity( L, pEnt );
	END_LUA_CALL_HOOK( 3, 0 );
}

//-----------------------------------------------------------------------------
// gm_giveswep: GMod's "give yourself this weapon".  The weapon goes into the
// player's inventory - no world weapon entity is created, which is exactly why the
// RPG cannot reach the code that assumed an owner.
//-----------------------------------------------------------------------------
CON_COMMAND( gm_giveswep, "Give yourself a weapon: gm_giveswep <class>  (GMod)" )
{
	CBasePlayer *pPlayer = GM_CommandPlayer();

	if ( pPlayer == NULL || args.ArgC() < 2 )
		return;

	const char *pszClass = args[ 1 ];

	// GMod's PlayerGiveSWEP veto point (the sandbox default answers true; no
	// limit rides on it).
	if ( !GM_GamemodeAllowsSpawn( "PlayerGiveSWEP", pPlayer, pszClass ) )
		return;

	// HL2SB (2026-09-26): no item_pickup here.  GiveNamedItem ends with
	// Touch( this ), so a NEW weapon routes through BumpWeapon, which fires
	// the event with the REAL entity classname and weapon=true; an owned
	// weapon returns from GiveNamedItem immediately (GiveNamedItem answers
	// NULL on Weapon_OwnsThisType) and must stay silent -- firing here gave
	// every menu-given weapon a SECOND event (double strip), and when the
	// spawnmenu class differed from the entity classname the second strip
	// even fell through to the item fallback (green row next to the orange).
	pPlayer->GiveNamedItem( pszClass );
}

//-----------------------------------------------------------------------------
// gm_spawn: an entity / scripted entity / prop, with an optional model.
//-----------------------------------------------------------------------------
CON_COMMAND( gm_spawn, "Spawn an entity: gm_spawn <class> [model]  (GMod)" )
{
	CBasePlayer *pPlayer = GM_CommandPlayer();

	if ( pPlayer == NULL || args.ArgC() < 2 )
		return;

	const char *pszClass = args[ 1 ];
	const char *pszModel = ( args.ArgC() > 2 ) ? args[ 2 ] : NULL;
	const char *pszName  = ( args.ArgC() > 3 ) ? args[ 3 ] : NULL;

	// GMod splits its object spawn by what the MODEL is; the caller here hands
	// us the real class, so the same split rides on the class name -- the prop /
	// ragdoll / effect limits key off those, everything else counts as a SENT.
	const char *pszPreMethod  = "PlayerSpawnSENT";
	const char *pszPostMethod = "PlayerSpawnedSENT";
	bool bModelArg = false;

	if ( !Q_strnicmp( pszClass, "prop_physics", 12 ) )
	{
		pszPreMethod  = "PlayerSpawnProp";
		pszPostMethod = "PlayerSpawnedProp";
		bModelArg = true;
	}
	else if ( !Q_stricmp( pszClass, "prop_ragdoll" ) )
	{
		pszPreMethod  = "PlayerSpawnRagdoll";
		pszPostMethod = "PlayerSpawnedRagdoll";
		bModelArg = true;
	}
	else if ( !Q_stricmp( pszClass, "prop_effect" ) )
	{
		pszPreMethod  = "PlayerSpawnEffect";
		pszPostMethod = "PlayerSpawnedEffect";
		bModelArg = true;
	}

	if ( !GM_GamemodeAllowsSpawn( pszPreMethod, pPlayer, bModelArg ? pszModel : pszClass ) )
		return;

	CBaseEntity *pEnt = GM_SpawnAtEyeTrace( pPlayer, pszClass, pszModel, NULL, NULL, pszName );

	if ( pEnt == NULL )
		return;

	if ( bModelArg )
		GM_GamemodeSpawnedWithModel( pszPostMethod, pPlayer, pszModel, pEnt );
	else
		GM_GamemodeSpawned( pszPostMethod, pPlayer, pEnt );

	GM_Record( pPlayer, pEnt, pszName );
}

//-----------------------------------------------------------------------------
// gm_spawnvehicle: the spawn menu sends <class> <model> <vehiclescript> -- the
// model and the script USED to be dropped here, so every menu-spawned vehicle
// was model-less (the exact shape that crashes the server or spawns nothing).
// args[5..] are the Vehicles registry's KeyValues as name/value pairs
// (the menu forwards list.Set( "Vehicles", ... ) data.KeyValues).
//-----------------------------------------------------------------------------
CON_COMMAND( gm_spawnvehicle, "Spawn a vehicle: gm_spawnvehicle <class> [model] [vehiclescript] [name] [key value...]  (GMod)" )
{
	CBasePlayer *pPlayer = GM_CommandPlayer();

	if ( pPlayer == NULL || args.ArgC() < 2 )
		return;

	const char *pszModel = ( args.ArgC() > 2 ) ? args[ 2 ] : NULL;
	const char *pszScript = ( args.ArgC() > 3 ) ? args[ 3 ] : NULL;
	const char *pszName = ( args.ArgC() > 4 ) ? args[ 4 ] : NULL;

	// PlayerSpawnVehicle( ply, model, vname, vtable ) -- the display name plays
	// GMod's `vname` role here; the registry table is not passed (the method
	// reads what it needs from the name).
	if ( !GM_GamemodeAllowsSpawn( "PlayerSpawnVehicle", pPlayer, pszModel, pszName ) )
		return;

	CBaseEntity *pEnt = GM_SpawnAtEyeTrace( pPlayer, args[ 1 ], pszModel, NULL,
		pszScript, pszName, &args, 5,
		( Q_stricmp( args[ 1 ], "prop_vehicle_prisoner_pod" ) == 0 ) );

	if ( pEnt == NULL )
		return;

	GM_GamemodeSpawned( "PlayerSpawnedVehicle", pPlayer, pEnt );

	GM_Record( pPlayer, pEnt, pszName );
}

//-----------------------------------------------------------------------------
// gm_spawnnpc: the weapon is the second argument when the caller sent one, and the
// gmod_npcweapon cvar otherwise.
//-----------------------------------------------------------------------------
CON_COMMAND( gm_spawnnpc, "Spawn an NPC: gm_spawnnpc <class> [weapon]  (GMod)" )
{
	CBasePlayer *pPlayer = GM_CommandPlayer();

	if ( pPlayer == NULL || args.ArgC() < 2 )
		return;

	const char *pszWeapon = ( args.ArgC() > 2 ) ? args[ 2 ] : gmod_npcweapon.GetString();
	const char *pszName = ( args.ArgC() > 3 ) ? args[ 3 ] : NULL;
	// reskin packs register Class = a stock NPC with their own Model (the
	// hutao pack: npc_combine_s wearing the hutao model) -- without the model
	// only the stock NPC comes out and the reskin "cannot be spawned".
	const char *pszModel = ( args.ArgC() > 4 ) ? args[ 4 ] : NULL;

	// PlayerSpawnNPC( ply, npc_type, equipment )
	if ( !GM_GamemodeAllowsSpawn( "PlayerSpawnNPC", pPlayer, args[ 1 ], pszWeapon ) )
		return;

	// args[5..] are the NPC registry's KeyValues as name/value pairs
	// (the menu forwards list.Set( "NPC", ... ) data.KeyValues).
	CBaseEntity *pEnt = GM_SpawnAtEyeTrace( pPlayer, args[ 1 ], pszModel, pszWeapon, NULL, pszName,
	                                        ( args.ArgC() > 5 ) ? &args : NULL, 5 );

	if ( pEnt == NULL )
		return;

	GM_GamemodeSpawned( "PlayerSpawnedNPC", pPlayer, pEnt );

	GM_Record( pPlayer, pEnt, pszName );
}

//-----------------------------------------------------------------------------
// gm_spawnprop: GMod spawns props with gm_spawn too, but the model-only form is
// what the Prop page and the toolgun's creator use.
//-----------------------------------------------------------------------------
CON_COMMAND( gm_spawnprop, "Spawn a prop: gm_spawnprop <model>  (GMod)" )
{
	CBasePlayer *pPlayer = GM_CommandPlayer();

	if ( pPlayer == NULL || args.ArgC() < 2 )
		return;

	if ( !GM_GamemodeAllowsSpawn( "PlayerSpawnProp", pPlayer, args[ 1 ] ) )
		return;

	CBaseEntity *pEnt = GM_SpawnAtEyeTrace( pPlayer, "prop_physics", args[ 1 ], NULL );

	if ( pEnt == NULL )
		return;

	GM_GamemodeSpawnedWithModel( "PlayerSpawnedProp", pPlayer, args[ 1 ], pEnt );

	GM_Record( pPlayer, pEnt );
}

