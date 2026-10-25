//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: GMod-style fullscreen "create server" dialog
//
// $NoKeywords: $
//=============================================================================//

#include "CreateMultiplayerGameDialog.h"

// include original page classes so we can still read cvars / bot settings
#include "CreateMultiplayerGameServerPage.h"
#include "CreateMultiplayerGameGameplayPage.h"
#include "CreateMultiplayerGameBotPage.h"

#include "EngineInterface.h"
#include "ModInfo.h"
#include "GameUI_Interface.h"
#include "PNGImagePanel.h"
#include "MouseMessageForwardingPanel.h"

#include <stdio.h>
#include <map>
#include <vector>
#include <string>
#include <regex>
#include <cmath>
#include <cctype>
#include <algorithm>

using namespace vgui;

#include "vgui_controls/ComboBox.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ToolTip.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>

#include "filesystem.h"
#include <KeyValues.h>
#include "tier1/convar.h"
#include <tier0/memdbgon.h>

#define RANDOM_MAP "#GameUI_RandomMap"
#define MAX_PLAYERS_DEFAULT 1


// GMod classifies exact names before the first underscore prefix, then tests
// gamemode patterns. Mount paths do not decide campaign membership.
struct NativeMapRule { const char *name; const char *category; };
static const NativeMapRule s_MapNames[] = {
 { "1v1_", "Counter-Strike (Custom)" },
 { "2koth_", "Team Fortress 2" },
 { "35hp_", "Counter-Strike (Custom)" },
 { "achievement_", "Team Fortress 2" },
 { "aim_", "Counter-Strike (Custom)" },
 { "am_", "Counter-Strike (Custom)" },
 { "aoc_", "Age of Chivalry" },
 { "ar_", "Counter-Strike" },
 { "arena_", "Team Fortress 2" },
 { "awp_", "Counter-Strike (Custom)" },
 { "ba_", "Jailbreak" },
 { "bhop_", "Bunny Hop" },
 { "bm_", "Black Mesa" },
 { "boot_camp", "Half-Life Deathmatch" },
 { "bounce", "Half-Life Deathmatch" },
 { "bt_", "Pirates, Vikings, & Knights II" },
 { "buhriz", "Insurgency" },
 { "buhriz_coop", "Insurgency" },
 { "buhriz_night", "Insurgency" },
 { "cinema_", "Cinema" },
 { "clocktower", "Nuclear Dawn" },
 { "cm_", "Fistful of Frags" },
 { "coast", "Nuclear Dawn" },
 { "contact", "Insurgency" },
 { "contact_coop", "Insurgency" },
 { "contact_night", "Insurgency" },
 { "coop_", "Cooperative" },
 { "cp_", "Team Fortress 2" },
 { "cp_docks", "Blade Symphony" },
 { "cp_parkour", "Blade Symphony" },
 { "cp_sequence", "Blade Symphony" },
 { "cp_terrace", "Blade Symphony" },
 { "cp_test", "Blade Symphony" },
 { "cppl_", "Team Fortress 2" },
 { "crossfire", "Half-Life Deathmatch" },
 { "cs_", "Counter-Strike" },
 { "ctf_", "Team Fortress 2" },
 { "curling_stadium", "Left 4 Dead 2" },
 { "d1_canals_01", "Half-Life 2" },
 { "d1_canals_01a", "Half-Life 2" },
 { "d1_canals_02", "Half-Life 2" },
 { "d1_canals_03", "Half-Life 2" },
 { "d1_canals_05", "Half-Life 2" },
 { "d1_canals_06", "Half-Life 2" },
 { "d1_canals_07", "Half-Life 2" },
 { "d1_canals_08", "Half-Life 2" },
 { "d1_canals_09", "Half-Life 2" },
 { "d1_canals_10", "Half-Life 2" },
 { "d1_canals_11", "Half-Life 2" },
 { "d1_canals_12", "Half-Life 2" },
 { "d1_canals_13", "Half-Life 2" },
 { "d1_eli_01", "Half-Life 2" },
 { "d1_eli_02", "Half-Life 2" },
 { "d1_town_01", "Half-Life 2" },
 { "d1_town_01a", "Half-Life 2" },
 { "d1_town_02", "Half-Life 2" },
 { "d1_town_02a", "Half-Life 2" },
 { "d1_town_03", "Half-Life 2" },
 { "d1_town_04", "Half-Life 2" },
 { "d1_town_05", "Half-Life 2" },
 { "d1_trainstation_01", "Half-Life 2" },
 { "d1_trainstation_02", "Half-Life 2" },
 { "d1_trainstation_03", "Half-Life 2" },
 { "d1_trainstation_04", "Half-Life 2" },
 { "d1_trainstation_05", "Half-Life 2" },
 { "d1_trainstation_06", "Half-Life 2" },
 { "d2_coast_01", "Half-Life 2" },
 { "d2_coast_03", "Half-Life 2" },
 { "d2_coast_04", "Half-Life 2" },
 { "d2_coast_05", "Half-Life 2" },
 { "d2_coast_07", "Half-Life 2" },
 { "d2_coast_08", "Half-Life 2" },
 { "d2_coast_09", "Half-Life 2" },
 { "d2_coast_10", "Half-Life 2" },
 { "d2_coast_11", "Half-Life 2" },
 { "d2_coast_12", "Half-Life 2" },
 { "d2_lostcoast", "Half-Life 2: Lost Coast" },
 { "d2_prison_01", "Half-Life 2" },
 { "d2_prison_02", "Half-Life 2" },
 { "d2_prison_03", "Half-Life 2" },
 { "d2_prison_04", "Half-Life 2" },
 { "d2_prison_05", "Half-Life 2" },
 { "d2_prison_06", "Half-Life 2" },
 { "d2_prison_07", "Half-Life 2" },
 { "d2_prison_08", "Half-Life 2" },
 { "d3_breen_01", "Half-Life 2" },
 { "d3_c17_01", "Half-Life 2" },
 { "d3_c17_02", "Half-Life 2" },
 { "d3_c17_03", "Half-Life 2" },
 { "d3_c17_04", "Half-Life 2" },
 { "d3_c17_05", "Half-Life 2" },
 { "d3_c17_06a", "Half-Life 2" },
 { "d3_c17_06b", "Half-Life 2" },
 { "d3_c17_07", "Half-Life 2" },
 { "d3_c17_08", "Half-Life 2" },
 { "d3_c17_09", "Half-Life 2" },
 { "d3_c17_10a", "Half-Life 2" },
 { "d3_c17_10b", "Half-Life 2" },
 { "d3_c17_11", "Half-Life 2" },
 { "d3_c17_12", "Half-Life 2" },
 { "d3_c17_12b", "Half-Life 2" },
 { "d3_c17_13", "Half-Life 2" },
 { "d3_citadel_01", "Half-Life 2" },
 { "d3_citadel_02", "Half-Life 2" },
 { "d3_citadel_03", "Half-Life 2" },
 { "d3_citadel_04", "Half-Life 2" },
 { "d3_citadel_05", "Half-Life 2" },
 { "datacore", "Half-Life Deathmatch" },
 { "ddd_", "Dino D-Day" },
 { "de_", "Counter-Strike" },
 { "de_dam", "DIPRIP" },
 { "deathrun_", "Deathrun" },
 { "district", "Insurgency" },
 { "district_coop", "Insurgency" },
 { "district_night", "Insurgency" },
 { "dm_", "Half-Life 2: Deathmatch" },
 { "dm_boom", "Black Mesa" },
 { "dm_bounce", "Black Mesa" },
 { "dm_chopper", "Black Mesa" },
 { "dm_city", "DIPRIP" },
 { "dm_crossfire", "Black Mesa" },
 { "dm_gasworks", "Black Mesa" },
 { "dm_lambdabunker", "Black Mesa" },
 { "dm_power", "Black Mesa" },
 { "dm_rail", "Black Mesa" },
 { "dm_refinery", "DIPRIP" },
 { "dm_stack", "Black Mesa" },
 { "dm_stalkyard", "Black Mesa" },
 { "dm_subtransit", "Black Mesa" },
 { "dm_supermarket", "DIPRIP" },
 { "dm_undertow", "Black Mesa" },
 { "dm_village", "DIPRIP" },
 { "dod_", "Day Of Defeat" },
 { "downtown", "Nuclear Dawn" },
 { "dr_", "Deathrun" },
 { "dragon_girl", "G String" },
 { "dragon_girl1", "G String" },
 { "dragon_girl2", "G String" },
 { "dragon_girl3", "G String" },
 { "drycanal", "Insurgency" },
 { "drycanal_coop", "Insurgency" },
 { "drycanal_night", "Insurgency" },
 { "duel_", "Blade Symphony" },
 { "dys_", "Dystopia" },
 { "dz_", "Counter-Strike" },
 { "e1912", "Portal 2" },
 { "embassy", "Insurgency" },
 { "embassy_coop", "Insurgency" },
 { "embassy_night", "Insurgency" },
 { "ep1_c17_00", "Half-Life 2: Episode 1" },
 { "ep1_c17_00a", "Half-Life 2: Episode 1" },
 { "ep1_c17_01", "Half-Life 2: Episode 1" },
 { "ep1_c17_01a", "Half-Life 2: Episode 1" },
 { "ep1_c17_02", "Half-Life 2: Episode 1" },
 { "ep1_c17_02a", "Half-Life 2: Episode 1" },
 { "ep1_c17_02b", "Half-Life 2: Episode 1" },
 { "ep1_c17_05", "Half-Life 2: Episode 1" },
 { "ep1_c17_06", "Half-Life 2: Episode 1" },
 { "ep1_citadel_00", "Half-Life 2: Episode 1" },
 { "ep1_citadel_01", "Half-Life 2: Episode 1" },
 { "ep1_citadel_02", "Half-Life 2: Episode 1" },
 { "ep1_citadel_02b", "Half-Life 2: Episode 1" },
 { "ep1_citadel_03", "Half-Life 2: Episode 1" },
 { "ep1_citadel_04", "Half-Life 2: Episode 1" },
 { "ep2_outland_01", "Half-Life 2: Episode 2" },
 { "ep2_outland_01a", "Half-Life 2: Episode 2" },
 { "ep2_outland_02", "Half-Life 2: Episode 2" },
 { "ep2_outland_03", "Half-Life 2: Episode 2" },
 { "ep2_outland_04", "Half-Life 2: Episode 2" },
 { "ep2_outland_05", "Half-Life 2: Episode 2" },
 { "ep2_outland_06", "Half-Life 2: Episode 2" },
 { "ep2_outland_06a", "Half-Life 2: Episode 2" },
 { "ep2_outland_07", "Half-Life 2: Episode 2" },
 { "ep2_outland_08", "Half-Life 2: Episode 2" },
 { "ep2_outland_09", "Half-Life 2: Episode 2" },
 { "ep2_outland_10", "Half-Life 2: Episode 2" },
 { "ep2_outland_10a", "Half-Life 2: Episode 2" },
 { "ep2_outland_11", "Half-Life 2: Episode 2" },
 { "ep2_outland_11a", "Half-Life 2: Episode 2" },
 { "ep2_outland_11b", "Half-Life 2: Episode 2" },
 { "ep2_outland_12", "Half-Life 2: Episode 2" },
 { "ep2_outland_12a", "Half-Life 2: Episode 2" },
 { "es_", "Counter-Strike" },
 { "escape_", "Portal" },
 { "ff_", "Fortress Forever" },
 { "ffa_community", "Blade Symphony" },
 { "fm_", "Flood" },
 { "fof_", "Fistful of Frags" },
 { "fofhr_", "Fistful of Frags" },
 { "free_", "Blade Symphony" },
 { "free_mars1", "G String" },
 { "free_mars2", "G String" },
 { "free_mars3", "G String" },
 { "frenzy", "Half-Life Deathmatch" },
 { "fy_", "Counter-Strike (Custom)" },
 { "gate", "Nuclear Dawn" },
 { "gd_", "Counter-Strike" },
 { "gg_", "Gun Game" },
 { "gms_", "Stranded" },
 { "gmt_", "GMod Tower" },
 { "gt_", "Fistful of Frags" },
 { "halls3", "Half-Life 2: Deathmatch" },
 { "hazardous_ai", "G String" },
 { "hazardous_ai0", "G String" },
 { "hazardous_ai01", "G String" },
 { "hazardous_ai2", "G String" },
 { "hazardous_ai3", "G String" },
 { "hazardous_ai3b", "G String" },
 { "hazardous_ai4", "G String" },
 { "hazardous_ai5", "G String" },
 { "hazardous_ai6", "G String" },
 { "hazardous_ai7", "G String" },
 { "hazardous_ai_more", "G String" },
 { "heights", "Insurgency" },
 { "heights_coop", "Insurgency" },
 { "heights_night", "Insurgency" },
 { "htf_", "Team Fortress 2" },
 { "human_waste1", "G String" },
 { "human_waste2", "G String" },
 { "human_waste3", "G String" },
 { "human_waste4", "G String" },
 { "human_waste5", "G String" },
 { "human_waste6", "G String" },
 { "human_waste7", "G String" },
 { "human_waste8", "G String" },
 { "human_waste9", "G String" },
 { "human_waste_x1", "G String" },
 { "hydro", "Nuclear Dawn" },
 { "infra_", "INFRA" },
 { "ins_", "Insurgency" },
 { "itemtest", "Team Fortress 2" },
 { "jail_", "Jailbreak" },
 { "jb_", "Jailbreak" },
 { "kandagal", "Insurgency" },
 { "kandagal_night", "Insurgency" },
 { "koth_", "Team Fortress 2" },
 { "l4d_", "Left 4 Dead" },
 { "lab_rat", "G String" },
 { "lab_rat1", "G String" },
 { "lambda_bunker", "Half-Life Deathmatch" },
 { "lightstyle_test", "Blade Symphony" },
 { "lobby", "Alien Swarm" },
 { "lobby_mapveto", "Counter-Strike" },
 { "lts_", "Pirates, Vikings, & Knights II" },
 { "market", "Insurgency" },
 { "market_coop", "Insurgency" },
 { "market_night", "Insurgency" },
 { "mcv_", "Military Conflict: Vietnam" },
 { "metro", "Nuclear Dawn" },
 { "metro_training", "Nuclear Dawn" },
 { "mg_", "Minigames" },
 { "ministry", "Insurgency" },
 { "ministry_coop", "Insurgency" },
 { "ministry_night", "Insurgency" },
 { "money_is_dead", "G String" },
 { "money_is_dead0", "G String" },
 { "money_is_dead2", "G String" },
 { "money_is_dead2_1", "G String" },
 { "money_is_dead2_2", "G String" },
 { "money_is_dead3", "G String" },
 { "money_is_dead3_2", "G String" },
 { "money_is_dead4", "G String" },
 { "money_is_dead4_2", "G String" },
 { "money_is_dead4_3", "G String" },
 { "money_is_dead_1", "G String" },
 { "murdock_air1", "G String" },
 { "murdock_air2", "G String" },
 { "murdock_air3", "G String" },
 { "murdock_air4", "G String" },
 { "murdock_air5", "G String" },
 { "murdock_air6", "G String" },
 { "murdock_air7", "G String" },
 { "murdock_air8", "G String" },
 { "murdock_air9", "G String" },
 { "murdock_air_x1", "G String" },
 { "murdock_air_x2", "G String" },
 { "murdock_air_x3", "G String" },
 { "murdock_air_x4", "G String" },
 { "murdock_air_x5", "G String" },
 { "murdock_air_x6", "G String" },
 { "mvm_", "Team Fortress 2" },
 { "oasis", "Nuclear Dawn" },
 { "oilfield", "Nuclear Dawn" },
 { "panj", "Insurgency" },
 { "panj_night", "Insurgency" },
 { "pass_", "Team Fortress 2" },
 { "pb_dojo", "Dystopia" },
 { "pb_rooftop", "Dystopia" },
 { "pb_round", "Dystopia" },
 { "pb_urbandome", "Dystopia" },
 { "pd_", "Team Fortress 2" },
 { "peak", "Insurgency" },
 { "peak_night", "Insurgency" },
 { "perp_org1", "G String" },
 { "perp_org2", "G String" },
 { "perp_org3", "G String" },
 { "perp_org4", "G String" },
 { "ph_", "Prop Hunt" },
 { "pl_", "Team Fortress 2" },
 { "plr_", "Team Fortress 2" },
 { "practice_box", "Blade Symphony" },
 { "pw_", "Pirate Ship Wars" },
 { "rapidcore", "Half-Life Deathmatch" },
 { "rd_", "Team Fortress 2" },
 { "revolt", "Insurgency" },
 { "revolt_coop", "Insurgency" },
 { "revolt_night", "Insurgency" },
 { "rp_", "Roleplay" },
 { "sabotage", "G String" },
 { "sabotage1", "G String" },
 { "sav_dojo6", "Dystopia" },
 { "sb_", "Spacebuild" },
 { "scoutzknivez", "Gun Game" },
 { "sd_", "Team Fortress 2" },
 { "siege", "Insurgency" },
 { "siege_coop", "Insurgency" },
 { "silo", "Nuclear Dawn" },
 { "sinjar", "Insurgency" },
 { "sinjar_coop", "Insurgency" },
 { "sinjar_night", "Insurgency" },
 { "sk_metro", "Nuclear Dawn" },
 { "slb_", "Sled Build" },
 { "slender_", "Stop it Slender" },
 { "smog_storm", "G String" },
 { "smog_storm1", "G String" },
 { "smog_storm2", "G String" },
 { "smog_storm3", "G String" },
 { "snarkpit", "Half-Life Deathmatch" },
 { "space_race", "G String" },
 { "space_race1", "G String" },
 { "space_race2", "G String" },
 { "space_race3", "G String" },
 { "space_race4", "G String" },
 { "space_race5", "G String" },
 { "stalkyard", "Half-Life Deathmatch" },
 { "station", "Insurgency" },
 { "station_night", "Insurgency" },
 { "subtransit", "Half-Life Deathmatch" },
 { "surf_", "Surf" },
 { "t_", "Klaus Veen's Treason" },
 { "tc_", "Team Fortress 2" },
 { "te_", "Pirates, Vikings, & Knights II" },
 { "tell", "Insurgency" },
 { "tell_coop", "Insurgency" },
 { "tell_night", "Insurgency" },
 { "terror_management_x1", "G String" },
 { "terror_management_x2", "G String" },
 { "terror_management_x3", "G String" },
 { "testchmb_", "Portal" },
 { "the_call1", "G String" },
 { "theater_", "Cinema" },
 { "tow_", "Team Fortress 2" },
 { "tp_", "Fistful of Frags" },
 { "tr_", "Team Fortress 2" },
 { "trade_", "Team Fortress 2" },
 { "training", "Nuclear Dawn" },
 { "training1", "Counter-Strike" },
 { "ts_", "The Stalker" },
 { "tut_training", "Blade Symphony" },
 { "tutorial_standards", "Left 4 Dead 2" },
 { "tutorial_standards_vs", "Left 4 Dead 2" },
 { "tw_", "Pirates, Vikings, & Knights II" },
 { "undertow", "Half-Life Deathmatch" },
 { "uprising", "Insurgency" },
 { "uprising_night", "Insurgency" },
 { "ur_city", "DIPRIP" },
 { "ur_refinery", "DIPRIP" },
 { "ur_supermarket", "DIPRIP" },
 { "ur_village", "DIPRIP" },
 { "varena", "Dystopia" },
 { "verticality", "Insurgency" },
 { "verticality_coop", "Insurgency" },
 { "verticality_night", "Insurgency" },
 { "vs_", "Fistful of Frags" },
 { "vsh_", "Team Fortress 2" },
 { "xc_", "Climb" },
 { "ze_", "Zombie Escape" },
 { "zi_", "Team Fortress 2" },
 { "zm_", "Zombie Survival" },
 { "zombiesurvival_", "Zombie Survival" },
 { "zpa_", "Zombie Panic! Source" },
 { "zph_", "Zombie Panic! Source" },
 { "zpl_", "Zombie Panic! Source" },
 { "zpo_", "Zombie Panic! Source" },
 { "zps_", "Zombie Panic! Source" },
 { "zs_", "Zombie Survival" },
};
static const NativeMapRule s_MapPatterns[] = {
 { "^asi-", "Alien Swarm" },
 { "^c1[%d]m", "Left 4 Dead 2" },
 { "^c[%d]a", "Half-Life" },
 { "^c[%d]m", "Left 4 Dead 2" },
 { "^mp_coop_", "Portal 2" },
 { "^sp_a", "Portal 2" },
 { "^t0a", "Half-Life" },
};
static const char *s_IgnoreNames[] = {
 "c4a1y",
 "c5m1_waterfront_sndscape",
 "credits",
 "d2_coast_02",
 "d3_c17_02_camera",
 "ep1_citadel_00_demo",
 "intro",
 "sdk_",
 "test",
 "test_",
 "vst_",
};
static const char *s_IgnorePrefixes[] = {
 "background",
 "devtest",
 "ep1_background",
 "ep2_background",
 "styleguide",
};
static std::vector<std::string> s_MapCategories;
struct NativeMapPattern { std::regex pattern; std::string category; };
static std::vector<NativeMapPattern> s_GamemodeMapPatterns;

static std::string MapPrefix(const std::string &name)
{
 size_t end=name.find('_');return end==std::string::npos?"":name.substr(0,end+1);
}
static bool IsMenuMap(const std::string &name)
{
 std::string prefix=MapPrefix(name);
 for(size_t i=0;i<ARRAYSIZE(s_IgnoreNames);++i)
  if(name==s_IgnoreNames[i]||prefix==s_IgnoreNames[i])return false;
 for(size_t i=0;i<ARRAYSIZE(s_IgnorePrefixes);++i)
  if(name.compare(0,strlen(s_IgnorePrefixes[i]),s_IgnorePrefixes[i])==0)return false;
 return true;
}
static void AddNativeMapPattern(const char *luaPattern,const char *category)
{
 // Convert the pattern operators used by gamemode map declarations.
 std::string regex;bool inClass=false;
 for(const char *p=luaPattern;*p;++p)
 {
  if(*p=='%'&&p[1])
  {
   ++p;const char *range=NULL;
   switch(*p){case 'd':range="0-9";break;case 'a':range="a-zA-Z";break;
    case 'w':range="a-zA-Z0-9";break;case 's':range=" \t\r\n";break;
    case 'l':range="a-z";break;case 'u':range="A-Z";break;}
   if(range){if(!inClass)regex+='[';regex+=range;if(!inClass)regex+=']';}
   else {regex+=(char)92;regex+=*p;}
  }
  else if(*p=='['){inClass=true;regex+=*p;}
  else if(*p==']'){inClass=false;regex+=*p;}
  else if(*p=='-'&&!inClass)regex+="*?";
  else if((*p=='{'||*p=='}'||*p=='|')&&!inClass){regex+=(char)92;regex+=*p;}
  else regex+=*p;
 }
 try { NativeMapPattern rule={std::regex(regex,std::regex::icase),category};s_GamemodeMapPatterns.push_back(rule); }
 catch(const std::regex_error &) { Warning("Invalid gamemode map pattern: %s\n",luaPattern); }
}
static void LoadNativeMapPatterns()
{
 s_GamemodeMapPatterns.clear();
 for(size_t i=0;i<ARRAYSIZE(s_MapPatterns);++i)AddNativeMapPattern(s_MapPatterns[i].name,s_MapPatterns[i].category);
 FileFindHandle_t find;
 const char *dir=g_pFullFileSystem->FindFirstEx("gamemodes/*","GAME",&find);
 while(dir)
 {
  if(dir[0]!='.'&&g_pFullFileSystem->FindIsDirectory(find))
  {
   char path[256];Q_snprintf(path,sizeof(path),"gamemodes/%s/%s.txt",dir,dir);
   KeyValues *gm=new KeyValues("Gamemode");
   if(gm->LoadFromFile(g_pFullFileSystem,path,"GAME"))
   {
    std::string patterns=gm->GetString("maps"),title=gm->GetString("title",dir);
    size_t begin=0;
    while(begin<patterns.size())
    {
     size_t end=patterns.find('|',begin);std::string part=patterns.substr(begin,end-begin);
     if(!part.empty())AddNativeMapPattern(part.c_str(),title.c_str());
     if(end==std::string::npos)break;begin=end+1;
    }
   }
   gm->deleteThis();
  }
  dir=g_pFullFileSystem->FindNext(find);
 }
 g_pFullFileSystem->FindClose(find);
}
static std::string NativeMapCategory(const char *map)
{
 std::string name=map?map:"";std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return (char)tolower(c);});
 const std::string prefix=MapPrefix(name);
 for(size_t i=0;i<ARRAYSIZE(s_MapNames);++i)if(name==s_MapNames[i].name)return s_MapNames[i].category;
 for(size_t i=0;i<ARRAYSIZE(s_MapNames);++i)if(prefix==s_MapNames[i].name)return s_MapNames[i].category;
 std::string category="Other";
 for(size_t i=0;i<s_GamemodeMapPatterns.size();++i)
  if(std::regex_search(name,s_GamemodeMapPatterns[i].pattern))category=s_GamemodeMapPatterns[i].category;
 return category;
}

//-----------------------------------------------------------------------------
// HL2SB: one numeric limits row -- label on the left, number entry on the
// right.  Lives inside the scrollable limits list, so it relayouts itself to
// whatever width the list gives it.
//-----------------------------------------------------------------------------
class CSandboxLimitRow : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CSandboxLimitRow, vgui::EditablePanel );
public:
	CSandboxLimitRow( vgui::PanelListPanel *parent, const char *pszToken )
		: BaseClass( parent, "SandboxLimitRow" )
	{
		SetPaintBackgroundEnabled( true );

		m_pLabel = new vgui::Label( this, "LimitLabel", pszToken );
		m_pLabel->SetContentAlignment( vgui::Label::a_west );
		m_pLabel->SetTextInset( 4, 0 );
		m_pLabel->SetMouseInputEnabled( false );

		m_pEntry = new vgui::TextEntry( this, "LimitEntry" );
		m_pEntry->SetAllowNumericInputOnly( true );
		m_pEntry->SetMaximumCharCount( 7 );

		SetSize( 300, 26 );
	}

	vgui::TextEntry *GetEntry() { return m_pEntry; }

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int w, h;
		GetSize( w, h );
		if ( m_pLabel )
			m_pLabel->SetBounds( 0, 0, w - 100, h );
		if ( m_pEntry )
			m_pEntry->SetBounds( w - 96, ( h - 22 ) / 2, 92, 22 );
	}

	virtual void PaintBackground()
	{
		vgui::surface()->DrawSetColor( 0, 0, 0, 110 );
		vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
	}

private:
	vgui::Label *m_pLabel;
	vgui::TextEntry *m_pEntry;
};


// Build settings from the gamemode definition, just like the GMod new-game
// page. Keep values independently of visible rows so mode changes lose no edits.
class CLaunchSettingsList : public PanelListPanel
{
public:
 CLaunchSettingsList( Panel *parent ) : PanelListPanel( parent, "SandboxList" ),
  m_pDefinition( new KeyValues( "Gamemode" ) ), m_pValues( new KeyValues( "LaunchValues" ) ),
  m_bSingle( true ), m_bConfigured( false ) {}
 ~CLaunchSettingsList() { m_pDefinition->deleteThis(); m_pValues->deleteThis(); }
 void Configure( KeyValues *saved )
 {
  ConVarRef mode( "gamemode", true );
  char path[256];
  Q_snprintf( path, sizeof(path), "gamemodes/%s/%s.txt",
   mode.IsValid() ? mode.GetString() : "sandbox", mode.IsValid() ? mode.GetString() : "sandbox" );
  if ( !m_pDefinition->LoadFromFile( g_pFullFileSystem, path, "GAME" ) )
   m_pDefinition->LoadFromFile( g_pFullFileSystem, "gamemodes/sandbox/sandbox.txt", "GAME" );
  KeyValues *settings = m_pDefinition->FindKey( "settings" );
  if ( settings ) FOR_EACH_SUBKEY( settings, def )
  {
   const char *name = def->GetString( "name" );
   if ( !name[0] ) continue;
   m_pValues->SetString( name, saved && saved->FindKey(name) ? saved->GetString(name) : def->GetString("default") );
  }
  m_bConfigured = true;
  SetSinglePlayer( true );
 }
 void Capture()
 {
  for ( int i=0; i<m_Rows.Count(); ++i )
  {
   Row &r=m_Rows[i];
   if ( r.check ) m_pValues->SetInt( r.name, r.check->IsSelected() ? 1 : 0 );
   else
   {
    char value[512];r.entry->GetText(value,sizeof(value));
    // Empty numeric fields retain the definition's default; text can be empty.

    if(r.numeric)
    {
     char *end=NULL;double number=strtod(value,&end);
     if(end==value||*end||!std::isfinite(number))m_pValues->SetString(r.name,r.def);
     else {char numeric[48];Q_snprintf(numeric,sizeof(numeric),"%.9g",number);m_pValues->SetString(r.name,numeric);}
    }
    else m_pValues->SetString(r.name,value);
   }
  }
 }
 void Save( KeyValues *saved )
 {
  Capture();
  FOR_EACH_VALUE(m_pValues, value) saved->SetString(value->GetName(),value->GetString());
 }
 void SetSinglePlayer( bool single )
 {
  if ( !m_bConfigured || (m_bSingle==single && m_Rows.Count()) ) return;
  Capture();
  for ( int i=0;i<m_Rows.Count();++i ) m_Rows[i].panel->SetVisible(false);
  DeleteAllItems();m_Rows.RemoveAll();m_bSingle=single;
  KeyValues *settings=m_pDefinition->FindKey("settings");
  if ( !settings ) return;
  FOR_EACH_SUBKEY(settings,def)
  {
   if ( single && !def->GetBool("singleplayer") ) continue;
   const char *name=def->GetString("name");if(!name[0])continue;
   char token[128];Q_snprintf(token,sizeof(token),"#%s",def->GetString("text",name));
   Row row;row.name=name;row.def=def->GetString("default");row.check=NULL;row.entry=NULL;
   row.numeric=!Q_stricmp(def->GetString("type"),"Numeric");
   if ( !Q_stricmp(def->GetString("type"),"CheckBox") )
   {
    row.check=new CheckButton(this,"LaunchCheck",token);
    row.check->SetSelected(m_pValues->GetInt(name)!=0);row.panel=row.check;row.panel->SetTall(26);
   }
   else
   {
    CSandboxLimitRow *field=new CSandboxLimitRow(this,token);
    row.entry=field->GetEntry();row.entry->SetAllowNumericInputOnly(row.numeric);
    row.entry->SetMaximumCharCount(row.numeric?12:128);
    row.entry->SetText(m_pValues->GetString(name));row.panel=field;
   }
   char helpToken[160];Q_snprintf(helpToken,sizeof(helpToken),"#%s.help",name);
   row.panel->GetTooltip()->SetText(g_pVGuiLocalize->Find(helpToken)?helpToken:def->GetString("help"));
   if(row.entry)row.entry->GetTooltip()->SetText(def->GetString("help"));
   AddItem(NULL,row.panel);m_Rows.AddToTail(row);
  }
  MoveScrollBarToTop();InvalidateLayout();
 }
private:
 struct Row { const char *name;const char *def;Panel *panel;TextEntry *entry;CheckButton *check;bool numeric; };
 KeyValues *m_pDefinition,*m_pValues;
 CUtlVector<Row> m_Rows;
 bool m_bSingle,m_bConfigured;
};

//-----------------------------------------------------------------------------
// Purpose: A single selectable map card (thumbnail + name)
//-----------------------------------------------------------------------------
class CMapCardPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMapCardPanel, vgui::EditablePanel );
public:
	CMapCardPanel( PanelListPanel *parent, const char *name, CCreateMultiplayerGameDialog *pOwner, const char *pszMapName )
		: BaseClass( parent, name )
	{
		m_pOwner = pOwner;
		Q_strncpy( m_szMapName, pszMapName, sizeof( m_szMapName ) );

		m_pThumb = new CPNGImagePanel( this, "MapThumb" );
		m_pThumb->SetMouseInputEnabled( false );
		m_pThumb->SetMapImage( pszMapName );

		m_pName = new Label( this, "MapName", pszMapName );
		m_pName->SetMouseInputEnabled( false );

		// transparent panel that fills the card and forwards clicks to us
		m_pClickCatcher = new CMouseMessageForwardingPanel( this, NULL );
		m_pClickCatcher->SetZPos( 2 );

		// Square-ish card to match the 128x128 map thumbnails (image + name bar)
		SetSize( 150, 178 );
		SetPaintBackgroundEnabled( true );
		m_bSelected = false;
	}

	~CMapCardPanel() {}

	const char *GetMapName() const { return m_szMapName; }

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		if ( m_pName )
		{
			m_pName->SetContentAlignment( Label::a_center );
			m_pName->SetTextInset( 0, 0 );
		}
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		int w, h;
		GetSize( w, h );

		// Square thumbnail area (matches the 128x128 source), name bar at bottom
		int nameBarH = 24;
		int pad = 6;

		if ( m_pThumb )
		{
			// Keep the thumb square, centered horizontally, filling the height above the name bar
			int thumbArea = h - nameBarH - pad;
			int size = w - pad * 2;
			if ( size > thumbArea )
				size = thumbArea;
			int x = (w - size) / 2;
			m_pThumb->SetBounds( x, pad, size, size );
		}
		if ( m_pName )
		{
			m_pName->SetBounds( pad, h - nameBarH - 2, w - pad * 2, nameBarH - 2 );
		}
	}

	// Forwarded from the click-catcher panel (CMouseMessageForwardingPanel ->
	// CallParentFunction("MousePressed")).
	virtual void OnMousePressed( vgui::MouseCode code )
	{
		BaseClass::OnMousePressed( code );
		if ( m_pOwner )
		{
			m_pOwner->OnMapSelected( m_szMapName );
		}
	}

	virtual void PaintBackground()
	{
		// selected highlight
		if ( m_bSelected )
		{
			vgui::surface()->DrawSetColor( 255, 200, 0, 90 );
			vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
		}
		else
		{
			vgui::surface()->DrawSetColor( 0, 0, 0, 120 );
			vgui::surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
		}
	}

	void SetSelected( bool b ) { m_bSelected = b; }

private:
	CPNGImagePanel *m_pThumb;
	Label *m_pName;
	CMouseMessageForwardingPanel *m_pClickCatcher;
	CCreateMultiplayerGameDialog *m_pOwner;
	char m_szMapName[256];
	bool m_bSelected;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameDialog::CCreateMultiplayerGameDialog(vgui::Panel *parent) : BaseClass(parent, "CreateMultiplayerGameDialog")
{
	m_bBotsEnabled = false;
	m_bBuilt = false;
	m_iSelectedCategory = -1;
	m_pSavedData = NULL;
	m_szSelectedMap[0] = 0;

	// Zero every child pointer up front. The children are only created inside
	// ApplySchemeSettings() (guarded by m_bBuilt), but OnMapSelected() ->
	// RefreshSelection() can run from that same path, and any reentrant or
	// early call would otherwise read an uninitialised garbage pointer
	// (observed: 0xffeeffee -> access violation in m_pSelectedMapLabel->SetText).
	m_pGameModeList = NULL;
	m_pMapList = NULL;
	m_pHostName = NULL;
	m_pPassword = NULL;
	m_pMaxPlayers = NULL;
	m_pSelectedMapLabel = NULL;
	m_pStartButton = NULL;
	m_pBackButton = NULL;
	m_pTitleLabel = NULL;
	m_pHostNameLabel = NULL;
	m_pPasswordLabel = NULL;
	m_pMaxPlayersLabel = NULL;
	m_pFallDamageCheck = NULL;
	m_pMaxAmmoLabel = NULL;
	m_pMaxAmmo = NULL;
	m_pLoadoutCheck = NULL;
	m_pGodModeCheck = NULL;
	m_pSuitCheck = NULL;
	m_pLanCheck = NULL;
	m_pPvPCheck = NULL;
	m_pPhysgunLimitedCheck = NULL;
	m_pSandboxHeader = NULL;
	m_pSandboxList = NULL;

	SetDeleteSelfOnClose(true);

	// Content & chrome are built in ApplySchemeSettings() (first call), which
	// runs after the Frame is fully constructed. Building children or invoking
	// Frame chome mutators here crashes the Frame's half-built internals.
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameDialog::~CCreateMultiplayerGameDialog()
{
	if ( m_pSavedData )
	{
		m_pSavedData->deleteThis();
		m_pSavedData = NULL;
	}

	for ( int i = 0; i < m_MapNames.Count(); ++i )
	{
		delete[] m_MapNames[i];
	}
	m_MapNames.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Layout everything
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	int nScreenW, nScreenH;
	vgui::surface()->GetScreenSize( nScreenW, nScreenH );
	int sw = nScreenW;
	int sh = nScreenH;

	// left column (map categories)
	int leftX = (int)(sw * 0.02);
	int leftW = (int)(sw * 0.14);
	int topY = (int)(sh * 0.16);
	int bottomY = (int)(sh * 0.92);

	if ( m_pGameModeList )
		m_pGameModeList->SetBounds( leftX, topY, leftW, bottomY - topY );

	for(int i=0;i<m_CategoryButtons.Count();++i)
		if(m_CategoryButtons[i])m_CategoryButtons[i]->SetSize(leftW-24,28);

	// center map grid
	int mapX = (int)(sw * 0.18);
	int mapW = (int)(sw * 0.52);
	if ( m_pMapList )
		m_pMapList->SetBounds( mapX, topY, mapW, bottomY - topY );

	// right settings panel
	int rightX = (int)(sw * 0.72);
	int rightW = sw - rightX - (int)(sw * 0.02);

	if ( m_pTitleLabel )
		m_pTitleLabel->SetBounds( (int)(sw * 0.18), (int)(sh * 0.05), (int)(sw * 0.5), 40 );

	if ( m_pSelectedMapLabel )
		m_pSelectedMapLabel->SetBounds( rightX, topY, rightW, 24 );

	if ( m_pHostNameLabel )
		m_pHostNameLabel->SetBounds( rightX, topY + 34, rightW, 20 );
	if ( m_pHostName )
		m_pHostName->SetBounds( rightX, topY + 54, rightW, 26 );

	if ( m_pPasswordLabel )
		m_pPasswordLabel->SetBounds( rightX, topY + 90, rightW, 20 );
	if ( m_pPassword )
		m_pPassword->SetBounds( rightX, topY + 110, rightW, 26 );

	if ( m_pMaxPlayersLabel )
		m_pMaxPlayersLabel->SetBounds( rightX, topY + 146, rightW, 20 );
	if ( m_pMaxPlayers )
		m_pMaxPlayers->SetBounds( rightX, topY + 166, rightW, 26 );

	// HL2SB: the sidebar has two shapes (GMod-style).  The single player row
	// keeps the small column; every multiplayer row swaps in the sandbox
	// options and the scrollable limits list.  UpdateSidebarMode() owns the
	// visibility flip; this only positions whichever shape is active.
	bool bSinglePlayerSidebar = ( GetMaxPlayers() <= 1 );
	int btnHLayout = (int)(sh * 0.05);
	int btnYLayout = sh - btnHLayout - (int)(sh * 0.02);

	if ( m_pLanCheck ) m_pLanCheck->SetBounds(rightX,topY+200,rightW,24);
	if ( m_pSandboxList )
	{
		int listY=topY+(bSinglePlayerSidebar?200:232);
		m_pSandboxList->SetBounds(rightX,listY,rightW,MAX(60,btnYLayout-8-listY));
		for(int id=m_pSandboxList->FirstItem();id!=m_pSandboxList->InvalidItemID();id=m_pSandboxList->NextItem(id))
			m_pSandboxList->GetItemPanel(id)->SetWide(rightW-20);
	}

	int btnH = btnHLayout;
	int btnY = btnYLayout;

	if ( m_pStartButton )
	{
		m_pStartButton->SetBounds( rightX, btnY, (int)(rightW * 0.55), btnH );
		const wchar_t *pwsz = g_pVGuiLocalize->Find( "#GameUI_StartGame" );
		m_pStartButton->SetText( pwsz ? pwsz : L"Start Game" );
	}

	if ( m_pBackButton )
	{
		m_pBackButton->SetBounds( (int)(sw * 0.02), btnY, (int)(rightW * 0.3), btnH );
		const wchar_t *pwsz = g_pVGuiLocalize->Find( "#GameUI_Back" );
		m_pBackButton->SetText( pwsz ? pwsz : L"Back" );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( !m_bBuilt )
	{
		m_bBuilt = true;

		SetTitle("#GameUI_CreateServer", false);

		if ( ModInfo().UseBots() )
		{
			m_bBotsEnabled = true;
		}

		m_pGameModeList = new vgui::PanelListPanel( this, "GameModeList" );
		m_pGameModeList->SetFirstColumnWidth( 0 );
		// replaced by directly-owned category buttons; keep the empty list
		// panel from eating clicks behind them.
		m_pGameModeList->SetVisible( true );

		m_pMapList = new vgui::PanelListPanel( this, "MapList" );
		m_pMapList->SetFirstColumnWidth( 0 );

		m_pSelectedMapLabel = new Label( this, "SelectedMapLabel", "" );
		m_pHostName = new TextEntry( this, "HostName" );
		m_pPassword = new TextEntry( this, "Password" );
		m_pMaxPlayers = new ComboBox( this, "MaxPlayers", 8, false );
		// HL2SB: receive the combo's stock "TextChanged" action signal (row
		// picks re-fire it through OnMenuItemSelected -> SetText).
		m_pMaxPlayers->AddActionSignalTarget( this );

		m_pTitleLabel = new Label( this, "TitleLabel", "#GameUI_CreateServer" );
		m_pHostNameLabel = new Label( this, "HostNameLabel", "#GameUI_ServerName" );
		m_pPasswordLabel = new Label( this, "PasswordLabel", "#GameUI_Password" );
		m_pMaxPlayersLabel = new Label( this, "MaxPlayersLabel", "#GameUI_MaxPlayers" );

		m_pLanCheck = new CheckButton(this,"LanCheck","#HL2SB_LAN");
		m_pLanCheck->SetSelected(true);
		m_pSandboxList = new CLaunchSettingsList( this );
		m_pSandboxList->SetFirstColumnWidth( 0 );


		m_pStartButton = new Button( this, "StartButton", "#GameUI_Start" );
		m_pStartButton->SetCommand( "CreateGame" );
		m_pStartButton->SetVisible( true );

		m_pBackButton = new Button( this, "BackButton", "#GameUI_Back" );
		m_pBackButton->SetCommand( "Close" );
		m_pBackButton->SetVisible( true );

		// HL2SB: start at 1 (real single player - game.SinglePlayer() is
		// maxclients<=1) and stop at 128, the limit GetPlayerLimits serves
		// (reference behavior: GMod's menu offers 1..128).  The 1 entry reads
		// "single player" and is the default selection.
		for ( int i = 1; i <= 128; i *= 2 )
		{
			if ( i == 1 )
			{
				const wchar_t *pwsz = g_pVGuiLocalize->Find( "#HL2SB_SinglePlayer" );
				m_pMaxPlayers->AddItem( pwsz ? pwsz : L"Single Player",
					new KeyValues( "maxplayers", "val", i ) );
				continue;
			}
			char sz[16];
			Q_snprintf( sz, sizeof( sz ), "%d", i );
			m_pMaxPlayers->AddItem( sz, new KeyValues( "maxplayers", "val", i ) );
		}
		m_pMaxPlayers->ActivateItemByRow( 0 );

		m_pSavedData = new KeyValues( "ServerConfig" );
		if ( m_pSavedData )
		{
			m_pSavedData->LoadFromFile( g_pFullFileSystem, "ServerConfig.vdf", "GAME" );
		}

		if(m_pSavedData->FindKey("sv_lan"))m_pLanCheck->SetSelected(m_pSavedData->GetInt("sv_lan",1)!=0);
		// HL2SB: restore the saved player count; the default is the single
		// player row.  The single player row hides server name / password / LAN
		// on purpose - they reappear when a multiplayer row is picked.
		{
			int nSaved = m_pSavedData->GetInt( "maxplayers", 1 );
			int nRow = 0;	// row 0 == single player (1); rows double: 2,4,8,16,32,64,128
			while ( nRow < 7 && ( 1 << ( nRow + 1 ) ) <= nSaved )
				nRow++;
			m_pMaxPlayers->ActivateItemByRow( nRow );
		}
		static_cast<CLaunchSettingsList *>(m_pSandboxList)->Configure(m_pSavedData);

		UpdateSidebarMode();

		BuildGameModeList();
		BuildMapGrid();

		if ( m_pHostName )
			m_pHostName->SetText( m_pSavedData->GetString("hostname",ModInfo().GetGameName()) );
		if ( m_pPassword )
			m_pPassword->SetText( m_pSavedData->GetString("sv_password","") );
		if ( m_pHostName )
			m_pHostName->SetMultiline( false );
		if ( m_pPassword )
			m_pPassword->SetMultiline( false );
	}

	int nScreenW, nScreenH;
	vgui::surface()->GetScreenSize( nScreenW, nScreenH );
	SetSize( nScreenW, nScreenH );
	SetPos( 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Build the game mode / category list (left). Each entry is a Button
//          owned by the dialog so clicking it filters the map grid.
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::BuildGameModeList()
{
	// Remove any existing category buttons
	for ( int i = 0; i < m_CategoryButtons.Count(); ++i )
	{
		if ( m_CategoryButtons[i] )
			m_CategoryButtons[i]->MarkForDeletion();
	}
	m_CategoryButtons.RemoveAll();
	if(m_pGameModeList)m_pGameModeList->RemoveAll();

	std::string previous=(s_MapCategories.empty()||m_iSelectedCategory<0)?"Sandbox":GetCategoryName(m_iSelectedCategory);
	LoadNativeMapPatterns();s_MapCategories.clear();
	FileFindHandle_t handle;
	const char *file=g_pFullFileSystem->FindFirstEx("maps/*.bsp","GAME",&handle);
	while(file)
	{
		char name[256];Q_StripExtension(file,name,sizeof(name));Q_strlower(name);
		if(IsMenuMap(name))
		{
			std::string category=NativeMapCategory(name);
			if(std::find(s_MapCategories.begin(),s_MapCategories.end(),category)==s_MapCategories.end())s_MapCategories.push_back(category);
		}
		file=g_pFullFileSystem->FindNext(handle);
	}
	g_pFullFileSystem->FindClose(handle);
	std::sort(s_MapCategories.begin(),s_MapCategories.end());
	m_iSelectedCategory=0;
	for(size_t i=0;i<s_MapCategories.size();++i)if(s_MapCategories[i]==previous)m_iSelectedCategory=(int)i;
	for ( int i = 0; i < (int)s_MapCategories.size(); ++i )
	{
		Button *pBtn = new Button( m_pGameModeList, "GameModeLabel", GetCategoryName( i ) );
		pBtn->SetContentAlignment( Label::a_west );
		pBtn->SetTextInset( 8, 0 );
		pBtn->SetTall( 28 );

		char szCmd[32];
		Q_snprintf( szCmd, sizeof( szCmd ), "MapCat %d", i );
		pBtn->SetCommand( szCmd );
		pBtn->AddActionSignalTarget(this);
		m_pGameModeList->AddItem(NULL,pBtn);

		// Highlight the active category WITHOUT using SetSelected(): the
		// game scheme's ButtonSelected text colour matches the hot list
		// background, so a selected button's label vanishes. Instead paint
		// an explicit background + keep a bright label visible.
		if ( i == m_iSelectedCategory )
		{
			pBtn->SetBgColor( Color( 255, 176, 32, 255 ) ); // GMod-tan
			pBtn->SetFgColor( Color( 20, 20, 20, 255 ) );
		}
		else
		{
			pBtn->SetBgColor( Color( 40, 42, 46, 255 ) );
			pBtn->SetFgColor( Color( 200, 200, 200, 255 ) );
		}
		pBtn->SetPaintBackgroundEnabled( true );
		pBtn->SetVisible( true );

		m_CategoryButtons.AddToTail( pBtn );
	}

	// relayout the buttons
	PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Human-readable name for a map category
//-----------------------------------------------------------------------------
const char *CCreateMultiplayerGameDialog::GetCategoryName( int category )
{
 return category>=0 && category<(int)s_MapCategories.size()?s_MapCategories[category].c_str():"Other";
}

int CCreateMultiplayerGameDialog::MapNameToCategory(const char *name)
{
 std::string category=NativeMapCategory(name);
 for(size_t i=0;i<s_MapCategories.size();++i)if(s_MapCategories[i]==category)return (int)i;
 return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Rebuild the map grid honouring the currently selected category.
//          When a category is active, only maps belonging to it are shown.
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::ApplyCategoryFilter()
{
	if ( !m_pMapList )
		return;

	m_pMapList->DeleteAllItems();
	for(int i=0;i<m_MapNames.Count();++i)delete[] m_MapNames[i];
	m_MapNames.RemoveAll();

	FileFindHandle_t findHandle = NULL;
	const char *pszFilename = g_pFullFileSystem->FindFirstEx( "maps/*.bsp", "GAME", &findHandle );
	while ( pszFilename )
	{
		char mapname[256];
		Q_strncpy( mapname, pszFilename, sizeof( mapname ) - 1 );
		mapname[ sizeof(mapname) - 1 ] = 0;
		char *ext = Q_strstr( mapname, ".bsp" );
		if ( ext )
			*ext = 0;

		Q_strlower(mapname);
		if(!IsMenuMap(mapname)||MapNameToCategory(mapname)!=m_iSelectedCategory)goto nextFile;

		// dedup
		{
			bool bDup = false;
			for ( int i = 0; i < m_MapNames.Count(); ++i )
			{
				if ( !Q_stricmp( m_MapNames[i], mapname ) )
					{ bDup = true; break; }
			}
			if ( !bDup )
			{

				char *pszCopy = new char[ strlen(mapname) + 1 ];
				Q_strcpy( pszCopy, mapname );
				m_MapNames.AddToTail( pszCopy );
			}
		}

	nextFile:
		pszFilename = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );

	if(m_MapNames.Count()>1)std::sort(m_MapNames.Base(),m_MapNames.Base()+m_MapNames.Count(),[](const char *a,const char *b){return Q_stricmp(a,b)<0;});
	bool selectedExists=false;
	for(int i=0;i<m_MapNames.Count();++i)
	{
		m_pMapList->AddItem(NULL,new CMapCardPanel(m_pMapList,"MapCard",this,m_MapNames[i]));
		if(!Q_stricmp(m_szSelectedMap,m_MapNames[i]))selectedExists=true;
	}
	if(!selectedExists)m_szSelectedMap[0]=0;
	m_pMapList->SetNumColumns( 3 );
	RefreshSelection();
}

//-----------------------------------------------------------------------------
// Purpose: Build the map grid
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::BuildMapGrid()
{
	if ( !m_pMapList )
		return;

	// Builds the grid honouring the currently selected category.
	ApplyCategoryFilter();
}

//-----------------------------------------------------------------------------
// Purpose: Loads the list of available maps into the map list
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Purpose: Scan one search path for maps and add them to the grid (deduped)
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::LoadMaps( const char *pszPathID )
{
	FileFindHandle_t findHandle = NULL;

	KeyValues *hiddenMaps = ModInfo().GetHiddenMaps();

	const char *pszFilename = g_pFullFileSystem->FindFirstEx( "maps/*.bsp", pszPathID, &findHandle );
	while ( pszFilename )
	{
		char mapname[256];
		char *ext;

		Q_strncpy( mapname, pszFilename, sizeof( mapname ) - 1 );
		mapname[ sizeof(mapname) - 1 ] = 0;

		ext = Q_strstr( mapname, ".bsp" );
		if ( ext )
		{
			*ext = 0;
		}

		// skip hidden maps
		if ( hiddenMaps )
		{
			if ( hiddenMaps->GetInt( mapname, 0 ) )
			{
				goto nextFile;
			}
		}

		// skip duplicates
		{
			bool bDup = false;
			for ( int i = 0; i < m_MapNames.Count(); ++i )
			{
				if ( !Q_stricmp( m_MapNames[i], mapname ) )
				{
					bDup = true;
					break;
				}
			}
			if ( !bDup )
			{
				// add a card to the grid
				CMapCardPanel *pCard = new CMapCardPanel( m_pMapList, "MapCard", this, mapname );
				m_pMapList->AddItem( NULL, pCard );

				// store map name for retrieval
				char *pszCopy = new char[ strlen(mapname) + 1 ];
				Q_strcpy( pszCopy, mapname );
				m_MapNames.AddToTail( pszCopy );
			}
		}

	nextFile:
		pszFilename = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Load all available maps into the grid
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::LoadMapList()
{
	// GAME search path covers the mod dir + mounted custom dirs
	LoadMaps( "GAME" );

	// fall back to MOD in case the mod dir doesn't resolve through GAME
	if ( m_MapNames.Count() == 0 )
	{
		LoadMaps( "MOD" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when a map card is clicked
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnMapSelected( const char *pszMapName )
{
	Q_strncpy( m_szSelectedMap, pszMapName, sizeof( m_szSelectedMap ) );

	// highlight the matching card
	RefreshSelection();

	// reflect the selection on the label
	if ( m_pSelectedMapLabel )
	{
		char szLabel[512];
		Q_snprintf( szLabel, sizeof( szLabel ), "Map: %s", m_szSelectedMap );
		m_pSelectedMapLabel->SetText( szLabel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: highlight the selected map card
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::RefreshSelection()
{
	if ( !m_pMapList )
		return;

	// OnMapSelected() calls back into RefreshSelection(); the default-selection
	// block below calls OnMapSelected(). Guard against re-entering here (e.g. an
	// empty default map name would otherwise loop forever).
	if ( m_bInRefreshSelection )
		return;
	m_bInRefreshSelection = true;

	for ( int nItemID = m_pMapList->FirstItem(); nItemID != m_pMapList->InvalidItemID(); nItemID = m_pMapList->NextItem( nItemID ) )
	{
		CMapCardPanel *pCard = dynamic_cast< CMapCardPanel * >( m_pMapList->GetItemPanel( nItemID ) );
		if ( pCard )
		{
			bool bSelected = ( m_szSelectedMap[0] != 0 && !Q_stricmp( pCard->GetMapName(), m_szSelectedMap ) );
			pCard->SetSelected( bSelected );
		}
	}

	// default selection
	if ( m_szSelectedMap[0] == 0 )
	{
		const char *startMap = m_pSavedData ? m_pSavedData->GetString("map", "") : "";
		bool inCategory=false;
		for(int i=0;i<m_MapNames.Count();++i)if(!Q_stricmp(startMap,m_MapNames[i]))inCategory=true;
		if ( startMap[0] && inCategory )
		{
			OnMapSelected( startMap );
		}
		else if ( m_MapNames.Count() )
		{
			OnMapSelected( m_MapNames[0] );
		}
	}

	m_bInRefreshSelection = false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if randomly selected map
//-----------------------------------------------------------------------------
bool CCreateMultiplayerGameDialog::IsRandomMapSelected()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns currently selected map
//-----------------------------------------------------------------------------
const char *CCreateMultiplayerGameDialog::GetMapName()
{
	if ( m_szSelectedMap[0] == 0 && m_MapNames.Count() )
		return m_MapNames[0];
	return m_szSelectedMap;
}

//-----------------------------------------------------------------------------
// Purpose: getters for server settings
//-----------------------------------------------------------------------------
const char *CCreateMultiplayerGameDialog::GetHostName()
{
	static char szValue[256];
	if ( m_pHostName )
		m_pHostName->GetText( szValue, sizeof( szValue ) );
	else
		szValue[0] = 0;
	return szValue;
}

const char *CCreateMultiplayerGameDialog::GetPassword()
{
	static char szValue[256];
	if ( m_pPassword )
		m_pPassword->GetText( szValue, sizeof( szValue ) );
	else
		szValue[0] = 0;
	return szValue;
}

int CCreateMultiplayerGameDialog::GetMaxPlayers()
{
	if ( m_pMaxPlayers )
	{
		KeyValues *kv = m_pMaxPlayers->GetActiveItemUserData();
		if ( kv )
			return kv->GetInt( "val", MAX_PLAYERS_DEFAULT );
	}
	return MAX_PLAYERS_DEFAULT;
}

//-----------------------------------------------------------------------------
// Purpose: HL2SB - the max-players combo posted its stock "TextChanged"
//          action signal.  Swapping between the single player row and a
//          multiplayer row swaps the whole sidebar shape, GMod-style.
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnMaxPlayersChanged( void )
{
	UpdateSidebarMode();
}

//-----------------------------------------------------------------------------
// Purpose: HL2SB - flip the sidebar between the singleplayer column and the
//          multiplayer sandbox column whenever the player-count combo moves.
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::UpdateSidebarMode()
{
	bool bSingle = ( GetMaxPlayers() <= 1 );

	Panel *legacy[] = {m_pFallDamageCheck,m_pMaxAmmoLabel,m_pMaxAmmo,m_pLoadoutCheck,m_pGodModeCheck,m_pSuitCheck,m_pPvPCheck,m_pPhysgunLimitedCheck,m_pSandboxHeader};
	for(int i=0;i<ARRAYSIZE(legacy);++i)if(legacy[i])legacy[i]->SetVisible(false);
	if(m_pHostName)m_pHostName->SetVisible(!bSingle);
	if(m_pHostNameLabel)m_pHostNameLabel->SetVisible(!bSingle);
	if(m_pPassword)m_pPassword->SetVisible(!bSingle);
	if(m_pPasswordLabel)m_pPasswordLabel->SetVisible(!bSingle);
	if(m_pLanCheck)m_pLanCheck->SetVisible(!bSingle);
	if(m_pSandboxList)
	{
		m_pSandboxList->SetVisible(true);
		static_cast<CLaunchSettingsList *>(m_pSandboxList)->SetSinglePlayer(bSingle);
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: current mp_falldamage value.  The cvar is registered by the server
// DLL, which is not loaded while the main menu is up, so this normally falls
// back to the sandbox default (off).
//-----------------------------------------------------------------------------
bool CCreateMultiplayerGameDialog::GetFallDamage()
{
	ConVarRef falldamage( "mp_falldamage", true );
	if ( falldamage.IsValid() )
		return ( falldamage.GetInt() != 0 );

	// Not visible from the main menu (the server DLL is not loaded), so fall
	// back to the player's last choice; 0 = the sandbox default.
	if ( m_pSavedData )
		return ( m_pSavedData->GetInt( "mp_falldamage", 0 ) != 0 );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: HL2SB sidebar getters (2026-09-28).  ServerConfig.vdf - the dialog's
// own persisted state - wins; the live convar is only a fallback for a key the
// file does not carry yet.
//
// The first version was the other way round (convar first) and that is the
// reported "ammo field is 0 again on the next open": gmod_maxammo/gmod_suit
// live in the game DLLs, and once a game has run this process keeps client.dll
// loaded, so ConVarRef succeeds with the REGISTERED default "0" and buries the
// saved 9999 on every later open (the HEV checkbox had the same trap).  The
// sbox_* pair comes from sandbox's init.lua and usually cannot exist at the
// menu, but they get the same order for uniformity.
//-----------------------------------------------------------------------------
int CCreateMultiplayerGameDialog::GetMaxAmmo()
{
	if ( m_pSavedData && m_pSavedData->FindKey( "gmod_maxammo" ) )
		return m_pSavedData->GetInt( "gmod_maxammo", 9999 );

	ConVarRef maxammo( "gmod_maxammo", true );
	if ( maxammo.IsValid() && maxammo.GetInt() > 0 )
		return maxammo.GetInt();

	// GMod's start-menu field default (its HTML menu's own default table).
	// The registered convar default is "0", which means "keep the HL2 per-type
	// caps" - not what the field should display.
	return 9999;
}

bool CCreateMultiplayerGameDialog::GetLoadout()
{
	if ( m_pSavedData && m_pSavedData->FindKey( "sbox_weapons" ) )
		return ( m_pSavedData->GetInt( "sbox_weapons", 1 ) != 0 );

	ConVarRef loadout( "sbox_weapons", true );
	if ( loadout.IsValid() )
		return ( loadout.GetInt() != 0 );

	return true;
}

bool CCreateMultiplayerGameDialog::GetGodMode()
{
	if ( m_pSavedData && m_pSavedData->FindKey( "sbox_godmode" ) )
		return ( m_pSavedData->GetInt( "sbox_godmode", 0 ) != 0 );

	ConVarRef godmode( "sbox_godmode", true );
	if ( godmode.IsValid() )
		return ( godmode.GetInt() != 0 );

	return false;
}

bool CCreateMultiplayerGameDialog::GetSuit()
{
	if ( m_pSavedData && m_pSavedData->FindKey( "gmod_suit" ) )
		return ( m_pSavedData->GetInt( "gmod_suit", 0 ) != 0 );

	ConVarRef suit( "gmod_suit", true );
	if ( suit.IsValid() )
		return ( suit.GetInt() != 0 );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: writes cfg/listenserver.cfg, which the engine execs at
// SV_ActivateServer.  That is the first moment a cvar owned by the server DLL
// (mp_falldamage) exists, so it cannot be put on the command line before the
// "map" command.  Anything the player already has in the file is preserved;
// only the lines this function owns are replaced.
//-----------------------------------------------------------------------------
static const char *kListenServerHeader[] =
{
	"// Generated by the HL2SB create-server dialog.",
	"// The engine execs this at SV_ActivateServer.",
};

// HL2SB: every line this dialog owns.  Carried-over copies from previous runs
// are dropped by prefix so each option is written exactly once per start.
static const char *kListenServerOwnedLines[] =
{
	"mp_falldamage",
	"gmod_maxammo",
	"gmod_suit",
};

void CCreateMultiplayerGameDialog::WriteListenServerConfig( bool bFallDamage, int nMaxAmmo, bool bSuit )
{
	CUtlBuffer existing( 0, 0, CUtlBuffer::TEXT_BUFFER );
	g_pFullFileSystem->ReadFile( "cfg/listenserver.cfg", "MOD", existing );

	CUtlBuffer out( 0, 0, CUtlBuffer::TEXT_BUFFER );
	for ( int i = 0; i < ARRAYSIZE( kListenServerHeader ); i++ )
	{
		out.PutString( kListenServerHeader[i] );
		out.PutChar( '\n' );
	}

	char szLine[64];
	Q_snprintf( szLine, sizeof( szLine ), "mp_falldamage %d\n", bFallDamage ? 1 : 0 );
	out.PutString( szLine );
	Q_snprintf( szLine, sizeof( szLine ), "gmod_maxammo %d\n", nMaxAmmo );
	out.PutString( szLine );
	Q_snprintf( szLine, sizeof( szLine ), "gmod_suit %d\n", bSuit ? 1 : 0 );
	out.PutString( szLine );

	if ( existing.TellMaxPut() > 0 )
	{
		const char *pCur = (const char *)existing.Base();
		const char *pEnd = pCur + existing.TellMaxPut();

		while ( pCur < pEnd )
		{
			const char *pLine = pCur;
			while ( pCur < pEnd && *pCur != '\n' )
				++pCur;

			int nLen = (int)( pCur - pLine );
			if ( pCur < pEnd )
				++pCur;	// consume the newline

			// Skip leading whitespace before classifying the line.
			const char *pTrim = pLine;
			const char *pTrimEnd = pLine + nLen;
			while ( pTrim < pTrimEnd && ( *pTrim == ' ' || *pTrim == '\t' ) )
				++pTrim;
			while ( pTrimEnd > pTrim && ( pTrimEnd[-1] == '\r' || pTrimEnd[-1] == ' ' ) )
				--pTrimEnd;

			int nTrimLen = (int)( pTrimEnd - pTrim );

			// Ours are already written above; never carry a previous copy over,
			// otherwise every run would stack another header block.
			bool bOurs = false;
			for ( int i = 0; i < ARRAYSIZE( kListenServerOwnedLines ); i++ )
			{
				size_t nPrefix = Q_strlen( kListenServerOwnedLines[i] );
				if ( (size_t)nTrimLen >= nPrefix &&
					 !Q_strnicmp( pTrim, kListenServerOwnedLines[i], nPrefix ) &&
					 ( (size_t)nTrimLen==nPrefix || pTrim[nPrefix]==' ' || pTrim[nPrefix]=='\t' ) )
				{
					bOurs = true;
					break;
				}
			}
			if ( bOurs )
				continue;

			for ( int i = 0; i < ARRAYSIZE( kListenServerHeader ); i++ )
			{
				if ( (int)Q_strlen( kListenServerHeader[i] ) == nTrimLen &&
					 !Q_strnicmp( pTrim, kListenServerHeader[i], nTrimLen ) )
				{
					bOurs = true;
					break;
				}
			}
			if ( bOurs )
				continue;

			out.Put( pLine, nLen );
			out.PutChar( '\n' );
		}
	}

	g_pFullFileSystem->WriteFile( "cfg/listenserver.cfg", "MOD", out );
}

//-----------------------------------------------------------------------------
// Purpose: Save the config to disk
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::SaveConfig()
{
	if ( m_pSavedData )
	{
		m_pSavedData->SetString( "map", GetMapName() );
		m_pSavedData->SetInt("sv_lan",m_pLanCheck&&m_pLanCheck->IsSelected()?1:0);
		if(m_pSandboxList)static_cast<CLaunchSettingsList *>(m_pSandboxList)->Save(m_pSavedData);
		m_pSavedData->SetInt("maxplayers",GetMaxPlayers());
		m_pSavedData->SetString("hostname",GetHostName());
		m_pSavedData->SetString("sv_password",GetPassword());
		m_pSavedData->SaveToFile( g_pFullFileSystem, "ServerConfig.vdf", "GAME" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: runs the server when the Start button is pressed
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::CreateGame()
{
	// "gamemode" carries FCVAR_REPLICATED, so the RevertFlaggedConVars() calls
	// below would silently reset it to its default (sandbox). The gamemode is
	// whatever the player has set (console / config); the map category must NOT
	// override it, otherwise picking a map from any category silently switches
	// the gamemode. Remember the player's choice first, then hand it back to the
	// server with the map command.
	ConVarRef gamemodeVar( "gamemode", true );
	char szGamemode[32];
	Q_strncpy( szGamemode, gamemodeVar.IsValid() ? gamemodeVar.GetString() : "sandbox", sizeof( szGamemode ) );

	// reset server enforced cvars
	g_pCVar->RevertFlaggedConVars( FCVAR_REPLICATED );
	g_pCVar->RevertFlaggedConVars( FCVAR_CHEAT );

	char szMapName[64]={0}, szHostName[64]={0}, szPassword[64]={0};
	Q_strncpy( szMapName, GetMapName(), sizeof( szMapName ) );
	Q_strncpy( szHostName, GetHostName(), sizeof( szHostName ) );
	Q_strncpy( szPassword, GetPassword(), sizeof( szPassword ) );

	for(int i=0;szHostName[i]||szPassword[i];++i)
	{
		if(szHostName[i]=='"'||szHostName[i]==';'||szHostName[i]=='\n'||szHostName[i]=='\r')szHostName[i]=' ';
		if(szPassword[i]=='"'||szPassword[i]==';'||szPassword[i]=='\n'||szPassword[i]=='\r')szPassword[i]=' ';
	}
	for(const char *c=szMapName;*c;++c)if(!isalnum((unsigned char)*c)&&*c!='_'&&*c!='-'&&*c!='/')return;
	SaveConfig();

	// Native server settings are applied after the server DLL has loaded.
	WriteListenServerConfig( m_pSavedData->GetInt("mp_falldamage",0)!=0,
		m_pSavedData->GetInt("gmod_maxammo",9999),m_pSavedData->GetInt("gmod_suit",0)!=0 );

	// HL2SB: single player always starts as a LAN listen server (GMod does the
	// same); multiplayer rows honour the LAN checkbox, defaulting to on to
	// match the old unconditional "sv_lan 1".
	bool bLan = ( m_pLanCheck && m_pLanCheck->IsSelected() ) || ( GetMaxPlayers() <= 1 );

	char szMapCommand[1024];
	Q_snprintf(szMapCommand, sizeof( szMapCommand ), "disconnect\nwait\nwait\nsv_lan %d\nsetmaster enable\ngamemode \"%s\"\nmaxplayers %i\nsv_password \"%s\"\nhostname \"%s\"\nprogress_enable\nmap %s\n",
		bLan ? 1 : 0,
		szGamemode,
		GetMaxPlayers(),
		szPassword,
		szHostName,
		szMapName
	);

	engine->ClientCmd_Unrestricted(szMapCommand);
}

//-----------------------------------------------------------------------------
// Purpose: command handler
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "CreateGame" ) )
	{
		CreateGame();
		return;
	}
	else if ( !Q_stricmp( command, "Close" ) )
	{
		Close();
		return;
	}
	else if ( !Q_strnicmp( command, "MapCat ", 7 ) )
	{
		int iCat = atoi( command + 7 );
		if ( iCat >= 0 && iCat < (int)s_MapCategories.size() )
		{
			m_iSelectedCategory = iCat;
			// Selection does not require rescanning gamemode definitions or categories.
			for(int i=0;i<m_CategoryButtons.Count();++i)
			{
				m_CategoryButtons[i]->SetBgColor(i==iCat?Color(255,176,32,255):Color(40,42,46,255));
				m_CategoryButtons[i]->SetFgColor(i==iCat?Color(20,20,20,255):Color(200,200,200,255));
			}
			BuildMapGrid();
		}
		return;
	}

	BaseClass::OnCommand( command );
}

void CCreateMultiplayerGameDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	ButtonCode_t nButtonCode = GetBaseButtonCode( code );

	if ( nButtonCode == KEY_XBUTTON_B || nButtonCode == STEAMCONTROLLER_B )
	{
		Close();
		return;
	}
	else if ( nButtonCode == KEY_ENTER || nButtonCode == KEY_XBUTTON_A || nButtonCode == STEAMCONTROLLER_A )
	{
		CreateGame();
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}
