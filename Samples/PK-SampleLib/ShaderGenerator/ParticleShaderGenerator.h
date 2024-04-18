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

#include <pk_rhi/include/interfaces/SShaderBindings.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

// TODO: This needs refactoring, we should only need:
// - Option_None
// - Option_VertexPassThrough
// - Option_Billboard_VertexBillboarding
// - Option_Billboard_GeomBillboarding
// - Option_Triangle_VertexBillboarding
// Everything else is deduced from the .pkma and activated features

/*!
*	@brief enum to select options needed in shader pass.
*	 Helper operators: enum | enum -> enum // enum & enum -> bool
*/
enum	EShaderOptions
{
	Option_None = 0,
	Option_VertexPassThrough						= 0x1 << 0,
	Option_VertexBillboarding						= 0x1 << 1, // Vertex billboarding (billboard)
	Option_GeomBillboarding							= 0x1 << 2, // Geometry billboarding (billboard)
	Option_TriangleVertexBillboarding				= 0x1 << 3, // Vertex billboarding (triangle)
	Option_RibbonVertexBillboarding					= 0x1 << 4, // Vertex billboarding (ribbon)
	Option_GPUMesh									= 0x1 << 5, // Compute shader "billboarding" (mesh)
	Option_Axis_C1									= 0x1 << 6, // One billboarding constraint axis
	Option_Axis_C2									= 0x1 << 7 | Option_Axis_C1, // Two billboarding constraint axis
	Option_Capsule									= 0x1 << 8, // Capsule billboard: 6 vertices instead of 4
	Option_BillboardSizeFloat2						= 0x1 << 9, // Size as a float2 instead of float
	Option_GPUStorage								= 0x1 << 10, // GPU storage (= GPU simulation)
	Option_GPUSort									= 0x1 << 11, // Uses a compute shader sort (ex: alpha blended GPU simulated particles)

	//Option_CorrectDeformation				= 0x1 << 8,
};

//----------------------------------------------------------------------------

enum	EComputeShaderType
{
	ComputeType_None,
	ComputeType_ComputeParticleCountPerMesh,
	ComputeType_ComputeParticleCountPerMesh_MeshAtlas,
	ComputeType_ComputeParticleCountPerMesh_LOD,
	ComputeType_ComputeParticleCountPerMesh_LOD_MeshAtlas,
	ComputeType_InitIndirectionOffsetsBuffer,
	ComputeType_InitIndirectionOffsetsBuffer_LODNoAtlas,
	ComputeType_ComputeMeshIndirectionBuffer,
	ComputeType_ComputeMeshIndirectionBuffer_MeshAtlas,
	ComputeType_ComputeMeshIndirectionBuffer_LOD,
	ComputeType_ComputeMeshIndirectionBuffer_LOD_MeshAtlas,
	ComputeType_ComputeMeshMatrices,
	ComputeType_ComputeSortKeys,
	ComputeType_ComputeSortKeys_CameraDistance,
	ComputeType_ComputeSortKeys_RibbonIndirection,
	ComputeType_ComputeSortKeys_CameraDistance_RibbonIndirection,
	ComputeType_SortUpSweep,
	ComputeType_SortPrefixSum,
	ComputeType_SortDownSweep,
	ComputeType_SortUpSweep_KeyStride64,
	ComputeType_SortPrefixSum_KeyStride64,
	ComputeType_SortDownSweep_KeyStride64,
	ComputeType_ComputeRibbonSortKeys,
	ComputeType_Count,
};

//----------------------------------------------------------------------------

struct	SPerStageShaderName
{
	EShaderOptions	m_ShaderOption;

	const char		*m_Vertex;
	const char		*m_Geom;
	const char		*m_Fragment;
};

//----------------------------------------------------------------------------

namespace	ShaderOptionsUtils
{
	// returns a CStringView into 'outStorage'. If the capacity of 'outStorage' is too small, returns an empty view
	CStringView		GetShaderName(EShaderOptions options, RHI::EShaderStage stage, const TMemoryView<char> &outStorage);
}

//----------------------------------------------------------------------------

namespace	ParticleShaderGenerator
{
	// Helpers
	CString		GenGetMeshTransformHelper();

	// Vertex pass through
	CString		GenVertexPassThrough(const RHI::SShaderDescription &description, CGuid positionIdx, EShaderOptions options);
	CString		GetVertexPassThroughFunctionName();

	// Geometry billboarding
	CString		GenGeomBillboarding(const RHI::SShaderDescription &description);
	CString		GetGeomBillboardingFunctionName();

	// Concat all needed functions
	CString		GenAdditionalFunction(	const RHI::SShaderDescription &description,
										EShaderOptions options,
										TArray<CString> &funcToCall,
										RHI::EShaderStage stage);
	RHI::EShaderStagePipeline	GetShaderStagePipeline(EShaderOptions options);
};

//----------------------------------------------------------------------------

inline EShaderOptions	operator|(EShaderOptions a, EShaderOptions b)
{
	return static_cast<EShaderOptions>(static_cast<int>(a) | static_cast<int>(b));
}

//----------------------------------------------------------------------------

inline bool		operator&(EShaderOptions a, EShaderOptions b)
{
	return (static_cast<int>(a) & static_cast<int>(b)) == b;
}

//----------------------------------------------------------------------------

inline EShaderOptions	operator^(EShaderOptions a, EShaderOptions b)
{
	return static_cast<EShaderOptions>(static_cast<int>(a) ^ static_cast<int>(b));
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
