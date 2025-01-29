#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "PK-SampleLib/PKSample.h"

#include <pk_particles/include/ps_config.h> // PK_COMPILER_BUILD_COMPILER

 #if defined(PK_DESKTOP_TOOLS) && !defined(PK_RETAIL) && (PK_COMPILER_BUILD_COMPILER != 0)
 #	define	PK_HAS_PARTICLES_SELECTION	1
 #else
 #	define	PK_HAS_PARTICLES_SELECTION	0
 #endif // !defined(PK_RETAIL)

#if	(PK_HAS_PARTICLES_SELECTION != 0)

#include <pk_render_helpers/include/draw_requests/rh_billboard_cpu.h>
#include <pk_render_helpers/include/draw_requests/rh_ribbon_cpu.h>
#include <pk_render_helpers/include/draw_requests/rh_copystream_cpu.h>
#include <pk_render_helpers/include/draw_requests/rh_mesh_cpu.h>
#include <pk_render_helpers/include/draw_requests/rh_triangle_cpu.h>

#include <pk_rhi/include/interfaces/IGpuBuffer.h>

#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
#include <pk_particles/include/Storage/D3D11/storage_d3d11.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
#include <pk_particles/include/Storage/D3D12/storage_d3d12.h>
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12U != 0)
#include <pk_particles/include/Storage/D3D12U/storage_d3d12U.h>
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SMediumParticleSelection
{
	struct	SRange
	{
		u32					m_StartIdAndCount;

		SRange(u32 startId, u32 count)
		:	m_StartIdAndCount((startId << 16) | (count & 0xFFFF))
		{
			// Assert for truncation during packing
			PK_ASSERT(StartID() == startId);
			PK_ASSERT(Count() == count);
		}

		PK_FORCEINLINE u32	StartID() const { return m_StartIdAndCount >> 16; }
		PK_FORCEINLINE u32	Count() const { return m_StartIdAndCount & 0xFFFF; }

		bool		operator < (const SRange &oth) const { return StartID() < oth.StartID(); }
		bool		operator <= (const SRange &oth) const { return StartID() <= oth.StartID(); }
		bool		operator == (const SRange &oth) const { return StartID() == oth.StartID(); }
	};

	struct	SSelectedRange
	{
		SRange				m_Range;
		u32					m_PageId;

		SSelectedRange(u32 startId, u32 count, u32 pageId)
		:	m_Range(startId, count)
		,	m_PageId(pageId)
		{
			// Assert for truncation during packing
			PK_ASSERT(StartID() == startId);
			PK_ASSERT(Count() == count);
			PK_ASSERT(PageID() == pageId);
		}

		PK_FORCEINLINE u32		StartID() const { return m_Range.StartID(); }
		PK_FORCEINLINE u32		Count() const { return m_Range.Count(); }
		PK_FORCEINLINE u32		PageID() const { return m_PageId; }

		bool		operator < (const SSelectedRange &oth) const { return m_PageId < oth.m_PageId || (m_PageId == oth.m_PageId && m_Range < oth.m_Range); }
		bool		operator <= (const SSelectedRange &oth) const { return m_PageId < oth.m_PageId || (m_PageId == oth.m_PageId && m_Range <= oth.m_Range); }
		bool		operator == (const SSelectedRange &oth) const { return m_PageId == oth.m_PageId && m_Range == oth.m_Range; }
	};

	const CParticleMedium		*m_Medium = null;
	TArray<SSelectedRange>		m_Ranges;
	CGuid						m_CurrentSelectedId;
};

//----------------------------------------------------------------------------

struct	SMediumParticleSelection_GPU : public CRefCountedObject
{
	const CParticleMedium			*m_Medium = null;
#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
	SBuffer_D3D11					m_BufferD3D11;
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
	SBuffer_D3D12					m_BufferD3D12;
#endif
	bool							m_Processed = false;

	~SMediumParticleSelection_GPU()
	{
#if	(PK_PARTICLES_UPDATER_USE_D3D11 != 0)
		m_BufferD3D11.Release();
#endif
#if	(PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		m_BufferD3D12.Release();
#endif
	}
};

typedef		TRefPtr<SMediumParticleSelection_GPU>	PMediumParticleSelection_GPU;

//----------------------------------------------------------------------------

struct	SRendererSelection
{
	TArray<const CRendererDataBase*>	m_Renderers;

	SRendererSelection() {}
};

struct	SMediumParticleSelection_GPU_Context
{
	struct	SSelectionInfoData	// Keep up-to-date with PKSample::CreateEditorSelectorConstantSetLayout
	{
		CUint4		m_PositionsOffsets;
		CUint4		m_RadiusOffsets;
		CUint4		m_EnabledOffsets;
		CFloat4		m_RayOrigin;
		CFloat4		m_RayDirection;
		CFloat4x4	m_Planes;
		CUint4		m_Mode; // only 'x()' is used.
	};

	enum	EMode
	{
		ModeSelectWithRay = 0,
		ModeSelectWithRect = 1,
		ModeSelectAll = 2,
		ModeParticleAdvanceNext = 8,
		ModeParticleAdvancePrev = 9,
		ModeParticleReset = 10,
	};

	RHI::PGpuBuffer				m_StreamBuffer;
	RHI::PGpuBuffer				m_StreamInfo;
	RHI::PGpuBuffer				m_DstBuffer;
	TArray<SSelectionInfoData>	m_ConstantData;

	PMediumParticleSelection_GPU	m_Selection;
};

//----------------------------------------------------------------------------

struct	SEffectParticleSelection
{
	TArray<SMediumParticleSelection>				m_AllParticleSelections;
	TArray<SMediumParticleSelection_GPU_Context>	m_AllParticleSelections_GPU;
	CGuid											m_FocusedMedium; // address one of the 'm_AllParticleSelections' or 'm_AllParticleSelections_GPU'

	SEffectParticleSelection() {}

	bool	Empty() const { return m_AllParticleSelections.Empty() && m_AllParticleSelections_GPU.Empty(); }

	void	ClearParticleSelection()
	{
		m_AllParticleSelections.Clear();
		m_AllParticleSelections_GPU.Clear();
		m_FocusedMedium = CGuid::INVALID;
	}
};

//----------------------------------------------------------------------------

struct	SEffectParticleSelectionView
{
	TMemoryView<const SMediumParticleSelection>					m_AllParticleSelectionsView;
	TMemoryView<const SMediumParticleSelection_GPU_Context>		m_AllParticleSelectionsView_GPU;
	CGuid														m_FocusedMedium;

	SEffectParticleSelectionView() {}

	SEffectParticleSelectionView(const SEffectParticleSelection &fromSelection)
	:	m_AllParticleSelectionsView(fromSelection.m_AllParticleSelections)
	,	m_AllParticleSelectionsView_GPU(fromSelection.m_AllParticleSelections_GPU)
	,	m_FocusedMedium(fromSelection.m_FocusedMedium)
	{
	}

	bool	HasParticlesSelected() const { return !m_AllParticleSelectionsView.Empty(); }
	bool	HasGPUParticlesSelected() const { return !m_AllParticleSelectionsView_GPU.Empty(); }

	void	ClearAllViews()
	{
		m_AllParticleSelectionsView.Clear();
		m_AllParticleSelectionsView_GPU.Clear();
	}
};

//----------------------------------------------------------------------------
// CPU billboarding selection:

class	PK_EXPORT CBillboard_Exec_WireframeDiscard
{
public:
	TStridedMemoryView<float>						m_DstSelectedParticles;
	SEffectParticleSelectionView					m_SrcParticleSelected;

	CBillboard_Exec_WireframeDiscard() { }
	void	Clear() { Mem::Reinit(*this); }
	void	operator()(const Drawers::SBillboard_ExecPage &batch);
};

//----------------------------------------------------------------------------
// CPU billboarding selection (geometry shader):

class	PK_EXPORT CCopyStream_Exec_WireframeDiscard
{
public:
	TStridedMemoryView<float>						m_DstSelectedParticles;
	SEffectParticleSelectionView					m_SrcParticleSelected;

	void	Clear() { Mem::Reinit(*this); }
	void	operator()(const Drawers::SCopyStream_ExecPage &execPage);
};

//----------------------------------------------------------------------------
// Ribbon billboarding selection:

class	PK_EXPORT CRibbon_Exec_WireframeDiscard
{
public:
	TStridedMemoryView<float>						m_DstSelectedParticles;
	SEffectParticleSelectionView					m_SrcParticleSelected;

	CRibbon_Exec_WireframeDiscard() { }
	void	Clear() { Mem::Reinit(*this); }
	void	operator()(const Drawers::SRibbon_ExecBatch &ribbonData);
};

//----------------------------------------------------------------------------
// CPU meshes selection:

class	PK_EXPORT CMesh_Exec_WireframeDiscard
{
public:
	TStridedMemoryView<float>						m_DstSelectedParticles;
	SEffectParticleSelectionView					m_SrcParticleSelected;

	CMesh_Exec_WireframeDiscard() { }
	void	Clear() { Mem::Reinit(*this); }
	void	operator()(const Drawers::SMesh_ExecPage &batch);
};

//----------------------------------------------------------------------------
// CPU triangle selection:

class	PK_EXPORT CTriangle_Exec_WireframeDiscard
{
public:
	TStridedMemoryView<float>						m_DstSelectedParticles;
	SEffectParticleSelectionView					m_SrcParticleSelected;

	void	Clear() { Mem::Reinit(*this); }
	void	operator()(const Drawers::STriangle_ExecPage &execPage);
};

//----------------------------------------------------------------------------

// GPU-sim selection:

RHI::PGpuBuffer	GetIsSelectedBuffer(const SEffectParticleSelectionView &selectionView, const Drawers::SBillboard_DrawRequest &dr);
RHI::PGpuBuffer	GetIsSelectedBuffer(const SEffectParticleSelectionView &selectionView, const Drawers::SRibbon_DrawRequest &dr);
RHI::PGpuBuffer	GetIsSelectedBuffer(const SEffectParticleSelectionView &selectionView, const Drawers::SMesh_DrawRequest &dr);
RHI::PGpuBuffer	GetIsSelectedBuffer(const SEffectParticleSelectionView &selectionView, const Drawers::STriangle_DrawRequest &dr);

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
