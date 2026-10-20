# Minecraft binding round 5 implementation record

Updated: 2026-10-20.

## Completed work

All six round 4-5 implementation items are complete:

1. Entity class aliases and the lowercase `ai_` prefix.
2. Render mode bounds and unchanged-value handling.
3. Shared `GetMoveCollide` registration.
4. Viewmodel index bounds using `MAX_VIEWMODELS`.
5. Physics box axis ordering, coincident-point fallback, and volume-based mass.
6. Silent `SetSkin` calls on valid non-animating entities.

The implementation modifies `game/shared/lua/lbaseentity_shared.cpp` and
`game/shared/lua/lbaseplayer_shared.cpp`. No public headers or class layouts were
changed. Earlier audit fixes are included in the same working set.

The final behavior and remaining differences are described in
[the binding audit](mc_swep_binding_audit.md).

## Corrections to the initial handoff

- Skin access uses an animating object's field, rather than a global skin table.
- Physics box endpoints are local coordinates; origin is supplied separately.
  Existing addon calls that pass world coordinates are not converted here.
- The old box mass came from the backend's fixed default of 85.
- One apparent weapon-side PhysicsInitBox call is inside a block comment.
- A unit cube's computed mass is 0.08, but the backend minimum makes its observed
  mass 1. Only fully coincident endpoints receive this fallback.
- Detailed reference notes remain in the local development archive and are not
  included in repository documentation.

## Build and deployment

Single-file compilation and client/server builds passed. Both modules and their
matching symbol files were copied to the game bin directory and the distribution
mirror, with matching MD5 checksums. The test script was mirrored as well.

## Remaining validation

The game was not launched automatically. Fully restart it, enter a map with a
player, and run the paired content repository's server test:

```text
lua_dofile mc_swep_round5_test.lua
```

The script contains eight groups of real binding checks and has passed syntax
validation. In-game execution remains pending. Optional follow-up API work is
listed under retained differences in the binding audit.
