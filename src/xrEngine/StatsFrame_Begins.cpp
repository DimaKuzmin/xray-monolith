#include "stdafx.h"
#include "Stats.h"

#include "../xrcdb/ISpatial.h"
#include "IGame_Persistent.h"

void CStats::FrameStart()
{
	RenderSSFX_Terrain.FrameEnd(); 


	EngineFrame.FrameEnd();
	EngineMTFrame.FrameEnd();
	EngineMTFrameSCore.FrameEnd();

	Sheduler.FrameEnd();
	ShedulerLow.FrameEnd();

	UpdateClient.FrameEnd();
	Physics.FrameEnd();
	ph_collision.FrameEnd();
	ph_core.FrameEnd();
	Animation.FrameEnd();
	AI_Think.FrameEnd();
	AI_Range.FrameEnd();
	AI_Path.FrameEnd();
	AI_Node.FrameEnd();
	AI_Vis.FrameEnd();
	AI_Vis_Query.FrameEnd();
	AI_Vis_RayTests.FrameEnd();

	RenderTOTAL.FrameEnd();
	RenderCALC.FrameEnd();
	RenderCALC_HOM.FrameEnd();

	RenderDUMP.FrameEnd();
	RenderDUMP_Second.FrameEnd();

	RenderDUMP_Wait.FrameEnd();
	RenderDUMP_Lights.FrameEnd();
 	RenderDUMP_DT_VIS.FrameEnd();
	RenderDUMP_DT_Render.FrameEnd();
	RenderDUMP_DT_Cache.FrameEnd();
 
	Sound.FrameEnd();
	Input.FrameEnd();
	clRAY.FrameEnd();
	clBOX.FrameEnd();
	clFRUSTUM.FrameEnd();

	netClient1.FrameEnd();
	netClient2.FrameEnd();
	netServer.FrameEnd();

	netClientCompressor.FrameEnd();
	netServerCompressor.FrameEnd();

	TEST0.FrameEnd();
	TEST1.FrameEnd();
	TEST2.FrameEnd();
	TEST3.FrameEnd();

	g_SpatialSpace->stat_insert.FrameEnd();
	g_SpatialSpace->stat_remove.FrameEnd();
	g_SpatialSpacePhysic->stat_insert.FrameEnd();
	g_SpatialSpacePhysic->stat_remove.FrameEnd();

	RenderTOTAL_Real.FrameEnd();
	RenderMain.FrameEnd();
	RenderMain_Calcualte.FrameEnd();

	RenderMainVIS_Static.FrameEnd();
	RenderMainVIS_Dynamic.FrameEnd();
	RenderSun.FrameEnd();
	RenderLights.FrameEnd();
 	Render_postprocess.FrameEnd();

	NetworkSpawnCreate_xrEngine.FrameEnd();
	NetworkSpawn.FrameEnd();
	NetworkSpawn_ProcessCSE.FrameEnd();
	NetworkRelcase.FrameEnd();

	Particles_update_Time.FrameEnd();
	Particles_render_Time.FrameEnd();

	ThreadEngine.FrameEnd();
	ThreadParticles.FrameEnd();
	ThreadSecond.FrameEnd();


	OnFrame1.FrameEnd();
	OnFrame2.FrameEnd();
	OnFrame3.FrameEnd();
	OnFrame4.FrameEnd();

	// UpdateCL DATA !!!!
	UpdateClientPH.FrameEnd();
	UpdateClientUnsorted.FrameEnd();
	UpdateClientA.FrameEnd();
	UpdateClientAI_mutant.FrameEnd();
	UpdateClientAI.FrameEnd();
	UpdateClientInv.FrameEnd();

	RenderMainVIS_StaticTraverce.FrameEnd();
 
	RenderSubspace.FrameEnd();
 }

void CStats::FrameEnd()
{
	RenderSSFX_Terrain.FrameStart();

	EngineFrame.FrameStart();
	EngineMTFrame.FrameStart();
	EngineMTFrameSCore.FrameStart();

	Sheduler.FrameStart();
	ShedulerLow.FrameStart();
	UpdateClient.FrameStart();
	Physics.FrameStart();
	ph_collision.FrameStart();
	ph_core.FrameStart();
	Animation.FrameStart();

	AI_Think.FrameStart();
	AI_Range.FrameStart();
	AI_Path.FrameStart();
	AI_Node.FrameStart();
	AI_Vis.FrameStart();
	AI_Vis_Query.FrameStart();
	AI_Vis_RayTests.FrameStart();

	RenderTOTAL.FrameStart();
	RenderCALC.FrameStart();
	RenderCALC_HOM.FrameStart();
	RenderDUMP.FrameStart();
	RenderDUMP_Second.FrameStart();
 
	RenderDUMP_Wait.FrameStart();
	RenderDUMP_Lights.FrameStart();
 	RenderDUMP_DT_VIS.FrameStart();
	RenderDUMP_DT_Render.FrameStart();
	RenderDUMP_DT_Cache.FrameStart();
  
	Sound.FrameStart();
	Input.FrameStart();
	clRAY.FrameStart();
	clBOX.FrameStart();
	clFRUSTUM.FrameStart();

	netClient1.FrameStart();
	netClient2.FrameStart();
	netServer.FrameStart();
	netClientCompressor.FrameStart();
	netServerCompressor.FrameStart();

	TEST0.FrameStart();
	TEST1.FrameStart();
	TEST2.FrameStart();
	TEST3.FrameStart();

	g_SpatialSpace->stat_insert.FrameStart();
	g_SpatialSpace->stat_remove.FrameStart();

	g_SpatialSpacePhysic->stat_insert.FrameStart();
	g_SpatialSpacePhysic->stat_remove.FrameStart();

	RenderTOTAL_Real.FrameStart();
	RenderMain.FrameStart();
	RenderMain_Calcualte.FrameStart();

	RenderMainVIS_Static.FrameStart();
	RenderMainVIS_Dynamic.FrameStart();

	RenderSun.FrameStart();
	RenderLights.FrameStart();
 
 	Render_postprocess.FrameStart();

	NetworkSpawnCreate_xrEngine.FrameStart();
	NetworkSpawn.FrameStart();
	NetworkSpawn_ProcessCSE.FrameStart();
	NetworkRelcase.FrameStart();

	Particles_update_Time.FrameStart();
	Particles_render_Time.FrameStart();

	ThreadEngine.FrameStart();
	ThreadParticles.FrameStart();
	ThreadSecond.FrameStart();

	OnFrame1.FrameStart();
	OnFrame2.FrameStart();
	OnFrame3.FrameStart();
	OnFrame4.FrameStart();
	 
	UpdateClientPH.FrameStart();
	UpdateClientUnsorted.FrameStart();
	UpdateClientA.FrameStart();
	UpdateClientAI_mutant.FrameStart();
	UpdateClientAI.FrameStart();
	UpdateClientInv.FrameStart();
	 
	RenderMainVIS_StaticTraverce.FrameStart();
	RenderSubspace.FrameStart(); 
}