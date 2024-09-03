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

#include "EditorShaderDefinitions.h"
#include "ShaderDefinitions.h"
#include "SampleLibShaderDefinitions.h"
#include "UnitTestsShaderDefinitions.h"
#include "BasicSceneShaderDefinitions.h"

#include "PK-SampleLib/RenderIntegrationRHI/RHIGraphicResources.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHIRenderIntegrationConfig.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

void	FillEditorDebugParticleInputVertexBuffers(	TArray<RHI::SVertexInputBufferDesc>	&inputVertexBuffers,
													ParticleDebugShader shaderMode,
													EParticleDebugBillboarderType bbType,
													bool gpuStorage)
{
	if (bbType == ParticleDebugBT_Billboard)
	{
		// Position:
		inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4)));
	}
	else if (bbType == ParticleDebugBT_Mesh)
	{
		// Position:
		inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3)));
		// Mesh transforms:
		if (!gpuStorage)
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(CFloat4x4)));
		// Vertex colors
		if (shaderMode == ParticleDebugShader_VertexColor)
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4)));
	}
	else if (bbType == ParticleDebugBT_Light)
	{
		// Position:
		inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3)));
		// Light transforms:
		inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(CFloat3)));
		// Light scales:
		inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(float)));
	}
	else if (bbType <= ParticleDebugBT_VertexBillboarding_SizeFloat2_C2)
	{
		inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2))); // Texcoords (expand direction)
	}
	else if (bbType != ParticleDebugBT_TriangleVertexBillboarding)
	{
		// Geom billboarding vertex inputs:
		if (gpuStorage)
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3))); // Position
		else
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))); // Position + DrID:

		if (bbType >= ParticleDebugBT_GeomBillboarding_SizeFloat2_C0 &&
			bbType <= ParticleDebugBT_GeomBillboarding_SizeFloat2_C2)
		{
			// Size float2
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2)));
		}
		else
		{
			// Size float
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(float)));
		}

		// Rotation:
		if (bbType == ParticleDebugBT_GeomBillboarding_C0 || bbType == ParticleDebugBT_GeomBillboarding_C2 ||
			bbType == ParticleDebugBT_GeomBillboarding_SizeFloat2_C0 || bbType == ParticleDebugBT_GeomBillboarding_SizeFloat2_C2)
		{
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(float)));
		}

		// If > C1
		if ((bbType >= ParticleDebugBT_GeomBillboarding_C1 && bbType <= ParticleDebugBT_GeomBillboarding_C2) ||
			(bbType >= ParticleDebugBT_GeomBillboarding_SizeFloat2_C1 && bbType <= ParticleDebugBT_GeomBillboarding_SizeFloat2_C2))
		{
			// Axis0:
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3)));

			if (bbType == ParticleDebugBT_GeomBillboarding_SizeFloat2_C2 || bbType == ParticleDebugBT_GeomBillboarding_C2)
			{
				// Axis1:
				inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3)));
			}
		}
		if (gpuStorage)
		{
			// Enabled
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(u32)));
		}
	}

	// Common vertex inputs:
	if (shaderMode == ParticleDebugShader_Color)
	{
		// Color:
		if (((bbType == ParticleDebugBT_Mesh) && !gpuStorage) || bbType == ParticleDebugBT_Light)
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(CFloat4)));
		else if (bbType == ParticleDebugBT_Billboard ||
				 bbType >= ParticleDebugBT_GeomBillboarding_C0)
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4)));
	}
	else if (shaderMode == ParticleDebugShader_Selection)
	{
		// IsSelected
		if (bbType == ParticleDebugBT_Mesh || bbType == ParticleDebugBT_Light)
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(float)));
		else if (bbType == ParticleDebugBT_Billboard ||
				 bbType >= ParticleDebugBT_GeomBillboarding_C0)
			inputVertexBuffers.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(float)));
	}

}

//----------------------------------------------------------------------------

void	FillEditorDebugParticleShaderBindings(	RHI::SShaderBindings &bindings,
												ParticleDebugShader shaderMode,
												EParticleDebugBillboarderType bbType,
												bool gpuStorage,
												RHI::SShaderDescription *shaderDescription)
{
	const bool	vertexBB = bbType >= ParticleDebugBT_VertexBillboarding_C0 && bbType <= ParticleDebugBT_TriangleVertexBillboarding;
	const bool	geomBB = bbType >= ParticleDebugBT_GeomBillboarding_C0;

	bindings.Reset();
	if (shaderDescription != null)
	{
		shaderDescription->m_VertexOutput.Clear();
		shaderDescription->m_GeometryOutput.m_GeometryOutput.Clear();
		if (geomBB)
		{
			shaderDescription->m_DrawMode = RHI::DrawModePoint;
			shaderDescription->m_GeometryOutput.m_MaxVertices = 6;
			shaderDescription->m_GeometryOutput.m_PrimitiveType = RHI::DrawModeTriangleStrip;
		}
		else
		{
			shaderDescription->m_DrawMode = RHI::DrawModeTriangle;
		}
	}

	// Add the scene info constant set:
	RHI::SConstantSetLayout		layout;
	CreateSceneInfoConstantLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);

	u32		shaderLocationBinding = 0;
	u32		vBufferLocationBinding = 0;

	RHI::SConstantSetLayout		simDataSetLayout(RHI::VertexShaderMask);
	RHI::SConstantSetLayout		streamOffsetsSetLayout(RHI::VertexShaderMask); // gpu sim
	RHI::SConstantSetLayout		selectionSetLayout(RHI::VertexShaderMask);

	if (bbType <= ParticleDebugBT_Light)
	{
		PK_ASSERT(!geomBB && !vertexBB);
		bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Position", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding));
		++shaderLocationBinding;
		++vBufferLocationBinding;

		if (bbType == ParticleDebugBT_Mesh)
		{
			if (gpuStorage)
			{
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData"));
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("MeshTransforms"));
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Indirection"));
				streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("MeshTransformsOffsets"));
				streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("IndirectionOffsets"));

				RHI::SPushConstantBuffer pushConstant;
				pushConstant.m_Name = "GPUMeshPushConstants";
				pushConstant.m_ShaderStagesMask = static_cast<RHI::EShaderStageMask>(RHI::VertexShaderMask);
				pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "DrawRequest"));
				pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "IndirectionOffsetsIndex"));
				bindings.m_PushConstants.PushBack(pushConstant).Valid();
			}
			else
			{
				bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("MeshTransform", shaderLocationBinding, RHI::TypeFloat4x4, vBufferLocationBinding)).Valid();
				++vBufferLocationBinding;
				shaderLocationBinding += RHI::VarType::GetRowNumber(RHI::TypeFloat4x4);
			}
			if (shaderMode == ParticleDebugShader_VertexColor)
			{
				// This would conflict with per instance color if those shaderMode weren't  exclusive
				bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Color", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding));
				++shaderLocationBinding;
				++vBufferLocationBinding;
			}
		}
		else if (bbType == ParticleDebugBT_Light)
		{
			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("MeshPosition", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding)).Valid();
			++vBufferLocationBinding;
			++shaderLocationBinding;
			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("MeshScale", shaderLocationBinding, RHI::TypeFloat, vBufferLocationBinding)).Valid();
			++vBufferLocationBinding;
			++shaderLocationBinding;
		}
		// Color output from vertex shader:
		if (shaderDescription != null)
			shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("fragColor", RHI::TypeFloat4));
	}
	else if (vertexBB)
	{
		PK_ASSERT(!geomBB);

		const ERendererClass	rendererType = (bbType != ParticleDebugBT_TriangleVertexBillboarding) ? Renderer_Billboard : Renderer_Triangle;

		// Vertex billboarding vertex inputs:
		if (gpuStorage)
		{
			// GPU sim: only a single raw buffer, and define "BB_GPU_SIM"
			// TODO: Rename BB_GPU_SIM with to as more generic GPU_STORAGE or GPU_SIM define. (make sure to change.vert .geom debug and standard billboarding shaders)
			bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_GPU_SIM"));
			simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData"));

			streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("EnabledsOffsets"));
			streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("PositionsOffsets"));
		}
		else
		{
			simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Indices")); // Right now, always add the indices raw buffer. Later, shader permutation
			if (rendererType == Renderer_Billboard)
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Positions"));
			else
			{
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("VertexPosition0"));
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("VertexPosition1"));
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("VertexPosition2"));
			}
		}

		if (rendererType == Renderer_Billboard)
		{
			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("TexCoords", shaderLocationBinding, RHI::TypeFloat2, vBufferLocationBinding));
			++shaderLocationBinding;
			++vBufferLocationBinding;
		}

		if (gpuStorage)
		{
			RHI::SPushConstantBuffer	desc;
			desc.m_ShaderStagesMask = RHI::VertexShaderMask;
			if (SConstantDrawRequests::GetConstantBufferDesc(desc, 1, Renderer_Billboard))
				bindings.m_PushConstants.PushBack(desc);
		}
		else
			bindings.m_ConstantSets.PushBack(PKSample::SConstantDrawRequests::GetConstantSetLayout(Renderer_Billboard));

		{
			RHI::SPushConstantBuffer	desc;

			if (SConstantVertexBillboarding::GetPushConstantBufferDesc(desc, gpuStorage))
				bindings.m_PushConstants.PushBack(desc);
		}

		// If the shader is a geom shader, we will output the color from the geom to the fragment:
		if (shaderDescription != null)
			shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("fragColor", RHI::TypeFloat4));

		RHI::EVarType	sizeType = RHI::TypeFloat;

		if (bbType >= ParticleDebugBT_VertexBillboarding_SizeFloat2_C0 &&
			bbType <= ParticleDebugBT_VertexBillboarding_SizeFloat2_C2)
		{
			bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "HAS_SizeFloat2"));
			sizeType = RHI::TypeFloat2;
		}
		if (rendererType == Renderer_Billboard)
		{
			if (gpuStorage)
			{
				if (sizeType == RHI::TypeFloat2)
					streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Size2sOffsets"));
				else
					streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("SizesOffsets"));
			}
			else
			{
				if (sizeType == RHI::TypeFloat2)
					simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Size2s"));
				else
					simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Sizes"));
			}

			bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_FeatureC0"));
		}

		if (bbType == ParticleDebugBT_VertexBillboarding_C0 || bbType == ParticleDebugBT_VertexBillboarding_C2 ||
			bbType == ParticleDebugBT_VertexBillboarding_SizeFloat2_C0 || bbType == ParticleDebugBT_VertexBillboarding_SizeFloat2_C2)
		{
			if (gpuStorage)
				streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("RotationsOffsets"));
			else
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Rotations"));
		}

		// If > C1
		if ((bbType >= ParticleDebugBT_VertexBillboarding_C1 && bbType <= ParticleDebugBT_VertexBillboarding_C2) ||
			(bbType >= ParticleDebugBT_VertexBillboarding_SizeFloat2_C1 && bbType <= ParticleDebugBT_VertexBillboarding_SizeFloat2_C2))
		{
			bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_FeatureC1"));

			if (gpuStorage)
				streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Axis0sOffsets"));
			else
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Axis0s"));

			if (bbType == ParticleDebugBT_VertexBillboarding_C1_Capsule || bbType == ParticleDebugBT_VertexBillboarding_SizeFloat2_C1_Capsule)
			{
				bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_FeatureC1_Capsule"));
			}

			if (bbType == ParticleDebugBT_VertexBillboarding_SizeFloat2_C2 || bbType == ParticleDebugBT_VertexBillboarding_C2)
			{
				bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_FeatureC2"));
				if (gpuStorage)
					streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Axis1sOffsets"));
				else
					simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Axis1s"));
			}
		}
	}
	else
	{
		PK_ASSERT(geomBB);

		bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Position", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding));
		++shaderLocationBinding;

		// Geom billboarding vertex inputs:
		if (!gpuStorage)
		{
			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("DrawRequestID", shaderLocationBinding, RHI::TypeFloat, vBufferLocationBinding, sizeof(CFloat3)));
			++shaderLocationBinding;
		}
		++vBufferLocationBinding;

		// Geom billboarding:
		if (gpuStorage)
		{
			RHI::SPushConstantBuffer	desc;
			desc.m_ShaderStagesMask = RHI::GeometryShaderMask;
			if (SConstantDrawRequests::GetConstantBufferDesc(desc, 1, Renderer_Billboard))
				bindings.m_PushConstants.PushBack(desc);
		}
		else
			bindings.m_ConstantSets.PushBack(PKSample::SConstantDrawRequests::GetConstantSetLayout(Renderer_Billboard));

		// If the shader is a geom shader, we will output the color from the geom to the fragment:
		if (shaderDescription != null)
		{
			shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("geomColor", RHI::TypeFloat4));
			shaderDescription->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragColor", RHI::TypeFloat4));
		}

		RHI::EVarType	sizeType = RHI::TypeFloat;

		if (bbType >= ParticleDebugBT_GeomBillboarding_SizeFloat2_C0 &&
			bbType <= ParticleDebugBT_GeomBillboarding_SizeFloat2_C2)
		{
			bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::GeometryShaderStage, "HAS_SizeFloat2"));
			sizeType = RHI::TypeFloat2;
		}
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::GeometryShaderStage, "BB_FeatureC0"));

		bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Size", shaderLocationBinding, sizeType, vBufferLocationBinding));
		if (shaderDescription != null)
			shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("geomSize", sizeType));
		++shaderLocationBinding;
		++vBufferLocationBinding;

		if (bbType == ParticleDebugBT_GeomBillboarding_C0 || bbType == ParticleDebugBT_GeomBillboarding_C2 ||
			bbType == ParticleDebugBT_GeomBillboarding_SizeFloat2_C0 || bbType == ParticleDebugBT_GeomBillboarding_SizeFloat2_C2)
		{
			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Rotation", shaderLocationBinding, RHI::TypeFloat, vBufferLocationBinding));
			if (shaderDescription != null)
				shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("geomRotation", RHI::TypeFloat));
			++shaderLocationBinding;
			++vBufferLocationBinding;
		}

		// If > C1
		if ((bbType >= ParticleDebugBT_GeomBillboarding_C1 && bbType <= ParticleDebugBT_GeomBillboarding_C2) ||
			(bbType >= ParticleDebugBT_GeomBillboarding_SizeFloat2_C1 && bbType <= ParticleDebugBT_GeomBillboarding_SizeFloat2_C2))
		{
			bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::GeometryShaderStage, "BB_FeatureC1"));

			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Axis0", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding));
			if (shaderDescription != null)
				shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("geomAxis0", RHI::TypeFloat3));
			++shaderLocationBinding;
			++vBufferLocationBinding;

			if (bbType == ParticleDebugBT_GeomBillboarding_C1_Capsule || bbType == ParticleDebugBT_GeomBillboarding_SizeFloat2_C1_Capsule)
			{
				bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::GeometryShaderStage, "BB_FeatureC1_Capsule"));
			}

			if (bbType == ParticleDebugBT_GeomBillboarding_SizeFloat2_C2 || bbType == ParticleDebugBT_GeomBillboarding_C2)
			{
				bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::GeometryShaderStage, "BB_FeatureC2"));

				bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Axis1", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding));
				if (shaderDescription != null)
					shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("geomAxis1", RHI::TypeFloat3));
				++shaderLocationBinding;
				++vBufferLocationBinding;
			}
		}
		if (gpuStorage)
		{
			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Enabled", shaderLocationBinding, RHI::TypeUint, vBufferLocationBinding));
			++shaderLocationBinding;
			++vBufferLocationBinding;
		}
	}

	// Common vertex inputs:
	if (shaderMode == ParticleDebugShader_Color)
	{
		if (vertexBB)
		{
			if (gpuStorage)
				streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("ColorsOffsets"));
			else
				simDataSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Colors"));
		}
		else if ((bbType == ParticleDebugBT_Mesh) && gpuStorage)
		{
			streamOffsetsSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("ColorsOffsets"));
		}
		else
		{
			bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Color", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding));
			++shaderLocationBinding;
			++vBufferLocationBinding;
		}
	}
	else if (shaderMode == ParticleDebugShader_Selection)
	{
		if (vertexBB)
		{
			selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("Selections"));
		}
		else
		{
			if ((bbType == ParticleDebugBT_Mesh) && gpuStorage)
				selectionSetLayout.AddConstantsLayout(RHI::SRawBufferDesc("IsSelected"));
			else
			{
				bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("IsSelected", shaderLocationBinding, RHI::TypeFloat, vBufferLocationBinding));
				++shaderLocationBinding;
				++vBufferLocationBinding;
			}
			// Just needed in the vertex shader passthrough for geometry shader billboarding:
			if (geomBB)
			{
				if (shaderDescription != null)
				{
					shaderDescription->m_VertexOutput.PushBack(RHI::SVertexOutput("geomIsSelected", RHI::TypeFloat));
				}
			}
		}
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::GeometryShaderStage, "BB_Selection"));
	}
	else if (shaderMode == ParticleDebugShader_Error)
	{
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "DebugShader_Error"));
	}

	if (vertexBB || ((bbType == ParticleDebugBT_Mesh) && gpuStorage))
	{
		PK_ASSERT(!streamOffsetsSetLayout.m_Constants.Empty() || !gpuStorage);
		if (!streamOffsetsSetLayout.m_Constants.Empty())
			bindings.m_ConstantSets.PushBack(streamOffsetsSetLayout);

		PK_ASSERT(!simDataSetLayout.m_Constants.Empty());
		bindings.m_ConstantSets.PushBack(simDataSetLayout);

		if (!selectionSetLayout.m_Constants.Empty())
			bindings.m_ConstantSets.PushBack(selectionSetLayout);
	}
}

//----------------------------------------------------------------------------

void	AddEditorDebugParticleDefinition(TArray<SShaderCombination> &shaders)
{
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::VsPs;

		// Fragment output
		description.m_FragmentOutput.PushBack();
		description.m_FragmentOutput.Last().m_Name = "outColor";
		description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

		shaders.PushBack();
		shaders.Last().m_VertexShader = "DebugParticle.vert";
		shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, ParticleDebugBT_Billboard, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, ParticleDebugBT_Billboard, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, ParticleDebugBT_Billboard, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, ParticleDebugBT_Billboard, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, ParticleDebugBT_Mesh, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, ParticleDebugBT_Mesh, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_VertexColor, ParticleDebugBT_Mesh, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, ParticleDebugBT_Mesh, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, ParticleDebugBT_Mesh, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, ParticleDebugBT_Light, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, ParticleDebugBT_Light, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, ParticleDebugBT_Light, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, ParticleDebugBT_Light, false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
	}

	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::VsPs;

		// Vertex output
		description.m_VertexOutput.PushBack();
		description.m_VertexOutput.Last().m_Name = "fragColor";
		description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

		// Fragment output
		description.m_FragmentOutput.PushBack();
		description.m_FragmentOutput.Last().m_Name = "outColor";
		description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

		shaders.PushBack();
		shaders.Last().m_VertexShader = "DebugMesh_GPU.vert";
		shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, ParticleDebugBT_Mesh, true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, ParticleDebugBT_Mesh, true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_VertexColor, ParticleDebugBT_Mesh, true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, ParticleDebugBT_Mesh, true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, ParticleDebugBT_Mesh, true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
	}
}

//----------------------------------------------------------------------------

void	AddEditorDebugVertexBBParticleDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Fragment output
	PK_VERIFY(description.m_FragmentOutput.PushBack().Valid());
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	PK_VERIFY(shaders.PushBack().Valid());
	shaders.Last().m_VertexShader = "DebugParticleVertexBB.vert";
	shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

	for (u32 bbType = ParticleDebugBT_VertexBillboarding_C0; bbType <= ParticleDebugBT_VertexBillboarding_SizeFloat2_C2; ++bbType)
	{
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION
	}

	PK_VERIFY(shaders.PushBack().Valid());
	shaders.Last().m_VertexShader = "DebugParticleVertexTri.vert";
	shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

	FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, EParticleDebugBillboarderType::ParticleDebugBT_TriangleVertexBillboarding, false, &description);
	PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION
	FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, EParticleDebugBillboarderType::ParticleDebugBT_TriangleVertexBillboarding, false, &description);
	PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION
	FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, EParticleDebugBillboarderType::ParticleDebugBT_TriangleVertexBillboarding, false, &description);
	PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION
	FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, EParticleDebugBillboarderType::ParticleDebugBT_TriangleVertexBillboarding, false, &description);
	PK_VERIFY(shaders.Last().m_ShaderDescriptions.PushBack(description).Valid()); // PUSH SHADER DESCRIPTION

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
	// GPU vertex shader changes, there is no DrawRequestId
	shaders.PushBack();
	shaders.Last().m_VertexShader = "DebugParticleVertexBB.vert";
	shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

	for (u32 bbType = ParticleDebugBT_VertexBillboarding_C0; bbType <= ParticleDebugBT_VertexBillboarding_SizeFloat2_C2; ++bbType)
	{
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
	}
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
}

//----------------------------------------------------------------------------

void	AddEditorDebugGeomBBParticleDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsGsPs;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	shaders.PushBack();
	shaders.Last().m_VertexShader = "DebugParticleGeom_CPU.vert";
	shaders.Last().m_GeometryShader = "DebugParticleGeom.geom";
	shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

	for (u32 bbType = ParticleDebugBT_GeomBillboarding_C0; bbType < _ParticleDebugBT_Count; ++bbType)
	{
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, static_cast<EParticleDebugBillboarderType>(bbType), false, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
	}

	// GPU vertex shader changes, there is no DrawRequestId
	shaders.PushBack();
	shaders.Last().m_VertexShader = "DebugParticleGeom_GPU.vert";
	shaders.Last().m_GeometryShader = "DebugParticleGeom.geom";
	shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

	for (u32 bbType = ParticleDebugBT_GeomBillboarding_C0; bbType < _ParticleDebugBT_Count; ++bbType)
	{
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_White, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Color, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Selection, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
		FillEditorDebugParticleShaderBindings(description.m_Bindings, ParticleDebugShader_Error, static_cast<EParticleDebugBillboarderType>(bbType), true, &description);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
	}
}

//----------------------------------------------------------------------------

void	FillEditorDebugDrawShaderBindings(RHI::SShaderBindings &bindings, bool hasVertexColor, bool isClipSpace)
{
	bindings.Reset();

	// Vertex inputs
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	if (hasVertexColor)
	{
		bindings.m_InputAttributes.PushBack();
		bindings.m_InputAttributes.Last().m_Name = "Color";
		bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
		bindings.m_InputAttributes.Last().m_BufferIdx = 1;
		bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
	}

	if (!isClipSpace)
	{
		// Scene info:
		RHI::SConstantSetLayout		layout;
		CreateSceneInfoConstantLayout(layout);
		bindings.m_ConstantSets.PushBack(layout);
	}

	if (!hasVertexColor)
	{
		// Debug color:
		RHI::SPushConstantBuffer	debugColor("DebugColor", RHI::VertexShaderMask);
		debugColor.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "Color"));
		bindings.m_PushConstants.PushBack(debugColor);
	}
}

//----------------------------------------------------------------------------

void	AddEditorDebugDrawDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;
	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	shaders.PushBack();

	shaders.Last().m_VertexShader = "DebugDraw.vert";
	shaders.Last().m_FragmentShader = "DebugDrawColor.frag";

	FillEditorDebugDrawShaderBindings(description.m_Bindings, false, false);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillEditorDebugDrawShaderBindings(description.m_Bindings, false, true);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillEditorDebugDrawShaderBindings(description.m_Bindings, true, false);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillEditorDebugDrawShaderBindings(description.m_Bindings, false, false);
	description.m_Bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("MeshTransform", 1, RHI::TypeFloat4x4, 1));
	description.m_Bindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Color", 5, RHI::TypeFloat4, 2));
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillEditorDebugDrawShaderBindings(description.m_Bindings, true, true);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillDebugDrawValueConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	// Data buffer
	layout.AddConstantsLayout(RHI::SRawBufferDesc("Data"));
}

//----------------------------------------------------------------------------

void	FillEditorDebugDrawValueShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();

	// Vertex inputs
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "TexCoords";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	// Scene info
	RHI::SConstantSetLayout		sceneConstantSetlayout;
	CreateSceneInfoConstantLayout(sceneConstantSetlayout);
	bindings.m_ConstantSets.PushBack(sceneConstantSetlayout);

	// Data buffers (instanced)
	RHI::SConstantSetLayout		layout(RHI::VertexShaderMask);
	FillDebugDrawValueConstantSetLayout(layout);
	bindings.m_ConstantSets.PushBack(layout);

	// Texture atlas texture (TODO: Glyphs SDF texture for better rendering)
	RHI::SConstantSetLayout	textureAtlasSetLayout(RHI::FragmentShaderMask);
	textureAtlasSetLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("TextAtlas", RHI::SamplerTypeSingle));
	bindings.m_ConstantSets.PushBack(textureAtlasSetLayout);

	// Text atlas
	bindings.m_ConstantSets.PushBack(SConstantAtlasKey::GetAtlasConstantSetLayout());
}

//----------------------------------------------------------------------------

void	AddEditorDebugDrawValueDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;
	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoords";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationFlat;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragFlags";
	description.m_VertexOutput.Last().m_Type = RHI::TypeUint;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationFlat;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragDrawDigitCounts";
	description.m_VertexOutput.Last().m_Type = RHI::TypeUint4;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationFlat;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragValue";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationFlat;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	shaders.PushBack();

	shaders.Last().m_VertexShader = "DebugDrawValue.vert"; // DEBUG_DRAW_VALUE_VERTEX_SHADER_PATH
	shaders.Last().m_FragmentShader = "DebugDrawValue.frag"; // DEBUG_DRAW_VALUE_FRAGMENT_SHADER_PATH

	FillEditorDebugDrawValueShaderBindings(description.m_Bindings);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillEditorDebugDrawLineShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();

	// Scene info
	RHI::SConstantSetLayout		sceneConstantSetlayout;
	CreateSceneInfoConstantLayout(sceneConstantSetlayout);
	bindings.m_ConstantSets.PushBack(sceneConstantSetlayout);

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.Last().m_BufferIdx = 0;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
	bindings.m_InputAttributes.Last().m_StartOffset = 4 * sizeof(float);
	bindings.m_InputAttributes.Last().m_BufferIdx = 1;
}

//----------------------------------------------------------------------------

void	AddEditorDebugDrawLineDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;
	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationSmooth;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA; // TODO: What about the alpha?

	shaders.PushBack();
	shaders.Last().m_VertexShader = "DebugDrawLine.vert"; // DEBUG_DRAW_LINE_VERTEX_SHADER_PATH
	shaders.Last().m_FragmentShader = "DebugDrawLine.frag"; // DEBUG_DRAW_LINE_FRAGMENT_SHADER_PATH

	FillEditorDebugDrawLineShaderBindings(description.m_Bindings);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillEditorHeatmapOverdrawBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	// Overdraw Info
	RHI::SPushConstantBuffer	overdrawInfo("OverdrawInfo", RHI::FragmentShaderMask);
	overdrawInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "ScaleFactor"));
	bindings.m_PushConstants.PushBack(overdrawInfo);
	bindings.m_ConstantSets.PushBack(layout);
}

void	AddEditorHeatmapOverdrawDefinition(TArray<SShaderCombination> &shaders)
{

	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;
	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	RHI::SConstantSetLayout		setLayout(RHI::FragmentShaderMask);

	setLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("IntensityMap", RHI::SamplerTypeSingle));
	setLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("OverdrawLUT", RHI::SamplerTypeSingle));

	FillEditorHeatmapOverdrawBindings(description.m_Bindings, setLayout);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "Heatmap.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------

void	CreateBrushBackdropInfoConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	// Scene info:
	RHI::SConstantBufferDesc	bufferLayout("Info");

	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "Top"));
	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "Bottom"));
	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "Origin"));
	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "UserToRHY"));
	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "InvMat"));
	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "EnvironmentMapColor"));
	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "EnvironmentMapMipLevel"));
	bufferLayout.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "EnvironmentMapVisible"));
	layout.AddConstantsLayout(bufferLayout);
}

//----------------------------------------------------------------------

void	FillBrushBackdropShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &brushInfo, const RHI::SConstantSetLayout &environmentMapLayout)
{
	bindings.Reset();

	// Vertex inputs
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(brushInfo);
	bindings.m_ConstantSets.PushBack(environmentMapLayout);
}

//----------------------------------------------------------------------

void	AddBrushBackdropDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;
	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	// Create environment map sampler layout
	RHI::SConstantSetLayout environmentMapConstSetLayout;
	CreateCubemapSamplerConstantSetLayout(environmentMapConstSetLayout);

	RHI::SConstantSetLayout brushInfoConstSetLayout;
	CreateBrushBackdropInfoConstantSetLayout(brushInfoConstSetLayout);

	FillBrushBackdropShaderBindings(description.m_Bindings, brushInfoConstSetLayout, environmentMapConstSetLayout);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "BrushBackdrop.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillParticleLightShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();

	// Vertex inputs
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.Last().m_BufferIdx = 0;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "LightPosition";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.Last().m_BufferIdx = 1;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "LightRadius";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 2;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat;
	bindings.m_InputAttributes.Last().m_BufferIdx = 2;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "LightColor";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 3;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.Last().m_BufferIdx = 3;

	// Samplers layout:
	RHI::SConstantSetLayout		samplersLayout;
	CreateDeferredSamplerLightingConstSetLayout(samplersLayout);
	bindings.m_ConstantSets.PushBack(samplersLayout);

	// Scene info:
	RHI::SConstantSetLayout		sceneInfoLayout;
	CreateSceneInfoConstantLayout(sceneInfoLayout);
	bindings.m_ConstantSets.PushBack(sceneInfoLayout);
}

//----------------------------------------------------------------------------

void	AddParticleLightDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragWorldPosition";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragLightPosition";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat3;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationFlat;

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragLightRadius";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationFlat;

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragLightColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat3;
	description.m_VertexOutput.Last().m_Interpolation = RHI::InterpolationFlat;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	FillParticleLightShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "LightInstanced.vert";
	shaders.Last().m_FragmentShader = "PointLightInstanced.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}


//----------------------------------------------------------------------------

void	CreateEditorSelectorConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;

	RHI::SConstantBufferDesc	constantBufferDesc("SelectionInfo");
	constantBufferDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint4, "PositionsOffset"));
	constantBufferDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint4, "RadiusOffset"));
	constantBufferDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint4, "EnabledOffset"));
	constantBufferDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "RayOrigin"));
	constantBufferDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "RayDirection"));
	constantBufferDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Planes"));
	constantBufferDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Mode"));

	layout.AddConstantsLayout(constantBufferDesc);
	layout.AddConstantsLayout(RHI::SRawBufferDesc("Stream", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("StreamInfo", true));
	layout.AddConstantsLayout(RHI::SRawBufferDesc("Selection", false));
}

//----------------------------------------------------------------------------

void	AddEditorSelectorDefinition(TArray<SShaderCombination> &shaders)
{
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		RHI::SConstantSetLayout	layout;
		CreateEditorSelectorConstantSetLayout(layout);
		description.m_Bindings.m_ConstantSets.PushBack(layout);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ParticleSelector.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}

	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(PK_RH_GPU_THREADGROUP_SIZE, 1, 1);

		RHI::SConstantSetLayout	layout;
		CreateEditorSelectorConstantSetLayout(layout);
		description.m_Bindings.m_ConstantSets.PushBack(layout);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "ParticleSelectorCycle.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
