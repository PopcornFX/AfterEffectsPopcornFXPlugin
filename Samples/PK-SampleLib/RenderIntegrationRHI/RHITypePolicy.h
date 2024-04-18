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

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_render_helpers/include/frame_collector/rh_frame_data.h>

#include "PK-SampleLib/PKSample.h"
#include "PK-SampleLib/RenderIntegrationRHI/RendererCache.h"
#include "PK-SampleLib/RenderIntegrationRHI/SoundPoolCache.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHICustomTasks.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CRHIBillboardingBatchPolicy;

struct	SRHIDrawCall
{
	enum	EDrawCallType
	{
		DrawCall_Regular,
		DrawCall_IndexedInstanced,
		DrawCall_InstancedIndirect,
		DrawCall_IndexedInstancedIndirect,
	};

	// Only used for debug draws (Wireframe, ..)
	enum	EDebugDrawGPUBuffer
	{
		// Indices fetched in vertex shader
		DebugDrawGPUBuffer_Indices,
		// Geometry shader billboarding (billboards):
		DebugDrawGPUBuffer_Position,
		DebugDrawGPUBuffer_Size,
		DebugDrawGPUBuffer_Rotation,
		DebugDrawGPUBuffer_Axis0,
		DebugDrawGPUBuffer_Axis1,
		// Vertex shader billboarding (billboards):
		DebugDrawGPUBuffer_Texcoords,
		// Vertex shader billboarding (triangles):
		DebugDrawGPUBuffer_VertexPosition0,
		DebugDrawGPUBuffer_VertexPosition1,
		DebugDrawGPUBuffer_VertexPosition2,
		// Lights:
		DebugDrawGPUBuffer_InstancePositions,
		DebugDrawGPUBuffer_InstanceScales,
		// Meshes:
		DebugDrawGPUBuffer_InstanceTransforms,
		// Enabled:
		DebugDrawGPUBuffer_Enabled,
		// Additional field:
		DebugDrawGPUBuffer_Color,
		// Is selected:
		DebugDrawGPUBuffer_IsParticleSelected,
		// For GPU storage:
		DebugDrawGPUBuffer_ColorsOffsets,
		DebugDrawGPUBuffer_TransformsOffsets, // mesh only
		DebugDrawGPUBuffer_IndirectionOffsets, // mesh only

		_DebugDrawGPUBuffer_Count
	};

	enum	EUniformBufferSemantic
	{
		UBSemantic_GPUBillboard,
		_UBSemantic_Count
	};

	const void													*m_Batch; // Batch pointer

	EDrawCallType												m_Type;

	PRendererCacheInstance_UpdateThread							m_RendererCacheInstance;

	TArray<RHI::PGpuBuffer>										m_VertexBuffers;
	TArray<u32>													m_VertexOffsets;
	RHI::PGpuBuffer												m_IndexBuffer;
	RHI::EIndexBufferSize										m_IndexSize;

	RHI::PConstantSet											m_GPUStorageSimDataConstantSet;
	RHI::PConstantSet											m_GPUStorageOffsetsConstantSet;

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	RHI::PConstantSet											m_SelectionConstantSet;
#endif	// (PK_HAS_PARTICLES_SELECTION != 0)

	// Additional buffers info:
	TStaticArray<RHI::PGpuBuffer, _DebugDrawGPUBuffer_Count>	m_DebugDrawGPUBuffers;
	TStaticArray<u32, _DebugDrawGPUBuffer_Count>				m_DebugDrawGPUBufferOffsets;

	// Uniform buffers
	TStaticArray<RHI::PGpuBuffer, _UBSemantic_Count>			m_UBSemanticsPtr;

	// For Regular draws:
	u32															m_IndexOffset;
	u32															m_IndexCount;

	// For Geom shaders:
	u32															m_VertexOffset;
	u32															m_VertexCount;

	// For instanced draw:
	u32															m_InstanceCount;

	// For indirect draw:
	u32															m_IndirectBufferOffset;
	RHI::PGpuBuffer												m_IndirectBuffer;
	u32															m_EstimatedParticleCount;

	// Shaders info:
	u32															m_ShaderOptions;	// As the renderer cache holds multiple shaders, this is to know which one to use for this draw call

	// Only set required bytes depending on shader bindings for this draw call
	// Data type doesn't matter
	TStaticCountedArray<CFloat4, 4>								m_PushConstants;	// Geometry/Vertex shader billboarding constants

	// Editor viewport debug
	bool								m_SelectedDrawCall; // Whether the draw call is selected
	bool								m_SlicedDC; // Whether or not it's a sliced draw call
	bool								m_Valid; // Whether or not the draw call is valid for rendering. If false, fallbacks on the debug materials
	CAABB								m_BBox; // Drawcall bbox (Contains the CameraSortOffset, if any)
	CAABB								m_TotalBBox; // Drawcall's owning batch total bbox (Doesn't contain the CameraSortOffset, this is the batch's total bounding box)

	ERendererClass						m_RendererType;

	SRHIDrawCall()
	:	m_Batch(null)
	,	m_Type(DrawCall_Regular)
	,	m_RendererCacheInstance(null)
	,	m_IndexBuffer(null)
	,	m_IndexSize(RHI::IndexBuffer16Bit)
	,	m_GPUStorageSimDataConstantSet(null)
	,	m_GPUStorageOffsetsConstantSet(null)
	,	m_IndexOffset(0)
	,	m_IndexCount(0)
	,	m_VertexOffset(0)
	,	m_VertexCount(0)
	,	m_InstanceCount(0)
	,	m_IndirectBufferOffset(0)
	,	m_IndirectBuffer(null)
	,	m_EstimatedParticleCount(0)
	,	m_ShaderOptions(0)
	,	m_SelectedDrawCall(false)
	,	m_SlicedDC(false)
	,	m_Valid(true)
	,	m_BBox(CAABB::DEGENERATED)
	,	m_TotalBBox(CAABB::DEGENERATED)
	,	m_RendererType(Renderer_Invalid)
	{
		for (u32 i = 0; i < _DebugDrawGPUBuffer_Count; ++i)
		{
			m_DebugDrawGPUBuffers[i] = null;
			m_DebugDrawGPUBufferOffsets[i] = 0;
		}

		Mem::Clear(m_PushConstants.Begin(), sizeof(CFloat4) * 4);
	}
};

//----------------------------------------------------------------------------

struct	SGpuBuffer
{
	RHI::PGpuBuffer		m_Buffer;

	SGpuBuffer() : m_Buffer(null), m_UsedThisFrame(false) { }

	void	Unmap(const RHI::PApiManager &manager) { if (m_Buffer != null) { PK_ASSERT(m_Buffer->IsMapped()); PK_VERIFY(manager->UnmapCpuView(m_Buffer)); } }
	void	UnmapIFN(const RHI::PApiManager &manager) { if (m_Buffer != null && m_Buffer->IsMapped()) PK_VERIFY(manager->UnmapCpuView(m_Buffer)); }
	void	SetGpuBuffer(RHI::PGpuBuffer buffer) { m_Buffer = buffer; m_UsedThisFrame = PK_VERIFY(m_Buffer != null); }
	void	Use() { m_UsedThisFrame = PK_VERIFY(m_Buffer != null); }
	bool	Used() const { PK_ASSERT(m_Buffer != null || !m_UsedThisFrame); return m_UsedThisFrame; }
	void	Clear() { m_UsedThisFrame = false; /* More than some amount of frames unused, we could destroy the vertex buffer */ }

private:
	bool	m_UsedThisFrame;
};

//----------------------------------------------------------------------------

struct	SAdditionalInputs
{
	SRHIDrawCall::EDebugDrawGPUBuffer	m_Semantic; // Internal - Editor only

	SGpuBuffer							m_Buffer;
	u32									m_ByteSize;
	u32									m_AdditionalInputIndex;

	SAdditionalInputs()
	:	m_Semantic(SRHIDrawCall::EDebugDrawGPUBuffer::_DebugDrawGPUBuffer_Count)
	,	m_ByteSize(0)
	,	m_AdditionalInputIndex(0)
	{

	}
};

//----------------------------------------------------------------------------

struct	SRHICopyCommand
{
	u32					m_SrcOffset;
	RHI::PGpuBuffer		m_SrcBuffer;

	u32					m_DstOffset;
	RHI::PGpuBuffer		m_DstBuffer;

	u32					m_SizeToCopy;
};

//----------------------------------------------------------------------------

struct	SRHIComputeDispatchs
{
	RHI::PConstantSet				m_ConstantSet;
	CInt3							m_ThreadGroups;
	RHI::PComputeState				m_State;
	TStaticCountedArray<CFloat4, 4>	m_PushConstants;
	bool							m_NeedSceneInfoConstantSet;
	TArray<RHI::PGpuBuffer>			m_BufferMemoryBarriers;

	SRHIComputeDispatchs()
	: m_ConstantSet(0)
	, m_ThreadGroups(0)
	, m_State(0)
	, m_NeedSceneInfoConstantSet(false)
	{
		Mem::Clear(m_PushConstants.Begin(), sizeof(CFloat4) * 4);
	}
};

//----------------------------------------------------------------------------

struct	SRHIDrawOutputs
{
	TArray<SRHICopyCommand>			m_CopyCommands;
	TArray<SRHIDrawCall>			m_DrawCalls;
	TArray<SRHIComputeDispatchs>	m_ComputeDispatchs;

	void		Clear()
	{
		m_CopyCommands.Clear();
		m_DrawCalls.Clear();
		m_ComputeDispatchs.Clear();
	}
};

//----------------------------------------------------------------------------

struct	SRenderContext
{
	enum	EPass
	{
		EPass_PostUpdateFence,
		EPass_RenderThread
	};

	EPass					Pass() const { return m_Pass; }
	bool					IsPostUpdateFencePass() const { return m_Pass == EPass_PostUpdateFence; }
	bool					IsRenderThreadPass() const { return m_Pass == EPass_RenderThread; }
	RHI::PApiManager		ApiManager() const { return m_ApiManager; }

	CSoundPoolCache			&SoundPool() { PK_ASSERT(m_SoundPool != null); return *m_SoundPool; }
	float					SimSpeed() { return m_SimSpeed; }

#if	(PK_HAS_PARTICLES_SELECTION != 0)
	const PKSample::SEffectParticleSelectionView			&Selection() const { return m_Selection; }
	PKSample::SEffectParticleSelectionView					m_Selection;
#endif

	SRenderContext(EPass pass, RHI::PApiManager apiManager)
	:	m_Pass(pass)
	,	m_ApiManager(apiManager)
	,	m_SoundPool(null)
	,	m_SimSpeed(0)
	{
	}

	SRenderContext(EPass pass, CSoundPoolCache *soundPool, const float simSpeed)
	:	m_Pass(pass)
	,	m_ApiManager(null)
	,	m_SoundPool(soundPool)
	,	m_SimSpeed(simSpeed)
	{
	}

private:
	EPass					m_Pass;
	RHI::PApiManager		m_ApiManager;

	CSoundPoolCache			*m_SoundPool;
	float					m_SimSpeed;
};

//----------------------------------------------------------------------------

struct	SAudioContext
{
};

//----------------------------------------------------------------------------

struct	SViewUserData
{
};

//----------------------------------------------------------------------------

class	CRHIParticleBatchTypes
{
public:
	typedef SRenderContext		CRenderContext;
	typedef SRHIDrawOutputs		CFrameOutputData;
	typedef SViewUserData		CViewUserData;

	enum { kMaxQueuedCollectedFrame = 2U };
};

typedef TSceneView<SViewUserData>	SSceneView;

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
