//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Vehicle metatable access through the native entity and vehicle
//          interfaces, including driveable vehicles and prisoner-pod seats.
//
//=============================================================================//

#ifndef LVEHICLE_SHARED_H
#define LVEHICLE_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#include "c_prop_vehicle.h"
#else
	#include "vehicle_base.h"
#endif

/* Vehicle methods use the entity and its native vehicle interface. */
typedef CBaseEntity lua_Vehicle;



/*
** access functions (stack -> C)
*/

LUA_API lua_Vehicle     *(lua_tovehicle) (lua_State *L, int idx);


/*
** push functions (C -> stack)
**
** lua_pushvehicle() installs the "Vehicle" metatable.  lua_pushentity() and
** CBaseEntity::PushLuaInstanceSafe() reach it through lua_pushvehicleentity(),
** which is declared in lbaseentity_shared.h so those two files never have to
** include this vehicle header.
*/
LUA_API void  (lua_pushvehicle) (lua_State *L, lua_Vehicle *pVehicle);



LUALIB_API lua_Vehicle *(luaL_checkvehicle) (lua_State *L, int narg);
LUALIB_API lua_Vehicle *(luaL_optvehicle) (lua_State *L, int narg,
                                                      lua_Vehicle *def);


/*
** HL2SB (sbrust): the camera-state / vehicle-table-name accessors, exported so
** the ENTITY metatable can carry the same six entries the Vehicle metatable
** does.  Chairs (prisoner pods) stay plain entities in this fork -- giving
** them the Vehicle meta would route every driveable-only method through
** luaL_checkvehicle and raise for anything that iterates all entities -- but
** GM:CalcVehicleView's `Vehicle.GetThirdPersonMode == nil` guard and the Lua
** Vehicle:SetThirdPersonMode pair must still work on a chair.  Defined in
** lvehicle_shared.cpp, registered in both Vehiclemeta and Entitymeta.
*/
int Vehicle_GetThirdPersonMode (lua_State *L);
int Vehicle_SetThirdPersonMode (lua_State *L);
int Vehicle_GetCameraDistance  (lua_State *L);
int Vehicle_SetCameraDistance  (lua_State *L);
int Vehicle_GetVehicleClass    (lua_State *L);
int Vehicle_SetVehicleClass    (lua_State *L);


#endif // LVEHICLE_SHARED_H
