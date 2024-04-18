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
#include "ShaderDefinitions.h"

#include "UnitTestsShaderDefinitions.h"
#include "SampleLibShaderDefinitions.h"
#include "BasicSceneShaderDefinitions.h"
#include "EditorShaderDefinitions.h"

#include <pk_rhi/include/interfaces/SShaderBindings.h>
#include <pk_rhi/include/interfaces/SConstantSetLayout.h>
#include <pk_rhi/include/EnumHelper.h>

#include <pk_kernel/include/kr_buffer_parsing_utils.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

void	CreateShaderDefinitions(TArray<SShaderCombination> &shaders)
{
	// Unit Tests
	AddTriangleNoVboDefinition(shaders);
	AddPosColVBODefinition(shaders);
	AddConstMat4Definition(shaders);
	AddUniformTestDefinition(shaders);
	AddUnitTestStructLayoutDefinition(shaders);
	AddGaussianBlurDefinition(shaders);
	AddCopyDefinition(shaders);
	AddDistortionDefinition(shaders);
	AddColorRemapDefinition(shaders);
	AddTextureFormatDefinition(shaders);
	AddMipmappingDefinition(shaders);
	AddMipmappingModeDefinition(shaders);
	AddHDRTextureDefinition(shaders);
	AddRTMSAADefinition(shaders);
	AddDrawInstancedDefinition(shaders);
	AddGeometryDefinition(shaders);
	AddGeometry2Definition(shaders);
	AddCubemapRenderDefinition(shaders);
	AddCubemapComputeDefinition(shaders);
	AddComputeUnitTestBufferDefinition(shaders);
	AddComputeUnitTestTextureDefinition(shaders);

	// Sample Lib
	AddGizmoDefinition(shaders);
	AddImGuiDefinition(shaders);
	AddProfilerDefinition(shaders);
	AddProfilerDrawNodeDefinition(shaders);
	AddComputeParticleCountPerMeshDefinition(shaders);
	AddComputeMeshMatricesDefinition(shaders);
	AddInitIndirectionOffsetsBufferDefinition(shaders);
	AddComputeMeshIndirectionBufferDefinition(shaders);
	AddComputeSortKeysDefinition(shaders);
	AddSortUpSweepDefinition(shaders);
	AddSortPrefixSumDefinition(shaders);
	AddSortDownSweepDefinition(shaders);
	AddFXAADefinition(shaders);
	AddComputeRibbonSortKeysDefinition(shaders);
	AddComputeCubemapDefinition(shaders);
	AddFilterCubemapDefinition(shaders);
	AddComputeMipMapDefinition(shaders);
	AddBlurCubemapProcessDefinition(shaders);
	AddBlurCubemapRenderFaceDefinition(shaders);

	// Basic Scene
	AddGBufferDefinition(shaders);
	AddGBufferShadowDefinition(shaders);
	AddDeferredLightDefinition(shaders);
	AddDeferredMergingDefinition(shaders);

	// Pop ed
	AddEditorDebugParticleDefinition(shaders);
	AddEditorDebugVertexBBParticleDefinition(shaders);
	AddEditorDebugGeomBBParticleDefinition(shaders);
	AddEditorDebugDrawDefinition(shaders);
	AddEditorDebugDrawValueDefinition(shaders);
	AddEditorDebugDrawLineDefinition(shaders);
	AddEditorHeatmapOverdraw(shaders);
	AddBrushBackdropDefinition(shaders);
	AddParticleLightDefinition(shaders);
	AddEditorSelectorDefinition(shaders);
}

//----------------------------------------------------------------------------

void	GenerateDefinesFromDefinition(TArray<CString> &defines, const RHI::SShaderDescription &desc, RHI::EShaderStage stage)
{
	for (const RHI::SShaderDefine &define : desc.m_Bindings.m_Defines)
	{
		if ((define.m_ShaderStages & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			if (define.m_Value.Empty())
				defines.PushBack(define.m_Define);
			else
				defines.PushBack(CString::Format("%s=%s", define.m_Define.Data(), define.m_Value.Data()));
		}
	}
	for (const RHI::SConstantSetLayout &constSet : desc.m_Bindings.m_ConstantSets)
	{
		if ((constSet.m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			for (const RHI::SConstantSetLayout::SConstantDesc &constant : constSet.m_Constants)
			{
				if (constant.m_Type == RHI::TypeConstantBuffer)
				{
					for (const RHI::SConstantVarDesc &var : constant.m_ConstantBuffer.m_Constants)
					{
						defines.PushBack(CString("CONST_") + constant.m_ConstantBuffer.m_Name + "_" + var.m_Name);
					}
				}
				else if (constant.m_Type == RHI::TypeRawBuffer)
				{
					defines.PushBack(CString("VRESOURCE_") + constant.m_RawBuffer.m_Name);
				}
				else if (constant.m_Type == RHI::TypeConstantSampler)
				{
					if (constant.m_ConstantSampler.m_Type == RHI::SamplerTypeMulti)
					{
						defines.PushBack(CString("SAMPLER_MS_") + constant.m_ConstantSampler.m_Name);
					}
					else
					{
						defines.PushBack(CString("SAMPLER_") + constant.m_ConstantSampler.m_Name);
					}
				}
			}
		}
	}
	if (stage == RHI::VertexShaderStage)
	{
		for (const RHI::SVertexAttributeDesc &vertexInput : desc.m_Bindings.m_InputAttributes)
			defines.PushBack(CString("VINPUT_") + vertexInput.m_Name);
	}
	for (const RHI::SPushConstantBuffer &pushConst : desc.m_Bindings.m_PushConstants)
	{
		if ((pushConst.m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			for (const RHI::SConstantVarDesc &var : pushConst.m_Constants)
			{
				defines.PushBack(CString("CONST_") + pushConst.m_Name + "_" + var.m_Name);
			}
		}
	}
	const bool	vertexShaderStage = stage == RHI::VertexShaderStage;
	const bool	geomShaderStage = stage == RHI::GeometryShaderStage;
	const bool	fragmentShaderStage = stage == RHI::FragmentShaderStage;
	for (const RHI::SVertexOutput &finput : desc.m_VertexOutput)
	{
		if (vertexShaderStage)
			defines.PushBack("VOUTPUT_" + finput.m_Name);

		if (desc.m_Pipeline == RHI::VsPs)
		{
			if (fragmentShaderStage)
				defines.PushBack("FINPUT_" + finput.m_Name);
		}
		else
		{
			if (geomShaderStage)
				defines.PushBack("GINPUT_" + finput.m_Name);
		}
	}
	for (const RHI::SVertexOutput &finput : desc.m_GeometryOutput.m_GeometryOutput)
	{
		if (geomShaderStage)
			defines.PushBack("GOUTPUT_" + finput.m_Name);

		if (desc.m_Pipeline == RHI::VsGsPs)
		{
			if (fragmentShaderStage)
				defines.PushBack("FINPUT_" + finput.m_Name);
		}
	}
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
