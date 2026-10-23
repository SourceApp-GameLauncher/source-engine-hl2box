#ifndef HL2SB_PLAYERBUTTONS_H
#define HL2SB_PLAYERBUTTONS_H

class CUserCmd;
class bf_read;
class bf_write;
class CBasePlayer;
#ifdef CLIENT_DLL
class C_BasePlayer;
void HL2SB_PlayerButtonEvent(int key, bool down);
void HL2SB_CapturePlayerButtons(const CUserCmd *cmd, bool active);
void HL2SB_RunPlayerButtons(C_BasePlayer *player, const CUserCmd *cmd);
#else
void HL2SB_BeginPlayerButtonsPacket(CBasePlayer *player);
void HL2SB_RunPlayerButtons(CBasePlayer *player, const CUserCmd *cmd);
#endif
void HL2SB_WritePlayerButtons(bf_write *buf, const CUserCmd *cmd);
void HL2SB_ReadPlayerButtons(bf_read *buf, const CUserCmd *cmd);

#endif
