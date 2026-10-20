# ConVar parity: ConVars_In_Garrysmod (2026-10-20)

Scope: the three documented GMod convar groups on the wiki page (Lua related,
physgun, miscellaneous). 27 entries. Each name is checked against this tree by
grepping every `ConVar`/`CreateConVar` declaration and comparing name, default
and flags, and the reference behaviour is taken from the reference binaries /
content where it could be established.

## Already matched (no change)

Lua: `sv_allowcslua` 1, `sv_kickerrornum` 0, `lua_networkvar_bytespertick` 256,
`lua_strict` 0.
Physgun: `physgun_timeToArrive` 0.05, `physgun_timeToArriveRagdoll` 0.1,
`physgun_teleportDistance` 0, `physgun_maxAngular` 5000,
`physgun_maxAngularDamping` 10000, `physgun_maxSpeed` 5000,
`physgun_maxSpeedDamping` 10000, `physgun_rotation_sensitivity` 0.05,
`physgun_wheelspeed` 10, `gm_snapgrid` 0, `gm_snapangles` 45.
Misc: `gmod_maxammo` 0, `gmod_suit` 0, `gmod_mcore_test` 0, `gmod_language` "".

## `physgun_limited`: a gamemode switch, not an engine one

The reference registers this switch from **Lua, in the sandbox gamemode**, with
`FCVAR_REPLICATED` and the help text "Prevent the Physics Gun from picking up
certain map entities.", and consumes it in the same file's `GM:PhysgunPickup`:

- reject `prop_dynamic`, `prop_door`;
- reject `prop_*` carrying `SF_PHYSPROP_MOTIONDISABLED` or
  `SF_PHYSPROP_PREVENT_PICKUP`;
- reject `func_physbox` carrying `SF_PHYSBOX_MOTIONDISABLED`;
- reject every `func_*` except `func_physbox`.

Ordinary props **and NPCs stay pickable**.  The switch therefore does not exist
in the reference's game binaries at all (a whole-binary string scan finds zero
occurrences), so no C++ rule belongs in the weapon.

This tree registers the switch next to the other physgun convars
(`game/shared/hl2sb/weapon_physgun.cpp`) and applies the rules above in
`gamemodes/sandbox/gamemode/shared.lua` (`GM:PhysgunPickup`).  An earlier
C++ "only `prop_*` may be picked up" rule was removed: it rejected NPCs, which
the reference allows.

## Added, behaviour wired to a real subsystem

| ConVar | Default | Realm | Implementation |
|---|---|---|---|
| `gmod_delete_temp_files` | 1 | client | `game/client/cdll_client_int.cpp`: on the first HUD frame, delete `materials/temp/`, `sound/temp/`, `download/user_custom/` and print the reference's "Clearning temporary files..." line; `downloads/server/` is always cleared. Runs once, after the filesystem and convar systems exist. |
| `gmod_unload_test` | 0 | engine (client shutdown) | `engine/cdll_engine_int.cpp`: when set, the client-DLL shutdown path asks the material system to drop unreferenced materials after the existing `UncacheAllMaterials()` call. |
| `fs_tellmeyoursecrets` | 0 | filesystem module | `filesystem/basefilesystem.cpp`: prints one `[fs] ...` line per open (read/write), read and write when enabled. Lives in the filesystem module, so it ships in `D:\srceng\bin`, not the game DLLs. |

`cl_steamoverlay_pos` 1 is registered client-side; a nosteam build has no
overlay layer to position, so it stays a queryable switch only.

## Registered for compatibility, behaviour marked [-]

| ConVar | Default | Realm | Why it cannot be wired here |
|---|---|---|---|
| `gmod_sneak_attack` | 1 | server | the HL2 sneak-attack mechanic does not exist in this tree (a case-insensitive grep of the game tree for `sneak` finds only cstrike bot AI); the reference registers the switch but no reader consults it either |
| `mp_weaponrespawn` | 0 | server | HL2MP weapon/item respawn is not implemented here: no `sv_hl2mp_weapon_respawn_time` / `sv_hl2mp_item_respawn_time` consumer exists |

Both names still resolve for addons that query or set them.

## Deployment

`client.dll` and `server.dll` in `D:\srceng\hl2sb\bin` carry the switches of
their realm (each added name appears exactly once per DLL that registers it);
`fs_tellmeyoursecrets` is in the filesystem module under `D:\srceng\bin` and
`gmod_unload_test` in `engine.dll` there.
