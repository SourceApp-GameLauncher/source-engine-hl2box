#include "cbase.h"
#include "baseplayer_shared.h"
#include "hl2sb_playerbuttons.h"
#include "usercmd.h"
#include "bitbuf.h"
#include "luamanager.h"
#include "lbaseentity_shared.h"
#include "igamesystem.h"
#include "inputsystem/ButtonCode.h"
#ifdef CLIENT_DLL
#include "inputsystem/iinputsystem.h"
#endif

#include "tier0/memdbgon.h"

namespace
{
const int BUTTON_SLOTS = 5;
const int COMMAND_SLOTS = 256;
struct ButtonCommand
{
	int number;
	unsigned char keys[BUTTON_SLOTS];
};
struct PlayerButtons
{
	EHANDLE owner;
	int lastCommand;
	unsigned char previous[BUTTON_SLOTS];
	ButtonCommand commands[COMMAND_SLOTS];
	PlayerButtons() { Reset(); }
	void Reset()
	{
		owner = NULL;
		lastCommand = -1;
		Q_memset(previous, 0, sizeof(previous));
		Q_memset(commands, 0, sizeof(commands));
		for (int i = 0; i < COMMAND_SLOTS; ++i) commands[i].number = -1;
	}
};
#ifdef CLIENT_DLL
PlayerButtons s_Local;
bool s_Held[256];
#else
PlayerButtons s_Players[MAX_PLAYERS + 1];
EHANDLE s_PacketPlayer;
PlayerButtons *ForPlayer(CBasePlayer *player)
{
	if (!player || player->entindex() < 1 || player->entindex() > MAX_PLAYERS) return NULL;
	PlayerButtons &state = s_Players[player->entindex()];
	if (state.owner != player) { state.Reset(); state.owner = player; }
	return &state;
}
#endif

class PlayerButtonSystem : public CAutoGameSystem
{
public:
	void LevelInitPreEntity() { Reset(); }
	void LevelShutdownPreEntity() { Reset(); }
	void Reset()
	{
#ifdef CLIENT_DLL
		s_Local.Reset();
		Q_memset(s_Held, 0, sizeof(s_Held));
#else
		for (int i = 0; i <= MAX_PLAYERS; ++i) s_Players[i].Reset();
		s_PacketPlayer = NULL;
#endif
	}
} s_PlayerButtonSystem;

bool Contains(const unsigned char *keys, int key)
{
	for (int i = 0; i < BUTTON_SLOTS; ++i) if (keys[i] == key) return true;
	return false;
}

void Dispatch(CBasePlayer *player, const char *event, int key)
{
	if (!L) return;
	int top = lua_gettop(L);
	BEGIN_LUA_CALL_HOOK(event);
		CBaseEntity::PushLuaInstanceSafe(L, player);
		lua_pushinteger(L, key);
	END_LUA_CALL_HOOK(2, 0);
	lua_settop(L, top);
}
}

#ifdef CLIENT_DLL
void HL2SB_PlayerButtonEvent(int key, bool down)
{
	if (key > 0 && key < ARRAYSIZE(s_Held))
		s_Held[key] = down && !engine->Con_IsVisible();
}

void HL2SB_CapturePlayerButtons(const CUserCmd *cmd, bool active)
{
	if (!cmd || cmd->command_number < 0) return;
	if (!active || engine->Con_IsVisible()) Q_memset(s_Held, 0, sizeof(s_Held));
	ButtonCommand &entry = s_Local.commands[cmd->command_number % COMMAND_SLOTS];
	entry.number = cmd->command_number;
	Q_memset(entry.keys, 0, sizeof(entry.keys));
	int count = 0;
	for (int key = 1; key < ARRAYSIZE(s_Held); ++key)
	{
		if (s_Held[key] && !inputsystem->IsButtonDown((ButtonCode_t)key)) s_Held[key] = false;
		if (s_Held[key] && count < BUTTON_SLOTS) entry.keys[count++] = (unsigned char)key;
	}
}
#else
void HL2SB_BeginPlayerButtonsPacket(CBasePlayer *player)
{
	s_PacketPlayer = player;
}
#endif

void HL2SB_WritePlayerButtons(bf_write *buf, const CUserCmd *cmd)
{
	const unsigned char empty[BUTTON_SLOTS] = {};
	const unsigned char *keys = empty;
#ifdef CLIENT_DLL
	if (cmd->command_number >= 0)
	{
		const ButtonCommand &entry = s_Local.commands[cmd->command_number % COMMAND_SLOTS];
		if (entry.number == cmd->command_number) keys = entry.keys;
	}
#endif
	for (int i = 0; i < BUTTON_SLOTS; ++i) buf->WriteUBitLong(keys[i], 8);
}

void HL2SB_ReadPlayerButtons(bf_read *buf, const CUserCmd *cmd)
{
	unsigned char keys[BUTTON_SLOTS] = {};
	int count = 0;
	for (int i = 0; i < BUTTON_SLOTS; ++i)
	{
		int key = buf->ReadUBitLong(8);
		if (key > KEY_NONE && key < BUTTON_CODE_LAST && !Contains(keys, key)) keys[count++] = (unsigned char)key;
	}
#ifndef CLIENT_DLL
	PlayerButtons *state = ForPlayer(ToBasePlayer(s_PacketPlayer.Get()));
	if (state && cmd->command_number >= 0 && !buf->IsOverflowed())
	{
		ButtonCommand &entry = state->commands[cmd->command_number % COMMAND_SLOTS];
		entry.number = cmd->command_number;
		Q_memcpy(entry.keys, keys, sizeof(keys));
	}
#endif
}

void HL2SB_RunPlayerButtons(CBasePlayer *player, const CUserCmd *cmd)
{
	if (!player || !cmd || cmd->command_number < 0) return;
#ifdef CLIENT_DLL
	if (s_Local.owner != player) { s_Local.owner = player; s_Local.lastCommand = -1; Q_memset(s_Local.previous, 0, sizeof(s_Local.previous)); }
	PlayerButtons *state = &s_Local;
#else
	PlayerButtons *state = ForPlayer(player);
#endif
	if (!state || cmd->command_number <= state->lastCommand) return;
	const ButtonCommand &entry = state->commands[cmd->command_number % COMMAND_SLOTS];
	// Missing synthetic commands retain the last physical state until a real command arrives.
	if (entry.number != cmd->command_number) return;
	unsigned char previous[BUTTON_SLOTS], current[BUTTON_SLOTS];
	Q_memcpy(previous, state->previous, sizeof(previous));
	Q_memcpy(current, entry.keys, sizeof(current));
	Q_memcpy(state->previous, current, sizeof(current));
	state->lastCommand = cmd->command_number;
	for (int i = 0; i < BUTTON_SLOTS; ++i)
		if (current[i] && !Contains(previous, current[i])) Dispatch(player, "PlayerButtonDown", current[i]);
	for (int i = 0; i < BUTTON_SLOTS; ++i)
		if (previous[i] && !Contains(current, previous[i])) Dispatch(player, "PlayerButtonUp", previous[i]);
}
