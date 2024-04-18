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

#include "UnitTestsShaderDefinitions.h"
#include "SampleLibShaderDefinitions.h"
#include "ShaderDefinitions.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------
//
//	Create the constant set layouts:
//
//----------------------------------------------------------------------------

void	CreateSimpleSamplerConstSetLayouts(RHI::SConstantSetLayout &layout, bool multiSampled)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;

	// sampler2d Texture:
	RHI::SConstantSamplerDesc	colorSamplerLayout("Texture", multiSampled ? RHI::SamplerTypeMulti : RHI::SamplerTypeSingle);
	layout.AddConstantsLayout(colorSamplerLayout);
}

//----------------------------------------------------------------------------

void	CreateMultiSamplersConstSetLayouts(RHI::SConstantSetLayout &layout, u32 textureCount)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;

	for (u32 i = 0; i < textureCount; ++i)
	{
		// sampler2d Texturei
		CString						samplerName = CString::Format("Texture%u", i);
		RHI::SConstantSamplerDesc	colorSamplerLayout(samplerName, RHI::SamplerTypeSingle);
		layout.AddConstantsLayout(colorSamplerLayout);
	}
}

//----------------------------------------------------------------------------

void	CreatePerViewConstSetLayouts(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::AllShaderMask;

	// buffer ViewData:
	RHI::SConstantBufferDesc	viewData("ViewData");
	viewData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Model"));
	viewData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "View"));
	viewData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "Proj"));
	viewData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "MatNormal"));
	layout.AddConstantsLayout(viewData);
}

//----------------------------------------------------------------------------

void	CreatePerFrameConstSetLayouts(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;

	// buffer LightData:
	RHI::SConstantBufferDesc	lightData("LightData");
	lightData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "LightCount"));
	lightData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "LightDirections", 256));
	lightData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "LightColors", 256));
	layout.AddConstantsLayout(lightData);
}

//----------------------------------------------------------------------------

void	CreateProcessTextureConstSetLayouts(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::FragmentShaderMask;

	// sampler2d ToBlur:
	RHI::SConstantSamplerDesc	toBlurSamplerLayout("Texture", RHI::SamplerTypeSingle);
	layout.AddConstantsLayout(toBlurSamplerLayout);
	// sampler2d Distortion:
	RHI::SConstantSamplerDesc	distortionSamplerLayout("LookUp", RHI::SamplerTypeSingle);
	layout.AddConstantsLayout(distortionSamplerLayout);
}

//----------------------------------------------------------------------------

void	FillPositionInputSingleConstSetShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddTriangleNoVboDefinition(TArray<SShaderCombination> &shaders)
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
	shaders.Last().m_VertexShader = "TriangleNoVBO.vert";
	shaders.Last().m_FragmentShader = "Basic.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillPosColVBOShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;
}

//----------------------------------------------------------------------------

void	AddPosColVBODefinition(TArray<SShaderCombination> &shaders)
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

	FillPosColVBOShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "PosColVBO.vert";
	shaders.Last().m_FragmentShader = "Basic.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillUniformTestShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &perView, const RHI::SConstantSetLayout &perFrame)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Normal";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_ConstantSets.PushBack(perView);
	bindings.m_ConstantSets.PushBack(perFrame);
}

//----------------------------------------------------------------------------

void	AddUniformTestDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragWorldNormal";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat3;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	description.m_Pipeline = RHI::VsPs;

	shaders.PushBack();
	shaders.Last().m_VertexShader = "ConstMat4.vert";
	shaders.Last().m_FragmentShader = "TestArray.frag";

	RHI::SConstantSetLayout	perView;
	RHI::SConstantSetLayout	perFrame;

	CreatePerViewConstSetLayouts(perView);
	CreatePerFrameConstSetLayouts(perFrame);

	FillUniformTestShaderBindings(description.m_Bindings, perView, perFrame);

	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------

void	FillConstMat4ShaderBindings(RHI::SShaderBindings &bindings,
									const RHI::SConstantSetLayout &perView,
									const RHI::SConstantSetLayout *perFrame,
									const RHI::SConstantSetLayout *simpleSampler)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;

	bindings.m_ConstantSets.PushBack(perView);
	if (perFrame != null)
		bindings.m_ConstantSets.PushBack(*perFrame);
	if (simpleSampler != null)
	{
		bindings.m_InputAttributes.PushBack();
		bindings.m_InputAttributes.Last().m_Name = "TexCoord";
		bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 2;
		bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;
		bindings.m_ConstantSets.PushBack(*simpleSampler);
	}
}

//----------------------------------------------------------------------------

void	AddConstMat4Definition(TArray<SShaderCombination> &shaders)
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
	shaders.Last().m_VertexShader = "ConstMat4.vert";
	shaders.Last().m_FragmentShader = "Basic.frag";

	RHI::SConstantSetLayout	perView;
	RHI::SConstantSetLayout	perFrame;
	RHI::SConstantSetLayout	simpleSampler;

	CreatePerViewConstSetLayouts(perView);
	CreatePerFrameConstSetLayouts(perFrame);
	CreateSimpleSamplerConstSetLayouts(simpleSampler, false);

	FillConstMat4ShaderBindings(description.m_Bindings, perView);

	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillConstMat4ShaderBindings(description.m_Bindings, perView, &perFrame);

	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	FillConstMat4ShaderBindings(description.m_Bindings, perView, null, &simpleSampler);

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------
//
//	Cubemap
//
//----------------------------------------------------------------------------

void	FillCubemapRenderShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &perView, const RHI::SConstantSetLayout &simpleSampler)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(perView);
	bindings.m_ConstantSets.PushBack(simpleSampler);

}

//----------------------------------------------------------------------------

void	CreateCubemapRenderConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::AllShaderMask;
	RHI::SConstantBufferDesc	viewData("ViewData");
	viewData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "InvViewProj"));
	viewData.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4x4, "SceneInfo"));
	layout.AddConstantsLayout(viewData);
}

//----------------------------------------------------------------------------

void	AddCubemapRenderDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription	description;
	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	// Regular copy
	shaders.PushBack();
	shaders.Last().m_VertexShader = "CubemapFullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "Cubemap.frag";

	RHI::SConstantSetLayout	perView;
	CreateCubemapRenderConstantSetLayout(perView);

	RHI::SConstantSetLayout	simpleSampler;
	simpleSampler.Reset();
	simpleSampler.m_ShaderStagesMask = RHI::FragmentShaderMask;
	RHI::SConstantSamplerDesc	colorSamplerLayout("Texture", RHI::SamplerTypeCube);
	simpleSampler.AddConstantsLayout(colorSamplerLayout);

	FillCubemapRenderShaderBindings(description.m_Bindings, perView, simpleSampler);

	shaders.Last().m_ShaderDescriptions.PushBack(description);
}


//----------------------------------------------------------------------------

void	CreateCubemapComputeConstantSetLayout(RHI::SConstantSetLayout &layout, bool inputTxtIsLatLong)
{
	layout.Reset();
	layout.m_ShaderStagesMask = RHI::ComputeShaderMask;

	RHI::SConstantBufferDesc	buffer("CubemapData");
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Face"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Level"));
	buffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "Size"));

	layout.AddConstantsLayout(buffer);
	if (!inputTxtIsLatLong)
		layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputTextureCube", RHI::SamplerTypeCube));
	else
		layout.AddConstantsLayout(RHI::SConstantSamplerDesc("InputTextureLatlong", RHI::SamplerTypeSingle));

	layout.AddConstantsLayout(RHI::STextureStorageDesc("OutData", false));
}

//----------------------------------------------------------------------------

void	FillCubemapComputeShaderBindings(RHI::SShaderBindings &bindings, bool inputTxtIsLatLong)
{
	bindings.Reset();
	RHI::SConstantSetLayout	layout;
	CreateCubemapComputeConstantSetLayout(layout, inputTxtIsLatLong);
	bindings.m_ConstantSets.PushBack(layout);
	bindings.m_Defines.PushBack();
	bindings.m_Defines.Last().m_ShaderStages = RHI::ComputeShaderMask;
	if (inputTxtIsLatLong)
		bindings.m_Defines.Last().m_Define = "INPUTLATLONG";
	else
		bindings.m_Defines.Last().m_Define = "INPUTCUBE";
}

//----------------------------------------------------------------------------

void	AddCubemapComputeDefinition(TArray<SShaderCombination> &shaders)
{
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(32, 32, 1);

		FillCubemapComputeShaderBindings(description.m_Bindings, true);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "UnitTestGenerateCubemap.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
	{
		RHI::SShaderDescription		description;

		description.m_Pipeline = RHI::Cs;
		description.m_DrawMode = RHI::DrawModeInvalid;
		description.m_DispatchThreadSize = CUint3(32, 32, 1);

		FillCubemapComputeShaderBindings(description.m_Bindings, false);

		shaders.PushBack();
		shaders.Last().m_ComputeShader = "UnitTestGenerateCubemap.comp";
		shaders.Last().m_ShaderDescriptions.PushBack(description);
	}
}

//----------------------------------------------------------------------------

void	CreateUnitTestStructLayoutConstantSetLayout(RHI::SConstantSetLayout &layout)
{
	layout.Reset();

	RHI::SConstantBufferDesc	testAlignmentBuffer("AlignmentBuffer");

	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat , "value1"));		// offset:   0, size:  4 (+padding: 12)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat3, "value3"));		// offset:  16, size: 12 (+padding:  0)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat , "padding0"));	// offset:  28, size:  4 (+padding:  0)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat , "padding1"));	// offset:  32, size:  4 (+padding:  8)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "value2"));		// offset:  40, size:  8 (+padding:  0)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "value4"));		// offset:  48, size: 16 (+padding:  0)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat , "array1", 3));	// offset:  96, size: 48 (+padding:  0)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "array2", 3));	// offset: 144, size: 48 (+padding:  0)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat3, "array3", 3));	// offset: 192, size: 48 (+padding:  0)
	testAlignmentBuffer.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "array4", 3));	// offset: 240, size: 48 (+padding:  0)

	layout.AddConstantsLayout(testAlignmentBuffer);
}

//----------------------------------------------------------------------------

void	AddUnitTestStructLayoutDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outValue";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	description.m_Pipeline = RHI::VsPs;

	RHI::SConstantSetLayout	layout;
	CreateUnitTestStructLayoutConstantSetLayout(layout);
	description.m_Bindings.m_ConstantSets.PushBack(layout);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "UnitTestStructLayout.vert";
	shaders.Last().m_FragmentShader = "UnitTestStructLayout.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}


//----------------------------------------------------------------------------
//
//	Gaussian Blur
//
//----------------------------------------------------------------------------

void	FillGaussianBlurShaderBindings(	EGaussianBlurCombination		combination,
										RHI::SShaderBindings			&bindings,
										const RHI::SConstantSetLayout	&layoutSampler)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(layoutSampler);

	// Blur Info
	RHI::SPushConstantBuffer	blurInfo("BlurInfo", RHI::FragmentShaderMask);

	blurInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "ContextSize"));
	blurInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "Direction"));
	blurInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "SubUV"));

	bindings.m_PushConstants.PushBack(blurInfo);

	bindings.m_Defines.PushBack();
	bindings.m_Defines.Last().m_ShaderStages = RHI::FragmentShaderMask;
	if (combination == GaussianBlurCombination_5_Tap)
		bindings.m_Defines.Last().m_Define = "BLUR_5_TAP";
	else if (combination == GaussianBlurCombination_9_Tap)
		bindings.m_Defines.Last().m_Define = "BLUR_9_TAP";
	else if (combination == GaussianBlurCombination_13_Tap)
		bindings.m_Defines.Last().m_Define = "BLUR_13_TAP";
}

//----------------------------------------------------------------------------

void	AddGaussianBlurDefinition(TArray<SShaderCombination> &shaders)
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

	RHI::SConstantSetLayout		layoutSamplerNoDisto;
	RHI::SConstantSetLayout		layoutSamplerDisto;

	CreateSimpleSamplerConstSetLayouts(layoutSamplerNoDisto, false);
	CreateProcessTextureConstSetLayouts(layoutSamplerDisto);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "GaussianBlur.frag";

	for (u32 combination = 0; combination < GaussianBlurCombination_Count; ++combination)
	{
		FillGaussianBlurShaderBindings(	static_cast<EGaussianBlurCombination>(combination),
										description.m_Bindings,
										layoutSamplerNoDisto);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

		FillGaussianBlurShaderBindings(static_cast<EGaussianBlurCombination>(combination),
										description.m_Bindings,
										layoutSamplerDisto);
		shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
	}
}

//----------------------------------------------------------------------------
//
//	Copy Shader
//
//----------------------------------------------------------------------------

void	FillCopyShaderBindings(ECopyCombination combination, RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(layout);

	if (combination == CopyCombination_Basic)
	{
		return;
	}
	else if (combination == CopyCombination_FlippedBasic)
	{
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "COPY_FLIPPED"));
	}
	else if (combination == CopyCombination_Alpha)
	{
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "COPY_ALPHA"));
	}
	else if (combination == CopyCombination_Normal)
	{
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "COPY_NORMAL"));
	}
	else if (combination == CopyCombination_UnpackedNormal)
	{
		RHI::SConstantSetLayout		sceneInfo;
		CreateSceneInfoConstantLayout(sceneInfo);
		bindings.m_ConstantSets.PushBack(sceneInfo);
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "COPY_NORMAL_UNPACKED"));
	}
	else if (combination == CopyCombination_Specular)
	{
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "COPY_SPEC"));
	}
	else if (combination == CopyCombination_MulAdd)
	{
		RHI::SPushConstantBuffer	mulAddInfo("MulAddInfo", RHI::FragmentShaderMask);

		mulAddInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "MulValue"));
		mulAddInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "AddValue"));

		bindings.m_PushConstants.PushBack(mulAddInfo);
	}
	else if (combination == CopyCombination_ToneMapping)
	{
		RHI::SConstantSetLayout		blueNoise;
		blueNoise.m_ShaderStagesMask = RHI::FragmentShaderMask;
		blueNoise.AddConstantsLayout(RHI::SConstantSamplerDesc("BlueNoise", RHI::SamplerTypeSingle));
		bindings.m_ConstantSets.PushBack(blueNoise);

		RHI::SPushConstantBuffer	highPassInfo("ToneMappingInfo", RHI::FragmentShaderMask);

		highPassInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "Common"));
		highPassInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "ExpGammaSat"));
		highPassInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "Vignetting"));
		highPassInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "VignetteColor"));

		bindings.m_PushConstants.PushBack(highPassInfo);
	}
	else if (combination == CopyCombination_Depth)
	{
		RHI::SPushConstantBuffer	depthInfo("DepthInfo", RHI::FragmentShaderMask);

		depthInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "DepthNear"));
		depthInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "DepthFar"));

		bindings.m_PushConstants.PushBack(depthInfo);
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "COPY_DEPTH"));
	}
	else if (combination == CopyCombination_ComputeLuma)
	{
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "COPY_COMPUTE_LUMA"));
	}
	else
	{
		PK_ASSERT_NOT_REACHED();
	}
}

//----------------------------------------------------------------------------

void	AddCopyDefinition(TArray<SShaderCombination> &shaders)
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

	// Copy with VBO
	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "Copy.frag";

	RHI::SConstantSetLayout		layout;

	CreateSimpleSamplerConstSetLayouts(layout, false);

	// Basic copy
	FillCopyShaderBindings(CopyCombination_Basic, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Flipped copy
	FillCopyShaderBindings(CopyCombination_FlippedBasic, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Alpha copy
	FillCopyShaderBindings(CopyCombination_Alpha, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Copy subtract
	FillCopyShaderBindings(CopyCombination_MulAdd, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Bright-pass filter
	FillCopyShaderBindings(CopyCombination_ToneMapping, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Depth copy
	FillCopyShaderBindings(CopyCombination_Depth, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Normal copy
	FillCopyShaderBindings(CopyCombination_Normal, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Unpacked normal copy
	FillCopyShaderBindings(CopyCombination_UnpackedNormal, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Specular & Glossiness copy
	FillCopyShaderBindings(CopyCombination_Specular, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION

	// Specular & Glossiness copy
	FillCopyShaderBindings(CopyCombination_ComputeLuma, description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description); // PUSH SHADER DESCRIPTION
}

//----------------------------------------------------------------------------
//
//	Color Remap Shader
//
//----------------------------------------------------------------------------

void	FillColorRemapShaderBindings(	RHI::SShaderBindings &bindings,
										RHI::SConstantSetLayout &layout)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	RHI::SPushConstantBuffer	colorRemapInfo("ColorRemapInfo", RHI::FragmentShaderMask);
	colorRemapInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "LutHeight"));
	bindings.m_PushConstants.PushBack(colorRemapInfo);
	bindings.m_ConstantSets.PushBack(layout);
}

void	AddColorRemapDefinition(TArray<SShaderCombination> &shaders)
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

	// Copy with VBO
	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "ColorRemap.frag";

	RHI::SConstantSetLayout		layout;
	CreateProcessTextureConstSetLayouts(layout);

	FillColorRemapShaderBindings(description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	Distortion Shader
//
//----------------------------------------------------------------------------

void	FillDistortionShaderBindings(RHI::SShaderBindings &bindings, RHI::SConstantSetLayout &layout)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(layout);

	// DistortionInfo
	RHI::SPushConstantBuffer	distortionInfo("DistortionInfo", RHI::FragmentShaderMask);
	distortionInfo.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat4, "DistortionMultipliers"));
	bindings.m_PushConstants.PushBack(distortionInfo);
}

//----------------------------------------------------------------------------

void	AddDistortionDefinition(TArray<SShaderCombination> &shaders)
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

	// Copy with VBO
	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "Distortion.frag";

	RHI::SConstantSetLayout		layout;

	CreateProcessTextureConstSetLayouts(layout);

	FillDistortionShaderBindings(description.m_Bindings, layout);
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	Texture Format
//
//----------------------------------------------------------------------------

void	FillTextureFormatShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(layout);
}

//----------------------------------------------------------------------------

void	AddTextureFormatDefinition(TArray<SShaderCombination> &shaders)
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

	RHI::SConstantSetLayout		layout;

	CreateMultiSamplersConstSetLayouts(layout);

	FillTextureFormatShaderBindings(description.m_Bindings, layout);

	// Regular copy
	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "GridTexture.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	Mipmapping
//
//----------------------------------------------------------------------------

void	FillMipmappingShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &perView, const RHI::SConstantSetLayout &simpleSampler)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "TexCoord";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 2;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(perView);
	bindings.m_ConstantSets.PushBack(simpleSampler);

	RHI::SPushConstantBuffer	constant("Mipmap", RHI::FragmentShaderMask);

	constant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "Level"));

	bindings.m_PushConstants.PushBack(constant);
}

//----------------------------------------------------------------------------

void	AddMipmappingDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;
	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragColor";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;

	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	RHI::SConstantSetLayout		perView;
	RHI::SConstantSetLayout		simpleSampler;

	CreatePerViewConstSetLayouts(perView);
	CreateSimpleSamplerConstSetLayouts(simpleSampler, false);

	FillMipmappingShaderBindings(description.m_Bindings, perView, simpleSampler);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "ConstMat4.vert";
	shaders.Last().m_FragmentShader = "SamplingMipmap.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	FillMipmappingModeShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &layout)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "TexCoord";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(layout);

	RHI::SPushConstantBuffer	constant("Mipmap", RHI::FragmentShaderMask);

	constant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "Level"));

	bindings.m_PushConstants.PushBack(constant);
}

//----------------------------------------------------------------------------

void	AddMipmappingModeDefinition(TArray<SShaderCombination> &shaders)
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

	RHI::SConstantSetLayout		simpleSampler;

	CreateSimpleSamplerConstSetLayouts(simpleSampler, false);

	FillMipmappingModeShaderBindings(description.m_Bindings, simpleSampler);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "SamplingMipmapMode.vert";
	shaders.Last().m_FragmentShader = "SamplingMipmapMode.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	HDR Texture
//
//----------------------------------------------------------------------------

void	FillHDRTextureShaderBindings(bool gammaCorrect, RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &simpleSampler)
{
	bindings.Reset();

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat2;

	bindings.m_ConstantSets.PushBack(simpleSampler);

	if (gammaCorrect)
	{
		bindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::FragmentShaderStage, "GAMMA_CORRECTION"));
	}
}

//----------------------------------------------------------------------------

void	AddHDRTextureDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;

	description.m_Pipeline = RHI::VsPs;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragTexCoord";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat2;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	RHI::SConstantSetLayout		simpleSampler;

	CreateSimpleSamplerConstSetLayouts(simpleSampler, false);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "FullScreenQuad.vert";
	shaders.Last().m_FragmentShader = "Tonemapping.frag";

	FillHDRTextureShaderBindings(false, description.m_Bindings, simpleSampler);
	shaders.Last().m_ShaderDescriptions.PushBack(description);
	FillHDRTextureShaderBindings(true, description.m_Bindings, simpleSampler);
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	MSAA
//
//----------------------------------------------------------------------------

void	FillRTMSAAShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &perView)
{
	bindings.Reset();

	// Vertex inputs
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Normal";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;

	bindings.m_ConstantSets.PushBack(perView);
}

//----------------------------------------------------------------------------

void	AddRTMSAADefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;
	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "fragWorldNormal";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat3;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	RHI::SConstantSetLayout		perView;

	CreatePerViewConstSetLayouts(perView);

	FillRTMSAAShaderBindings(description.m_Bindings, perView);

	shaders.PushBack();
	shaders.Last().m_VertexShader = "ConstMat4.vert";
	shaders.Last().m_FragmentShader = "StaticLighting.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	Draw Instanced
//
//----------------------------------------------------------------------------

void	FillDrawInstancedShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "LocalPosition";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "WorldPosition";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "ColorInstance";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 2;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat;

	RHI::SPushConstantBuffer	pushConstant("ImageInfo", RHI::VertexShaderMask);
	pushConstant.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat2, "Size"));
	bindings.m_PushConstants.PushBack(pushConstant);
}

//----------------------------------------------------------------------------

void	AddDrawInstancedDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;
	description.m_Pipeline = RHI::VsPs;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "ColorInstance";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	FillDrawInstancedShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_VertexShader	= "BasicDrawInstanced.vert";
	shaders.Last().m_FragmentShader	= "BasicDrawInstanced.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	Geometry Shaders
//
//----------------------------------------------------------------------------

void	FillGeometryShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat;
}

//----------------------------------------------------------------------------
//
//	Geometry Definition
//
//----------------------------------------------------------------------------

void	AddGeometryDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;
	description.m_Pipeline = RHI::EShaderStagePipeline::VsGsPs;
	description.m_DrawMode = RHI::DrawModePoint;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "Color";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat;

	// Geometry output
	description.m_GeometryOutput.m_PrimitiveType = RHI::DrawModeLine;
	description.m_GeometryOutput.m_MaxVertices = 16;
	description.m_GeometryOutput.m_GeometryOutput.PushBack();
	description.m_GeometryOutput.m_GeometryOutput.Last().m_Name = "ColorGeom";
	description.m_GeometryOutput.m_GeometryOutput.Last().m_Type = RHI::TypeFloat;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	FillGeometryShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_VertexShader	= "BasicGeometry.vert";
	shaders.Last().m_GeometryShader	= "BasicGeometry.geom";
	shaders.Last().m_FragmentShader	= "BasicGeometry.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------

void	FillGeometry2ShaderBindings(RHI::SShaderBindings &bindings)
{
	bindings.Reset();
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Position";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 0;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat3;
	bindings.m_InputAttributes.PushBack();
	bindings.m_InputAttributes.Last().m_Name = "Color";
	bindings.m_InputAttributes.Last().m_ShaderLocationBinding = 1;
	bindings.m_InputAttributes.Last().m_Type = RHI::TypeFloat4;

	RHI::SPushConstantBuffer	vertexDesc("NumberOfInstanceConstBuf", RHI::VertexShaderMask);
	vertexDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeUint, "NumberOfInstance"));
	bindings.m_PushConstants.PushBack(vertexDesc);

	RHI::SPushConstantBuffer	geometryDesc("RadiusConstBuf", RHI::GeometryShaderMask);
	geometryDesc.AddConstant(RHI::SConstantVarDesc(RHI::TypeFloat, "Radius"));
	bindings.m_PushConstants.PushBack(geometryDesc);
}

//----------------------------------------------------------------------------
//
//	Geometry Definition
//
//----------------------------------------------------------------------------

void	AddGeometry2Definition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription		description;
	description.m_Pipeline = RHI::EShaderStagePipeline::VsGsPs;
	description.m_DrawMode = RHI::DrawModePoint;

	// Vertex output
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "Color";
	description.m_VertexOutput.Last().m_Type = RHI::TypeFloat4;
	description.m_VertexOutput.Last().m_InputRelated = 1;
	description.m_VertexOutput.PushBack();
	description.m_VertexOutput.Last().m_Name = "NumberOfPoints";
	description.m_VertexOutput.Last().m_Type = RHI::TypeUint;

	// Geometry output
	description.m_GeometryOutput.m_PrimitiveType = RHI::DrawModeLine;
	description.m_GeometryOutput.m_MaxVertices = 64;
	description.m_GeometryOutput.m_GeometryOutput.PushBack();
	description.m_GeometryOutput.m_GeometryOutput.Last().m_Name = "ColorGeom";
	description.m_GeometryOutput.m_GeometryOutput.Last().m_Type = RHI::TypeFloat4;

	// Fragment output
	description.m_FragmentOutput.PushBack();
	description.m_FragmentOutput.Last().m_Name = "outColor";
	description.m_FragmentOutput.Last().m_Type = RHI::FlagRGBA;

	FillGeometry2ShaderBindings(description.m_Bindings);

	shaders.PushBack();
	shaders.Last().m_VertexShader	= "BasicGeometry2.vert";
	shaders.Last().m_GeometryShader	= "BasicGeometry2.geom";
	shaders.Last().m_FragmentShader	= "BasicGeometry2.frag";
	shaders.Last().m_ShaderDescriptions.PushBack(description);
}

//----------------------------------------------------------------------------
//
//	Compute Shader Unit Test
//
//----------------------------------------------------------------------------

void	AddComputeUnitTestBufferDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription	description;
	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(32, 32, 1);

	description.m_Bindings.m_ConstantSets.PushBack();

	RHI::SConstantSetLayout	&set = description.m_Bindings.m_ConstantSets.Last();
	set.m_ShaderStagesMask = RHI::ComputeShaderMask;
	set.AddConstantsLayout(RHI::SRawBufferDesc("OutData", false));

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "GenerateGeometry.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);

}

//----------------------------------------------------------------------------

void	AddComputeUnitTestTextureDefinition(TArray<SShaderCombination> &shaders)
{
	RHI::SShaderDescription	description;
	description.m_Pipeline = RHI::Cs;
	description.m_DrawMode = RHI::DrawModeInvalid;
	description.m_DispatchThreadSize = CUint3(32, 32, 1);

	description.m_Bindings.m_ConstantSets.PushBack();

	RHI::SConstantSetLayout	&set = description.m_Bindings.m_ConstantSets.Last();
	set.m_ShaderStagesMask = RHI::ComputeShaderMask;
	set.AddConstantsLayout(RHI::STextureStorageDesc("OutData", false));

	shaders.PushBack();
	shaders.Last().m_ComputeShader = "GenerateTexture.comp";
	shaders.Last().m_ShaderDescriptions.PushBack(description);

}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
