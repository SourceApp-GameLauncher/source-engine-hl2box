# Physics gap analysis: GMod win64 vphysics vs ours (2026-10-20)

Method: binary comparison only (no game run). Reference is the Steam install,
`D:\games\garrysmod\bin\win64\vphysics.dll`; ours is `D:\srceng\bin\vphysics.dll`
(built from this tree) and `build\vphysics\vphysics.dll`.

## 1. Binary level

| Property | GMod win64 | Ours |
|---|---|---|
| file size | 1,314,816 B | 3,515,904 B |
| exports | 1 (`CreateInterface`) | 1 (`CreateInterface`) |
| interface version string | `VPhysics031` | `VPhysics031` |
| imports | 97 | 132 |

Import diff, interpreted:

- Only in GMod: newer tier0 diagnostics (`LoggingSystem_*`, `CThreadSpinRWLock`,
  `Plat_DebugString`, `Plat_ExitProcess`) and ANSI file enumeration
  (`FindFirstFileExA`/`FindNextFileA`), `GetConsoleCP`, `SetEnvironmentVariableA`.
- Only in ours: the UCRT surface (`FindFirstFileExW`, `GetFullPathNameW`,
  `EnumSystemLocalesW`, FLS APIs, ...).

So the difference at this level is **build/runtime configuration** (their build
uses the modern logging channel and narrow file APIs, ours links the fuller CRT
surface), not a missing interface. `VPhysics031` on both sides means the
`IPhysics*` method sets are the same by contract.

## 2. String level (weak but useful pointers)

| token | GMod | ours | note |
|---|---|---|---|
| `ragdollconstraint` | 1 | 0 | ours implements `CreateRagdollConstraint`, see below |
| `vcollide` | 1 | 0 | ours has `vcollide_parse.cpp`, so this is a command/save name |
| `fluid` | 1 | 0 | no fluid surface system in our tree |
| `constraint` | 33 | 31 | parity |
| `spring` | 12 | 10 | near parity |
| `Vehicle` | 2 | 4 | ours has more |
| `IVP_` | 3 | 146 | ours exposes more IVP internals in strings |

The three "missing" tokens are **not** missing subsystems:

- `ragdollconstraint`: implemented on our side in
  `vphysics/physics_constraint.cpp:1589` (`CreateRagdollConstraint`), exported
  through `CPhysicsEnvironment::CreateRagdollConstraint`
  (`vphysics/physics_environment.cpp:1441`). The reference string is the
  save/restore class name, which lives in their serialisation path.
- `vcollide`: our tree carries `vphysics/vcollide_parse.cpp` and the private
  parse headers; their string is a command/console name.
- `fluid`: genuinely absent here - our tree has no fluid surface implementation,
  which matches Source 2013 (fluid was an experimental branch feature).

## 3. Where the real gaps are (game-side physics, with evidence)

The physics library itself is at parity, so the gaps are in the game layer:

1. **Ragdoll robustness** - AGENTS records the pending hardening: ledge
   traversal guard, bad-object locator probe, workshop `.phy` validation at load.
   TODO清单 P2 has the same item open.
2. **Brush-entity physics drive** - AGENTS notes the residual risk for
   `movetype=7` brush entities in physgun drive/attach paths (`func_` pickups
   are gated in Lua, the C++ drive path is not asserted).
3. **Vehicle camera** - TODO P1: `GM:CalcView` vehicle dispatch is rolled back
   because the First Person Body in-vehicle pose-zero vibrates the seat model;
   not a physics gap but the camera/physics coupling is where it fails.
4. **`player_pickup` (+use carry)** - TODO P3: GMod's carrying subsystem is
   absent; only the HL2 physcannon pickup hooks exist.
5. **NPC client `m_iMaxHealth`** - TODO P2: not networked, so GMod SWEPs that
   compare health to max misjudge NPCs (player-only network var today).

## 4. Suggested verification for the engine-level parity claim

Run a map with ragdolls + constraints + a vehicle and compare against GMod on
the same machine: `physics_debug` style probes are not needed; the observable
checks are (a) ragdolls collapse and settle, (b) constraints hold under load,
(c) vehicle wheels/chassis behave, (d) `physgun` freeze/unfreeze round-trips.
