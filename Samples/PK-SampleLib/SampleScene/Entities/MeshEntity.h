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

#include <PK-SampleLib/PKSample.h>

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_kernel/include/kr_resources.h>
#include <pk_geometrics/include/ge_mesh_resource.h>
#include <pk_geometrics/include/ge_coordinate_frame.h>
#include <pk_imaging/include/im_image.h>
#include <PK-SampleLib/ShaderDefinitions/BasicSceneShaderDefinitions.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	STextureMap
{
	CString					m_Path;
	TResourcePtr<CImage>	m_Image;
	PCImage					m_ImageData;
	RHI::PTexture			m_Texture;
	RHI::PConstantSampler	m_Sampler;

	STextureMap();
};

//----------------------------------------------------------------------------

struct	SMeshVertexConstant
{
	CFloat4x4	m_ModelMatrix;
	CFloat4x4	m_NormalMatrix;
};

//----------------------------------------------------------------------------

struct	SMeshFragmentConstant
{
	float		m_Roughness;
	float		m_Metalness;
};

//----------------------------------------------------------------------------

// Render ready skinned data
struct	SSkinnedMeshData
{
	struct	SSkinnedDataSubMesh
	{
		u32			m_SubMeshID;
		TArray<u8>	m_RawData;
		SSkinnedDataSubMesh() : m_SubMeshID(0) {}
	};

	bool		m_Valid;
	TSemiDynamicArray<SSkinnedDataSubMesh, 4>	m_SubMeshes;

	SSkinnedMeshData() : m_Valid(false) { }
};

//----------------------------------------------------------------------------

struct	SMesh
{
	SMesh();
	~SMesh();

	bool					Load(	const RHI::PApiManager &apiManager,
									TMemoryView<const RHI::SConstantSetLayout> constLayout,
									CResourceManager *resourceManager,
									u32 colorSet = 0,
									bool loadAlphaAsColor = false,
									const RHI::PTexture whiteFallbackTex = null,
									const RHI::PTexture normalFallbackTx = null,
									const RHI::PConstantSampler samplerFallback = null);

	struct	SMeshBatch
	{
		struct	SInstance
		{
			bool			m_HasValidSkinnedData;
			RHI::PGpuBuffer	m_SkinnedVertexBuffers;

			SInstance() : m_HasValidSkinnedData(false), m_SkinnedVertexBuffers(null) { }
		};

		TArray<SInstance>		m_Instances;
		RHI::PGpuBuffer			m_BindPoseVertexBuffers;
		RHI::PGpuBuffer			m_Texcoords;

		u32						m_PositionsOffset;
		u32						m_NormalsOffset;
		u32						m_TangentsOffset;
		u32						m_TexCoordsOffset;
		u32						m_ColorsOffset;

		RHI::PGpuBuffer			m_IndexBuffer;
		RHI::EIndexBufferSize	m_IndexSize;
		u32						m_IndexCount;

		CAABB					m_BBox;
	};

	TArray<CFloat4x4>				m_Transforms;
	TResourcePtr<CResourceMesh>		m_MeshResource;

	STextureMap						m_DiffuseMap;
	STextureMap						m_NormalMap;
	STextureMap						m_RoughMetalMap;
	u32								m_Maps;
	EGBufferCombination				m_Material;

	RHI::PConstantSet				m_ConstantSet;

	RHI::PGpuBuffer					m_MeshInfo;

	ECoordinateFrame				m_MeshBatchCoordinateFrame;
	TArray<SMeshBatch>				m_MeshBatches;
	CString							m_MeshPath;
	CGuid							m_MeshLOD;

	float							m_Roughness;
	float							m_Metalness;
	bool							m_HasVertexColors;

	PImage							m_DummyWhiteImage;
	PImage							m_DummyNormalImage;
	PImage							m_DummyRoughMetalImage;

	TAtomic<u32>					m_ResourceDirtyKey;	// written by update thread
	u32								m_ResourceCleanKey;	// written by render thread

	bool							RefreshSkinnedDatas(const RHI::PApiManager &apiManager, TMemoryView<const SSkinnedMeshData> skinnedDatas);
	bool							IsDirty() const { return m_ResourceDirtyKey.Load() != m_ResourceCleanKey; }

private:
	static bool				_LoadMap(const RHI::PApiManager &apiManager, STextureMap &map, CResourceManager *resourceManager, bool interpretAsSrgb = true);
	bool					_AddMeshBatch(const RHI::PApiManager &apiManager, CMeshNew *mesh, u32 colorSet, bool loadAlphaAsColor);
	void					_OnResourceReloaded(CResourceMesh *resource);
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
