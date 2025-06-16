#include "stdafx.h"
#include "x_ray.h"

#include "std_classes.h"
#include "../xrCDB/ISpatial.h"


#include "XR_IOConsole.h"
#include "xr_ioc_cmd.h"
#include "igame_level.h"
#include "igame_persistent.h"
#include "../xrNetServer/NET_AuthCheck.h"
#include "xr_input.h"
#include "LightAnimLibrary.h"
#include "GameFont.h"

#define XRAY_MONOLITH_VERSION "X-Ray Monolith v1.5.3 se7kills "
extern void _InitializeFont(CGameFont*& F, LPCSTR section, u32 flags);
extern void compute_build_id();
extern void Init_Discord();

static CTimer phase_timer;
extern ENGINE_API BOOL g_appLoaded = FALSE;
extern ENGINE_API BOOL g_bootComplete = FALSE;


extern CRenderDevice Device;

//---------------------------------------------------------------------
// 2446363
// umbt@ukr.net
//////////////////////////////////////////////////////////////////////////
struct _SoundProcessor : public pureFrame
{
	virtual void _BCL OnFrame()
	{
		//Msg ("------------- sound: %d [%3.2f,%3.2f,%3.2f]",u32(Device.dwFrame),VPUSH(Device.vCameraPosition));
		Device.Statistic->Sound.Begin();
		::Sound->update(Device.vCameraPosition, Device.vCameraDirection, Device.vCameraTop);
		Device.Statistic->Sound.End();
	}
} SoundProcessor;

// CApplication Initlizer

CApplication::CApplication()
{
	ll_dwReference = 0;

	max_load_stage = 0;

	// events
	eQuit = Engine.Event.Handler_Attach("KERNEL:quit", this);
	eStart = Engine.Event.Handler_Attach("KERNEL:start", this);
	eStartLoad = Engine.Event.Handler_Attach("KERNEL:load", this);
	eDisconnect = Engine.Event.Handler_Attach("KERNEL:disconnect", this);
	eConsole = Engine.Event.Handler_Attach("KERNEL:console", this);
	eStartMPDemo = Engine.Event.Handler_Attach("KERNEL:start_mp_demo", this);

	// levels
	Level_Current = u32(-1);
	Level_Scan();

	// Font
	pFontSystem = NULL;

	// Register us
	Device.seqFrame.Add(this, REG_PRIORITY_HIGH + 1000);

	if (psDeviceFlags.test(mtSound)) Device.seqFrameMT.Add(&SoundProcessor);
	else Device.seqFrame.Add(&SoundProcessor);

	Console->Show();

	// App Title
	// app_title[ 0 ] = '\0';
	ls_header[0] = '\0';
	ls_tip_number[0] = '\0';
	ls_tip[0] = '\0';
}

CApplication::~CApplication()
{
	Console->Hide();

	// font
	xr_delete(pFontSystem);

	Device.seqFrameMT.Remove(&SoundProcessor);
	Device.seqFrame.Remove(&SoundProcessor);
	Device.seqFrame.Remove(this);


	// events
	Engine.Event.Handler_Detach(eConsole, this);
	Engine.Event.Handler_Detach(eDisconnect, this);
	Engine.Event.Handler_Detach(eStartLoad, this);
	Engine.Event.Handler_Detach(eStart, this);
	Engine.Event.Handler_Detach(eQuit, this);
	Engine.Event.Handler_Detach(eStartMPDemo, this);
}

void CApplication::OnEvent(EVENT E, u64 P1, u64 P2)
{
	if (E == eQuit)
	{
		PostQuitMessage(0);

		for (u32 i = 0; i < Levels.size(); i++)
		{
			xr_free(Levels[i].folder);
			xr_free(Levels[i].name);
		}
	}
	else if (E == eStart)
	{
		LPSTR op_server = LPSTR(P1);
		LPSTR op_client = LPSTR(P2);
		Level_Current = u32(-1);
		R_ASSERT(0 == g_pGameLevel);
		R_ASSERT(0 != g_pGamePersistent);

		{
			Console->Execute("main_menu off");
			Console->Hide();
			//! this line is commented by Dima
			//! because I don't see any reason to reset device here
			//! Device.Reset (false);
			//-----------------------------------------------------------
			g_pGamePersistent->PreStart(op_server);
			//-----------------------------------------------------------
			g_pGameLevel = (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
			pApp->LoadBegin();
			g_pGamePersistent->Start(op_server);
			g_pGameLevel->net_Start(op_server, op_client);
			pApp->LoadEnd();
		}

		xr_free(op_server);
		xr_free(op_client);
	}
	else if (E == eDisconnect)
	{
		ls_header[0] = '\0';
		ls_tip_number[0] = '\0';
		ls_tip[0] = '\0';

		if (g_pGameLevel)
		{
			Console->Hide();
			g_pGameLevel->net_Stop();
			DEL_INSTANCE(g_pGameLevel);
			Console->Show();

			if ((FALSE == Engine.Event.Peek("KERNEL:quit")) && (FALSE == Engine.Event.Peek("KERNEL:start")))
			{
				Console->Execute("main_menu off");
				Console->Execute("main_menu on");
			}
		}
		R_ASSERT(0 != g_pGamePersistent);
		g_pGamePersistent->Disconnect();
	}
	else if (E == eConsole)
	{
		LPSTR command = (LPSTR)P1;
		Console->ExecuteCommand(command, false);
		xr_free(command);
	}
	else if (E == eStartMPDemo)
	{
		LPSTR demo_file = LPSTR(P1);

		R_ASSERT(0 == g_pGameLevel);
		R_ASSERT(0 != g_pGamePersistent);

		Console->Execute("main_menu off");
		Console->Hide();
		Device.Reset(false);

		g_pGameLevel = (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
		shared_str server_options = g_pGameLevel->OpenDemoFile(demo_file);

		//-----------------------------------------------------------
		g_pGamePersistent->PreStart(server_options.c_str());
		//-----------------------------------------------------------

		pApp->LoadBegin();
		g_pGamePersistent->Start(""); //server_options.c_str()); - no prefetch !
		g_pGameLevel->net_StartPlayDemo();
		pApp->LoadEnd();

		xr_free(demo_file);
	}
}
  
void CApplication::LoadBegin()
{
	ll_dwReference++;
	if (1 == ll_dwReference)
	{
		g_appLoaded = FALSE;

		//AVO:
		g_bootComplete = FALSE;
		//-AVO

#ifndef DEDICATED_SERVER
		_InitializeFont(pFontSystem, "ui_font_letterica18_russian", 0);

		m_pRender->LoadBegin();
#endif
		phase_timer.Start();
		load_stage = 0;
	}
}

void CApplication::LoadEnd()
{
	ll_dwReference--;
	if (0 == ll_dwReference)
	{
		Msg("* phase time: %d ms", phase_timer.GetElapsed_ms());
		Msg("* phase cmem: %lld K", Memory.mem_usage() / 1024);
		Console->Execute("stat_memory");
		g_appLoaded = TRUE;
		// DUMP_PHASE;
	}
}

void CApplication::destroy_loading_shaders()
{
	m_pRender->destroy_loading_shaders();
	g_bootComplete = TRUE;
}

void CApplication::LoadDraw()
{
	if (g_appLoaded) return;
	Device.dwFrame += 1;


	if (!Device.Begin()) return;

	if (g_dedicated_server)
		Console->OnRender();
	else
		load_draw_internal();

	Device.End();
}

void CApplication::LoadTitleInt(LPCSTR str1, LPCSTR str2, LPCSTR str3)
{
	xr_strcpy(ls_header, str1);
	xr_strcpy(ls_tip_number, str2);
	xr_strcpy(ls_tip, str3);
	// LoadDraw ();
}

void CApplication::LoadStage()
{
	load_stage++;
	VERIFY(ll_dwReference);
	Msg("* phase time: %d ms", phase_timer.GetElapsed_ms());
	phase_timer.Start();
	Msg("* phase cmem: %lld K", Memory.mem_usage() / 1024);

	if (g_pGamePersistent->GameType() == 1 && !xr_strcmp(g_pGamePersistent->m_game_params.m_alife, "alife"))
		max_load_stage = 17;
	else
		max_load_stage = 14;
	LoadDraw();
}

void CApplication::LoadSwitch()
{
}

// Sequential
void CApplication::OnFrame()
{
	Engine.Event.OnFrame();
	g_SpatialSpace->update();
	g_SpatialSpacePhysic->update();
	if (g_pGameLevel)
		g_pGameLevel->SoundEvent_Dispatch();
}

void CApplication::Level_Append(LPCSTR folder)
{
	string_path N1, N2, N3, N4;
	strconcat(sizeof(N1), N1, folder, "level");
	strconcat(sizeof(N2), N2, folder, "level.ltx");
	strconcat(sizeof(N3), N3, folder, "level.geom");
	strconcat(sizeof(N4), N4, folder, "level.cform");
	if (
		FS.exist("$game_levels$", N1) &&
		FS.exist("$game_levels$", N2) &&
		FS.exist("$game_levels$", N3) &&
		FS.exist("$game_levels$", N4)
		)
	{
		sLevelInfo LI;
		LI.folder = xr_strdup(folder);
		LI.name = 0;
		Levels.push_back(LI);
	}
}

void CApplication::Level_Scan()
{
	for (u32 i = 0; i < Levels.size(); i++)
	{
		xr_free(Levels[i].folder);
		xr_free(Levels[i].name);
	}
	Levels.clear();


	xr_vector<char*>* folder = FS.file_list_open("$game_levels$", FS_ListFolders | FS_RootOnly);
	for (u32 i = 0; i < folder->size(); ++i)
		Level_Append((*folder)[i]);

	FS.file_list_close(folder);
}

void gen_logo_name(string_path& dest, LPCSTR level_name, int num)
{
	strconcat(sizeof(dest), dest, "intro\\intro_", level_name);

	u32 len = xr_strlen(dest);
	if (dest[len - 1] == '\\')
		dest[len - 1] = 0;

	string16 buff;
	xr_strcat(dest, sizeof(dest), "_");
	xr_strcat(dest, sizeof(dest), itoa(num + 1, buff, 10));
}

void CApplication::Level_Set(u32 L)
{
	if (L >= Levels.size())
		return;
	FS.get_path("$level$")->_set(Levels[L].folder);

	static string_path path;

	if (Level_Current != L)
	{
		path[0] = 0;
		Level_Current = L;

		int count = 0;
		while (true)
		{
			string_path temp2;
			gen_logo_name(path, Levels[L].folder, count);
			if (FS.exist(temp2, "$game_textures$", path, ".dds") || FS.exist(temp2, "$level$", path, ".dds"))
				count++;
			else
				break;
		}

		if (count)
		{
			int num = ::Random.randI(count);
			gen_logo_name(path, Levels[L].folder, num);
		}
	}

	if (path[0])
		m_pRender->setLevelLogo(path);
}

int CApplication::Level_ID(LPCSTR name, LPCSTR ver, bool bSet)
{
	int result = -1;

	CLocatorAPI::archives_it it = FS.m_archives.begin();
	CLocatorAPI::archives_it it_e = FS.m_archives.end();
	bool arch_res = false;

	for (; it != it_e; ++it)
	{
		CLocatorAPI::archive& A = *it;
		if (A.hSrcFile == NULL)
		{
			LPCSTR ln = A.header->r_string("header", "level_name");
			LPCSTR lv = A.header->r_string("header", "level_ver");
			if (0 == stricmp(ln, name) && 0 == stricmp(lv, ver))
			{
				FS.LoadArchive(A);
				arch_res = true;
			}
		}
	}

	if (arch_res)
		Level_Scan();

	string256 buffer;
	strconcat(sizeof(buffer), buffer, name, "\\");
	for (u32 I = 0; I < Levels.size(); ++I)
	{
		if (0 == stricmp(buffer, Levels[I].folder))
		{
			result = int(I);
			break;
		}
	}

	if (bSet && result != -1)
		Level_Set(result);

	if (arch_res)
		g_pGamePersistent->OnAssetsChanged();

	return result;
}

CInifile* CApplication::GetArchiveHeader(LPCSTR name, LPCSTR ver)
{
	CLocatorAPI::archives_it it = FS.m_archives.begin();
	CLocatorAPI::archives_it it_e = FS.m_archives.end();

	for (; it != it_e; ++it)
	{
		CLocatorAPI::archive& A = *it;

		LPCSTR ln = A.header->r_string("header", "level_name");
		LPCSTR lv = A.header->r_string("header", "level_ver");
		if (0 == stricmp(ln, name) && 0 == stricmp(lv, ver))
		{
			return A.header;
		}
	}
	return NULL;
}

void CApplication::LoadAllArchives()
{
	if (FS.load_all_unloaded_archives())
	{
		Level_Scan();
		g_pGamePersistent->OnAssetsChanged();
	}
}

void CApplication::load_draw_internal()
{
	m_pRender->load_draw_internal(*this);
}


// Initialize Engine 
// Initialize

void InitSettings();
void InitConsole();
void InitInput();
void InitSound1();
void InitSound2();
void execUserScript();

// Destroy

void destroyInput();
void destroySound();
void destroySettings();
void destroyConsole();
void destroyEngine();

//Reshade
#pragma comment(lib, "reshadecompat.lib")
bool use_reshade = false;
extern bool init_reshade();
extern void unregister_reshade();


extern BOOL g_bIntroFinished;
extern float g_fTimeFactor;
extern ENGINE_API CInifile* pGameIni = nullptr;
 
void Startup()
{
	InitSound1();
	execUserScript();
	InitSound2();

	// ...command line for auto start
	{
		LPCSTR pStartup = strstr(Core.Params, "-start ");
		if (pStartup) Console->Execute(pStartup + 1);
	}
	{
		LPCSTR pStartup = strstr(Core.Params, "-load ");
		if (pStartup) Console->Execute(pStartup + 1);
	}

	// Initialize APP
	ShowWindow(Device.m_hWnd, SW_SHOWNORMAL);
	Device.Create();

	LALib.OnCreate();
	pApp = xr_new<CApplication>();
	g_pGamePersistent = (IGame_Persistent*)NEW_INSTANCE(CLSID_GAME_PERSISTANT);
	g_SpatialSpace = xr_new<ISpatial_DB>();
	g_SpatialSpacePhysic = xr_new<ISpatial_DB>();

 	//Discord Rich Presence - Rezy
	Init_Discord();

	//Reshade
	use_reshade = init_reshade();
	if (use_reshade)
		Msg("[ReShade]: Loaded compatibility addon");
	else
		Msg("[ReShade]: ReShade not installed or version too old - didn't load compatibility addon");

	// Main cycle
	Msg("* [x-ray]: Starting Main Loop");
	Memory.mem_usage();

	Device.Run();

	// Discord
	clearDiscordPresence();

	//Reshade
	if (use_reshade)
		unregister_reshade();

	// Destroy APP
	xr_delete(g_SpatialSpacePhysic);
	xr_delete(g_SpatialSpace);
	DEL_INSTANCE(g_pGamePersistent);
	xr_delete(pApp);
	Engine.Event.Dump();

	// Destroying
 	destroyInput();
	destroySettings();
	LALib.OnDestroy();
	destroyConsole();
	destroySound();
	destroyEngine();
}
 
void EngineStart1(LPCSTR lpCmdLine)
{
	Debug._initialize(false);
	g_dedicated_server = false;

	// AVI
	g_bIntroFinished = TRUE;

	LPCSTR fsgame_ltx_name = "-fsltx ";
 	string_path fsgame = "";
	if (strstr(lpCmdLine, fsgame_ltx_name))
	{
		int sz = xr_strlen(fsgame_ltx_name);
		sscanf(strstr(lpCmdLine, fsgame_ltx_name) + sz, "%[^ ] ", fsgame);
	}

	compute_build_id();
	Core._initialize("xray", NULL, TRUE, fsgame[0] ? fsgame : NULL);

	InitSettings();
	Msg(XRAY_MONOLITH_VERSION);

	// Adjust player & computer name for Asian
	if (pSettings->line_exist("string_table", "no_native_input"))
	{
		xr_strcpy(Core.UserName, sizeof(Core.UserName), "Player");
		xr_strcpy(Core.CompName, sizeof(Core.CompName), "Computer");
	}

 	// Initialize Engine 
	Engine.Initialize();
	while (!g_bIntroFinished)
		Sleep(100);
	Device.Initialize();

	InitInput();
	InitConsole();

	Engine.External.CreateRendererList();

	extern bool ignore_verify;
	ignore_verify = !strstr(Core.Params, "-dbgdev");
	Msg("command line %s", Core.Params);

	if (strstr(Core.Params, "-r2a"))
		Console->Execute("renderer renderer_r2a");
	else if (strstr(Core.Params, "-r2"))
		Console->Execute("renderer renderer_r2");
	else
	{
		CCC_LoadCFG_custom* pTmp = xr_new<CCC_LoadCFG_custom>("renderer ");
		pTmp->Execute(Console->ConfigFile);
		xr_delete(pTmp);
	}

 	Engine.External.Initialize();
	Console->Execute("stat_memory");

	Startup();
	Core._destroy();
}
 