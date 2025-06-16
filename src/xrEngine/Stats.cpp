#include "stdafx.h"
#include "GameFont.h"


#include "../xrcdb/ISpatial.h"
#include "IGame_Persistent.h"
#include "render.h"
#include "xr_object.h"

#include "../Include/xrRender/DrawUtils.h"
#include "IGame_Level.h"

int		g_ErrorLineCount = 15;
Flags32 g_stats_flags = { 0 };

// stats
DECLARE_RP(Stats);

class	optimizer
{
	float	average_;
	BOOL	enabled_;
public:
	optimizer() {
		average_ = 30.f;
		//		enabled_	= TRUE;
		//		disable		();
				// because Engine is not exist
		enabled_ = FALSE;
	}

	BOOL	enabled() { return enabled_; }
	void	enable() { if (!enabled_) { Engine.External.tune_resume();	enabled_ = TRUE; } }
	void	disable() { if (enabled_) { Engine.External.tune_pause();	enabled_ = FALSE; } }
	void	update(float value) {
		if (value < average_ * 0.7f) {
			// 25% deviation
			enable();
		}
		else {
			disable();
		};
		average_ = 0.99f * average_ + 0.01f * value;
	};
};
static	optimizer	vtune;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
BOOL			g_bDisableRedText = FALSE;

bool DisabledThread = FALSE;

size_t MemoryUsageSec = 0;
size_t MemoryUsageSecReserved = 0;


void MemoryUpdateThread()
{
	while (!DisabledThread)
	{
		size_t  w_free, w_reserved, w_committed;
		vminfo(&w_free, &w_reserved, &w_committed);

		MemoryUsageSec = w_committed;
		MemoryUsageSecReserved = w_reserved;

		Sleep(1000);
	}
}


CStats::CStats()
{
	fFPS = 30.f;
	fRFPS = 30.f;
	fTPS = 0;
	pFont = 0;
	fMem_calls = 0;
	RenderDUMP_DT_Count = 0;
	Device.seqRender.Add(this, REG_PRIORITY_LOW - 1000);

	DisabledThread = false;

	std::thread* thread_stats = new std::thread(&MemoryUpdateThread);
	thread_stats->detach();
}

CStats::~CStats()
{
	Device.seqRender.Remove(this);
	xr_delete(pFont);
	DisabledThread = true;
}

void _draw_cam_pos(CGameFont* pFont)
{
	float sz = pFont->GetHeight();
	pFont->SetHeightI(0.02f);
	pFont->SetColor(0xffffffff);
	pFont->Out(10, 100, "CAMERA POSITION  : [%3.2f,%3.2f,%3.2f]", VPUSH(Device.vCameraPosition));
	//pFont->Out		(10, 32, "CAMERA DIRECTION : [%3.2f,%3.2f,%3.2f]", VPUSH(Device.vCameraDirection));

	pFont->SetHeight(sz);
	pFont->OnRender();
}

void drawStatParam(CGameFont* F, LPCSTR text)
{
	F->SetColor(color_rgba(128, 128, 192, 255));
	F->OutNext(text);
}


void drawStatParamBy(CGameFont* F, CStats* stats, LPCSTR text, u32 calls)
{
	F->SetColor(color_rgba(0, 255, 0, 255));
	F->OutNext(text, calls);
};

void drawStatParamByMS(float MAX_Stat, CGameFont* F, CStats* stats, LPCSTR text, float stat)
{
	if (stat < MAX_Stat)
		F->SetColor(color_rgba(128, 128, 192, 255));
	else
		if (stat > MAX_Stat && stat < MAX_Stat * 2)
			F->SetColor(color_rgba(255, 0, 128, 255));
		else
			F->SetColor(color_rgba(255, 0, 0, 255));

	F->OutNext(text, stat);
};


void drawStatParamByMS(float MAX_Stat, CGameFont* F, CStats* stats, LPCSTR text, float stat, float stat2)
{
	if (stat < MAX_Stat)
		F->SetColor(color_rgba(128, 128, 192, 255));
	else
		if (stat > MAX_Stat && stat < MAX_Stat * 2)
			F->SetColor(color_rgba(255, 0, 128, 255));
		else
			F->SetColor(color_rgba(255, 0, 0, 255));

	F->OutNext(text, stat, stat2);
};

void drawStatParamByMS(float MAX_Stat, CGameFont* F, CStats* stats, LPCSTR text, float stat, u32 stat2, float stat3)
{
	if (stat < MAX_Stat)
		F->SetColor(color_rgba(128, 128, 192, 255));
	else
		if (stat > MAX_Stat && stat < MAX_Stat * 2)
			F->SetColor(color_rgba(255, 0, 128, 255));
		else
			F->SetColor(color_rgba(255, 0, 0, 255));

	F->OutNext(text, stat, stat2, stat3);
};


extern float FPS = 0;

u32 OldShowTime = 0;

extern u64 TickFirstThread;
extern u64 TickSecondaryThread;
extern u64 TickMTWorkflow;
extern u64 TickMT2Workflow;
extern u64 TickRPFRAMEWorkflow;


ENGINE_API extern int SafeModeOn = 0;
extern float StatsCurrentHeight = 0;


CGameFont* pFontGame = 0;

void CStats::Show()
{
	bool need_stop = true;

	// calc FPS & TPS
	if (Device.fTimeDelta > EPS_S)
	{
		float fps = 1.f / Device.fTimeDelta;
		//if (Engine.External.tune_enabled)	vtune.update	(fps);
		float fOne = 0.3f;
		float fInv = 1.f - fOne;
		fFPS = fInv * fFPS + fOne * fps;
		FPS = fFPS;
		if (RenderTOTAL.result > EPS_S)
		{
			u32	rendered_polies = Device.m_pRender->GetCacheStatPolys();
			fTPS = fInv * fTPS + fOne * float(rendered_polies) / (RenderTOTAL.result * 1000.f);
			//fTPS = fInv*fTPS + fOne*float(RCache.stat.polys)/(RenderTOTAL.result*1000.f);
			fRFPS = fInv * fRFPS + fOne * 1000.f / RenderTOTAL.result;
		}
	}

	{
		float mem_count = float(Memory.stat_calls);
		if (mem_count > fMem_calls)	fMem_calls = mem_count;
		else						fMem_calls = .9f * fMem_calls + .1f * mem_count;
		Memory.stat_calls = 0;
	}

	////////////////////////////////////////////////
	if (g_dedicated_server) return;
	////////////////////////////////////////////////
	 
	// Stop timers
	FrameStart();

	///////////////////////////////////////////////////
	/////////////// TIMERS END
	//////////////////////////////////////////////////

	if (!pFontGame)
 		pFontGame = xr_new<CGameFont>("stat_font", CGameFont::fsDeviceIndependent);
 
	CGameFont& F = *pFontGame;
 	pFontGame->OutSet(100, 10);

  	if (psDeviceFlags.test(rsStatistic))
	{
		pFontGame->SetHeightI(0.024f);
		F.SetColor(color_rgba(128, 128, 192, 255));
		F.OutSet(1550, 20);
	}

	if (psDeviceFlags.test(rsStatistic))
	{
		F.SetColor(color_rgba(128, 128, 192, 255));
		F.OutNext("FPS:   %3.0f", fFPS);

		string128 tmp;
		sprintf_s(tmp, "MemUse: %u mb, Reserved: %u mb", u32(MemoryUsageSec / 1024 / 1024), u32(MemoryUsageSecReserved / 1024 / 1024));
		pFontGame->OutNext(tmp);

		drawStatParam(pFontGame, "----------------");

		drawStatParamByMS(10, pFontGame, this, "EngineFrame:		%2.4fms", EngineFrame.result);

 		drawStatParamByMS(10, pFontGame, this, "uUpdateCL:			%2.4fms | %2.4fms(relcase)", UpdateClient.result, NetworkRelcase.result);
		drawStatParamByMS(10, pFontGame, this, "uShedule:			%2.4fms | %2.4fms(low)", Sheduler.result, ShedulerLow.result);

		drawStatParam(pFontGame, "----------------");
		drawStatParamByMS(6, pFontGame, this, "Render:				%2.4fms", RenderTOTAL_Real.result);

		drawStatParam(pFontGame, "----------------");
		drawStatParamByMS(1, pFontGame, this, "Render Wait Gpus:	%2.4fms", RenderDUMP_Wait.result);

		// drawStatParamByMS(1, pFontGame, this, "Render Water Reflaction: %2.4fms", RenderWaterReflection.result);
		drawStatParamByMS(1, pFontGame, this, "Render Build Static:		%2.4fms", RenderMainVIS_Static.result);
		drawStatParamByMS(1, pFontGame, this, "Render Build Dynamic:	%2.4fms", RenderMainVIS_Dynamic.result);
		drawStatParamByMS(1, pFontGame, this, "Render Build Traverse:	%2.4fms", RenderMainVIS_StaticTraverce.result);
		drawStatParamByMS(1, pFontGame, this, "Render Build Subspace:	%2.4fms", RenderSubspace.result);

		drawStatParamByMS(3, pFontGame, this, "Render GPU Draw:	%2.4fms", RenderDUMP.result);
		drawStatParamByMS(1, pFontGame, this, "Render GPU DrawFwd:	%2.4fms", RenderDUMP_Second.result);

		drawStatParam(pFontGame, "----------------");
		drawStatParamByMS(2, pFontGame, this, "R_Main_Sun:			%2.4fms", RenderSun.result);
		drawStatParamByMS(2, pFontGame, this, "R_Main_Lights:		%2.4fms", RenderLights.result);
		drawStatParamByMS(2, pFontGame, this, "R_Postprocess:		%2.4fms", Render_postprocess.result);

		drawStatParamByMS(1, pFontGame, this, "RDT_Ren:   %2.4fms", RenderDUMP_DT_Render.result);
		drawStatParamByMS(1, pFontGame, this, "RDT_Vis:   %2.4fms", RenderDUMP_DT_VIS.result);
		drawStatParamByMS(1, pFontGame, this, "RDT_Cache: %2.4fms", RenderDUMP_DT_Cache.result);

		float build_graph = RenderMainVIS_Static.result + RenderMainVIS_Dynamic.result + RenderMainVIS_StaticTraverce.result;
		float render_graph = RenderDUMP.result + RenderDUMP_Second.result + RenderSun.result + RenderLights.result + Render_postprocess.result;

		drawStatParamByMS(20, pFontGame, this, "Render Total: %f ms", build_graph + render_graph + RenderDUMP_DT_Render.result);

		// drawStatParam(pFontGame, "----------------");
		// u32 dcalls; u32 verts; u32 polys;
		// m_pRender->DrawCalls(dcalls); m_pRender->DrawVerticy(verts); m_pRender->DrawPoly(polys);

		// drawStatParamBy(pFontGame, this, "Draw Calls(DPI):		%u", dcalls);
		// drawStatParamBy(pFontGame, this, "Draw Vertex:			%u", verts);
		// drawStatParamBy(pFontGame, this, "Draw Pollys:			%u", polys);
	}

	if (psDeviceFlags.test(rsCameraPos))
 		_draw_cam_pos(pFontGame);
 	pFontGame->OnRender();
 	
	dwSND_Played				= dwSND_Allocated = 0;
	Particles_starting			= Particles_active = Particles_destroy = 0;
	FrameEnd();
}


void	_LogCallback(LPCSTR string)
{
	if (string && '!' == string[0] && ' ' == string[1])
		Device.Statistic->errors.push_back(shared_str(string));
}

void CStats::OnDeviceCreate()
{
	g_bDisableRedText = strstr(Core.Params, "-xclsx") ? TRUE : FALSE;

	if (!g_dedicated_server)
		pFont = xr_new<CGameFont>("ui_font_graffiti22_russian", CGameFont::fsDeviceIndependent);


	if (!pSettings->section_exist("evaluation")
		|| !pSettings->line_exist("evaluation", "line1")
		|| !pSettings->line_exist("evaluation", "line2")
		|| !pSettings->line_exist("evaluation", "line3"))
		FATAL("");

	eval_line_1 = pSettings->r_string_wb("evaluation", "line1");
	eval_line_2 = pSettings->r_string_wb("evaluation", "line2");
	eval_line_3 = pSettings->r_string_wb("evaluation", "line3");
}

void CStats::OnDeviceDestroy()
{
	SetLogCB(0);
	xr_delete(pFont);
}

void CStats::OnRender()
{
 
}
