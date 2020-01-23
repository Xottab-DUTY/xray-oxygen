#pragma once

#include "StateManager/dx10StateManager.h"
#include "StateManager/dx10ShaderResourceStateCache.h"

IC void CBackend::set_RT(ID3DRenderTargetView* RT, u32 ID)
{
	if (RT!=pRT[ID])
	{
		stat.target_rt	++;
		pRT[ID]			= RT;

		//	Reset all RT's here to allow RT to be bounded as input
		if (!m_bChangedRTorZB)
			HW.pContext->OMSetRenderTargets(0, 0, 0);

		m_bChangedRTorZB = true;
	}
}

IC void	CBackend::set_ZB(ID3DDepthStencilView* ZB)
{
	if (ZB!=pZB)
	{
		stat.target_zb	++;
		pZB				= ZB;

		//	Reset all RT's here to allow RT to be bounded as input
		if (!m_bChangedRTorZB)
			HW.pContext->OMSetRenderTargets(0, 0, 0);
		m_bChangedRTorZB = true;
	}
}

inline void CBackend::set_Format(SDeclaration* _decl)
{
	if (decl!=_decl)
	{
#ifdef DEBUG
		stat.decl		++;
#endif
		decl			= _decl;
	}
}

inline void CBackend::set_PS(ID3DPixelShader* _ps, LPCSTR _n)
{
	if (ps!=_ps)
	{
		stat.ps			++;
		ps				= _ps;
		HW.pContext->PSSetShader(ps, 0, 0);
		ps_name			= _n;
	}
}

inline void CBackend::set_GS(ID3DGeometryShader* _gs, LPCSTR _n)
{
	if (gs!=_gs)
	{
		gs				= _gs;
		HW.pContext->GSSetShader(gs, 0, 0);

#ifdef DEBUG
		gs_name			= _n;
#endif
	}
}

inline void CBackend::set_HS(ID3D11HullShader* _hs, LPCSTR _n)
{
	if (hs!=_hs)
	{
		hs				= _hs;
		HW.pContext->HSSetShader(hs, 0, 0);

#ifdef DEBUG
		hs_name			= _n;
#endif
	}
}

inline void CBackend::set_DS(ID3D11DomainShader* _ds, LPCSTR _n)
{
	if (ds!=_ds)
	{
		ds				= _ds;
		HW.pContext->DSSetShader(ds, 0, 0);

#ifdef DEBUG
		ds_name			= _n;
#endif
	}
}

inline void CBackend::set_CS(ID3D11ComputeShader* _cs, LPCSTR _n)
{
	if (cs!=_cs)
	{
		//	TODO: DX10: Get statistics for D Shader change
		//stat.cs			++;
		cs				= _cs;
		HW.pContext->CSSetShader(cs, 0, 0);

#ifdef DEBUG
		cs_name			= _n;
#endif
	}
}

inline	bool CBackend::is_TessEnabled()
{
	return HW.FeatureLevel>=D3D_FEATURE_LEVEL_11_0 && (ds!=0 || hs!=0);
}


inline void CBackend::set_VS(ID3DVertexShader* _vs, LPCSTR _n)
{
	if (vs!=_vs)
	{
		stat.vs			++;
		vs				= _vs;
		HW.pContext->VSSetShader(vs, 0, 0);
#ifdef DEBUG
		vs_name			= _n;
#endif
	}
}

inline void CBackend::set_Vertices(ID3DVertexBuffer* _vb, u32 _vb_stride)
{
	if ((vb!=_vb) || (vb_stride!=_vb_stride))
	{
#ifdef DEBUG
		stat.vb			++;
#endif
		vb				= _vb;
		vb_stride		= _vb_stride;

		u32	iOffset = 0;
		HW.pContext->IASetVertexBuffers( 0, 1, &vb, &_vb_stride, &iOffset);
	}
}

inline void CBackend::set_Indices(ID3DIndexBuffer* _ib)
{
	if (ib!=_ib)
	{
#ifdef DEBUG
		stat.ib			++;
#endif
		ib				= _ib;
		HW.pContext->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
	}
}

IC D3D_PRIMITIVE_TOPOLOGY TranslateTopology(D3DPRIMITIVETYPE T)
{
	static	D3D_PRIMITIVE_TOPOLOGY translateTable[] =
	{
		D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,		//	None
		D3D_PRIMITIVE_TOPOLOGY_POINTLIST,		//	D3DPT_POINTLIST = 1,
		D3D_PRIMITIVE_TOPOLOGY_LINELIST,		//	D3DPT_LINELIST = 2,
		D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,		//	D3DPT_LINESTRIP = 3,
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,	//	D3DPT_TRIANGLELIST = 4,
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,	//	D3DPT_TRIANGLESTRIP = 5,
		D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,		//	D3DPT_TRIANGLEFAN = 6,
	};

	VERIFY(T<sizeof(translateTable)/sizeof(translateTable[0]));
	VERIFY(T>=0);

	D3D_PRIMITIVE_TOPOLOGY	result = translateTable[T];

	VERIFY( result != D3D_PRIMITIVE_TOPOLOGY_UNDEFINED );

	return result;
}

IC u32 GetIndexCount(D3DPRIMITIVETYPE T, u32 iPrimitiveCount)
{
	switch (T)
	{
	case D3DPT_POINTLIST:
		return iPrimitiveCount;
	case D3DPT_LINELIST:
		return iPrimitiveCount*2;
	case D3DPT_LINESTRIP:
		return iPrimitiveCount+1;
	case D3DPT_TRIANGLELIST:
		return iPrimitiveCount*3;
	case D3DPT_TRIANGLESTRIP:
		return iPrimitiveCount+2;
	default: NODEFAULT;
#ifdef DEBUG
		return 0;
#endif // #ifdef DEBUG
	}
}

IC void CBackend::ApplyPrimitieTopology( D3D_PRIMITIVE_TOPOLOGY Topology )
{
	if ( m_PrimitiveTopology != Topology )
	{
		m_PrimitiveTopology = Topology;
		HW.pContext->IASetPrimitiveTopology(m_PrimitiveTopology);
	}
}

IC void CBackend::Compute(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	stat.calls++;

	SRVSManager.Apply();
	StateManager.Apply();
	//	State manager may alter constants
	constants.flush();
	HW.pContext->Dispatch(ThreadGroupCountX,ThreadGroupCountY,ThreadGroupCountZ);
}

inline void CBackend::ResolveSubresource(ID3DResource *pDstResource, UINT DstSubresource, ID3DResource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
	HW.pContext->ResolveSubresource	(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}

inline void CBackend::Clear(u32 Count, const D3DRECT* pRects, u32 Flags, u32 Color, float Z, u32 Stencil)
{
	if (Flags & D3DCLEAR_TARGET)
	{
		// Convert u32 color to float
		float r = (float)color_get_R(Color);
		float g = (float)color_get_G(Color);
		float b = (float)color_get_B(Color);
		float a = (float)color_get_A(Color);
		float clearColor[4] = { r/255.0f, g/255.0f, b/255.0f, a/255.0f };

		// Clear all RTs
		for (u32 i = 0; i < 3; ++i)
		{
			ID3DRenderTargetView* pRenderTarget = RCache.get_RT(i);
			if (pRenderTarget)
				HW.pContext->ClearRenderTargetView(pRenderTarget, clearColor);
		}
	}

	u32 dsFlags = 0;
	if (Flags & D3DCLEAR_ZBUFFER) dsFlags |= D3D_CLEAR_DEPTH;
	if (Flags & D3DCLEAR_STENCIL) dsFlags |= D3D_CLEAR_STENCIL;

	if (dsFlags != 0)
		HW.pContext->ClearDepthStencilView(RCache.get_ZB(), dsFlags, Z, (u8)Stencil);
}

IC void CBackend::Render(D3DPRIMITIVETYPE PrimitiveType, u32 baseV, u32 startV, u32 countV, u32 startI, u32 PC)
{
	D3D_PRIMITIVE_TOPOLOGY Topology = TranslateTopology(PrimitiveType);
	u32	iIndexCount = GetIndexCount(PrimitiveType, PC);

	//!!! HACK !!!
	if (hs != 0 || ds != 0)
	{
		R_ASSERT(Topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Topology = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	}

	stat.calls++;
	stat.verts += countV;
	stat.polys += PC;

	ApplyPrimitieTopology(Topology);
	
	SRVSManager.Apply();
	ApplyRTandZB();
	ApplyVertexLayout();
	StateManager.Apply();
	//	State manager may alter constants
	constants.flush();
	HW.pContext->DrawIndexed(iIndexCount, startI, baseV);
}

IC void CBackend::Render(D3DPRIMITIVETYPE PrimitiveType, u32 startV, u32 PC)
{
	//	TODO: DX10: Remove triangle fan usage from the engine
	if (PrimitiveType == D3DPT_TRIANGLEFAN)
		return;

	D3D_PRIMITIVE_TOPOLOGY Topology = TranslateTopology(PrimitiveType);
	u32	iVertexCount = GetIndexCount(PrimitiveType, PC);

	stat.calls++;
	stat.verts += 3*PC;
	stat.polys += PC;

	ApplyPrimitieTopology(Topology);
	SRVSManager.Apply();
	ApplyRTandZB();
	ApplyVertexLayout();
	StateManager.Apply();
	//	State manager may alter constants
	constants.flush();
	HW.pContext->Draw(iVertexCount, startV);
}

IC void CBackend::set_Geometry(SGeometry* _geom)
{
	set_Format			(&*_geom->dcl);
	
	set_Vertices		(_geom->vb, _geom->vb_stride);
	set_Indices			(_geom->ib);
}

IC void	CBackend::set_Scissor(Irect*	R)
{
	if (R)			
	{
		StateManager.EnableScissoring();
		RECT	*	clip	= (RECT	*)R;
		HW.pContext->RSSetScissorRects(1, clip);
	} 
	else
	{
		StateManager.EnableScissoring(FALSE);
		HW.pContext->RSSetScissorRects(0, 0);
	}
}

IC void CBackend::set_Stencil(u32 _enable, u32 _func, u32 _ref, u32 _mask, u32 _writemask, u32 _fail, u32 _pass, u32 _zfail)
{
	StateManager.SetStencil(_enable, _func, _ref, _mask, _writemask, _fail, _pass, _zfail);
}

IC  void CBackend::set_Z(u32 _enable)
{
	StateManager.SetDepthEnable(_enable);
}

IC  void CBackend::set_ZFunc(u32 _func)
{
	StateManager.SetDepthFunc(_func);
}

IC  void CBackend::set_AlphaRef(u32 _value)
{
	//	TODO: DX10: Implement rasterizer state update to support alpha ref
	VERIFY(!"Not implemented.");
}

IC void	CBackend::set_ColorWriteEnable(u32 _mask )
{
	StateManager.SetColorWriteEnable(_mask);
}
inline void CBackend::set_CullMode(u32 _mode)
{
	StateManager.SetCullMode(_mode);
}

inline void CBackend::set_Fill(u32 _mode)
{
	StateManager.SetFillMode(_mode);
}

IC void CBackend::ApplyVertexLayout()
{
	VERIFY(vs);
	VERIFY(decl);
	VERIFY(m_pInputSignature);

	xr_map<ID3DBlob*, ID3DInputLayout*>::iterator	it;

	it = decl->vs_to_layout.find(m_pInputSignature);

	if (it==decl->vs_to_layout.end())
	{
		ID3DInputLayout* pLayout;

		CHK_DX(HW.pDevice->CreateInputLayout(
			&decl->dx10_dcl_code[0],
			(UINT)decl->dx10_dcl_code.size()-1,
			m_pInputSignature->GetBufferPointer(),
			m_pInputSignature->GetBufferSize(),
			&pLayout
			));

		it = decl->vs_to_layout.insert(
			std::pair<ID3DBlob*, ID3DInputLayout*>(m_pInputSignature, pLayout)).first;
	}

	if ( m_pInputLayout != it->second)
	{
		m_pInputLayout = it->second;
		HW.pContext->IASetInputLayout(m_pInputLayout);
	}	
}

inline void CBackend::set_VS(ref_vs& _vs)
{
	m_pInputSignature = _vs->signature->signature;
	set_VS(_vs->vs,_vs->cName.c_str());
}

inline void CBackend::set_VS(SVS* _vs)
{
	m_pInputSignature = _vs->signature->signature;
	set_VS(_vs->vs,_vs->cName.c_str());
}

IC bool CBackend::CBuffersNeedUpdate( ref_cbuffer buf1[MaxCBuffers], ref_cbuffer buf2[MaxCBuffers], u32 &uiMin, u32 &uiMax)
{
	bool	bRes = false;
	int i=0;
	while ( (i<MaxCBuffers) && (buf1[i]==buf2[i]))
		++i;

	uiMin = i;

	for ( ; i<MaxCBuffers; ++i)
	{
		if (buf1[i]!=buf2[i])
		{
			bRes = true;
			uiMax = i;
		}
	}

	return bRes;
}

IC void CBackend::set_Constants			(R_constant_table* pConstantTable)
{
	// caching
	if (ctable==pConstantTable)	return;
	ctable			= pConstantTable;
	xforms.unmap	();
	hemi.unmap		();
	tree.unmap		();
	LOD.unmap		();
	StateManager.UnmapConstants();

	if (pConstantTable == nullptr)		return;

	//	Setup constant tables
	{
		ref_cbuffer	aPixelConstants[MaxCBuffers];
		ref_cbuffer	aVertexConstants[MaxCBuffers];
		ref_cbuffer	aGeometryConstants[MaxCBuffers];
		ref_cbuffer	aHullConstants[MaxCBuffers];
		ref_cbuffer	aDomainConstants[MaxCBuffers];
		ref_cbuffer	aComputeConstants[MaxCBuffers];

		for (int i=0; i<MaxCBuffers; ++i)
		{
			aPixelConstants[i] = m_aPixelConstants[i];
			aVertexConstants[i] = m_aVertexConstants[i];
			aGeometryConstants[i] = m_aGeometryConstants[i];
			aHullConstants[i] = m_aHullConstants[i];
			aDomainConstants[i] = m_aDomainConstants[i];
			aComputeConstants[i] = m_aComputeConstants[i];
			m_aPixelConstants[i] = 0;
			m_aVertexConstants[i] = 0;
			m_aGeometryConstants[i] = 0;
			m_aHullConstants[i] = 0;
			m_aDomainConstants[i] = 0;
			m_aComputeConstants[i] = 0;
		}
		R_constant_table::cb_table::iterator	it	= pConstantTable->m_CBTable.begin();
		R_constant_table::cb_table::iterator	end	= pConstantTable->m_CBTable.end	();
		for (; it!=end; ++it)
		{
			u32				uiBufferIndex = it->first; 

			if ( (uiBufferIndex&CB_BufferTypeMask) == CB_BufferPixelShader)
			{
				VERIFY((uiBufferIndex&CB_BufferIndexMask)<MaxCBuffers);
				m_aPixelConstants[uiBufferIndex&CB_BufferIndexMask] = it->second;
			}
			else if ( (uiBufferIndex&CB_BufferTypeMask) == CB_BufferVertexShader)
			{
				VERIFY((uiBufferIndex&CB_BufferIndexMask)<MaxCBuffers);
				m_aVertexConstants[uiBufferIndex&CB_BufferIndexMask] = it->second;
			}
			else if ( (uiBufferIndex&CB_BufferTypeMask) == CB_BufferGeometryShader)
			{
				VERIFY((uiBufferIndex&CB_BufferIndexMask)<MaxCBuffers);
				m_aGeometryConstants[uiBufferIndex&CB_BufferIndexMask] = it->second;
			}
			else if ( (uiBufferIndex&CB_BufferTypeMask) == CB_BufferHullShader)
			{
				VERIFY((uiBufferIndex&CB_BufferIndexMask)<MaxCBuffers);
				m_aHullConstants[uiBufferIndex&CB_BufferIndexMask] = it->second;
			}
			else if ( (uiBufferIndex&CB_BufferTypeMask) == CB_BufferDomainShader)
			{
				VERIFY((uiBufferIndex&CB_BufferIndexMask)<MaxCBuffers);
				m_aDomainConstants[uiBufferIndex&CB_BufferIndexMask] = it->second;
			}
			else if ( (uiBufferIndex&CB_BufferTypeMask) == CB_BufferComputeShader)
			{
				VERIFY((uiBufferIndex&CB_BufferIndexMask)<MaxCBuffers);
				m_aComputeConstants[uiBufferIndex&CB_BufferIndexMask] = it->second;
			}
			else
				VERIFY("Invalid enumeration");
		}

		ID3DBuffer*	tempBuffer[MaxCBuffers];

		u32 uiMin;
		u32 uiMax;

		if (CBuffersNeedUpdate(m_aPixelConstants, aPixelConstants, uiMin, uiMax))
		{
			++uiMax;

			for (u32 i=uiMin; i<uiMax; ++i)
			{
				if (m_aPixelConstants[i])
					tempBuffer[i] = m_aPixelConstants[i]->GetBuffer();
				else
					tempBuffer[i] = 0;
			}

			HW.pContext->PSSetConstantBuffers(uiMin, uiMax-uiMin, &tempBuffer[uiMin]);
		}
		

		if (CBuffersNeedUpdate(m_aVertexConstants, aVertexConstants, uiMin, uiMax))
		{
			++uiMax;

			for (u32 i=uiMin; i<uiMax; ++i)
			{
				if (m_aVertexConstants[i])
					tempBuffer[i] = m_aVertexConstants[i]->GetBuffer();
				else
					tempBuffer[i] = 0;
			}
			HW.pContext->VSSetConstantBuffers(uiMin, uiMax-uiMin, &tempBuffer[uiMin]);
		}

			
		if (CBuffersNeedUpdate(m_aGeometryConstants, aGeometryConstants, uiMin, uiMax))
		{
			++uiMax;

			for (u32 i=uiMin; i<uiMax; ++i)
			{
				if (m_aGeometryConstants[i])
					tempBuffer[i] = m_aGeometryConstants[i]->GetBuffer();
				else
					tempBuffer[i] = 0;
			}
			HW.pContext->GSSetConstantBuffers(uiMin, uiMax-uiMin, &tempBuffer[uiMin]);
		}

		if (CBuffersNeedUpdate(m_aHullConstants, aHullConstants, uiMin, uiMax))
		{
			++uiMax;

			for (u32 i=uiMin; i<uiMax; ++i)
			{
				if (m_aHullConstants[i])
					tempBuffer[i] = m_aHullConstants[i]->GetBuffer();
				else
					tempBuffer[i] = 0;
			}
			HW.pContext->HSSetConstantBuffers(uiMin, uiMax-uiMin, &tempBuffer[uiMin]);
		}

		if (CBuffersNeedUpdate(m_aDomainConstants, aDomainConstants, uiMin, uiMax))
		{
			++uiMax;

			for (u32 i=uiMin; i<uiMax; ++i)
			{
				if (m_aDomainConstants[i])
					tempBuffer[i] = m_aDomainConstants[i]->GetBuffer();
				else
					tempBuffer[i] = 0;
			}
			HW.pContext->DSSetConstantBuffers(uiMin, uiMax-uiMin, &tempBuffer[uiMin]);
		}

		if (CBuffersNeedUpdate(m_aComputeConstants, aComputeConstants, uiMin, uiMax))
		{
			++uiMax;

			for (u32 i=uiMin; i<uiMax; ++i)
			{
				if (m_aComputeConstants[i])
					tempBuffer[i] = m_aComputeConstants[i]->GetBuffer();
				else
					tempBuffer[i] = 0;
			}
			HW.pContext->CSSetConstantBuffers(uiMin, uiMax-uiMin, &tempBuffer[uiMin]);
		}
	}

	// process constant-loaders
	R_constant_table::c_table::iterator	it	= pConstantTable->table.begin();
	R_constant_table::c_table::iterator	end	= pConstantTable->table.end	();
	for (; it!=end; it++)	
	{
		R_constant* Cs = &**it;

		if (Cs->handler)
			Cs->handler->setup(Cs);
	}
}

inline void CBackend::ApplyRTandZB()
{
	if (m_bChangedRTorZB)
	{
		m_bChangedRTorZB = false;
		HW.pContext->OMSetRenderTargets(sizeof(pRT)/sizeof(pRT[0]), pRT, pZB);
	}
}

IC	void CBackend::get_ConstantDirect(shared_str& n, u32 DataSize, void** pVData, void** pGData, void** pPData)
{
	ref_constant pConstantRef = get_c(n);

	if (pConstantRef)
		constants.access_direct(&*pConstantRef, DataSize, pVData, pGData, pPData);
	else
	{
		if (pVData)	*pVData = 0;
		if (pGData)	*pGData = 0;
		if (pPData)	*pPData = 0;
	}
}