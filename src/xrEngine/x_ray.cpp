//-----------------------------------------------------------------------------
// File: x_ray.cpp
//
// Programmers:
// Oles - Oles Shishkovtsov
// AlexMX - Alexander Maksimchuk
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "igame_level.h"
#include "igame_persistent.h"

#include "dedicated_server_only.h"
#include "no_single.h"
#include "../xrNetServer/NET_AuthCheck.h"

#include "xr_input.h"
#include "xr_ioconsole.h"
#include "x_ray.h"
#include "std_classes.h"
#include "GameFont.h"
#include "resource.h"
#include "LightAnimLibrary.h"
#include "../xrcdb/ispatial.h"
#include "Text_Console.h"
#include <process.h>
#include <locale.h>

#include <unicode\unistr.h>
#include <unicode\ucnv.h>
#include <discord\discord.h>

#include "xr_ioc_cmd.h"
#include <boost/crc.hpp>
 
// computing build id
XRCORE_API LPCSTR build_date;
XRCORE_API u32 build_id;

//Discord
discord::Core* discord_core{};
discord::Activity discordPresence{};
static int64_t StartTime;
bool use_discord = true;
#pragma comment(lib, "discord_game_sdk.lib")
rpc_info discord_gameinfo;
rpc_strings discord_strings;
float discord_update_rate = .5f;

//UTF-8 (ICU)
#pragma comment(lib, "icuuc.lib")
static LPSTR month_id[12] =
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int days_in_month[12] =
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int start_day = 31; // 31
static int start_month = 1; // January
static int start_year = 1999; // 1999

// binary hash, mainly for copy-protection 

void compute_build_id()
{
	build_date = __DATE__;

	int days;
	int months = 0;
	int years;
	string16 month;
	string256 buffer;
	xr_strcpy(buffer, __DATE__);
	sscanf(buffer, "%s %d %d", month, &days, &years);

	for (int i = 0; i < 12; i++)
	{
		if (_stricmp(month_id[i], month))
			continue;

		months = i;
		break;
	}

	build_id = (years - start_year) * 365 + days - start_day;

	for (int i = 0; i < months; ++i)
		build_id += days_in_month[i];

	for (int i = 0; i < start_month - 1; ++i)
		build_id -= days_in_month[i];
}


//////////////////////////////////////////////////////////////////////////
// global variables
ENGINE_API CApplication* pApp = NULL;

LPCSTR xr_ToUTF8(LPCSTR input, int max_length)
{
	UErrorCode errorCode = U_ZERO_ERROR;
	UConverter *conv_from = ucnv_open("cp1251", &errorCode);
	R_ASSERT3(conv_from, "[Discord RPC] Error creating UConverter!\n", std::to_string(errorCode).c_str());

	std::vector<UChar> converted(strlen(input) * 2);
	int32_t conv_len = ucnv_toUChars(conv_from, &converted[0], converted.size(), input, strlen(input), &errorCode);
	if (errorCode != U_ZERO_ERROR)
	{
		Msg("[Discord RPC] Failed to convert string! (%s)", std::to_string(errorCode).c_str());
		return input;
	}

	converted.resize(conv_len);
	ucnv_close(conv_from);

	// needs to be static so the data buffer is still valid after this function returns
	static std::string g;
	g.clear();

	g.resize(converted.size() * 4);

	UConverter *conv_u8 = ucnv_open("UTF-8", &errorCode);
	int32_t u8_len = ucnv_fromUChars(conv_u8, &g[0], g.size(), &converted[0], converted.size(), &errorCode);
	if (errorCode != U_ZERO_ERROR)
	{
		Msg("[Discord RPC] Failed to convert string! (%s)", std::to_string(errorCode).c_str());
		return input;
	}

	g.resize(_min(u8_len, max_length));
	ucnv_close(conv_u8);

	return g.data();
}

//Discord Rich Presence - Rezy ------------------------------------------------

void DiscordLog(discord::LogLevel level, std::string message)
{
	Msg("[Discord RPC]: %s", message.c_str());
}

void updateDiscordPresence()
{
	if (!use_discord)
		return;

	static char details_buffer[128];
	static char state_buffer[128];

	// Main Menu
	if (discord_gameinfo.mainmenu)
	{
		snprintf(state_buffer, 128, discord_strings.mainmenu);
		discordPresence.GetAssets().SetLargeImage("gamelogo");
		discordPresence.GetAssets().SetLargeText("");
		discordPresence.GetAssets().SetSmallImage("");
		discordPresence.GetAssets().SetSmallText("");
			
		// Pause Menu
		if (discord_gameinfo.ingame)
			snprintf(state_buffer, 128, discord_strings.paused);
		else
			discordPresence.SetDetails("");
	}	

	// Loading
	else if (discord_gameinfo.loadscreen)
	{
		snprintf(state_buffer, 128, discord_strings.loading);
		discordPresence.SetDetails("");
		discordPresence.GetAssets().SetLargeImage("gamelogo");
		discordPresence.GetAssets().SetLargeText("");
		discordPresence.GetAssets().SetSmallImage("");
		discordPresence.GetAssets().SetSmallText("");
		discord_gameinfo.ex_update = true;
	}

	// In Game
	else if (discord_gameinfo.ingame)
	{
		// Time + Level Name
		char levelname_time[128];
		if (discord_gameinfo.level_name && discord_gameinfo.currenttime)
		{
			snprintf(levelname_time, 128, "%s | %s", discord_gameinfo.level_name, discord_gameinfo.currenttime);
			discordPresence.GetAssets().SetLargeText(levelname_time);
		}
		else if (discord_gameinfo.level_name)
		{
			snprintf(levelname_time, 128, discord_gameinfo.level_name);
			discordPresence.GetAssets().SetLargeText(levelname_time);
		}
		else
			discord_gameinfo.ex_update = true;

		//Faction, Rank, Rep
		if (discord_gameinfo.faction && discord_gameinfo.faction_name)
		{
			discordPresence.GetAssets().SetSmallImage(discord_gameinfo.faction);
			char rank_faction_rep[128];
			if (discord_gameinfo.rank_name && discord_gameinfo.reputation)
				snprintf(rank_faction_rep, 128, "%s | %s", discord_gameinfo.rank_name, discord_gameinfo.reputation);
			else
				snprintf(rank_faction_rep, 128, discord_gameinfo.faction_name);
			discordPresence.GetAssets().SetSmallText(rank_faction_rep);
		}

		// GameMode + Active Task
		if (discord_gameinfo.gamemode)
		{
			if (discord_gameinfo.task_name && 0 != xr_strcmp(discord_gameinfo.task_name, ""))
				snprintf(details_buffer, 128, "%s | %s", discord_gameinfo.gamemode, discord_gameinfo.task_name);
			else
				snprintf(details_buffer, 128, discord_gameinfo.gamemode);
			discordPresence.SetDetails(details_buffer);
		}

		// God Mode
		if (discord_gameinfo.godmode)
			snprintf(state_buffer, 128, discord_strings.godmode);

		// Health
		else if (discord_gameinfo.health)
		{
			// Iron Man
			if (discord_gameinfo.ironman && discord_gameinfo.lives_left)
			{
				if (discord_gameinfo.lives_left == 0 || discord_gameinfo.lives_left > 1)
					snprintf(state_buffer, 128, "%s: %i | %i %s", discord_strings.health, discord_gameinfo.health,
					        discord_gameinfo.lives_left, discord_strings.livesleft);
				else
					snprintf(state_buffer, 128, "%s: %i | %i %s", discord_strings.health, discord_gameinfo.health,
					        discord_gameinfo.lives_left, discord_strings.livesleftsingle);
			}

			// Azazel
			else if (discord_gameinfo.possessed_lives)
			{
				if (discord_gameinfo.possessed_lives == 0 || discord_gameinfo.possessed_lives > 1)
					snprintf(state_buffer, 128, "%s: %i | %i %s", discord_strings.health, discord_gameinfo.health,
					        discord_gameinfo.possessed_lives, discord_strings.livespossessed);
				else
					snprintf(state_buffer, 128, "%s: %i | %i %s", discord_strings.health, discord_gameinfo.health,
					        discord_gameinfo.possessed_lives, discord_strings.livespossessedsingle);
			}

			// No Iron Man or Azazel
			else
				snprintf(state_buffer, 128, "%s: %i", discord_strings.health, discord_gameinfo.health);

			discordPresence.SetState(state_buffer);
		}
		else
		{
			// Iron Man
			if (discord_gameinfo.ironman && discord_gameinfo.lives_left)
			{
				int real_lives = discord_gameinfo.lives_left - 1;
				if (real_lives == 0 || real_lives > 1)
					snprintf(state_buffer, 128, "%s | %i %s", discord_strings.dead, real_lives, discord_strings.livesleft);
				else
					snprintf(state_buffer, 128, "%s | %i %s", discord_strings.dead, real_lives,
						discord_strings.livesleftsingle);
			}


			// Azazel
			else if (discord_gameinfo.possessed_lives)
			{
				if (discord_gameinfo.possessed_lives == 0 || discord_gameinfo.possessed_lives > 1)
					snprintf(state_buffer, 128, "%s | %i %s", discord_strings.dead, discord_gameinfo.possessed_lives,
						discord_strings.livespossessed);
				else
					snprintf(state_buffer, 128, "%s | %i %s", discord_strings.dead, discord_gameinfo.possessed_lives,
						discord_strings.livespossessedsingle);
			}

			// No Iron Man or Azazel
			else
				snprintf(state_buffer, 128, "%s", discord_strings.dead);

			discordPresence.SetState(state_buffer);
		}

		// Level Icon
		if (discord_gameinfo.level && discord_gameinfo.level_icon_index)
		{
			char icon_buffer[32];
			snprintf(icon_buffer, 32, "%s_%i", discord_gameinfo.level, discord_gameinfo.level_icon_index);
			discordPresence.GetAssets().SetLargeImage(icon_buffer);
		}
	}

	discordPresence.SetState(state_buffer);
	discord_core->ActivityManager().UpdateActivity(discordPresence, [](discord::Result result) {});
}

void Init_Discord()
{
	auto result = discord::Core::Create(477910171964801060, DiscordCreateFlags_NoRequireDiscord, &discord_core);
	
	if (result != discord::Result::Ok)
	{
		Msg("[Discord RPC] Failed to create Discord RPC");
		use_discord = false;
		return;
	}

	discord_core->SetLogHook(discord::LogLevel::Error, DiscordLog);
	Msg("[Discord RPC] Created successfully!");

	//Set up basic RPC
	StartTime = time(0);
	discordPresence.SetType(discord::ActivityType::Playing);
	discordPresence.GetTimestamps().SetStart(StartTime);
	discordPresence.GetAssets().SetLargeImage("gamelogo");
	discord_core->ActivityManager().UpdateActivity(discordPresence, [](discord::Result result) {});
}

void clearDiscordPresence()
{
	if (discord_core)
		discord_core->ActivityManager().ClearActivity([](discord::Result result) {});
} 

ENGINE_API bool g_dedicated_server = false;


LPCSTR _GetFontTexName(LPCSTR section)
{
	static char* tex_names[] = {"texture800", "texture", "texture1600", "texture2160"};
	int def_idx = 1; //default 1024x768
	int idx = def_idx;
 
	u32 h = Device.dwHeight;

	if (h <= 600) idx = 0;
	else if (h < 1024) idx = 1;
	else if (h < 1440) idx = 2;
	else idx = 3;
 
	while (idx >= 0)
	{
		if (pSettings->line_exist(section, tex_names[idx]))
			return pSettings->r_string(section, tex_names[idx]);
		--idx;
	}
	return pSettings->r_string(section, tex_names[def_idx]);
}

void _InitializeFont(CGameFont*& F, LPCSTR section, u32 flags)
{
	LPCSTR font_tex_name = _GetFontTexName(section);
	R_ASSERT(font_tex_name);

	LPCSTR sh_name = pSettings->r_string(section, "shader");
	if (!F)
		F = xr_new<CGameFont>(sh_name, font_tex_name, flags);
	else
		F->Initialize(sh_name, font_tex_name);

	if (pSettings->line_exist(section, "size"))
	{
		float sz = pSettings->r_float(section, "size");
		if (flags & CGameFont::fsDeviceIndependent) F->SetHeightI(sz);
		else F->SetHeight(sz);
	}
	if (pSettings->line_exist(section, "interval"))
		F->SetInterval(pSettings->r_fvector2(section, "interval"));
}
 

// Engine Initializers

struct path_excluder_predicate
{
	explicit path_excluder_predicate(xr_auth_strings_t const* ignore) :
		m_ignore(ignore)
	{
	}

	bool xr_stdcall is_allow_include(LPCSTR path)
	{
		if (!m_ignore)
			return true;

		return allow_to_include_path(*m_ignore, path);
	}

	xr_auth_strings_t const* m_ignore;
};


// Initialize

BOOL g_bIntroFinished = FALSE;
extern float g_fTimeFactor;

 
void InitSettings()
{
	string_path fname;
	FS.update_path(fname, "$game_config$", "system.ltx");
	pSettings = xr_new<CInifile>(fname, TRUE);
	CHECK_OR_EXIT(0 != pSettings->section_count(),
		make_string("Cannot find file %s.\nReinstalling application may fix this problem.", fname));

	xr_auth_strings_t tmp_ignore_pathes;
	xr_auth_strings_t tmp_check_pathes;
	fill_auth_check_params(tmp_ignore_pathes, tmp_check_pathes);

	path_excluder_predicate tmp_excluder(&tmp_ignore_pathes);
	CInifile::allow_include_func_t tmp_functor;
	tmp_functor.bind(&tmp_excluder, &path_excluder_predicate::is_allow_include);

	pSettingsAuth = xr_new<CInifile>(
		fname,
		TRUE,
		TRUE,
		FALSE,
		0,
		tmp_functor
	);

	FS.update_path(fname, "$game_config$", "game.ltx");
	pGameIni = xr_new<CInifile>(fname, TRUE);
	CHECK_OR_EXIT(0 != pGameIni->section_count(), make_string("Cannot find file %s.\nReinstalling application may fix this problem.", fname));

	g_fTimeFactor = pSettings->r_float("alife", "time_factor");
}

void InitConsole()
{
	Console = xr_new<CConsole>();
	Console->Initialize();

	xr_strcpy(Console->ConfigFile, "user.ltx");
	if (strstr(Core.Params, "-ltx "))
	{
		string64 c_name;
		sscanf(strstr(Core.Params, "-ltx ") + 5, "%[^ ] ", c_name);
		xr_strcpy(Console->ConfigFile, c_name);
	}
}

void InitInput()
{
	BOOL bCaptureInput = FALSE; // !strstr(Core.Params, "-i");

	pInput = xr_new<CInput>(bCaptureInput);
}

void InitSound1()
{
	CSound_manager_interface::_create(0);
}

void InitSound2()
{
	CSound_manager_interface::_create(1);
}

void execUserScript()
{
	Console->Execute("default_controls");
	Console->ExecuteScript(Console->ConfigFile);
}

// Destroy

void destroyInput()
{
	xr_delete(pInput);
}

void destroySound()
{
	CSound_manager_interface::_destroy();
}

void destroySettings()
{
	auto s = const_cast<CInifile**>(&pSettings);
	xr_delete(*s);
	auto sa = const_cast<CInifile**>(&pSettingsAuth);
	xr_delete(*sa);
	xr_delete(pGameIni);
}

void destroyConsole()
{
	Console->Execute("cfg_save");
	Console->Destroy();
	xr_delete(Console);
}

void destroyEngine()
{
	Device.Destroy();
	Engine.Destroy();
}
