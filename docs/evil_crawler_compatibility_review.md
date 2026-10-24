# Evil API and Crawler compatibility review

PR #69 supplies useful missing entry points, but needs correction before integration.
The installed `evil_api` and `evil_crawler` Lua files are the callers for this review;
they must remain unchanged.

## Confirmed implementation problems

- Dynamic lighting calls ordinary lighting with a different clamp argument. GMod
  uses the engine's separate dynamic-lighting entry point, with two vector inputs.
- The proposed mesh implementation retains a locked builder across Lua calls.
  An error before `mesh.End` leaves that mesh available to unrelated rendering.
  Recovery only at the next `mesh.Begin` is too late. Primitive counts, texture
  channels and excess vertices also need bounds checks before native buffer writes.
- `GetRenderContext` returns an owned reference in this engine. It does not call
  `BeginRender`. Pairing that call with only `EndRender` is incorrect. Render-context
  RAII must own the complete native submission.
- Server bone manipulation is already networked by the current base branch.
  Replacing it with an independent record-only table would regress replication.
  `HasBoneManipulations` must query the existing controller, including default-valued
  controllers, rather than a disconnected table.
- `Vector:Unpack`, entity flag getters and several singular flag methods already
  exist. Add only missing spellings; avoid duplicate method implementations.
- The actual grab/release path also requires jiggle state, player spectator methods,
  vehicle exit, physics-attacker lookup and player-held physics detection. The PR
  omits these. `GetChildBones` is an original Lua extension, not a missing native
  GMod export; the server compatibility binding can provide the same direct-child list.
- Debug scent messages additionally require `net.WriteNormal` and `net.ReadNormal`.

## Reported initialization failures

The original entity initializes `BaseHeight` and `Vel` inside `Initialize`.
The PR's claim that these are uninitialized addon bugs is not established.
Likewise, `sv_attack.lua` captures `EvilCrawler.Victim` at file load time, so
`autorun/server/evil_crawler.lua` and its includes must finish before entity loading.
The current base branch already has a separate sorted server-autorun pass and
correct include return propagation. Verify those real loaders with the unchanged
scripts instead of inserting addon fallbacks for the later symptoms.

## Validation boundary

Native translation-unit checks, focused regressions and client/server builds are
required. The user performs live rendering, pursuit, player/NPC capture, escape,
release and map-reload checks after direct DLL/PDB deployment and a full restart.
Offline tests do not establish gameplay success. Static IMesh construction and
other mesh APIs beyond this addon's dynamic submission remain separate work.

## Local implementation and verification

The local correction uses the existing shared flag methods and network bone
controller. It adds the missing direct-child lookup and controller-presence query,
replicated jiggle settings with actual bone rendering, the spectator/vehicle and
physics queries used by capture/release, normal-vector network messages and the
missing in-place vector setter. Living spectator transitions preserve health and
life state and can follow a non-player entity. Dynamic and ordinary lighting use
their respective native entry points.

Dynamic mesh submission buffers vertex data until End. Primitive/index capacity,
texture channels and vertex writes are bounded. Native context ownership, mesh
locking, unlocking and drawing all happen within that one call; a Lua error cannot
leave a native mesh locked. Partial trailing primitives are discarded safely.

The actual native Lua loader and vector/mesh function bodies run the unchanged API,
server autorun and entity scripts in an isolated harness. The attack function
captures the correct Victim table, original initialization assigns BaseHeight and
Vel, 300 Crawl updates run, and the original Flesh.Tube submits geometry. Controller
tests cover defaults, network/local state, duplicate writes, snapshots, jiggle and
handle reuse; spectator tests cover state preservation, arbitrary targets and two
players. Rendering, physics, networking and camera services are isolated fixtures.

The remaining name-scan report for GetBodyGroups is a macro-registration false
positive: the shared native binding already exists. Optional VJBase/DrGBase/Homigrad
methods belong to those external addons and are not generic entity APIs to invent.
Current-main startup tests do not reproduce the PR's old Victim/Vel/wish tracebacks;
no addon fallback or source rewrite was added for those symptoms. Full in-game
pursuit, capture, escape, rendering and map changes remain the user's next test.

## Capture regression found during the first game test

The October 25 game log identifies `sv_victim.lua:319`: Player:GetAmmo is absent.
The original Take function creates a ragdoll before saving the equipment. That
error prevents spectator entry and the transition to dragging, so each later
grapple update creates another body. The earlier startup/movement tests did not
cover Take and did not establish capture compatibility. The name audit incorrectly
accepted Vehicle:GetAmmo as evidence that Player:GetAmmo existed.

The shared player binding now returns a fresh sparse table of positive reserve
counts keyed by ammo ID. It scans the fork's supported ammo slots; the current
fork has 64 slots, while the reference scans 256. No player layout was expanded.
Native StripWeapons replaces the old RemoveAllItems adapter so ammo and the suit
survive weapon stripping. Give's second argument is now the original no-ammo
boolean, with recipient-scoped reserve-ammo suppression and empty nonnegative
clips. It no longer reads that boolean as an integer weapon subtype.

A regression runs the original Overpower, Take, UpdateAct, Release and player
recovery functions, with the actual edited inventory binding bodies. It first
reproduces four body creations and four GetAmmo errors, then verifies four capture
and recovery cycles with 600 updates each: one body per capture, spectator HUD
state entered, no repeated creation, and restored health, armor, suit, clips,
selected weapon and sparse ammo. Two player inventories remain independent.
World physics, navigation and observer transport are isolated; the next live test
must verify the visible HUD/camera and ragdoll behavior.

## Visibility and silent-death regression

The next game log contains one NPC capture and one player capture, followed by
the addon's explicit death notification, with no new capture Lua error. The older
KillSilent binding sends ordinary damage while spectator mode sets DAMAGE_NO.
That emits a scripted silent-death notification without changing life state; the
engine therefore never reaches dead-player respawn processing. It also cannot
provide the reference's silent lifecycle, inventory cleanup or callback order.

KillSilent now performs the direct player-state transition and dispatches
PlayerSilentDeath before cleanup and PostPlayerDeath after the dead state is set.
It creates no ordinary death event or corpse and does not award a second death;
the original addon owns its explicit PlayerDeath notification and death counter.
The explicit ordinary Kill binding likewise enters normal death events directly
rather than trying to overcome damage filters with an arbitrary damage amount.

The local-player reflection exception previously returned visible unconditionally.
It now obeys native visibility and observer movement first, including the derived
player's ragdoll exception. Visible local players remain eligible for reflections.

The post-death path also exposed an absent server GetRagdollEntity and a client
implementation that always returned NULL. Both now query the real retained player
ragdoll handle. This is independent of the extra prop_ragdoll owned by the addon.

The extended test first reproduces the old still-alive observer after an addon's
death notification, then runs original Victim.Kill, native silent-death state and
callback macros, original PlayerSilentDeath/PlayerDeathThink and delayed respawn.
It also checks the original optional inside/pilot respawn veto and its release
path, ordinary kill dispatch, and hidden/observer/visible player rendering gates.
Engine camera transport, physics and rendering remain isolated services. Native
silent-death cleanup in this fork does not claim parity with every auxiliary
reference-only camera/controller field. Live verification remains required.

## Observer input blocks original respawn keys

The next red-enraged instant-kill session establishes a real dead player, an
expired NextSpawnTime, and no held/inside restriction. Only movement button bits
reach the server; attack, secondary attack and jump never appear. The earlier
trace merged identical states across stages, so missing death-think records did
not establish a missing native dispatch. Checking the native player hierarchy
also confirms that the dead-player callback is reached normally.

ClientModeShared inherited Source spectator input that translates these three
bindings to spec_next/spec_prev/spec_mode and consumes their key-down events.
Consequently the original gamemode cannot see its required respawn input.
Current GMod input instead runs PlayerBindPress and then chat/HUD/weapon routing,
without this spectator translation. The compatibility build now follows that
route. Non-Lua builds retain their existing spectator handler. The obsolete
KeyInput hook is replaced with PlayerBindPress(player, binding, pressed, keycode),
where only boolean true consumes the event; console and toggleconsole bypass it.
The addon and original gamemode Lua are unchanged.

The regression runs the actual old and edited native KeyInput, native button-state
conversion, unchanged hook library and original GM:PlayerDeathThink. Old input
produces all three spectator commands and no respawn bits; fixed input reaches
the Spawn call boundary in observer modes 0/1/3/4/5/6. Lua vetoes, release events,
the original wait interval, console/chat/HUD/weapon routing, missing player/binding
and repeated stack balance also pass. Spawn itself and network/UI services remain
isolated, so this is not a claim that live respawn has passed. Earlier capture,
silent-death and visibility regressions also still pass with their stated limits.

All temporary lifecycle traces were removed. Changed translation units compile
and client/server build succeeds in 20.602 seconds. Four DLL/PDB SHA256 pairs match
after direct deployment. The user must fully restart and retest both dismemberment
and the enraged instant kill. The corrected code supersedes the conflicting original PR.


## Struggle progress and victim coils

The 03:25 log has no corresponding addon Lua traceback. The user confirms that
SPACE is visible while its fill never increases, and held victims have no coil.
Original addon code draws a fill bar and key pulse; it does not draw a numeric
press count. No replacement HUD or animation was added.

Server StartCommand was dispatched only from CBasePlayer::PreThink. The actual
CHL2MP player derives through CHL2_Player, whose PreThink implements its own body
without calling CBasePlayer::PreThink. Thus the original struggle input hook never
called Struggled on the server, leaving its progress at zero. Current GMod dispatches
StartCommand from the movement command entry, independently of player thinking.
The fork now does the same, before input is latched for button edges and movement,
and the unreachable base-player dispatch is removed to avoid duplicate callbacks.

Generated client NetworkVar getters had a separate decoder that omitted Entity
payloads. GetVictim and GetPrey consequently returned NULL even though the payload
had arrived. The getter now uses the existing family-aware decoder, which resolves
the target each time. Client-local prediction, subsequent server replacement and
other value types retain their existing semantics. An unused duplicate decoder
was removed. The current reference GetDTEntity confirms retained-handle resolution
and NULL for an unresolved target; the fork transport remains its existing user
message implementation rather than claiming GMod's snapshot transport internals.

The server regression executes actual old/new command-entry bodies, command-table
methods, dispatch macros, unchanged hook library, original sh_struggle and Struggled.
Old input never enters the hook; new press/release sequences increase progress by
the original gain, while a held key does not count twice and another victim is
rejected. The client regression uses actual serialization, message receiver and
generated getters with unchanged client UpdateLimbs/Coil/Draw and Flesh.Tube. The
old getter produces no coil; repaired input produces a 26-point coil. Original HUD
fill increases with replicated progress and disappears on release. Target changes,
delayed arrival, NULL, local overrides, scalar/vector/angle types and repeated stack
balance are covered. Network delivery, collision geometry and GPU execution are
isolated services; visible results remain for the user's full-restart game test.

Relevant translation units compile, client/server build succeeds in 19.751s, and
DLL/PDB are directly deployed with four matching SHA256 pairs. All 39 addon Lua
baseline hashes remain unchanged. Vehicle and crash changes from other work are excluded.
The corrected code supersedes the conflicting original PR.
