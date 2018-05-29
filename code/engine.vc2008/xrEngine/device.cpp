#include "stdafx.h"
#include "../xrCDB/frustum.h"

#pragma warning(disable:4995)
// mmsystem.h
#define MMNOSOUND
#define MMNOMIDI
#define MMNOAUX
#define MMNOMIXER
#define MMNOJOY
#include <mmsystem.h>
// d3dx9.h
#include <d3dx9.h>
#pragma warning(default:4995)

#include "x_ray.h"
#include "render.h"

#ifdef INGAME_EDITOR
#	include "../include/editor/ide.hpp"
#	include "engine_impl.hpp"
#endif // #ifdef INGAME_EDITOR
#include "igame_persistent.h"

#pragma comment(lib, "d3dx9.lib")

ENGINE_API CRenderDevice Device;
ENGINE_API CLoadScreenRenderer load_screen_renderer;

DWORD gMainThreadId      = 0xFFFFFFFF;
DWORD gSecondaryThreadId = 0xFFFFFFFF;
DWORD gRenderThreadId    = 0xFFFFFFFF;

ENGINE_API bool IsMainThread()
{
    return GetCurrentThreadId() == gMainThreadId;
}

ENGINE_API bool IsSecondaryThread()
{
    return GetCurrentThreadId() == gSecondaryThreadId;
}

ENGINE_API bool IsRenderThread()
{
    return GetCurrentThreadId() == gRenderThreadId;
}

ENGINE_API BOOL g_bRendering = FALSE; 

BOOL		g_bLoaded = FALSE;
ref_light	precache_light = 0;

BOOL CRenderDevice::Begin	()
{
#ifndef DEDICATED_SERVER
	switch (m_pRender->GetDeviceState())
	{
	case IRenderDeviceRender::dsOK:
		break;

	case IRenderDeviceRender::dsLost:
		// If the device was lost, do not render until we get it back
		Sleep(33);
		return FALSE;
		break;

	case IRenderDeviceRender::dsNeedReset:
		// Check if the device is ready to be reset
		Reset();
		break;

	default:
		R_ASSERT(0);
	}

	m_pRender->Begin();

	FPU::m24r	();
	g_bRendering = 	TRUE;
#endif
	return		TRUE;
}

void CRenderDevice::Clear	()
{
	m_pRender->Clear();
}

extern void CheckPrivilegySlowdown();


void CRenderDevice::End		(void)
{
#ifndef DEDICATED_SERVER


#ifdef INGAME_EDITOR
	bool							load_finished = false;
#endif // #ifdef INGAME_EDITOR
	if (dwPrecacheFrame)
	{
		::Sound->set_master_volume	(0.f);
		dwPrecacheFrame	--;
//.		pApp->load_draw_internal	();
		if (0==dwPrecacheFrame)
		{

#ifdef INGAME_EDITOR
			load_finished			= true;
#endif // #ifdef INGAME_EDITOR
			//Gamma.Update		();
			m_pRender->updateGamma();

			if(precache_light) precache_light->set_active	(false);
			if(precache_light) precache_light.destroy		();
			::Sound->set_master_volume						(1.f);
//			pApp->destroy_loading_shaders					();

			m_pRender->ResourcesDestroyNecessaryTextures	();
			Memory.mem_compact								();
			Msg												("* MEMORY USAGE: %d K",Memory.mem_usage()/1024);
			Msg												("* End of synchronization A[%d] R[%d]",b_is_Active, b_is_Ready);

			CheckPrivilegySlowdown							();
			
			if(g_pGamePersistent->GameType()==1)//haCk
			{
				WINDOWINFO	wi;
				GetWindowInfo(m_hWnd,&wi);
				if(wi.dwWindowStatus!=WS_ACTIVECAPTION)
					Pause(TRUE,TRUE,TRUE,"application start");
			}
		}
	}

	g_bRendering		= FALSE;
	// end scene
	//	Present goes here, so call OA Frame end.
	m_pRender->End();

#	ifdef INGAME_EDITOR
		if (load_finished && m_editor)
			m_editor->on_load_finished	();
#	endif // #ifdef INGAME_EDITOR
#endif
}


volatile u32	mt_Thread_marker		= 0x12345678;
void 			mt_Thread	(void *ptr)	
{
    gSecondaryThreadId = GetCurrentThreadId();

	while (true) {
		// waiting for Device permission to execute
		Device.mt_csEnter.lock	();

		if (Device.mt_bMustExit) {
			Device.mt_bMustExit = FALSE;				// Important!!!
			Device.mt_csEnter.unlock();					// Important!!!
			return;
		}
		// we has granted permission to execute
		mt_Thread_marker			= Device.dwFrame;
 
		for (u32 pit=0; pit<Device.seqParallel.size(); pit++)
			Device.seqParallel[pit]	();
		Device.seqParallel.clear	();
		Device.seqFrameMT.Process	(rp_Frame);

		// now we give control to device - signals that we are ended our work
		Device.mt_csEnter.unlock	();
		// waits for device signal to continue - to start again
		Device.mt_csLeave.lock	();
		// returns sync signal to device
		Device.mt_csLeave.unlock	();
	}
}

void mt_render(void* ptr)
{
    gRenderThreadId = GetCurrentThreadId();
    while (true)
    {
        Device.cs_RenderEnter.lock();

        if (Device.mt_bMustExit) {
            //Device.mt_bMustExit = FALSE;				// Important!!!
            Device.cs_RenderEnter.unlock();					// Important!!!
            return;
        }
        if (Device.b_is_Active) {
            if (Device.Begin()) {
                //Calculate camera matrices
                // Precache
                if (Device.dwPrecacheFrame)
                {
                    float factor = float(Device.dwPrecacheFrame) / float(Device.dwPrecacheTotal);
                    float angle = PI_MUL_2 * factor;
                    Device.vCameraDirection.set(_sin(angle), 0, _cos(angle));	Device.vCameraDirection.normalize();
                    Device.vCameraTop.set(0, 1, 0);
                    Device.vCameraRight.crossproduct(Device.vCameraTop, Device.vCameraDirection);

                    Device.mView.build_camera_dir(Device.vCameraPosition, Device.vCameraDirection, Device.vCameraTop);
                }

                // Matrices
                Device.mFullTransform.mul(Device.mProject, Device.mView);
                Device.m_pRender->SetCacheXform(Device.mView, Device.mProject);
                D3DXMatrixInverse((D3DXMATRIX*)&Device.mInvFullTransform, 0, (D3DXMATRIX*)&Device.mFullTransform);

                Device.vCameraPosition_saved = Device.vCameraPosition;
                Device.mFullTransform_saved = Device.mFullTransform;
                Device.mView_saved = Device.mView;
                Device.mProject_saved = Device.mProject;

                //BEGIN RENDER THREAD SEQUENCE
                if (!Device.Paused() || Device.dwPrecacheFrame)
                {
                    g_pGamePersistent->Environment().OnFrame();
                }

                Device.seqRender.Process(rp_Render);

                Device.End();
            }
        }
        Device.cs_RenderEnter.unlock();

        Device.cs_RenderLeave.lock();
        // returns sync signal to device
        Device.cs_RenderLeave.unlock();
    }
}

#include "igame_level.h"
void CRenderDevice::PreCache	(u32 amount, bool b_draw_loadscreen, bool b_wait_user_input)
{
	if (m_pRender->GetForceGPU_REF()) amount=0;
#ifdef DEDICATED_SERVER
	amount = 0;
#endif
	// Msg			("* PCACHE: start for %d...",amount);
	dwPrecacheFrame	= dwPrecacheTotal = amount;
	if (amount && !precache_light && g_pGameLevel && g_loading_events.empty()) {
		precache_light					= ::Render->light_create();
		precache_light->set_shadow		(false);
		precache_light->set_position	(vCameraPosition);
		precache_light->set_color		(255,255,255);
		precache_light->set_range		(5.0f);
		precache_light->set_active		(true);
	}

	if(amount && b_draw_loadscreen && load_screen_renderer.b_registered==false)
	{
		load_screen_renderer.start	(b_wait_user_input);
	}
}


int g_svDedicateServerUpdateReate = 100;

ENGINE_API xr_list<LOADING_EVENT>			g_loading_events;

void CRenderDevice::on_idle		()
{
	if (!b_is_Ready) {
		Sleep	(100);
		return;
	}

#ifdef DEDICATED_SERVER
	u32 FrameStartTime = TimerGlobal.GetElapsed_ms();
#endif
	if (psDeviceFlags.test(rsStatistic))	g_bEnableStatGather	= TRUE;
	else									g_bEnableStatGather	= FALSE;
	if (g_loading_events.size())
	{
		if (g_loading_events.front()())
			g_loading_events.pop_front();
		pApp->LoadDraw();
		return;
	}
	else
	{
        cs_RenderLeave.lock();
        cs_RenderEnter.unlock();
		FrameMove();
	}

	// *** Resume threads
	// Capture end point - thread must run only ONE cycle
	// Release start point - allow thread to run
	mt_csLeave.lock			();
	mt_csEnter.unlock			();
	Sleep						(0);

#if 0

#ifndef DEDICATED_SERVER
	Statistic->RenderTOTAL_Real.FrameStart	();
	Statistic->RenderTOTAL_Real.Begin		();
	if (b_is_Active)							{
		if (Begin())				{

			seqRender.Process						(rp_Render);
			if (psDeviceFlags.test(rsCameraPos) || psDeviceFlags.test(rsStatistic) || Statistic->errors.size())	
				Statistic->Show						();
			//	TEST!!!
			//Statistic->RenderTOTAL_Real.End			();
			//	Present goes here
			End										();
		}
	}
	Statistic->RenderTOTAL_Real.End			();
	Statistic->RenderTOTAL_Real.FrameEnd	();
	Statistic->RenderTOTAL.accum	= Statistic->RenderTOTAL_Real.accum;
#endif // #ifndef DEDICATED_SERVER

#endif
	// *** Suspend threads
	// Capture startup point
	// Release end point - allow thread to wait for startup point
	mt_csEnter.lock						();
	mt_csLeave.unlock						();
    cs_RenderEnter.lock();
    cs_RenderLeave.unlock();

	// Ensure, that second thread gets chance to execute anyway
	if (dwFrame!=mt_Thread_marker)			{
		for (u32 pit=0; pit<Device.seqParallel.size(); pit++)
			Device.seqParallel[pit]			();
		Device.seqParallel.clear	();
		seqFrameMT.Process					(rp_Frame);
	}

    //Lastly, execute functions, that cannot be executed while several threads running
    Device.seqSinglethreaded.Process(rp_Singlethreaded);

#ifdef DEDICATED_SERVER
	u32 FrameEndTime = TimerGlobal.GetElapsed_ms();
	u32 FrameTime = (FrameEndTime - FrameStartTime);
	/*
	string1024 FPS_str = "";
	string64 tmp;
	xr_strcat(FPS_str, "FPS Real - ");
	if (dwTimeDelta != 0)
		xr_strcat(FPS_str, ltoa(1000/dwTimeDelta, tmp, 10));
	else
		xr_strcat(FPS_str, "~~~");

	xr_strcat(FPS_str, ", FPS Proj - ");
	if (FrameTime != 0)
		xr_strcat(FPS_str, ltoa(1000/FrameTime, tmp, 10));
	else
		xr_strcat(FPS_str, "~~~");
	
*/
	u32 DSUpdateDelta = 1000/g_svDedicateServerUpdateReate;
	if (FrameTime < DSUpdateDelta)
	{
		Sleep(DSUpdateDelta - FrameTime);
//		Msg("sleep for %d", DSUpdateDelta - FrameTime);
//		xr_strcat(FPS_str, ", sleeped for ");
//		xr_strcat(FPS_str, ltoa(DSUpdateDelta - FrameTime, tmp, 10));
	}
//	Msg(FPS_str);
#endif // #ifdef DEDICATED_SERVER

	if (!b_is_Active)
		Sleep		(1);
}

#ifdef INGAME_EDITOR
void CRenderDevice::message_loop_editor	()
{
	m_editor->run();
	m_editor_finalize		(m_editor);
	xr_delete				(m_engine);
}
#endif // #ifdef INGAME_EDITOR

void CRenderDevice::message_loop()
{
#ifdef INGAME_EDITOR
	if (editor()) {
		message_loop_editor	();
		return;
	}
#endif // #ifdef INGAME_EDITOR

	MSG						msg;
    PeekMessage				(&msg, NULL, 0U, 0U, PM_NOREMOVE );
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage	(&msg);
			continue;
		}

		on_idle				();
    }
}

void CRenderDevice::Run			()
{
//	DUMP_PHASE;
	g_bLoaded		= FALSE;
	Log				("Starting engine...");
	thread_name		("X-RAY Primary thread");

	// Startup timers and calculate timer delta
	dwTimeGlobal				= 0;
	Timer_MM_Delta				= 0;
	{
		u32 time_mm			= timeGetTime	();
		while (timeGetTime()==time_mm);			// wait for next tick
		u32 time_system		= timeGetTime	();
		u32 time_local		= TimerAsync	();
		Timer_MM_Delta		= time_system-time_local;
	}

	// Start all threads
//	InitializeCriticalSection	(&mt_csEnter);
//	InitializeCriticalSection	(&mt_csLeave);
	mt_csEnter.lock			();
    cs_RenderEnter.lock();
	mt_bMustExit				= FALSE;
	thread_spawn				(mt_Thread,"X-RAY Secondary thread",0,0);
	thread_spawn				(mt_render,"X-RAY Render thread",0,0);

	// Message cycle
	seqAppStart.Process			(rp_AppStart);

	//CHK_DX(HW.pDevice->Clear(0,0,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1,0));
	m_pRender->ClearTarget		();

	message_loop				();

	seqAppEnd.Process		(rp_AppEnd);

	// Stop Balance-Thread
	mt_bMustExit			= TRUE;
	mt_csEnter.unlock		();
    cs_RenderEnter.unlock();
	while (mt_bMustExit)	Sleep(0);
//	DeleteCriticalSection	(&mt_csEnter);
//	DeleteCriticalSection	(&mt_csLeave);
}

u32 app_inactive_time		= 0;
u32 app_inactive_time_start = 0;

void ProcessLoading(RP_FUNC *f);
void CRenderDevice::FrameMove()
{
	dwFrame			++;

	Core.dwFrame = dwFrame;

	dwTimeContinual	= TimerMM.GetElapsed_ms() - app_inactive_time;

	if (psDeviceFlags.test(rsConstantFPS))	{
		// 20ms = 50fps
		//fTimeDelta		=	0.020f;			
		//fTimeGlobal		+=	0.020f;
		//dwTimeDelta		=	20;
		//dwTimeGlobal	+=	20;
		// 33ms = 30fps
		fTimeDelta		=	0.033f;
        Statistic->fRawFrameDeltaTime = fTimeDelta;
		fTimeGlobal		+=	0.033f;
		dwTimeDelta		=	33;
		dwTimeGlobal	+=	33;
	} else {
		// Timer
		float fPreviousFrameTime = Timer.GetElapsed_sec(); Timer.Start();	// previous frame
		fTimeDelta = 0.1f * fTimeDelta + 0.9f*fPreviousFrameTime;			// smooth random system activity - worst case ~7% error
        Statistic->fRawFrameDeltaTime = fTimeDelta;                         // copy unmodified fTimeDelta, for statistic purpose
		//fTimeDelta = 0.7f * fTimeDelta + 0.3f*fPreviousFrameTime;			// smooth random system activity
		if (fTimeDelta>.1f)    
			fTimeDelta = .1f;							// limit to 15fps minimum

		if (fTimeDelta <= 0.f) 
			fTimeDelta = EPS_S + EPS_S;					// limit to 15fps minimum

		if(Paused())	
			fTimeDelta = 0.0f;

//		u64	qTime		= TimerGlobal.GetElapsed_clk();
		fTimeGlobal		= TimerGlobal.GetElapsed_sec(); //float(qTime)*CPU::cycles2seconds;
		u32	_old_global	= dwTimeGlobal;
		dwTimeGlobal = TimerGlobal.GetElapsed_ms();
		dwTimeDelta		= dwTimeGlobal-_old_global;
	}

	// Frame move
	Statistic->EngineTOTAL.Begin	();

	//	TODO: HACK to test loading screen.
	//if(!g_bLoaded) 
		ProcessLoading				(rp_Frame);
	//else
	//	seqFrame.Process			(rp_Frame);
	Statistic->EngineTOTAL.End	();
}

void ProcessLoading				(RP_FUNC *f)
{
	Device.seqFrame.Process				(rp_Frame);
	g_bLoaded							= TRUE;
}

ENGINE_API BOOL bShowPauseString = TRUE;
#include "IGame_Persistent.h"

void CRenderDevice::Pause(BOOL bOn, BOOL bTimer, BOOL bSound, LPCSTR reason)
{
	static int snd_emitters_ = -1;

	if (g_bBenchmark)	return;

#ifdef DEBUG
	Msg("pause [%s] timer=[%s] sound=[%s] reason=%s",bOn?"ON":"OFF", bTimer?"ON":"OFF", bSound?"ON":"OFF", reason);
#endif // DEBUG

#ifndef DEDICATED_SERVER	

	if(bOn)
	{
		if(!Paused())						
			bShowPauseString				= 
#ifdef INGAME_EDITOR
				editor() ? FALSE : 
#endif // #ifdef INGAME_EDITOR
				TRUE;

		if( bTimer && (!g_pGamePersistent || g_pGamePersistent->CanBePaused()) )
		{
			g_pauseMngr.Pause				(TRUE);
            Msg("PAUSED!");
		}
	
		if (bSound && ::Sound) {
			snd_emitters_ = ::Sound->pause_emitters(true);
		}
	}
    else
	{
		if( bTimer && g_pauseMngr.Paused() )
		{
			fTimeDelta						= EPS_S + EPS_S;
			g_pauseMngr.Pause				(FALSE);
            Msg("UNPAUSE!");
		}
		
		if(bSound)
		{
			if(snd_emitters_>0) //avoid crash
			{
				snd_emitters_ =				::Sound->pause_emitters(false);
			}
		}
	}

#endif

}

BOOL CRenderDevice::Paused()
{
	return g_pauseMngr.Paused();
};

void CRenderDevice::OnWM_Activate(WPARAM wParam, LPARAM lParam)
{
	u16 fActive						= LOWORD(wParam);
	BOOL fMinimized					= (BOOL) HIWORD(wParam);
	BOOL bActive					= ((fActive!=WA_INACTIVE) && (!fMinimized))?TRUE:FALSE;
	
	if (bActive!=Device.b_is_Active)
	{
		Device.b_is_Active			= bActive;

		if (Device.b_is_Active)	
		{
			Device.seqAppActivate.Process(rp_AppActivate);
			app_inactive_time		+= TimerMM.GetElapsed_ms() - app_inactive_time_start;

#ifndef DEDICATED_SERVER
#	ifdef INGAME_EDITOR
			if (!editor())
#	endif // #ifdef INGAME_EDITOR
				ShowCursor			(FALSE);
#endif // #ifndef DEDICATED_SERVER
		}else	
		{
			app_inactive_time_start	= TimerMM.GetElapsed_ms();
			Device.seqAppDeactivate.Process(rp_AppDeactivate);
			ShowCursor				(TRUE);
		}
	}
}

void	CRenderDevice::AddSeqFrame			( pureFrame* f, bool mt )
{
		if ( mt )	
		seqFrameMT.Add	(f,REG_PRIORITY_HIGH);
	else								
		seqFrame.Add		(f,REG_PRIORITY_LOW);

}

void	CRenderDevice::RemoveSeqFrame	( pureFrame* f )
{
	seqFrameMT.Remove	( f );
	seqFrame.Remove		( f );
}

CLoadScreenRenderer::CLoadScreenRenderer()
:b_registered(false)
{}

void CLoadScreenRenderer::start(bool b_user_input) 
{
	Device.seqRender.Add			(this, 0);
	b_registered					= true;
	b_need_user_input				= b_user_input;
}

void CLoadScreenRenderer::stop()
{
	if(!b_registered)				return;
	Device.seqRender.Remove			(this);
	pApp->destroy_loading_shaders	();
	b_registered					= false;
	b_need_user_input				= false;
}

void CLoadScreenRenderer::OnRender() 
{
	pApp->load_draw_internal();
}

void CRenderDevice::CSecondVPParams::SetSVPActive(bool bState) //--#SM+#-- +SecondVP+
 {
	m_bIsActive = bState;
	if (g_pGamePersistent != NULL)
		 g_pGamePersistent->m_pGShaderConstants.m_blender_mode.z = (m_bIsActive ? 1.0f : 0.0f);
}

bool CRenderDevice::CSecondVPParams::IsSVPFrame() //--#SM+#-- +SecondVP+
{
	return IsSVPActive() && ((Device.dwFrame % m_FrameDelay) == 0);
}