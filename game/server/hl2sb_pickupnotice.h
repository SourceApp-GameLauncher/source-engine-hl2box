// HL2SB: Player:SetSuppressPickupNotices( bool ) support.
//
// GMod gives scripts a switch that mutes the per-player pickup toasts; the
// NVG addon flips it around its weapon give/strip so equipping the goggles
// doesn't spam "Picked up ..." on the pickup HUD.  The flag lives in a
// server-side side table (no CBasePlayer layout change) keyed by entindex +
// handle serial so a recycled player slot cannot inherit a stale mute, and
// every item_pickup emitter asks for it before firing the event the pickup
// HUD listens on.

#ifndef HL2SB_PICKUPNOTICE_H
#define HL2SB_PICKUPNOTICE_H

class CBasePlayer;

void HL2SB_SetPickupNoticeSuppressed( CBasePlayer *pPlayer, bool bSuppressed );
bool HL2SB_IsPickupNoticeSuppressed( CBasePlayer *pPlayer );

#endif // HL2SB_PICKUPNOTICE_H
