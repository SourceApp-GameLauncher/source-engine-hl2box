# Minecraft addon binding audit

Updated: 2026-10-20.

This audit covers the Minecraft weapon, block entities, water effects, menus,
and related Lua APIs. The implementation follows GMod reference behavior where
the fork provides the corresponding engine facilities. Detailed investigation
notes remain in the local development archive.

## Implemented changes

| Area | Resulting behavior |
|---|---|
| Trace results | `Hit` includes start-solid and all-solid traces. `HitBoxBone` is available for valid studio hitboxes. Missing material and texture information returns nil. A zero-length direction falls back to the negated plane normal, then to `(0,0,1)`. |
| World sounds | `sound.Play` accepts a DSP preset as argument 6 and returns no values. The existing optional recipient filter remains supported. |
| Vectors | `Vector:Normalize` modifies the receiver and returns no values. |
| Spatial queries | `ents.FindInBox` tests bounding-box intersection instead of entity origins. |
| Solidity | `Entity:SetNotSolid` toggles the solid flag without replacing the solid type. `SetSolid` and `SetMoveType` limit values to supported ranges. |
| Downloads | The server `resource` library provides `AddFile`, `AddSingleFile`, and `AddWorkshop`. Model downloads include existing companion files. Calls skip single-player sessions. |
| Player audio | `Player:SetDSP` uses the engine player-DSP interface. |
| Particle effects | New emitters initialize a 256-unit cull radius and a small box around their origin so the leaf system can render their first particles. |
| Entity velocity | `Entity:GetVelocity` returns the velocity vector only. |
| Client menu helpers | `gui.SetMousePos` uses the existing cursor setter. Global `SetClipboardText` forwards to the existing system implementation. |

## Round 4-5 changes

| API | Resulting behavior |
|---|---|
| `Entity:GetClass` | Removes a lowercase `ai_` prefix and maps the complete name `prop_vehicle_jeep_old` to `prop_vehicle_jeep`. Matching is case-sensitive. `GetClassname` keeps the raw name. |
| `Entity:SetRenderMode` | Accepts numeric values, truncates fractions, limits modes to 0 through 10, and skips unchanged writes. |
| `Entity:GetMoveCollide` | Returns the current engine movement-collision mode in both realms. |
| `Player:GetViewModel` | Limits numeric indices to this engine's two slots, 0 and 1. Non-numeric arguments use slot 0. |
| `Entity:PhysicsInitBox` | Sorts each axis, replaces coincident endpoints with a unit cube, and sets mass to volume multiplied by 0.01. |
| `Entity:SetSkin` | Valid non-animating entities are ignored. Animating entities continue to store their skin in `m_nSkin`; numeric fractions are truncated. |

Physics box bounds remain local to the entity. The physics backend receives the
entity origin separately; the binding does not subtract that origin from its
arguments. The previous default box mass was 85. The new computed mass remains
subject to the backend minimum of 1, including a unit cube's computed mass of
0.08. Only fully coincident endpoints trigger the unit-cube fallback.

The reference skin accessors use an animating object's skin field. There is no
reason to replace the fork's existing skin storage with a shared model table.

## Retained differences and limitations

- `Entity:GetModel` keeps the fork's empty-string fallback. `GetOwner` keeps its
  existing nil result when no owner exists.
- `Player:GetInfo/GetInfoNum` keep the existing convar lookup and client-info
  fallback. The reference protected-name behavior is not added here.
- `PhysObj:GetVelocity` keeps its existing additional angular-velocity return.
- `PhysicsInitBox` keeps returning a physics object. Optional material and mass
  center arguments, and the reference boolean result, remain unsupported.
- The reference pending-removal and rendering guards are not fully reproduced.
  Net calls without an active message retain the fork's existing behavior.
- Download expansion does not resolve material-to-texture dependencies.
- Material override rendering, custom decal support, and additional effect-data
  accessors remain outside this change.
- The addon's `NewMesh`, `spawnmenu.RemoveProp`, and unused water-removal hook
  have no working reference counterpart or caller. Third-party addon files are
  not changed by this audit.

## Validation

The changed C++ translation units were checked with the existing single-file
compiler tool. Client and server builds completed successfully. Their modules
and matching symbols were deployed to the game and distribution mirror with
matching MD5 checksums.

`lua/mc_swep_round5_test.lua` in the paired content repository directly exercises
the game bindings. It covers eight groups: class aliases, render modes, movement
collision, non-animating skins, viewmodel slots, reversed boxes, Minecraft-sized
box mass, and coincident box endpoints. It also checks input-vector preservation
and physics origins. Syntax validation passed; in-game validation is pending.

After fully restarting the game and entering a map, run on the server:

```text
lua_dofile mc_swep_round5_test.lua
```
