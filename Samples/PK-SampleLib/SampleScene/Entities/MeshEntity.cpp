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

#include "MeshEntity.h"

#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/interfaces/SApiContext.h>
#include <pk_rhi/include/PixelFormatFallbacks.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

STextureMap::STextureMap()
:	m_Path(null)
,	m_Texture(null)
,	m_Sampler(null)
{
}

//----------------------------------------------------------------------------

SMesh::SMesh()
:	m_Maps(0)
,	m_Material(GBufferCombination_Count)
,	m_MeshBatchCoordinateFrame(CCoordinateFrame::GlobalFrame())
,	m_MeshLOD(0)
,	m_Roughness(0.5)
,	m_Metalness(0)
,	m_ResourceDirtyKey(0)
,	m_ResourceCleanKey(0)
{
}

//----------------------------------------------------------------------------

SMesh::~SMesh()
{
	FastDelegate<void(CResourceMesh *)>		callback = FastDelegate<void(CResourceMesh *)>(this, &SMesh::_OnResourceReloaded);
	if (m_MeshResource != null && !m_MeshResource->Empty())
	{
		if (m_MeshResource->m_OnReloaded.Contains(callback))
			m_MeshResource->m_OnReloaded -= callback;
		if (m_MeshResource->m_OnCoordinateFrameChanged.Contains(callback))
			m_MeshResource->m_OnCoordinateFrameChanged -= callback;
	}
}

//----------------------------------------------------------------------------

bool	SMesh::_AddMeshBatch(const RHI::PApiManager &apiManager, CMeshNew *mesh, u32 colorSet, bool loadAlphaAsColor)
{
	if (!m_MeshBatches.PushBack().Valid())
		return false;

	SMeshBatch	&meshBuffers = m_MeshBatches.Last();

	const CMeshTriangleBatch	&triangleBatch = mesh->TriangleBatch();

	const TStridedMemoryView<const CFloat3>		positions = triangleBatch.m_VStream.Positions();
	const TStridedMemoryView<const CFloat3>		normals = triangleBatch.m_VStream.Normals();
	const TStridedMemoryView<const CFloat4>		tangents = triangleBatch.m_VStream.Tangents();
	const TStridedMemoryView<const CFloat2>		texCoords = triangleBatch.m_VStream.Texcoords();
	const TStridedMemoryView<const CFloat4>		colors = triangleBatch.m_VStream.AbstractStream<CFloat4>(CVStreamSemanticDictionnary::ColorStreamToOrdinal(colorSet));

	m_HasVertexColors = colors.Count() == positions.Count();

	u32				offset = 0;
	const u32		vertexCount = positions.Count();
	const u32		totalVboSize =	vertexCount * (positions.ElementSizeInBytes() + normals.ElementSizeInBytes() + texCoords.ElementSizeInBytes() + (m_HasVertexColors ? colors.ElementSizeInBytes() : 0) + tangents.ElementSizeInBytes());
	RHI::PGpuBuffer	vertexBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Vertex Buffer"), RHI::VertexBuffer, totalVboSize);
	if (vertexBuffer == null)
		return false;

	void	*mappedVertexBuffer = apiManager->MapCpuView(vertexBuffer);
	if (mappedVertexBuffer == null)
		return false;

	// Transfer positions
	meshBuffers.m_PositionsOffset = offset;
	{
		CFloat3		*buffData = static_cast<CFloat3*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		for (u32 i = 0; i < vertexCount; ++i)
			buffData[i] = positions[i];
	}
	offset += positions.ElementSizeInBytes() * vertexCount;

	// Transfer normals
	meshBuffers.m_NormalsOffset = offset;
	PK_ASSERT(positions.Count() == normals.Count() || normals.Empty());
	if (positions.Count() == normals.Count())
	{
		CFloat3		*buffData = static_cast<CFloat3*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		for (u32 i = 0; i < vertexCount; ++i)
			buffData[i] = normals[i];
	}
	else // don't fail - fill with default
	{
		const CFloat3	defaultValue = CCoordinateFrame::Axis(Axis_Up);
		CFloat3			*buffData = static_cast<CFloat3*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		u32				i = 0;
		for (; i < normals.Count(); ++i)
			buffData[i] = normals[i];
		for (; i < vertexCount; ++i)
			buffData[i] = defaultValue;
	}
	offset += normals.ElementSizeInBytes() * vertexCount;

	// Transfer tangents
	meshBuffers.m_TangentsOffset = offset;
	PK_ASSERT(positions.Count() == tangents.Count() || tangents.Empty());
	if (positions.Count() == tangents.Count())
	{
		CFloat4		*buffData = static_cast<CFloat4*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		PK_ASSERT(!tangents.Virtual() && tangents.Contiguous());
		PK_ASSERT(tangents.CoveredBytes() == vertexCount * sizeof(*buffData));
		Mem::Copy_Uncached(buffData, tangents.Data(), vertexCount * sizeof(*buffData));
	}
	else // don't fail - fill with default
	{
		const CFloat4	defaultValue = CFloat4(CCoordinateFrame::Axis(Axis_Right), 1.0f);
		CFloat4			*buffData = static_cast<CFloat4*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		if (tangents.Count() > 0)
		{
			PK_ASSERT(!tangents.Virtual() && tangents.Contiguous());
			Mem::Copy_Uncached(buffData, tangents.Data(), tangents.Count() * sizeof(*buffData));
		}
		Mem::Fill128_Uncached(buffData + tangents.Count(), &defaultValue, vertexCount - tangents.Count());
	}
	offset += tangents.ElementSizeInBytes() * vertexCount;

	// Transfer UVs
	meshBuffers.m_TexCoordsOffset = offset;
	PK_ASSERT(positions.Count() == texCoords.Count() || texCoords.Empty());
	if (positions.Count() == texCoords.Count())
	{
		CFloat2		*buffData = static_cast<CFloat2*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		for (u32 i = 0; i < vertexCount; ++i)
			buffData[i] = texCoords[i];
	}
	else // don't fail - fill with default
	{
		const CFloat2	defaultValue = CFloat2::ZERO;
		CFloat2			*buffData = static_cast<CFloat2*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		u32 i = 0;
		for (; i < texCoords.Count(); ++i)
			buffData[i] = texCoords[i];
		for (; i < vertexCount; ++i)
			buffData[i] = defaultValue;
	}
	offset += texCoords.ElementSizeInBytes() * vertexCount;

	// Transfer colors
	meshBuffers.m_ColorsOffset = offset;
	PK_ASSERT(positions.Count() == colors.Count() || colors.Empty());
	if (m_HasVertexColors)
	{
		CFloat4		*buffData = static_cast<CFloat4*>(Mem::AdvanceRawPointer(mappedVertexBuffer, offset));
		if (loadAlphaAsColor)
		{
			for (u32 i = 0; i < vertexCount; ++i)
				buffData[i] = colors[i].www1();
		}
		else
		{
			for (u32 i = 0; i < vertexCount; ++i)
				buffData[i] = colors[i];
		}
		offset += colors.ElementSizeInBytes() * vertexCount;
	}

	PK_ASSERT(offset == totalVboSize);

//	buffers.PushBack(vertexBuffer);	// NOTE(Julien): Removed, v2.b1 : unused
	apiManager->UnmapCpuView(vertexBuffer);

	// Transfer indices
	const void		*indexData = triangleBatch.m_IStream.RawStream();
	if (!PK_VERIFY(indexData != null))
		return false;

	meshBuffers.m_IndexBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Index Buffer"), RHI::IndexBuffer, triangleBatch.m_IStream.StreamSize());
	if (meshBuffers.m_IndexBuffer == null)
		return false;

	void	*buffIndexData = apiManager->MapCpuView(meshBuffers.m_IndexBuffer);
	if (!PK_VERIFY(buffIndexData != null))
		return false;

	Mem::Copy(buffIndexData, indexData, triangleBatch.m_IStream.StreamSize());

	meshBuffers.m_BindPoseVertexBuffers = vertexBuffer;
	apiManager->UnmapCpuView(meshBuffers.m_IndexBuffer);
	meshBuffers.m_IndexCount = triangleBatch.m_IStream.IndexCount();
	meshBuffers.m_IndexSize = triangleBatch.m_IStream.IndexByteWidth() == 2 ? RHI::IndexBuffer16Bit : RHI::IndexBuffer32Bit;

	meshBuffers.m_BBox = mesh->BBox();
//	buffers.PushBack(meshBuffers.m_IndexBuffer);	// NOTE(Julien): Removed, v2.b1 : unused
	return true;
}

//----------------------------------------------------------------------------

void	SMesh::_OnResourceReloaded(CResourceMesh *resource)
{
	(void)resource;
	m_ResourceDirtyKey.Inc();
}

//----------------------------------------------------------------------------

bool	SMesh::Load(	const RHI::PApiManager &apiManager,
						TMemoryView<const RHI::SConstantSetLayout> constLayouts,
						CResourceManager *resourceManager,
						u32 colorSet /* = 0 */,
						bool loadAlphaAsColor /* = false */,
						const RHI::PTexture whiteFallbackTx /* = null */,
						const RHI::PTexture normalFallbackTx /* = null */,
						const RHI::PConstantSampler fallbackSampler /* = null */)
{
	if (!PK_VERIFY(resourceManager != null))
		return false;

	if (m_MeshResource != null)
		m_MeshResource->m_OnReloaded -= FastDelegate<void(CResourceMesh*)>(this, &SMesh::_OnResourceReloaded);

	const u32	prevDirtyKey = m_ResourceDirtyKey.Load();

	m_MeshResource = resourceManager->Load<CResourceMesh>(m_MeshPath, false, SResourceLoadCtl(false, true));
	if (m_MeshResource == null ||
		m_MeshResource->Empty())
	{
		CLog::Log(PK_ERROR, "Failed loading mesh resource \"%s\"", m_MeshPath.Data());
		return false;
	}

	if (m_MeshResource != null)
	{
		m_MeshResource->m_OnReloaded += FastDelegate<void(CResourceMesh*)>(this, &SMesh::_OnResourceReloaded);
		m_MeshResource->m_OnCoordinateFrameChanged += FastDelegate<void(CResourceMesh*)>(this, &SMesh::_OnResourceReloaded);
	}

	bool hasDiffuse = false;
	bool hasRoughMetal = false;
	bool hasNormal = false;

	// Choose a constant set layout depending on the mesh:
	if (!m_DiffuseMap.m_Path.Empty() || m_DiffuseMap.m_ImageData != null)
	{
		if (!PK_VERIFY(_LoadMap(apiManager, m_DiffuseMap, resourceManager)))
			return false;
		hasDiffuse = true;
	}
	else if (whiteFallbackTx != null && fallbackSampler != null)
	{
		m_DiffuseMap.m_Sampler = fallbackSampler;
		m_DiffuseMap.m_Texture = whiteFallbackTx;
		hasDiffuse = true;
	}

	if (!m_RoughMetalMap.m_Path.Empty() || m_RoughMetalMap.m_ImageData != null)
	{
		if (!PK_VERIFY(_LoadMap(apiManager, m_RoughMetalMap, resourceManager, false)))
			return false;
		hasRoughMetal = true;
	}
	else if (whiteFallbackTx != null && fallbackSampler != null)
	{
		m_RoughMetalMap.m_Sampler = fallbackSampler;
		m_RoughMetalMap.m_Texture = whiteFallbackTx;
		hasRoughMetal = true;
	}

	if (!m_NormalMap.m_Path.Empty() || m_NormalMap.m_ImageData != null)
	{
		if (!PK_VERIFY(_LoadMap(apiManager, m_NormalMap, resourceManager, false)))
			return false;
		hasNormal = true;
	}
	else if (normalFallbackTx != null && fallbackSampler != null)
	{
		m_NormalMap.m_Sampler = fallbackSampler;
		m_NormalMap.m_Texture = normalFallbackTx;
		hasNormal = true;
	}

	if (hasDiffuse && hasRoughMetal && hasNormal)
		m_Material = GBufferCombination_Diffuse_RoughMetal_Normal;
	if (hasDiffuse && hasRoughMetal)
		m_Material = GBufferCombination_Diffuse_RoughMetal;
	else if (hasDiffuse)
		m_Material = GBufferCombination_Diffuse;
	else
		m_Material = GBufferCombination_SolidColor;

	const RHI::SConstantSetLayout	&layout = constLayouts[m_Material];

	m_ConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("Mesh Constant Set"), layout);
	if (m_ConstantSet == null)
		return false;

	m_MeshBatchCoordinateFrame = m_MeshResource->CoordinateSystem();
	m_MeshInfo = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("MeshInfo Constant Buffer"), RHI::ConstantBuffer, sizeof(SMeshFragmentConstant));

	u32		bindingPoint = 0;

	m_ConstantSet->SetConstants(m_MeshInfo, bindingPoint);
	++bindingPoint;

	if (hasDiffuse)
	{
		m_ConstantSet->SetConstants(m_DiffuseMap.m_Sampler, m_DiffuseMap.m_Texture, bindingPoint);
		++bindingPoint;
	}

	if (hasRoughMetal)
	{
		m_ConstantSet->SetConstants(m_RoughMetalMap.m_Sampler, m_RoughMetalMap.m_Texture, bindingPoint);
		++bindingPoint;
	}

	if (hasNormal)
	{
		m_ConstantSet->SetConstants(m_NormalMap.m_Sampler, m_NormalMap.m_Texture, bindingPoint);
		++bindingPoint;
	}

	m_ConstantSet->UpdateConstantValues();

	TMemoryView<const PResourceMeshBatch>	batchList = m_MeshResource->BatchList(m_MeshLOD);

	for (const auto &batch : batchList)
	{
		if (PK_VERIFY(batch != null) &&
			!_AddMeshBatch(apiManager, batch->RawMesh(), colorSet, loadAlphaAsColor))
		{
			return false;
		}
	}

	m_ResourceCleanKey = prevDirtyKey;
	return true;
}

//----------------------------------------------------------------------------

bool	SMesh::_LoadMap(const RHI::PApiManager &apiManager, STextureMap &map, CResourceManager *resourceManager, bool interpretAsSrgb /* = true */)
{
	PCImage	image = map.m_ImageData;
	if (image == null)
	{
		map.m_Image = resourceManager->Load<CImage>(map.m_Path, false, SResourceLoadCtl(false, true));
		if (map.m_Image != null)
			image = &(*map.m_Image);
	}

	if (image == null || image->Empty())
	{
		CLog::Log(PK_ERROR, "Failed loading texture resource \"%s\"", map.m_Path.Data());
		return false;
	}

	map.m_Texture = RHI::PixelFormatFallbacks::CreateTextureAndFallbackIFN(apiManager, *image, interpretAsSrgb, map.m_Path.Empty() ? "<INTERNAL>" : map.m_Path.Data());
	if (map.m_Texture == null)
		return false;

	map.m_Sampler = apiManager->CreateConstantSampler(	RHI::SRHIResourceInfos("Mesh Texture Sampler"),
														RHI::SampleLinear,
														RHI::SampleLinearMipmapLinear,
														RHI::SampleRepeat,
														RHI::SampleRepeat,
														RHI::SampleRepeat,
														map.m_Texture->GetMipmapCount());
	if (map.m_Sampler == null)
		return false;
	return true;
}

//----------------------------------------------------------------------------

bool	SMesh::RefreshSkinnedDatas(const RHI::PApiManager &apiManager, TMemoryView<const SSkinnedMeshData> skinnedDatas)
{
	PK_NAMEDSCOPEDPROFILE("Refresh backdrop skinned datas");

	if (m_MeshResource == null || m_MeshResource->Empty())
		return true;

	const u32	batchCount = m_MeshBatches.Count();
	for (u32 iBatch = 0; iBatch < batchCount; ++iBatch)
	{
		SMeshBatch	&batch = m_MeshBatches[iBatch];

		PK_ASSERT(batch.m_BindPoseVertexBuffers != null);
		for (u32 iInstance = 0; iInstance < batch.m_Instances.Count(); ++iInstance)
			batch.m_Instances[iInstance].m_HasValidSkinnedData = false;
	}
	if (skinnedDatas.Empty())
		return true;

	PK_ASSERT(skinnedDatas.Count() == m_Transforms.Count());
	const u32	skinnedDataCount = m_Transforms.Count();
	for (u32 iData = 0; iData < skinnedDataCount; ++iData)
	{
		const SSkinnedMeshData	&skinnedData = skinnedDatas[iData];
		if (!skinnedData.m_Valid)
			continue;
		for (const auto &submesh : skinnedData.m_SubMeshes)
		{
			if (!PK_VERIFY(m_MeshBatches.Count() > submesh.m_SubMeshID))
				continue;
			if (submesh.m_RawData.Empty())	// Nothing to update there
				continue;

			SMeshBatch				&batch = m_MeshBatches[submesh.m_SubMeshID];
			SMeshBatch::SInstance	*instance = null;
			if (iData >= batch.m_Instances.Count())
			{
				if (!PK_VERIFY(batch.m_Instances.PushBack().Valid()))
					return false;
				instance = &batch.m_Instances.Last();
			}
			else
				instance = &batch.m_Instances[iData];

			instance->m_HasValidSkinnedData = true;

			// Transfer positions/normals
			PK_ASSERT(submesh.m_RawData.SizeInBytes() == (batch.m_NormalsOffset / sizeof(CFloat3)) * ( 2 * sizeof(CFloat3) + sizeof(CFloat4)));
			if (instance->m_SkinnedVertexBuffers == null)
				instance->m_SkinnedVertexBuffers = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Mesh Skinned Vertex Buffer"), RHI::VertexBuffer, submesh.m_RawData.SizeInBytes());
			if (!PK_VERIFY(instance->m_SkinnedVertexBuffers != null))
				return false;

			void	*mappedData = apiManager->MapCpuView(instance->m_SkinnedVertexBuffers);
			if (mappedData == null)
				return false;
			PK_NAMESPACE::Mem::Copy(mappedData, submesh.m_RawData.RawDataPointer(), submesh.m_RawData.SizeInBytes());
			apiManager->UnmapCpuView(instance->m_SkinnedVertexBuffers);
		}
	}
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
