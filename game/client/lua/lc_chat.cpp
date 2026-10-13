#include "cbase.h"
#include "luamanager.h"
#include "luasrclib.h"
#include <engine/IEngineSound.h>
#include <hud_chat.h>
#include <hud_basechat.h>
#include <iclientmode.h>
#include <lColor.h>
#include <lbaseentity_shared.h>
#include <lbaseplayer_shared.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LUA_REGISTRATION_INIT( Chats )

LUA_BINDING_BEGIN( Chats, PlaySound, "library", "Play the 'tick' chat sound." )
{
    CLocalPlayerFilter filter;
    C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "HudChat.Message" );

    return 0;
}
LUA_BINDING_END()

LUA_BINDING_BEGIN( Chats, StartMessageMode, "library", "Open the chat with the specified message mode." )
{
    CHAT_MESSAGE_MODE mode = LUA_BINDING_ARGUMENT_ENUM( CHAT_MESSAGE_MODE, 1, "messageMode" );
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( hudChat )
    {
        hudChat->StartMessageMode( mode );
    }
    else
    {
        Assert( 0 );  // does this happen?
    }

    return 0;
}
LUA_BINDING_END()

LUA_BINDING_BEGIN( Chats, StopMessageMode, "library", "Close the chat." )
{
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( hudChat )
    {
        hudChat->StopMessageMode();
    }
    else
    {
        Assert( 0 );  // does this happen?
    }

    return 0;
}
LUA_BINDING_END()

LUA_BINDING_BEGIN( Chats, GetChatBoxPosition, "library", "Get the position of the chat box." )
{
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( hudChat )
    {
        int x, y;
        hudChat->GetPos( x, y );
        lua_pushnumber( L, x );
        lua_pushnumber( L, y );
        return 2;
    }

    Assert( 0 );  // does this happen?
    lua_pushnil( L );
    lua_pushnil( L );
    return 2;
}
LUA_BINDING_END()

// HL2SB (2026-10-13): GMod-name aliases.  GMod's native chat library exposes
// chat.Open( mode ) / chat.Close() / chat.GetChatBoxPos(); this fork's binding
// historically used StartMessageMode/StopMessageMode/GetChatBoxPosition.  Both
// name sets stay registered -- reference behavior: GMod's chat.Open with mode 1
// opens public chat and any other value opens team chat, which maps straight
// onto the CHAT_MESSAGE_MODE values below.
LUA_BINDING_BEGIN( Chats, Open, "library", "Opens the chatbox with the specified message mode." )
{
    CHAT_MESSAGE_MODE mode = LUA_BINDING_ARGUMENT_ENUM_WITH_DEFAULT( CHAT_MESSAGE_MODE, 1, (int)CHAT_MESSAGE_MODE::MM_SAY, "mode" );
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( hudChat )
    {
        hudChat->StartMessageMode( mode );
    }

    return 0;
}
LUA_BINDING_END()

LUA_BINDING_BEGIN( Chats, Close, "library", "Closes the chatbox." )
{
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( hudChat )
    {
        hudChat->StopMessageMode();
    }

    return 0;
}
LUA_BINDING_END()

LUA_BINDING_BEGIN( Chats, GetChatBoxPos, "library", "Get the position of the chat box." )
{
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( hudChat )
    {
        int x, y;
        hudChat->GetPos( x, y );
        lua_pushnumber( L, x );
        lua_pushnumber( L, y );
        return 2;
    }

    lua_pushnil( L );
    lua_pushnil( L );
    return 2;
}
LUA_BINDING_END()

LUA_BINDING_BEGIN( Chats, GetChatBoxSize, "library", "Get the size of the chat box." )
{
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( hudChat )
    {
        int w, h;
        hudChat->GetSize( w, h );
        lua_pushnumber( L, w );
        lua_pushnumber( L, h );
        return 2;
    }

    Assert( 0 );  // does this happen?
    lua_pushnil( L );
    lua_pushnil( L );
    return 2;
}
LUA_BINDING_END()

LUA_BINDING_BEGIN( Chats, AddText, "library", "Adds (optionally colored) text to the local chatbox" )
{
    CBaseHudChat *hudChat = ( CBaseHudChat * )GET_HUDELEMENT( CHudChat );

    if ( !hudChat )
    {
        Assert( 0 );  // does this happen?
    }

    // HL2SB (2026-10-13): reference behavior - GMod's native chat.AddText folds
    // the whole vararg list into ONE chat line with per-run colors (Color args
    // switch the run color, Player entities render their name in the player
    // color, plain values append as text).  The previous binding issued one
    // ChatPrintf per argument, so the base gamemode's
    // chat.AddText( player, color_white, ": ", strText ) came out as four
    // separate lines.  Build a single string with inline COLOR_HEXCODE runs
    // (CBaseHudChatLine::InsertAndColorizeText parses them) and print once.
    char szLine[1024];
    char szPlain[1024];
    // The color run currently in effect.  Reference behavior: a Color argument
    // sets it, a Player argument only paints that player's own name and then
    // restores this, so the strings AFTER a player name keep the previous
    // color (the wiki note on chat.AddText, and the native handler keeps a
    // saved color slot it re-applies after the name run).
    char szCurColor[8] = { 0 };
    int nLen = 0, nPlainLen = 0;
    szLine[0] = 0;
    szPlain[0] = 0;

    int nArgs = lua_gettop( L );

    for ( int i = 1; i <= nArgs; i++ )
    {
        if ( lua_iscolor( L, i ) )
        {
            Color color = LUA_BINDING_ARGUMENT( luaL_checkcolor, i, "color" );
            if ( nLen < (int)sizeof( szLine ) - 16 )
            {
                nLen += Q_snprintf( szLine + nLen, sizeof( szLine ) - nLen, "%c%02x%02x%02x",
                    (char)COLOR_HEXCODE, color.r(), color.g(), color.b() );
                // Remember the active color so a following player name can
                // restore it (see szCurColor above).
                Q_snprintf( szCurColor, sizeof( szCurColor ), "%c%02x%02x%02x",
                    (char)COLOR_HEXCODE, color.r(), color.g(), color.b() );
            }
        }
        else if ( lua_toentity( L, i ) && lua_toentity( L, i )->IsPlayer() )
        {
            CBasePlayer *player = LUA_BINDING_ARGUMENT( luaL_checkplayer, i, "player" );
            // Player names render in the same color a normal chat line gives
            // them, resolved per player so multiple players on one line keep
            // their own colors.
            Color nameColor = hudChat->GetTextColorForClient( COLOR_PLAYERNAME, player->entindex() );
            if ( nLen < (int)sizeof( szLine ) - MAX_PLAYER_NAME_LENGTH - 16 )
            {
                nLen += Q_snprintf( szLine + nLen, sizeof( szLine ) - nLen, "%c%02x%02x%02x%s",
                    (char)COLOR_HEXCODE, nameColor.r(), nameColor.g(), nameColor.b(), player->GetPlayerName() );
            }
            // A Player argument colors only its own name -- restore the color
            // that was active before it so the following strings keep it
            // (reference behavior: the native handler re-applies its saved
            // color after the name run).  With no explicit color set yet, fall
            // back to the default text color.
            if ( nLen < (int)sizeof( szLine ) - 16 )
            {
                if ( szCurColor[0] != 0 )
                {
                    nLen += Q_snprintf( szLine + nLen, sizeof( szLine ) - nLen, "%s", szCurColor );
                }
                else
                {
                    szLine[ nLen++ ] = (char)COLOR_NORMAL;
                    szLine[ nLen ] = 0;
                }
            }
            if ( nPlainLen < (int)sizeof( szPlain ) - MAX_PLAYER_NAME_LENGTH - 2 )
            {
                nPlainLen += Q_snprintf( szPlain + nPlainLen, sizeof( szPlain ) - nPlainLen, "%s: ", player->GetPlayerName() );
            }
        }
        else if ( lua_isstring( L, i ) || lua_isnumber( L, i ) )
        {
            const char *pszText = lua_tostring( L, i );
            if ( pszText )
            {
                int nText = (int)strlen( pszText );
                if ( nLen < (int)sizeof( szLine ) - nText - 1 )
                {
                    Q_strncpy( szLine + nLen, pszText, sizeof( szLine ) - nLen );
                    nLen += nText;
                }
                if ( nPlainLen < (int)sizeof( szPlain ) - nText - 1 )
                {
                    Q_strncpy( szPlain + nPlainLen, pszText, sizeof( szPlain ) - nPlainLen );
                    nPlainLen += nText;
                }
            }
        }
        else
        {
            // GMod stringifies every other value (Entity -> class name).
            const char *pszText = lua_tostring( L, i );
            if ( pszText )
            {
                int nText = (int)strlen( pszText );
                if ( nLen < (int)sizeof( szLine ) - nText - 1 )
                {
                    Q_strncpy( szLine + nLen, pszText, sizeof( szLine ) - nLen );
                    nLen += nText;
                }
                if ( nPlainLen < (int)sizeof( szPlain ) - nText - 1 )
                {
                    Q_strncpy( szPlain + nPlainLen, pszText, sizeof( szPlain ) - nPlainLen );
                    nPlainLen += nText;
                }
            }
        }
    }

    if ( nLen > 0 )
    {
        // InsertAndColorizeText only engages the color-run parser when the
        // line's first byte is a color code, so a leading plain string would
        // leave later hex runs unparsed.  Prepend COLOR_NORMAL (default text
        // color) when the line does not already open with a code.
        if ( szLine[0] != (char)COLOR_NORMAL && szLine[0] != (char)COLOR_PLAYERNAME &&
             szLine[0] != (char)COLOR_LOCATION && szLine[0] != (char)COLOR_ACHIEVEMENT &&
             szLine[0] != (char)COLOR_CUSTOM && szLine[0] != (char)COLOR_HEXCODE &&
             szLine[0] != (char)COLOR_HEXCODE_ALPHA )
        {
            if ( nLen < (int)sizeof( szLine ) - 1 )
            {
                memmove( szLine + 1, szLine, nLen + 1 );
                szLine[0] = (char)COLOR_NORMAL;
                nLen++;
            }
        }

        hudChat->ChatPrintf( 0, CHAT_FILTER_NONE, "%s", szLine );
        // Reference behavior: the native AddText also mirrors the line to the
        // console without the color runs.
        if ( nPlainLen > 0 )
        {
            Msg( "%s\n", szPlain );
        }
    }

    return 0;
}
LUA_BINDING_END()

/*
** Open Chats library
*/
LUALIB_API int luaopen_Chats( lua_State *L )
{
    LUA_REGISTRATION_COMMIT_LIBRARY( Chats );

    LUA_SET_ENUM_LIB_BEGIN( L, "CHAT_MESSAGE_MODE" );
    lua_pushenum( L, CHAT_MESSAGE_MODE::MM_NONE, "NONE" );
    lua_pushenum( L, CHAT_MESSAGE_MODE::MM_SAY, "GLOBAL" );
    lua_pushenum( L, CHAT_MESSAGE_MODE::MM_SAY_TEAM, "TEAM" );
    LUA_SET_ENUM_LIB_END( L );

    return 1;
}
