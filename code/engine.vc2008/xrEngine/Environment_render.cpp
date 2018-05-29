#include "stdafx.h"
#pragma hdrstop

#include "Environment.h"
#ifndef _EDITOR
    #include "render.h"
#endif
#include "xr_efflensflare.h"
#include "rain.h"
#include "thunderbolt.h"
#	include "igame_level.h"

//-----------------------------------------------------------------------------
// Environment render
//-----------------------------------------------------------------------------
extern ENGINE_API float psHUD_FOV;
//BOOL bNeed_re_create_env = FALSE;
void CEnvironment::RenderSky		()
{
#ifndef _EDITOR
	if (0==g_pGameLevel)		return;
#endif

	m_pRender->RenderSky(*this);
	
	
}

void CEnvironment::RenderClouds			()
{
#ifndef _EDITOR
	if (0==g_pGameLevel)		return	;
#endif
	// draw clouds
	if (fis_zero(CurrentEnv->clouds_color.w,EPS_L))	return;

	m_pRender->RenderClouds(*this);
	/*

	::Render->rmFar				();

	Fmatrix						mXFORM, mScale;
	mScale.scale				(10,0.4f,10);
	mXFORM.rotateY				(CurrentEnv->sky_rotation);
	mXFORM.mulB_43				(mScale);
	mXFORM.translate_over		(Device.vCameraPosition);

	Fvector wd0,wd1;
	Fvector4 wind_dir;
	wd0.setHP					(PI_DIV_4,0);
	wd1.setHP					(PI_DIV_4+PI_DIV_8,0);
	wind_dir.set				(wd0.x,wd0.z,wd1.x,wd1.z).mul(0.5f).add(0.5f).mul(255.f);
	u32		i_offset,v_offset;
	u32		C0					= color_rgba(iFloor(wind_dir.x),iFloor(wind_dir.y),iFloor(wind_dir.w),iFloor(wind_dir.z));
	u32		C1					= color_rgba(iFloor(CurrentEnv->clouds_color.x*255.f),iFloor(CurrentEnv->clouds_color.y*255.f),iFloor(CurrentEnv->clouds_color.z*255.f),iFloor(CurrentEnv->clouds_color.w*255.f));

	// Fill index buffer
	u16*	pib					= RCache.Index.Lock	(CloudsIndices.size(),i_offset);
	CopyMemory					(pib,&CloudsIndices.front(),CloudsIndices.size()*sizeof(u16));
	RCache.Index.Unlock			(CloudsIndices.size());

	// Fill vertex buffer
	v_clouds* pv				= (v_clouds*)	RCache.Vertex.Lock	(CloudsVerts.size(),clouds_geom.stride(),v_offset);
	for (FvectorIt it=CloudsVerts.begin(); it!=CloudsVerts.end(); it++,pv++)
		pv->set					(*it,C0,C1);
	RCache.Vertex.Unlock		(CloudsVerts.size(),clouds_geom.stride());

	// Render
	RCache.set_xform_world		(mXFORM);
	RCache.set_Geometry			(clouds_geom);
	RCache.set_Shader			(clouds_sh);
	RCache.set_Textures			(&CurrentEnv->clouds_r_textures);
	RCache.Render				(D3DPT_TRIANGLELIST,v_offset,0,CloudsVerts.size(),i_offset,CloudsIndices.size()/3);

	::Render->rmNormal			();
	*/
}

void CEnvironment::RenderFlares		()
{
#ifndef _EDITOR
	if (0==g_pGameLevel)			return	;
#endif
	// 1
	eff_LensFlare->Render			(FALSE,TRUE,TRUE);
}

void CEnvironment::RenderLast		()
{
#ifndef _EDITOR
	if (0==g_pGameLevel)			return	;
#endif
	// 2
	eff_Rain->Render				();
	eff_Thunderbolt->Render			();
}

void CEnvironment::OnDeviceCreate()
{
//.	bNeed_re_create_env			= TRUE;
	m_pRender->OnDeviceCreate();
	/*
	sh_2sky.create			(&m_b_skybox,"skybox_2t");
	sh_2geom.create			(v_skybox_fvf,RCache.Vertex.Buffer(), RCache.Index.Buffer());
	clouds_sh.create		("clouds","null");
	clouds_geom.create		(v_clouds_fvf,RCache.Vertex.Buffer(), RCache.Index.Buffer());
	*/

	// weathers
	{
		auto _I		= WeatherCycles.begin();
		auto _E		= WeatherCycles.end();
		for (; _I!=_E; _I++)
			for (auto it=_I->second.begin(); it!=_I->second.end(); it++)
				(*it)->on_device_create();
	}
	// effects
	{
		auto _I		= WeatherFXs.begin();
		auto _E		= WeatherFXs.end();
		for (; _I!=_E; _I++)
			for (auto it=_I->second.begin(); it!=_I->second.end(); it++)
				(*it)->on_device_create();
	}


	Invalidate	();
	//OnFrame		();
}

void CEnvironment::OnDeviceDestroy()
{
	m_pRender->OnDeviceDestroy();
	/*
	tsky0->surface_set						(NULL);
	tsky1->surface_set						(NULL);
	
	sh_2sky.destroy							();
	sh_2geom.destroy						();
	clouds_sh.destroy						();
	clouds_geom.destroy						();
	*/
	// weathers
	{
		auto _I		= WeatherCycles.begin();
		auto _E		= WeatherCycles.end();
		for (; _I!=_E; _I++)
			for (auto it=_I->second.begin(); it!=_I->second.end(); it++)
				(*it)->on_device_destroy();
	}
	// effects
	{
		auto _I		= WeatherFXs.begin();
		auto _E		= WeatherFXs.end();
		for (; _I!=_E; _I++)
			for (auto it=_I->second.begin(); it!=_I->second.end(); it++)
				(*it)->on_device_destroy();
	}
	CurrentEnv->destroy();

}
