//-----------------------------------------------------------------------------
// File: x_ray.cpp
//
// Programmers:
//	Oles		- Oles Shishkovtsov
//	AlexMX		- Alexander Maksimchuk
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "igame_level.h"
#include "igame_persistent.h"

#include "dedicated_server_only.h"
#include "no_single.h"

#include "xr_input.h"
#include "xr_ioconsole.h"
#include "x_ray.h"
#include "std_classes.h"
#include "GameFont.h"
#include "../xrPlay/resource.h"
#include "LightAnimLibrary.h"
#include "../xrcdb/ispatial.h"
#include "Text_Console.h"
#include <process.h>
#include <locale.h>

#include "../FrayBuildConfig.hpp"
//---------------------------------------------------------------------
ENGINE_API CInifile* pGameIni		= nullptr;
#ifdef SPAWN_ANTIFREEZE
ENGINE_API bool g_bootComplete		= false;
#endif
bool	g_bIntroFinished			= false;
extern	void	Intro				( void* fn );
extern	void	Intro_DSHOW			( void* fn );
extern	int PASCAL IntroDSHOW_wnd	(HINSTANCE hInstC, HINSTANCE hInstP, LPSTR lpCmdLine, int nCmdShow);

#ifdef MASTER_GOLD
#	define NO_MULTI_INSTANCES
#endif // #ifdef MASTER_GOLD

//---------------------------------------------------------------------
// 2446363
// umbt@ukr.net
//////////////////////////////////////////////////////////////////////////
struct _SoundProcessor	: public pureFrame
{
	virtual void	_BCL	OnFrame	( )
	{
		Device.Statistic->Sound.Begin();
		::Sound->update				(Device.vCameraPosition,Device.vCameraDirection,Device.vCameraTop);
		Device.Statistic->Sound.End	();
	}
}	SoundProcessor;

//////////////////////////////////////////////////////////////////////////
// global variables
ENGINE_API	CApplication*	pApp			= NULL;
static		HWND			logoWindow		= NULL;

			void			doBenchmark		(LPCSTR name);
ENGINE_API	bool			g_bBenchmark	= false;
string512	g_sBenchmarkName;


ENGINE_API	string512		g_sLaunchOnExit_params;
ENGINE_API	string512		g_sLaunchOnExit_app;
ENGINE_API	string_path		g_sLaunchWorkingFolder;
// -------------------------------------------
// startup point
void InitEngine()
{
	Engine.Initialize();
	while (!g_bIntroFinished)	Sleep(100);
	Device.Initialize();
}

PROTECT_API void InitSettings	()
{
	string_path					fname; 
	FS.update_path				(fname,"$game_config$","system.ltx");
#ifdef DEBUG
	Msg							("Updated path to system.ltx is %s", fname);
#endif // #ifdef DEBUG
	pSettings					= xr_new<CInifile>	(fname,TRUE);
	CHECK_OR_EXIT				(0!=pSettings->section_count(), make_string("Cannot find file %s.\nReinstalling application may fix this problem.",fname));

	CInifile::allow_include_func_t	tmp_functor;
	tmp_functor.bind([](LPCSTR) { return true; });
	/*
	pSettingsAuth					= xr_new<CInifile>(
		fname,
		TRUE,
		TRUE,
		FALSE,
		0,
		tmp_functor
	);
	*/
	FS.update_path				(fname,"$game_config$","game.ltx");
	pGameIni					= xr_new<CInifile>	(fname,TRUE);
	CHECK_OR_EXIT				(0!=pGameIni->section_count(), make_string("Cannot find file %s.\nReinstalling application may fix this problem.",fname));
}
PROTECT_API void InitConsole	()
{
	Console						= xr_new<CConsole>	();
	Console->Initialize			( );

	xr_strcpy						(Console->ConfigFile,"user.ltx");
	if (strstr(Core.Params,"-ltx ")) {
		string64				c_name;
		sscanf					(strstr(Core.Params,"-ltx ")+5,"%[^ ] ",c_name);
		xr_strcpy					(Console->ConfigFile,c_name);
	}
}

PROTECT_API void InitInput		()
{
	BOOL bCaptureInput			= !strstr(Core.Params,"-i");

	pInput						= xr_new<CInput>		(bCaptureInput);
}
void destroyInput	()
{
	xr_delete					( pInput		);
}

PROTECT_API void InitSound1		()
{
	CSound_manager_interface::_create				(0);
}

PROTECT_API void InitSound2		()
{
	CSound_manager_interface::_create				(1);
}

void destroySound	()
{
	CSound_manager_interface::_destroy				( );
}

void destroySettings()
{
	CInifile** s				= (CInifile**)(&pSettings);
	xr_delete					( *s		);
	xr_delete					( pGameIni		);
}

void destroyConsole	()
{
	Console->Execute			("cfg_save");
	Console->Destroy			();
	xr_delete					(Console);
}

void destroyEngine	()
{
	Device.Destroy				( );
	Engine.Destroy				( );
}

void execUserScript				( )
{
	Console->Execute			("default_controls");
	Console->ExecuteScript		(Console->ConfigFile);
}

void slowdownthread	( void* )
{
//	Sleep		(30*1000);
	for (;;)	{
		if (Device.Statistic->fFPS<30) Sleep(1);
		if (Device.mt_bMustExit)	return;
		if (0==pSettings)			return;
		if (0==Console)				return;
		if (0==pInput)				return;
		if (0==pApp)				return;
	}
}
void CheckPrivilegySlowdown		( )
{
#ifdef DEBUG
	if	(strstr(Core.Params,"-slowdown"))	{
		thread_spawn(slowdownthread,"slowdown",0,0);
	}
	if	(strstr(Core.Params,"-slowdown2x"))	{
		thread_spawn(slowdownthread,"slowdown",0,0);
		thread_spawn(slowdownthread,"slowdown",0,0);
	}
#endif // DEBUG
}

void Startup()
{
	InitSound1		();
	execUserScript	();
	InitSound2		();

	// ...command line for auto start
	{
		LPCSTR	pStartup			= strstr				(Core.Params,"-start ");
		if (pStartup)				Console->Execute		(pStartup+1);
	}
	{
		LPCSTR	pStartup			= strstr				(Core.Params,"-load ");
		if (pStartup)				Console->Execute		(pStartup+1);
	}

	// Initialize APP
//#ifndef DEDICATED_SERVER
	ShowWindow( Device.m_hWnd , SW_SHOWNORMAL );
	Device.Create				( );
//#endif
	LALib.OnCreate				( );
	pApp						= xr_new<CApplication>	();
	g_pGamePersistent			= (IGame_Persistent*)	NEW_INSTANCE (CLSID_GAME_PERSISTANT);
	g_SpatialSpace				= xr_new<ISpatial_DB>	();
	g_SpatialSpacePhysic		= xr_new<ISpatial_DB>	();
	
	// Destroy LOGO
	DestroyWindow				(logoWindow);
	logoWindow					= NULL;

	// Main cycle
	Memory.mem_usage();
	Device.Run					( );

	// Destroy APP
	xr_delete					( g_SpatialSpacePhysic	);
	xr_delete					( g_SpatialSpace		);
	DEL_INSTANCE				( g_pGamePersistent		);
	xr_delete					( pApp					);
	Engine.Event.Dump			( );

	// Destroying
//.	destroySound();
	destroyInput();

	if(!g_bBenchmark)
		destroySettings();

	LALib.OnDestroy				( );
	
	if(!g_bBenchmark)
		destroyConsole();
	else
		Console->Destroy();

	destroySound();

	destroyEngine();
}

static INT_PTR CALLBACK logDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
	switch( msg ){
		case WM_DESTROY:
			break;
		case WM_CLOSE:
			DestroyWindow( hw );
			break;
		case WM_COMMAND:
			if( LOWORD(wp)==IDCANCEL )
				DestroyWindow( hw );
			break;
		default:
			return FALSE;
	}
	return TRUE;
}
extern void	testbed	(void);

// video
#define dwStickyKeysStructSize sizeof( STICKYKEYS )
#define dwFilterKeysStructSize sizeof( FILTERKEYS )
#define dwToggleKeysStructSize sizeof( TOGGLEKEYS )

struct damn_keys_filter {
	BOOL bScreenSaverState;

	// Sticky & Filter & Toggle keys

	STICKYKEYS StickyKeysStruct;
	FILTERKEYS FilterKeysStruct;
	TOGGLEKEYS ToggleKeysStruct;

	DWORD dwStickyKeysFlags,
		dwFilterKeysFlags,
		dwToggleKeysFlags;

	damn_keys_filter	()
	{
		// Screen saver stuff
		bScreenSaverState = FALSE;

		// Saveing current state
		SystemParametersInfo( SPI_GETSCREENSAVEACTIVE , 0 , ( PVOID ) &bScreenSaverState , 0 );

		if (bScreenSaverState)
			// Disable screensaver
			SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

		dwStickyKeysFlags = 0;
		dwFilterKeysFlags = 0;
		dwToggleKeysFlags = 0;

		std::memset(&StickyKeysStruct, 0, dwStickyKeysStructSize);
		std::memset(&FilterKeysStruct, 0, dwFilterKeysStructSize);
		std::memset(&ToggleKeysStruct, 0, dwToggleKeysStructSize);

		StickyKeysStruct.cbSize = dwStickyKeysStructSize;
		FilterKeysStruct.cbSize = dwFilterKeysStructSize;
		ToggleKeysStruct.cbSize = dwToggleKeysStructSize;

		// Saving current state
		SystemParametersInfo( SPI_GETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		SystemParametersInfo( SPI_GETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		SystemParametersInfo( SPI_GETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );

		if ( StickyKeysStruct.dwFlags & SKF_AVAILABLE ) {
			// Disable StickyKeys feature
			dwStickyKeysFlags = StickyKeysStruct.dwFlags;
			StickyKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		}

		if ( FilterKeysStruct.dwFlags & FKF_AVAILABLE ) {
			// Disable FilterKeys feature
			dwFilterKeysFlags = FilterKeysStruct.dwFlags;
			FilterKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		}

		if ( ToggleKeysStruct.dwFlags & TKF_AVAILABLE ) {
			// Disable FilterKeys feature
			dwToggleKeysFlags = ToggleKeysStruct.dwFlags;
			ToggleKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );
		}
	}

	~damn_keys_filter	()
	{
		if ( bScreenSaverState )
			// Restoring screen saver
			SystemParametersInfo( SPI_SETSCREENSAVEACTIVE , TRUE , NULL , 0 );

		if ( dwStickyKeysFlags) {
			// Restore StickyKeys feature
			StickyKeysStruct.dwFlags = dwStickyKeysFlags;
			SystemParametersInfo( SPI_SETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		}

		if ( dwFilterKeysFlags ) {
			// Restore FilterKeys feature
			FilterKeysStruct.dwFlags = dwFilterKeysFlags;
			SystemParametersInfo( SPI_SETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		}

		if ( dwToggleKeysFlags ) {
			// Restore FilterKeys feature
			ToggleKeysStruct.dwFlags = dwToggleKeysFlags;
			SystemParametersInfo( SPI_SETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );
		}

	}
};

#undef dwStickyKeysStructSize
#undef dwFilterKeysStructSize
#undef dwToggleKeysStructSize

#include "xr_ioc_cmd.h"

DLL_API int RunXRLauncher();
DLL_API const char* GetParams();

ENGINE_API int RunApplication(char* commandLine)
{
	Debug._initialize			(false);
	//MessageBox(0, lpCmdLine, "", 0);
	if (!IsDebuggerPresent()) 
	{
		HMODULE const kernel32	= LoadLibrary("kernel32.dll");
		R_ASSERT				(kernel32);

		size_t HeapFragValue = 2;
		HeapSetInformation(GetProcessHeap(), HeapCompatibilityInformation, &HeapFragValue, sizeof(HeapFragValue));
	}

    gMainThreadId = GetCurrentThreadId();
    Debug.set_mainThreadId(gMainThreadId);
	// Check for another instance
#ifdef NO_MULTI_INSTANCES
	#define STALKER_PRESENCE_MUTEX "Local\\STALKER-COP"
	
	HANDLE hCheckPresenceMutex = INVALID_HANDLE_VALUE;
	hCheckPresenceMutex = OpenMutex( READ_CONTROL , FALSE ,  STALKER_PRESENCE_MUTEX );
	if ( hCheckPresenceMutex == NULL ) {
		// New mutex
		hCheckPresenceMutex = CreateMutex( NULL , FALSE , STALKER_PRESENCE_MUTEX );
		if ( hCheckPresenceMutex == NULL )
			// Shit happens
			return 2;
	} else {
		// Already running
		CloseHandle( hCheckPresenceMutex );
		return 1;
	}
#endif

	//SetThreadAffinityMask		(GetCurrentThread(),1);

	// Title window
	logoWindow					= CreateDialog(GetModuleHandle(NULL),	MAKEINTRESOURCE(IDD_STARTUP), 0, logDlgProc );
	
	HWND logoPicture			= GetDlgItem(logoWindow, IDC_STATIC_LOGO);
	RECT logoRect;
	GetWindowRect(logoPicture, &logoRect);

    HWND logoInsertPos = HWND_TOPMOST;
    if (IsDebuggerPresent())
    {
        logoInsertPos = HWND_NOTOPMOST;
    }

	SetWindowPos(logoWindow, logoInsertPos, 0, 0, logoRect.right - logoRect.left, logoRect.bottom - logoRect.top, SWP_NOMOVE | SWP_SHOWWINDOW);
	UpdateWindow(logoWindow);

	// AVI
	g_bIntroFinished			= true;

	g_sLaunchOnExit_app[0]		= 0;
	g_sLaunchOnExit_params[0]	= 0;

	LPCSTR						fsgame_ltx_name = "-fsltx ";
	string_path					fsgame = "";

	if (strstr(commandLine, fsgame_ltx_name)) {
		int						sz = xr_strlen(fsgame_ltx_name);
		sscanf					(strstr(commandLine,fsgame_ltx_name)+sz,"%[^ ] ",fsgame);
	}

	Core._initialize			("xray", nullptr, TRUE, fsgame[0] ? fsgame : nullptr);

	InitSettings				();

	// Adjust player & computer name for Asian
	if ( pSettings->line_exist( "string_table" , "no_native_input" ) ) {
			xr_strcpy( Core.UserName , sizeof( Core.UserName ) , "Player" );
			xr_strcpy( Core.CompName , sizeof( Core.CompName ) , "Computer" );
	}

	{
		FPU::m24r				();
		InitEngine				();

		InitInput				();

		InitConsole				();

		Engine.External.CreateRendererList();

		LPCSTR benchName = "-batch_benchmark ";
		if(strstr(commandLine, benchName))
		{
			int sz = xr_strlen(benchName);
			string64				b_name;
			sscanf					(strstr(Core.Params,benchName)+sz,"%[^ ] ",b_name);
			doBenchmark				(b_name);
			return 0;
		}

		if(strstr(Core.Params,"-r2a"))	
			Console->Execute			("renderer renderer_r2a");
		else
		if(strstr(Core.Params,"-r2"))	
			Console->Execute			("renderer renderer_r2");
		else
		{
			CCC_LoadCFG_custom*	pTmp = xr_new<CCC_LoadCFG_custom>("renderer ");
			pTmp->Execute				(Console->ConfigFile);
			xr_delete					(pTmp);
		}

		Engine.External.Initialize	( );
		Console->Execute			("stat_memory");

		Startup	 					( );
		Core._destroy				( );
#ifdef NO_MULTI_INSTANCES		
		// Delete application presence mutex
		CloseHandle( hCheckPresenceMutex );
#endif
	}
	return 0;
}

LPCSTR _GetFontTexName (LPCSTR section)
{
	static char* tex_names[]={"texture800","texture","texture1600"};
	int def_idx		= 1;//default 1024x768
	int idx			= def_idx;

	u32 h = Device.dwHeight;

	if(h<=600)		idx = 0;
	else if(h<1024)	idx = 1;
	else 			idx = 2;
	
	while(idx>=0)
	{
		if( pSettings->line_exist(section,tex_names[idx]))
			return pSettings->r_string(section,tex_names[idx]);
		--idx;
	}
	return pSettings->r_string(section,tex_names[def_idx]);
}

void _InitializeFont(CGameFont*& F, LPCSTR section, u32 flags)
{
	LPCSTR font_tex_name = _GetFontTexName(section);
	R_ASSERT(font_tex_name);

	LPCSTR sh_name = pSettings->r_string(section,"shader");
	if(!F){
		F = xr_new<CGameFont> (sh_name, font_tex_name, flags);
	}else
		F->Initialize(sh_name, font_tex_name);

	if (pSettings->line_exist(section,"size")){
		float sz = pSettings->r_float(section,"size");
		if (flags&CGameFont::fsDeviceIndependent)	F->SetHeightI(sz);
		else										F->SetHeight(sz);
	}
	if (pSettings->line_exist(section,"interval"))
		F->SetInterval(pSettings->r_fvector2(section,"interval"));

}

CApplication::CApplication()
{
	ll_dwReference	= 0;

	max_load_stage = 0;

	// events
	eQuit						= Engine.Event.Handler_Attach("KERNEL:quit",this);
	eStart						= Engine.Event.Handler_Attach("KERNEL:start",this);
	eStartLoad					= Engine.Event.Handler_Attach("KERNEL:load",this);
	eDisconnect					= Engine.Event.Handler_Attach("KERNEL:disconnect",this);
	eConsole					= Engine.Event.Handler_Attach("KERNEL:console",this);
	eStartMPDemo				= Engine.Event.Handler_Attach("KERNEL:start_mp_demo",this);

	// levels
	Level_Current				= u32(-1);
	Level_Scan					( );

	// Font
	pFontSystem					= NULL;

	// Register us
	Device.seqFrame.Add			(this, REG_PRIORITY_HIGH+1000);
	
	if (psDeviceFlags.test(mtSound))	Device.seqFrameMT.Add		(&SoundProcessor);
	else								Device.seqFrame.Add			(&SoundProcessor);

	Console->Show				( );

	// App Title
//	app_title[ 0 ] = '\0';
	ls_header[ 0 ] = '\0';
	ls_tip_number[ 0 ] = '\0';
	ls_tip[ 0 ] = '\0';
}

CApplication::~CApplication()
{
	Console->Hide				( );

	// font
	xr_delete					( pFontSystem		);

	Device.seqFrameMT.Remove	(&SoundProcessor);
	Device.seqFrame.Remove		(&SoundProcessor);
	Device.seqFrame.Remove		(this);


	// events
	Engine.Event.Handler_Detach	(eConsole,this);
	Engine.Event.Handler_Detach	(eDisconnect,this);
	Engine.Event.Handler_Detach	(eStartLoad,this);
	Engine.Event.Handler_Detach	(eStart,this);
	Engine.Event.Handler_Detach	(eQuit,this);
	Engine.Event.Handler_Detach	(eStartMPDemo,this);
	
}

extern CRenderDevice Device;

void CApplication::OnEvent(EVENT E, u64 P1, u64 P2)
{
	if (E==eQuit)
	{
		PostQuitMessage	(0);
		
		for (u32 i=0; i<Levels.size(); i++)
		{
			xr_free(Levels[i].folder);
			xr_free(Levels[i].name);
		}
	}
	else if(E==eStart) 
	{
		LPSTR		op_server		= LPSTR	(P1);
		LPSTR		op_client		= LPSTR	(P2);
		Level_Current				= u32(-1);
		R_ASSERT	(!g_pGameLevel);
		R_ASSERT	(g_pGamePersistent);
		{		
			Console->Execute("main_menu off");
			Console->Hide();
//!			this line is commented by Dima
//!			because I don't see any reason to reset device here
			//-----------------------------------------------------------
			g_pGamePersistent->PreStart		(op_server);
			//-----------------------------------------------------------
			g_pGameLevel					= (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
			pApp->LoadBegin					(); 
			g_pGamePersistent->Start		(op_server);
			g_pGameLevel->net_Start			(op_server,op_client);
			pApp->LoadEnd					(); 
		}
		xr_free							(op_server);
		xr_free							(op_client);
	} 
	else if (E==eDisconnect) 
	{
		ls_header[0] = '\0';
		ls_tip_number[0] = '\0';
		ls_tip[0] = '\0';

		if (g_pGameLevel) 
		{
			Console->Hide			();
			g_pGameLevel->net_Stop	();
			DEL_INSTANCE			(g_pGameLevel);
			Console->Show			();
			
			if( (FALSE == Engine.Event.Peek("KERNEL:quit")) &&(FALSE == Engine.Event.Peek("KERNEL:start")) )
			{
				Console->Execute("main_menu off");
				Console->Execute("main_menu on");
			}
		}
		R_ASSERT			(0!=g_pGamePersistent);
		g_pGamePersistent->Disconnect();
	}
	else if (E == eConsole)
	{
		LPSTR command				= (LPSTR)P1;
		Console->ExecuteCommand		( command, false );
		xr_free						(command);
	}
	else if (E == eStartMPDemo)
	{
		LPSTR demo_file				= LPSTR	(P1);

		R_ASSERT	(0==g_pGameLevel);
		R_ASSERT	(0!=g_pGamePersistent);

		Console->Execute("main_menu off");
		Console->Hide();
		Device.Reset					(false);

		g_pGameLevel					= (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
		shared_str server_options		= g_pGameLevel->OpenDemoFile(demo_file);
		
		//-----------------------------------------------------------
		g_pGamePersistent->PreStart		(server_options.c_str());
		//-----------------------------------------------------------
		
		pApp->LoadBegin					(); 
		g_pGamePersistent->Start		("");//server_options.c_str()); - no prefetch !
		g_pGameLevel->net_StartPlayDemo	();
		pApp->LoadEnd					(); 

		xr_free						(demo_file);
	}
}

static	CTimer	phase_timer		;
extern	ENGINE_API BOOL			g_appLoaded = FALSE;

void CApplication::LoadBegin	()
{
	ll_dwReference++;
	if (1==ll_dwReference)	{

		g_appLoaded			= FALSE;
#ifdef SPAWN_ANTIFREEZE
		g_bootComplete		= false;
#endif
#ifndef DEDICATED_SERVER
		_InitializeFont		(pFontSystem,"ui_font_letterica18_russian",0);

		m_pRender->LoadBegin();
#endif
		phase_timer.Start	();
		load_stage			= 0;

	}
}

void CApplication::LoadEnd		()
{
	ll_dwReference--;
	if (0==ll_dwReference)		{
		Msg						("* phase time: %d ms",phase_timer.GetElapsed_ms());
		Msg						("* phase cmem: %d K", Memory.mem_usage()/1024);
		Console->Execute		("stat_memory");
		g_appLoaded				= TRUE;
//		DUMP_PHASE;
	}
}

void CApplication::destroy_loading_shaders()
{
	m_pRender->destroy_loading_shaders();
#ifdef SPAWN_ANTIFREEZE
	g_bootComplete = true;
#endif
}

PROTECT_API void CApplication::LoadDraw		()
{
	if(g_appLoaded)				return;
	Device.dwFrame				+= 1;

	if(!Device.Begin () )		return;
	load_draw_internal			();
	Device.End					();
}

void CApplication::SetLoadStageTitle(const char* _ls_title)
{
	xr_strcpy(ls_title, _ls_title);
}

void CApplication::LoadTitleInt(LPCSTR str1, LPCSTR str2, LPCSTR str3)
{
	xr_strcpy					(ls_header, str1);
	xr_strcpy					(ls_tip_number, str2);
	xr_strcpy					(ls_tip, str3);
//	LoadDraw					();
}
void CApplication::LoadStage()
{
	load_stage++;
	VERIFY						(ll_dwReference);
	Msg							("* phase time: %d ms",phase_timer.GetElapsed_ms());	phase_timer.Start();
//	Msg							("* phase cmem: %d K", Memory.mem_usage()/1024);
	
    if  (g_pGamePersistent->GameType()==1)//[FX]: "alife" add on clicked new_game in main_manu !!! DON'T EDIT THIS LINE
		max_load_stage			= 17;
	else
		max_load_stage			= 14;
	LoadDraw					();
}

void CApplication::LoadSwitch	()
{
}

// Sequential
void CApplication::OnFrame	( )
{
	Engine.Event.OnFrame			();
	g_SpatialSpace->update			();
	g_SpatialSpacePhysic->update	();
	if (g_pGameLevel)				
		g_pGameLevel->SoundEvent_Dispatch	( );
}

void CApplication::Level_Append		(LPCSTR folder)
{
	string_path	N1,N2,N3,N4;
	strconcat	(sizeof(N1),N1,folder,"level");
	strconcat	(sizeof(N2),N2,folder,"level.ltx");
	strconcat	(sizeof(N3),N3,folder,"level.geom");
	strconcat	(sizeof(N4),N4,folder,"level.cform");
	if	(
		FS.exist("$game_levels$",N1)		&&
		FS.exist("$game_levels$",N2)		&&
		FS.exist("$game_levels$",N3)		&&
		FS.exist("$game_levels$",N4)	
		)
	{
		sLevelInfo			LI;
		LI.folder			= xr_strdup(folder);
		LI.name				= 0;
		Levels.push_back	(LI);
	}
}

void CApplication::Level_Scan()
{
	for (u32 i=0; i<Levels.size(); i++)
	{
		xr_free(Levels[i].folder);
		xr_free(Levels[i].name);
	}
	Levels.clear	();

	xr_vector<char*>* folder			= FS.file_list_open		("$game_levels$",FS_ListFolders|FS_RootOnly);
	
	for (u32 i=0; i<folder->size(); ++i)	
		Level_Append((*folder)[i]);
	
	FS.file_list_close		(folder);
}

void gen_logo_name(string_path& dest, LPCSTR level_name, int num)
{
	strconcat	(sizeof(dest), dest, "intro\\intro_", level_name);
	
	u32 len = xr_strlen(dest);
	if(dest[len-1]=='\\')
		dest[len-1] = 0;

	string16 buff;
	xr_strcat(dest, sizeof(dest), "_");
	xr_strcat(dest, sizeof(dest), itoa(num+1, buff, 10));
}

void CApplication::Level_Set(u32 L)
{
	if (L >= Levels.size())	return;
	FS.get_path("$level$")->_set(Levels[L].folder);

	static string_path			path;

	if (Level_Current != L)
	{
		path[0] = 0;

		Level_Current = L;

		int count = 0;
		while (true)
		{
			string_path			temp2;
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
	bool arch_res					= false;

	for(auto it = FS.m_archives.begin(); it != FS.m_archives.end(); ++it)
	{
		CLocatorAPI::archive& A		= *it;
		if(A.hSrcFile==NULL)
		{
			LPCSTR ln = A.header->r_string("header", "level_name");
			LPCSTR lv = A.header->r_string("header", "level_ver");
			if ( 0==_stricmp(ln,name) && 0==_stricmp(lv,ver) )
			{
				FS.LoadArchive(A);
				arch_res = true;
			}
		}
	}

	if( arch_res )
		Level_Scan							();
	
	string256		buffer;
	strconcat		(sizeof(buffer),buffer,name,"\\");
	for (u32 I=0; I<Levels.size(); ++I)
	{
		if (0==stricmp(buffer,Levels[I].folder))	
		{
			result = int(I);	
			break;
		}
	}

	if(bSet && result!=-1)
		Level_Set(result);

	if( arch_res )
		g_pGamePersistent->OnAssetsChanged	();

	return result;
}

CInifile*  CApplication::GetArchiveHeader(LPCSTR name, LPCSTR ver)
{
	for(auto it = FS.m_archives.begin(); it!=FS.m_archives.end(); ++it)
	{
		CLocatorAPI::archive& A		= *it;

		LPCSTR ln = A.header->r_string("header", "level_name");
		LPCSTR lv = A.header->r_string("header", "level_ver");
		if(!_stricmp(ln,name) && 0==_stricmp(lv, ver))
		{
			return A.header;
		}
	}
	return nullptr;
}

void CApplication::LoadAllArchives()
{
	if( FS.load_all_unloaded_archives() )
	{
		Level_Scan							();
		g_pGamePersistent->OnAssetsChanged	();
	}
}

void doBenchmark(LPCSTR name)
{
	g_bBenchmark = true;
	string_path in_file;
	FS.update_path(in_file,"$app_data_root$", name);
	CInifile ini(in_file);
	int test_count = ini.line_count("benchmark");
	LPCSTR test_name,t;
	shared_str test_command;
	for(int i=0;i<test_count;++i){
		ini.r_line			( "benchmark", i, &test_name, &t);
		xr_strcpy				(g_sBenchmarkName, test_name);
		
		test_command		= ini.r_string_wb("benchmark",test_name);
		xr_strcpy			(Core.Params,*test_command);
		_strlwr_s				(Core.Params);
		
		InitInput					();
		if(i) InitEngine();

		Engine.External.Initialize	( );

		xr_strcpy						(Console->ConfigFile,"user.ltx");
		if (strstr(Core.Params,"-ltx ")) {
			string64				c_name;
			sscanf					(strstr(Core.Params,"-ltx ")+5,"%[^ ] ",c_name);
			xr_strcpy				(Console->ConfigFile,c_name);
		}

		Startup	 				();
	}
}
#pragma optimize("g", off)
void CApplication::load_draw_internal()
{
	m_pRender->load_draw_internal(*this);
}
