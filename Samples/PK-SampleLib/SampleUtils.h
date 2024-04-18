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

#include "PKSample.h"
#include <pk_geometrics/include/ge_coordinate_frame.h>
#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/SConstantSetLayout.h>
#include <pk_rhi/include/interfaces/SShaderBindings.h>

__PK_API_BEGIN
PK_FORWARD_DECLARE(ResourceMesh);
PK_FORWARD_DECLARE(ResourceMeshBatch);
PK_FORWARD_DECLARE(Skeleton);
__PK_API_END

__PK_SAMPLE_API_BEGIN

struct SSceneInfoData;
class CCameraBase;
struct SCamera;

//----------------------------------------------------------------------------

#define					INTERMEDIATE_SHADER_EXTENSION	".i"
#define					SHADER_ERROR_EXTENSION			".err"
#define					META_SHADER_EXTENSION			".meta"

//----------------------------------------------------------------------------

CString					GetDefineStringFromApi(RHI::EGraphicalApi api);

// These return the extension with the dot: ".ext"
const char				*GetShaderExtensionStringFromApi(RHI::EGraphicalApi api, bool tmpFile = false);
const char				*GetShaderLogExtensionStringFromAPI(RHI::EGraphicalApi API);
const char				*GetShaderExtensionStringFromStage(RHI::EShaderStage stage);

//----------------------------------------------------------------------------

template<u32 _D>
TVector<float, _D>	ConvertSRGBToLinear(const TVector<float, _D> &v)
{
	TVector<float, _D>	c;
	for (u32 i = 0; i < _D; ++i)
		c.Axis(i) = (v.Axis(i) <= 0.04045f) ? (v.Axis(i) / 12.92f) : (powf((v.Axis(i) + 0.055f) / 1.055f, 2.4f));
	return c;
}

//----------------------------------------------------------------------------

template<u32 _D>
TVector<float, _D>	ConvertLinearToSRGB(const TVector<float, _D> &v)
{
	TVector<float, _D>	c;
	for (u32 i = 0; i < _D; ++i)
		c.Axis(i) = (v.Axis(i) <= 0.0031308f) ? (v.Axis(i) * 12.92f) : (1.055f * powf(v.Axis(i), 1 / 2.4f) - 0.055f);
	return c;
}

//----------------------------------------------------------------------------

struct	SSamplableRenderTarget
{
	CUint2							m_Size;
	RHI::PRenderTarget				m_RenderTarget;
	RHI::PConstantSet				m_SamplerConstantSet;

	bool			CreateRenderTarget(	const RHI::SRHIResourceInfos	&infos,
										const RHI::PApiManager			&apiManager,
										const RHI::PConstantSampler		&sampler,
										RHI::EPixelFormat				format,
										const CUint2					&frameBufferSize,
										const RHI::SConstantSetLayout	&layoutFragment1Sampler);
};

//----------------------------------------------------------------------------

namespace	Utils
{
	struct	VertexHelper
	{
		u64											m_VertexCount;
		TMemoryView<const TMemoryView<const float>>	m_VertexData;
		TMemoryView<const u16>						m_IndexData16;
		TMemoryView<const u32>						m_IndexData32;

		VertexHelper(u64 vertexCount, TMemoryView<const TMemoryView<const float>> vertexViews, TMemoryView<const u16> indexViews)
		:	m_VertexCount(vertexCount)
		,	m_VertexData(vertexViews)
		,	m_IndexData16(indexViews)
		{}

		VertexHelper(u64 vertexCount, TMemoryView<const TMemoryView<const float>> vertexViews, TMemoryView<const u32> indexViews)
		:	m_VertexCount(vertexCount)
		,	m_VertexData(vertexViews)
		,	m_IndexData32(indexViews)
		{}

		VertexHelper(u64 vertexCount, TMemoryView<const TMemoryView<const float>> vertexViews)
		:	m_VertexCount(vertexCount)
		,	m_VertexData(vertexViews)
		{}

		u32	IndexCount() const
		{
			PK_ASSERT(m_IndexData16.Empty() || m_IndexData32.Empty());
			if (!m_IndexData16.Empty()) return m_IndexData16.Count();
			if (!m_IndexData32.Empty()) return m_IndexData32.Count();
			return 0;
		}
	};

	//----------------------------------------------------------------------------

	struct	GpuBufferViews
	{
		TArray<RHI::PGpuBuffer>			m_VertexBuffers;
		RHI::PGpuBuffer					m_IndexBuffer;
		RHI::EIndexBufferSize			m_IndexBufferSize;
	};

	//----------------------------------------------------------------------------

	extern const VertexHelper	ScreenQuadHelper;
	extern const VertexHelper	IndexedScreenQuadHelper;

	bool				CreateGpuBuffers(const RHI::PApiManager &apiManager, const VertexHelper &helper, TArray<RHI::PGpuBuffer> &vertexArray, RHI::PGpuBuffer &indexBuffer, RHI::EIndexBufferSize &indexSize);
	bool				CreateGpuBuffers(const RHI::PApiManager &apiManager, const VertexHelper &helper, GpuBufferViews &views);

	//----------------------------------------------------------------------------

	enum				MeshSemanticFlags
	{
		MeshPositions,
		MeshNormals,
		MeshTangents,
		MeshColors, // Color 0
		MeshTexcoords, // Texcoords 0
		MeshColors1,
		MeshTexcoords1,
		MeshBoneIds,
		MeshBoneWeights,
		__MaxMeshSemantics
	};

	bool				CreateGpuBuffers(const RHI::PApiManager &apiManager, const PCResourceMesh &mesh, const TMemoryView<const MeshSemanticFlags> &semantics, TArray<GpuBufferViews> &views);
	bool				CreateGpuBuffers(const RHI::PApiManager &apiManager, const CResourceMesh *mesh, const TMemoryView<const MeshSemanticFlags> &semantics, TArray<GpuBufferViews> &views);
	bool				CreateGpuBuffers(const RHI::PApiManager &apiManager, const PCResourceMeshBatch &meshBatch, const PSkeleton &skeleton, const TMemoryView<const MeshSemanticFlags> &semantics, GpuBufferViews &views);

	bool				CreateFrameBuffer(const RHI::SRHIResourceInfos &infos, const RHI::PApiManager &apiManager, const TMemoryView<const RHI::PRenderTarget> &renderTargets, RHI::PFrameBuffer &ptr, const RHI::PRenderPass &renderPassToBake);

	// Camera info
	struct	SBasicCameraData
	{
		CFloat4x4	m_CameraView;
		CFloat4x4	m_CameraProj;
		CFloat4x4	m_BillboardingView;
		CFloat2		m_CameraZLimit;
		CFloat2		m_ViewportSize;
	};

	void				SetupSceneInfoData(SSceneInfoData &data, const SCamera &camera, const ECoordinateFrame &frame);
	void				SetupSceneInfoData(SSceneInfoData &data, const SBasicCameraData &camera, const ECoordinateFrame &frame);
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
