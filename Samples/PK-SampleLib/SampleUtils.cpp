//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"

#include "SampleUtils.h"
#include "RenderIntegrationRHI/RHIGraphicResources.h"
#include "Camera.h"

// RHI
#include <pk_rhi/include/AllInterfaces.h>

// Resources
#include <pk_kernel/include/kr_resources.h>
#include <pk_geometrics/include/ge_mesh_resource.h>
#include <pk_geometrics/include/ge_mesh_tangent_basis.h>
#include <pk_geometrics/include/ge_mesh_deformers_skin.h>
#include <pk_kernel/include/kr_memoryviews_utils.h>
#include <pk_maths/include/pk_maths_type_converters.h>

#include <math.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

const char	*GetShaderExtensionStringFromApi(RHI::EGraphicalApi api, bool tmpFile /*= false*/)
{
	switch (api)
	{
	case RHI::GApi_OpenGL:
	case RHI::GApi_OES:
		return ".glsl";
	case RHI::GApi_Vulkan:
		return tmpFile ? ".vk.glsl" : ".spv";
	case RHI::GApi_D3D12:
	case RHI::GApi_D3D11:
		return tmpFile ? ".hlsl" : ".cso";
	case RHI::GApi_Metal:
		return tmpFile ? ".metal" : ".metallib";
	case RHI::GApi_Orbis:
		return tmpFile ? ".pssl" : ".sb";
	case RHI::GApi_UNKNOWN2:
		return tmpFile ? ".ags.pssl" : ".ags";
	default:
		PK_RELEASE_ASSERT_NOT_REACHED();
		break;
	}
	return null;
}

//----------------------------------------------------------------------------

const char	*GetShaderLogExtensionStringFromAPI(RHI::EGraphicalApi API)
{
	switch (API)
	{
	case RHI::GApi_OpenGL:
	case RHI::GApi_OES:
		return ".glsl.err";
	case RHI::GApi_Vulkan:
		return ".vk.glsl.err";
	case RHI::GApi_D3D12:
	case RHI::GApi_D3D11:
		return ".hlsl.err";
	case RHI::GApi_Metal:
		return ".metal.err";
	case RHI::GApi_Orbis:
		return ".pssl.err";
	case RHI::GApi_UNKNOWN2:
		return ".ags.err";
	default:
		PK_ASSERT_NOT_REACHED();
		break;
	}
	return null;
}

//----------------------------------------------------------------------------

const char	*GetShaderExtensionStringFromStage(RHI::EShaderStage stage)
{
	switch (stage)
	{
	case RHI::VertexShaderStage:
		return ".vert";
	case RHI::FragmentShaderStage:
		return ".frag";
	case RHI::GeometryShaderStage:
		return ".geom";
	case RHI::ComputeShaderStage:
		return ".comp";
	default:
		PK_ASSERT_NOT_REACHED();
		break;
	}
	return null;
}

//----------------------------------------------------------------------------

bool	SSamplableRenderTarget::CreateRenderTarget(	const RHI::SRHIResourceInfos	&infos,
													const RHI::PApiManager			&apiManager,
													const RHI::PConstantSampler		&sampler,
													RHI::EPixelFormat				format,
													const CUint2					&frameBufferSize,
													const RHI::SConstantSetLayout	&layoutFragment1Sampler)
{
	m_Size = frameBufferSize;
	// Create the render targets
	m_RenderTarget = apiManager->CreateRenderTarget(infos, format, frameBufferSize, true);
	if (m_RenderTarget == null)
		return false;

	// Create the constant sets
	m_SamplerConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("SSamplable Render Target Constant Set"), layoutFragment1Sampler);
	if (m_SamplerConstantSet == null)
		return false;
	m_SamplerConstantSet->SetConstants(sampler, m_RenderTarget->GetTexture(), 0);
	m_SamplerConstantSet->UpdateConstantValues();
	return true;
}

//----------------------------------------------------------------------------

namespace // static data
{
	// Screen quad static data
	const float	kScreenQuadVertex[] =
	{
		+1, +1, +1, -1, -1, -1,
		-1, -1, -1, +1, +1, +1,
	};

	const u16	kScreenQuadIndex[] =
	{
		0, 1, 2, 3, 4, 5
	};

	const u64	kScreenQuadVertexCount = PK_ARRAY_COUNT(kScreenQuadVertex) / 2;
	const TMemoryView<const float>	kScreenQuadVertexViews[] =
	{
		TMemoryView<const float>(kScreenQuadVertex),
	};

	PK_STATIC_ASSERT(kScreenQuadVertexCount*2 == PK_ARRAY_COUNT(kScreenQuadVertex));
}

//----------------------------------------------------------------------------

namespace	Utils
{
	const VertexHelper	ScreenQuadHelper(kScreenQuadVertexCount, kScreenQuadVertexViews);
	const VertexHelper	IndexedScreenQuadHelper(kScreenQuadVertexCount, kScreenQuadVertexViews, kScreenQuadIndex);

	//----------------------------------------------------------------------------

	template<typename _Type>
	TStridedMemoryView<const _Type>	ViewNotEmpty(TStridedMemoryView<const _Type>	view, u32 vCount, _Type defaultValue)
	{
		static	const _Type _defaultValue = defaultValue;
		if (view.Empty())
			return TStridedMemoryView<const _Type>(&_defaultValue, vCount, 0);
		return view;
	}

	//----------------------------------------------------------------------------

	template<typename _Type>
	bool	CopyDataToGpuBuffer(const RHI::PApiManager &apiManager, RHI::PGpuBuffer &buffer, const TStridedMemoryView<_Type> &view, u64 offset = 0)
	{
		//PK_ASSERT(view.CoveredBytes() <= buffer->GetByteSize()); // Can be different because of stride
		// Api Manager map the buffer and return virtual space
		void	*data = apiManager->MapCpuView(buffer, offset, view.ElementSizeInBytes() * view.Count());
		if (data == null)
			return false;
		// Copy Data to virtual space
		{
			typedef typename TStridedMemoryView<_Type>::ValueTypeNonConst	_TypeNonConst;
			typedef typename TStridedMemoryView<_Type>::ValueTypeConst		_TypeConst;
			TStridedMemoryView<_TypeNonConst>	bufferView(reinterpret_cast<_TypeNonConst*>(data), view.Count());
			TStridedMemoryView<_TypeConst>		srcView(view);
			Mem::CopyStreamToStream(bufferView, srcView);
		}
		// Unmap triggers async transfer to gpu space
		return apiManager->UnmapCpuView(buffer);
	}

	//----------------------------------------------------------------------------

	template<typename _Type>
	bool	CopyDataToGpuBuffer(const RHI::PApiManager &apiManager, RHI::PGpuBuffer &buffer, const TMemoryView<_Type> &view, u64 offset = 0)
	{
		const TStridedMemoryView<_Type>	stridedView(view.Data(), view.Count());
		return CopyDataToGpuBuffer(apiManager, buffer, stridedView, offset);
	}

	//----------------------------------------------------------------------------

	template<typename _Type>
	bool	CreateGpuBuffer(const RHI::SRHIResourceInfos &infos, const RHI::PApiManager &apiManager, const TStridedMemoryView<_Type> &view, RHI::PGpuBuffer &buffer, RHI::EBufferType bufferType)
	{
		buffer = apiManager->CreateGpuBuffer(infos, bufferType, view.ElementSizeInBytes() * view.Count());
		return buffer != null && CopyDataToGpuBuffer(apiManager, buffer, view);
	}

	//----------------------------------------------------------------------------

	template<typename _Type>
	bool	CreateGpuBuffer(const RHI::SRHIResourceInfos &infos, const RHI::PApiManager &apiManager, const TMemoryView<_Type> &view, RHI::PGpuBuffer &buffer, RHI::EBufferType bufferType)
	{
		const TStridedMemoryView<_Type>	stridedView(view.Data(), view.Count());
		return CreateGpuBuffer(infos, apiManager, stridedView, buffer, bufferType);
	}

	//----------------------------------------------------------------------------

	bool	CreateGpuBuffers(const RHI::PApiManager &apiManager, const VertexHelper &helper, TArray<RHI::PGpuBuffer> &vertexArray, RHI::PGpuBuffer &indexBuffer, RHI::EIndexBufferSize &indexSize)
	{
		PK_ASSERT(indexBuffer == null && vertexArray.Empty());
		if (!vertexArray.Reserve(helper.m_VertexData.Count()))
			return false;
		PK_FOREACH(vinput, helper.m_VertexData)
		{
			// Api Manager create api specific gpu buffer
			vertexArray.PushBack();
			if (!CreateGpuBuffer(RHI::SRHIResourceInfos("Vertex Data Buffer"), apiManager, *vinput, vertexArray.Last(), RHI::VertexBuffer))
				return false;
		}

		if (!helper.m_IndexData16.Empty())
		{
			indexSize = RHI::IndexBuffer16Bit;
			if (!CreateGpuBuffer(RHI::SRHIResourceInfos("Index Buffer"), apiManager, helper.m_IndexData16, indexBuffer, RHI::IndexBuffer))
				return false;
		}

		if (!helper.m_IndexData32.Empty())
		{
			indexSize = RHI::IndexBuffer32Bit;
			if (!CreateGpuBuffer(RHI::SRHIResourceInfos("Index Buffer"), apiManager, helper.m_IndexData32, indexBuffer, RHI::IndexBuffer))
				return false;
		}
		return true;
	}

	//----------------------------------------------------------------------------

	bool	CreateGpuBuffers(const RHI::PApiManager &apiManager, const VertexHelper &helper, GpuBufferViews &views)
	{
		return CreateGpuBuffers(apiManager, helper, views.m_VertexBuffers, views.m_IndexBuffer, views.m_IndexBufferSize);
	}

	//----------------------------------------------------------------------------

	template<typename T>
	void	RepackBoneIdxToFloat4(u32 maxBonePerVtx, const TMemoryView<const T> &boneIdx, const TMemoryView<float> &outFloat4BoneIdx, const PSkeleton &skeleton)
	{
		PK_ASSERT(skeleton != null);
		PK_ASSERT(maxBonePerVtx <= 0xFF);
		const u32	vtxCount = boneIdx.Count() / maxBonePerVtx;
		PK_ASSERT(vtxCount * 4 == outFloat4BoneIdx.Count());

		for (u32 currentVtx = 0; currentVtx < vtxCount; ++currentVtx)
		{
			u32		currentIdx = currentVtx * maxBonePerVtx;
			u32		outIdx = 0;

			while (outIdx < 4 && outIdx < maxBonePerVtx)
			{
				CGuid	remappedIdx = skeleton->RemapBoneIdx(boneIdx[currentIdx + outIdx]);
				outFloat4BoneIdx[currentVtx * 4 + outIdx] = static_cast<float>(remappedIdx.Get());
				++outIdx;
			}
			while (outIdx < 4)
			{
				outFloat4BoneIdx[currentVtx * 4 + outIdx] = 0.0f;
				++outIdx;
			}
		}
	}

	void	RepackWeightsToFloat4(u32 maxBonePerVtx, const TMemoryView<const float> &boneWeights, const TMemoryView<float> &outFloat4BoneWeights)
	{
		PK_ASSERT(maxBonePerVtx <= 0xFF);
		const u32	vtxCount = boneWeights.Count() / maxBonePerVtx;
		PK_ASSERT(vtxCount * 4 == outFloat4BoneWeights.Count());

		for (u32 currentVtx = 0; currentVtx < vtxCount; ++currentVtx)
		{
			u32		currentIdx = currentVtx * maxBonePerVtx;
			u32		outIdx = 0;

			while (outIdx < 4 && outIdx < maxBonePerVtx)
			{
				outFloat4BoneWeights[currentVtx * 4 + outIdx] = boneWeights[currentIdx + outIdx];
				++outIdx;
			}
			while (outIdx < 4)
			{
				outFloat4BoneWeights[currentVtx * 4 + outIdx] = 0.0f;
				++outIdx;
			}
			float	length = 0.0f;
			for (u32 i = 0; i < 4; ++i)
				length += outFloat4BoneWeights[currentVtx * 4 + i];
			for (u32 i = 0; i < 4; ++i)
				outFloat4BoneWeights[currentVtx * 4 + i] /= length;
		}
	}
	//----------------------------------------------------------------------------

	bool	CreateGpuBuffers(const RHI::PApiManager &apiManager, const PCResourceMeshBatch &meshBatch, const PSkeleton &skeleton, const TMemoryView<const MeshSemanticFlags> &semantics, GpuBufferViews &views)
	{
		if (meshBatch == null)
			return false;

		CMeshTriangleBatch		&trianglesBatch = meshBatch->RawMesh()->TriangleBatch();
		CBaseSkinningStreams	*skinningStreams = meshBatch->m_OptimizedStreams;

		// Note (Alex) : this is not perfect and modify the format directly on memory. To be improved. Tangent is computed separately.
		SVertexDeclaration	vertDecl(trianglesBatch.m_VStream.VertexDeclaration());
		PK_FOREACH(it, semantics)
		{
			switch (*it)
			{
			case	MeshPositions:
				if (!vertDecl.FindAbstractStreamInternalIndex(CVStreamSemanticDictionnary::Ordinal_Position).Valid())
					vertDecl.AddStreamCodeIFN(SVStreamCode(CVStreamSemanticDictionnary::Ordinal_Position, SVStreamCode::Element_Float3));
				break;
			case	MeshNormals:
				if (!vertDecl.FindAbstractStreamInternalIndex(CVStreamSemanticDictionnary::Ordinal_Normal).Valid())
					vertDecl.AddStreamCodeIFN(SVStreamCode(CVStreamSemanticDictionnary::Ordinal_Normal, SVStreamCode::Element_Float3, SVStreamCode::Normalized));
				break;
			case	MeshTangents:
				if (!vertDecl.FindAbstractStreamInternalIndex(CVStreamSemanticDictionnary::Ordinal_Tangent).Valid())
					vertDecl.AddStreamCodeIFN(SVStreamCode(CVStreamSemanticDictionnary::Ordinal_Tangent, SVStreamCode::Element_Float4, SVStreamCode::SIMD_Friendly | SVStreamCode::Normalized));
				break;
			case	MeshColors:
				if (!vertDecl.FindAbstractStreamInternalIndex(CVStreamSemanticDictionnary::Ordinal_Color).Valid())
					vertDecl.AddStreamCodeIFN(SVStreamCode(CVStreamSemanticDictionnary::Ordinal_Color, SVStreamCode::Element_Float4));
				break;
			case	MeshTexcoords:
				if (!vertDecl.FindAbstractStreamInternalIndex(CVStreamSemanticDictionnary::Ordinal_Texcoord).Valid())
					vertDecl.AddStreamCodeIFN(SVStreamCode(CVStreamSemanticDictionnary::Ordinal_Texcoord, SVStreamCode::Element_Float2));
				break;
			case	MeshColors1:
				if (!vertDecl.FindAbstractStreamInternalIndex(CVStreamSemanticDictionnary::ColorStreamToOrdinal(1)).Valid())
					vertDecl.AddStreamCodeIFN(SVStreamCode(CVStreamSemanticDictionnary::ColorStreamToOrdinal(1), SVStreamCode::Element_Float4));
				break;
			case	MeshTexcoords1:
				if (!vertDecl.FindAbstractStreamInternalIndex(CVStreamSemanticDictionnary::UvStreamToOrdinal(1)).Valid())
					vertDecl.AddStreamCodeIFN(SVStreamCode(CVStreamSemanticDictionnary::UvStreamToOrdinal(1), SVStreamCode::Element_Float2));
				break;
			case	MeshBoneIds:
			case	MeshBoneWeights:
				break;
			default:
				PK_ASSERT_NOT_IMPLEMENTED();
				break;
			};
		}

		if (!PK_VERIFY(!trianglesBatch.m_VStream.Interleaved()) || trianglesBatch.m_VStream.VertexDeclaration() != vertDecl)
		{
			if (!trianglesBatch.m_VStream.Reformat(vertDecl, false))
				return false;
		}
		// Notify whatever was hooked onto the old VB that it has changed
		meshBatch->RawMesh()->m_OnMeshDataChanged();

		if (!PK_VERIFY(views.m_VertexBuffers.Reserve(semantics.Count())))
			return false;
		bool				success = true;
		CMeshVStream		&vStream = trianglesBatch.m_VStream;
		const CMeshIStream	&iStream = trianglesBatch.m_IStream;

		if (!vStream.Tangents().Empty())
			success &= MeshTangentBasisUtils::ComputeTangents(	vStream.Positions(),
																vStream.Normals(),
																vStream.Texcoords(),
																vStream.Tangents(),
																iStream.RawStream(),
																iStream.IndexByteWidth(),
																iStream.PrimitiveType(),
																trianglesBatch.PrimitiveCount(),
																0, false);

		PK_FOREACH(it, semantics)
		{
			if (!views.m_VertexBuffers.PushBack().Valid())
				return false;
			switch (*it)
			{
			case	MeshPositions:
				success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Positions Vertex Buffer"), apiManager, vStream.Positions(), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				break;
			case	MeshNormals:
				if (vStream.Has<CVStreamSemanticDictionnary::Ordinal_Normal>())
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Normals Vertex Buffer"), apiManager, vStream.Normals(), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				break;
			case	MeshTangents:
				if (vStream.Has<CVStreamSemanticDictionnary::Ordinal_Tangent>())
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Tangents Vertex Buffer"), apiManager, vStream.Tangents(), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				break;
			case	MeshColors:
				if (vStream.Has<CVStreamSemanticDictionnary::Ordinal_Color>())
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Color0s Vertex Buffer"), apiManager, vStream.Colors(), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				break;
			case	MeshTexcoords:
				if (vStream.Has<CVStreamSemanticDictionnary::Ordinal_Texcoord>())
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Texcoord0s Vertex Buffer"), apiManager, vStream.Texcoords(), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				break;
			case	MeshColors1:
				if (vStream.Has(CVStreamSemanticDictionnary::ColorStreamToOrdinal(1)))
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Color1s Vertex Buffer"), apiManager, vStream.AbstractStream<CFloat4>(CVStreamSemanticDictionnary::ColorStreamToOrdinal(1)), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				break;
			case	MeshTexcoords1:
				if (vStream.Has(CVStreamSemanticDictionnary::UvStreamToOrdinal(1)))
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Texcoord1s Vertex Buffer"), apiManager, vStream.AbstractStream<CFloat2>(CVStreamSemanticDictionnary::UvStreamToOrdinal(1)), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				break;
			case	MeshBoneIds:
				if (skinningStreams != null && skeleton != null)
				{
					const u32	maxInfluencesPerVertex = skinningStreams->MaxInfluencesPerVertex();
					const u32	vertexCount = skinningStreams->VertexCount();
					const u32	elemCount = vertexCount * maxInfluencesPerVertex;

					TArray<float>		indices;
					if (!PK_VERIFY(indices.Resize(4 * vertexCount)))
						return false;

					if (skinningStreams->IndexSize() == sizeof(u8))
						RepackBoneIdxToFloat4(maxInfluencesPerVertex, TMemoryView<const u8>(skinningStreams->IndexStream<u8>(), elemCount), indices.ViewForWriting(), skeleton);
					else if (skinningStreams->IndexSize() == sizeof(u16))
						RepackBoneIdxToFloat4(maxInfluencesPerVertex, TMemoryView<const u16>(skinningStreams->IndexStream<u16>(), elemCount), indices.ViewForWriting(), skeleton);
					else
					{
						PK_ASSERT_NOT_REACHED();
						return false;
					}
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh bone indices Vertex Buffer"), apiManager, indices.View(), views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				}
				break;
			case	MeshBoneWeights:
				if (skinningStreams != null)
				{
					const u32	maxInfluencesPerVertex = skinningStreams->MaxInfluencesPerVertex();
					const u32	vertexCount = skinningStreams->VertexCount();
//					const u32	elemCount = vertexCount * maxInfluencesPerVertex;

					TArray<float>		weights;
					if (!PK_VERIFY(weights.Resize(4 * vertexCount)))
						return false;

					TMemoryView<const float>	weightsStream = TMemoryView<const float>(skinningStreams->WeightStream(), vertexCount * maxInfluencesPerVertex);
					RepackWeightsToFloat4(maxInfluencesPerVertex, weightsStream, weights.ViewForWriting());
					TMemoryView<CFloat4>	srcWeights = TMemoryView<CFloat4>((CFloat4*)weights.RawDataPointer(), vertexCount);
					success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh bone weights Vertex Buffer"), apiManager, srcWeights, views.m_VertexBuffers.Last(), RHI::VertexBuffer);
				}
				break;
			default:
				PK_ASSERT_NOT_REACHED();
				break;
			}
		}

		if (iStream.IndexByteWidth() == CMeshIStream::U16Indices)
		{
			const TStridedMemoryView<const u16>	indexView(trianglesBatch.m_IStream.Stream<u16>(), trianglesBatch.m_IStream.IndexCount());
			views.m_IndexBufferSize = RHI::IndexBuffer16Bit;
			success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Index Buffer"), apiManager, indexView, views.m_IndexBuffer, RHI::IndexBuffer);
		}
		else if (iStream.IndexByteWidth() == CMeshIStream::U32Indices)
		{
			const TStridedMemoryView<const u32>	indexView(trianglesBatch.m_IStream.Stream<u32>(), trianglesBatch.m_IStream.IndexCount());
			views.m_IndexBufferSize = RHI::IndexBuffer32Bit;
			success &= CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Index Buffer"), apiManager, indexView, views.m_IndexBuffer, RHI::IndexBuffer);
		}
		return success;
	}

	bool	CreateGpuBuffers(const RHI::PApiManager &apiManager, const PCResourceMesh &mesh, const TMemoryView<const MeshSemanticFlags> &semantics, TArray<GpuBufferViews> &views)
	{
		return CreateGpuBuffers(apiManager, mesh.Get(), semantics, views);
	}

	bool	CreateGpuBuffers(const RHI::PApiManager &apiManager, const CResourceMesh *mesh, const TMemoryView<const MeshSemanticFlags> &semantics, TArray<GpuBufferViews> &views)
	{
		const u32	lodCount = mesh->LODCount();
		u32			totalBatchCount = 0;

		for (u32 lodIdx = 0; lodIdx < lodCount; ++lodIdx)
		{
			totalBatchCount += mesh->BatchList(lodIdx).Count();
		}

		if (!PK_VERIFY(views.Resize(totalBatchCount)))
			return false;

		u32	bufferIdx = 0;
		for (u32 lodIdx = 0; lodIdx < lodCount; ++lodIdx)
		{
			for (u32 meshIdx = 0; meshIdx < mesh->BatchList(lodIdx).Count(); ++meshIdx)
			{
				const PCResourceMeshBatch	meshBatch(&*mesh->BatchList(lodIdx)[meshIdx]);
				if (!CreateGpuBuffers(apiManager, meshBatch, mesh->Skeleton(), semantics, views[bufferIdx++]))
					return false;
			}
		}
		return true;
	}

	//----------------------------------------------------------------------------

	bool	CreateFrameBuffer(const RHI::SRHIResourceInfos &infos, const RHI::PApiManager &apiManager, const TMemoryView<const RHI::PRenderTarget> &renderTargets, RHI::PFrameBuffer &ptr, const RHI::PRenderPass &renderPassToBake)
	{
		ptr = apiManager->CreateFrameBuffer(infos);
		if (ptr == null)
			return false;
		PK_FOREACH(rt, renderTargets)
		{
			if (ptr->AddRenderTarget(*rt) == false)
				return false;
		}
		return ptr->BakeFrameBuffer(renderPassToBake);
	}

	void	SetupSceneInfoData(SSceneInfoData &data, const SCamera &camera, const ECoordinateFrame &frame)
	{
		SBasicCameraData	camData;

		camData.m_CameraProj = camera.Proj();
		camData.m_CameraView = camera.View();
		camData.m_CameraZLimit = camera.ZLimits();
		camData.m_BillboardingView = camera.View();
		camData.m_ViewportSize = camera.WindowSize();
		SetupSceneInfoData(data, camData, frame);
	}

	void	SetupSceneInfoData(SSceneInfoData &data, const SBasicCameraData &camera, const ECoordinateFrame &frame)
	{
		CFloat4x4			view = camera.m_CameraView;
		CFloat4x4			invView = view.Inverse();
		CFloat4x4			userToRHY;

		CCoordinateFrame::BuildTransitionFrame(frame, Frame_RightHand_Y_Up, userToRHY);

		CFloat4x4			bbMatrix = (camera.m_BillboardingView * userToRHY).Inverse();

		data.m_ViewProj = view * camera.m_CameraProj;
		data.m_Proj = camera.m_CameraProj;
		data.m_View = view;
		data.m_InvView = invView;
		data.m_InvViewProj = data.m_ViewProj.Inverse();
		data.m_BillboardingView = bbMatrix;
		data.m_PackNormalView = view * userToRHY;
		data.m_UnpackNormalView = (view * userToRHY).Inverse();
		data.m_ZBufferLimits = CFloat2(camera.m_CameraZLimit);
		data.m_ViewportSize = camera.m_ViewportSize;
		data.m_Handedness = CFloat1(CCoordinateFrame::CrossSign(frame));
		data.m_SideVector = CCoordinateFrame::AxisSide().xyz0();
		data.m_DepthVector = CCoordinateFrame::AxisDepth().xyz0();
		// Helper matrix to convert from LHZup to user coord system.
		CCoordinateFrame::BuildTransitionFrame(CCoordinateFrame::GlobalFrame(), Frame_LeftHand_Z_Up, data.m_UserToLHZ);
		CCoordinateFrame::BuildTransitionFrame(Frame_LeftHand_Z_Up, CCoordinateFrame::GlobalFrame(), data.m_LHZToUser);
	}
}	// namespace Utils

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
