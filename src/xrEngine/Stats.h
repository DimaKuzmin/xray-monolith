// Stats.h: interface for the CStats class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
class ENGINE_API CGameFont;

ENGINE_API extern int SafeModeOn;

#include "../Include/xrRender/FactoryPtr.h"
#include "../Include/xrRender/StatsRender.h"

DECLARE_MESSAGE(Stats);

class ENGINE_API CStatsPhysics
{
public:
	CStatTimer	ph_collision;		// collision
	CStatTimer	ph_core;			// integrate
	CStatTimer	Physics;			// movement+collision
};

class ENGINE_API CStatsEngine
{
public:
  	u32			UpdateClient_updated;	//
	u32			UpdateClient_crows;		//
	u32			UpdateClient_active;	//
	u32			UpdateClient_total;		//


	u32			Particles_starting;	// starting
	u32			Particles_active;	// active
	u32			Particles_destroy;	// destroying

	// ENGINE
	CStatTimer ThreadEngineMessageLoop;
	CStatTimer ThreadEngine;
	CStatTimer ThreadParticles;
	CStatTimer ThreadSecond;

	CStatTimer	EngineFrame;			// 
	CStatTimer	EngineMTFrame;			// 
	CStatTimer	EngineMTFrameSCore;			// 

	CStatTimer	Sheduler;				// 
	CStatTimer	ShedulerLow;			// 

	CStatTimer	OnFrame1;
	CStatTimer	OnFrame2;
	CStatTimer	OnFrame3;
	CStatTimer	OnFrame4;

	CStatTimer	UpdateClient;			// 


	CStatTimer	UpdateClientPH;	// 
	CStatTimer	UpdateClientUnsorted;	// 
	CStatTimer	UpdateClientA;			// 

	CStatTimer	UpdateClientAI_mutant;	// 
	CStatTimer	UpdateClientAI;			// 
	CStatTimer	UpdateClientInv;			// 
	 
	CStatTimer	AI_Think;			// thinking
	CStatTimer	AI_Range;			// query: range
	CStatTimer	AI_Path;			// query: path
	CStatTimer	AI_Node;			// query: node
	CStatTimer	AI_Vis;				// visibility detection - total
	CStatTimer	AI_Vis_Query;		// visibility detection - portal traversal and frustum culling
	CStatTimer	AI_Vis_RayTests;	// visibility detection - ray casting
};

class ENGINE_API CStatsRender
{
public:
	// SSFX 
	CStatTimer RenderSSFX_Terrain;
 
	//RENDER
	CStatTimer	RenderTOTAL;		// 
	CStatTimer	RenderTOTAL_Real;
	CStatTimer	RenderCALC;			// portal traversal, frustum culling, entities "renderable_Render"
	CStatTimer	RenderCALC_HOM;		// HOM rendering

	CStatTimer	Animation;			// skeleton calculation
 
	// Secondary Прогод по графу
	CStatTimer	RenderDUMP;			// actual primitive rendering
	CStatTimer	RenderDUMP_Second;			// actual primitive rendering

	CStatTimer	RenderDUMP_Wait;	// ...waiting something back (queries results, etc.)
 	CStatTimer	RenderDUMP_Lights;	// ...d-lights building/rendering

	// DETAILS
	CStatTimer	RenderDUMP_DT_VIS;	// ...details visibility detection
	CStatTimer	RenderDUMP_DT_Render;// ...details rendering
	CStatTimer	RenderDUMP_DT_Cache;// ...details slot cache access
	u32			RenderDUMP_DT_Count;// ...number of DT-elements
	 
	// NEW STUFF SE7
	CStatTimer	Particles_update_Time;
	CStatTimer	Particles_render_Time;
 
	// NEW RENDER PARAMS
	CStatTimer RenderMain;
	CStatTimer RenderMain_Calcualte;
	CStatTimer RenderMainVIS_Static;
	CStatTimer RenderMainVIS_StaticTraverce;
	CStatTimer RenderMainVIS_Dynamic;

	CStatTimer RenderSubspace;
	CStatTimer RenderSun;
	CStatTimer RenderLights;
 	CStatTimer Render_postprocess;
};

class ENGINE_API CStatNetwork
{
public:

	CStatTimer  NetworkSpawn;
	CStatTimer  NetworkSpawnCreate_xrEngine;
	CStatTimer	NetworkSpawn_ProcessCSE;
	CStatTimer	NetworkRelcase;

	CStatTimer	netClient1;
	CStatTimer	netClient2;
	CStatTimer	netServer;
	CStatTimer	netClientCompressor;
	CStatTimer	netServerCompressor;
};

class ENGINE_API CStatsApi
{
public:
	CStatTimer	Sound;				// total time taken by sound subsystem (accurate only in single-threaded mode)
	CStatTimer	Input;				// total time taken by input subsystem (accurate only in single-threaded mode)
	CStatTimer	clRAY;				// total: ray-testing
	CStatTimer	clBOX;				// total: box query
	CStatTimer	clFRUSTUM;			// total: frustum query

	CStatTimer	TEST0;				// debug counter
	CStatTimer	TEST1;				// debug counter
	CStatTimer	TEST2;				// debug counter
	CStatTimer	TEST3;				// debug counter
};

class ENGINE_API CStats :
	public pureRender,
	public CStatsPhysics,
	public CStatsEngine,
	public CStatsRender,
	public CStatNetwork,
	public CStatsApi
{
public:
	CGameFont* pFont;

	float		fFPS, fRFPS, fTPS;			// FPS, RenderFPS, TPS
	float		fMem_calls;
	u32			dwMem_calls;
	u32			dwSND_Played, dwSND_Allocated;	// Play/Alloc
	float		fShedulerLoad;




	shared_str	eval_line_1;
	shared_str	eval_line_2;
	shared_str	eval_line_3;

	void			Show(void);

	virtual void 	OnRender();
	void			OnDeviceCreate(void);
	void			OnDeviceDestroy(void);

	void			FrameStart();
	void			FrameEnd();


public:
	xr_vector		<shared_str>	errors;
	CRegistrator	<pureStats>		seqStats;
public:
	CStats();
	~CStats();

	IC CGameFont* Font() { return pFont; }

private:
	FactoryPtr<IStatsRender>	m_pRender;
};

enum {
	st_sound = (1 << 0),
	st_sound_min_dist = (1 << 1),
	st_sound_max_dist = (1 << 2),
	st_sound_ai_dist = (1 << 3),
	st_sound_info_name = (1 << 4),
	st_sound_info_object = (1 << 5),
};

extern Flags32 g_stats_flags;