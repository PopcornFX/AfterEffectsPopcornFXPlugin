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
#include "FeatureRenderingSettings.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

HBO_CLASS_DEFINITION_BEGIN(CRHIRenderingSettings)
	.HBO_FIELD_DEFINITION(RenderingFeatures)
HBO_CLASS_DEFINITION_END

//----------------------------------------------------------------------------

CRHIRenderingSettings::CRHIRenderingSettings()
:	HBO_CONSTRUCT(CRHIRenderingSettings)
{
}

//----------------------------------------------------------------------------

CRHIRenderingSettings::~CRHIRenderingSettings()
{
}

//----------------------------------------------------------------------------

const char	*CRHIRenderingSettings::DisplayName() const
{
	return "Rendering Features";
}

//----------------------------------------------------------------------------

CRHIRenderingFeature	*CRHIRenderingSettings::FindFeature(const CString &name)
{
	for (u32 i = 0; i < RenderingFeatures().Count(); ++i)
	{
		if (RenderingFeatures()[i] != null && RenderingFeatures()[i]->FeatureName() == name)
			return RenderingFeatures()[i];
	}
	return null;
}

//----------------------------------------------------------------------------

PCRHIRenderingFeature	CRHIRenderingSettings::FindFeature(const CString &name) const
{
	for (u32 i = 0; i < RenderingFeatures().Count(); ++i)
	{
		if (RenderingFeatures()[i] != null && RenderingFeatures()[i]->FeatureName() == name)
			return RenderingFeatures()[i].Get();
	}
	return null;
}

//----------------------------------------------------------------------------
//
//	CRHIRenderingFeature
//
//----------------------------------------------------------------------------

HBO_CLASS_DEFINITION_BEGIN(CRHIRenderingFeature)
.Category("Internal")
	.HBO_FIELD_DEFINITION(FeatureName)
	[
		HBO::Properties::Description("Name of the renderer feature target")
	]
.Category("General")
	.HBO_FIELD_DEFINITION(UseUV)
	[
		HBO::Properties::Description("UVs are needed for this feature"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(UseNormal)
	[
		HBO::Properties::Description("Normals are needed for this feature"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(UseTangent)
	[
		HBO::Properties::Description("Tangents are needed for this feature"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(UseMeshUV1)
	[
		HBO::Properties::Description("Secondary UVs are needed for this feature (only for mesh renderer)"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(UseMeshVertexColor0)
	[
		HBO::Properties::Description("Primary colors are needed for this feature (only for mesh renderer)"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(UseMeshVertexColor1)
	[
		HBO::Properties::Description("Secondary colors are needed for this feature (only for mesh renderer)"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(UseMeshVertexBonesIndicesAndWeights)
	[
		HBO::Properties::Description("Bones indices and weights are accessible in the vertex shader (only for mesh renderer)"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(SampleDepth)
	[
		HBO::Properties::Description("Need depth texture for shading"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(SampleNormalRoughMetal)
	[
		HBO::Properties::Description("Need the packed normal / roughness / metalness texture for shading"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(SampleDiffuse)
	[
		HBO::Properties::Description("Need normal texture for shading"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(UseSceneLightingInfo)
	[
		HBO::Properties::Description("Need scene lights info"),
		HBO::Properties::DefaultValue(false)
	]
	.HBO_FIELD_DEFINITION(PropertiesAsShaderConstants)
	[
		HBO::Properties::Description("Properties needed in shader")
	]
	.HBO_FIELD_DEFINITION(TexturesUsedAsLookUp)
	[
		HBO::Properties::Description("Textures that do not need the sRGB gamma correction when sampled.")
	]
HBO_CLASS_DEFINITION_END

//----------------------------------------------------------------------------

CRHIRenderingFeature::CRHIRenderingFeature()
:	HBO_CONSTRUCT(CRHIRenderingFeature)
{
}

//----------------------------------------------------------------------------

CRHIRenderingFeature::~CRHIRenderingFeature()
{
}

//----------------------------------------------------------------------------
//
//	CRHIMaterialShaders
//
//----------------------------------------------------------------------------

HBO_CLASS_DEFINITION_BEGIN(CRHIMaterialShaders)
	// NOTE: If you rename these paths, or add other paths, make sure to update 'CCoreUpgrader::_UpgradeCoreFileBufferInPlace()'
	.HBO_FIELD_DEFINITION(VertexShader)
	[
		HBO::Properties::Caracs(HBO::Caracs_Path),
		HBO::Properties::Description("Vertex shader path")
	]
	.HBO_FIELD_DEFINITION(FragmentShader)
	[
		HBO::Properties::Caracs(HBO::Caracs_Path),
		HBO::Properties::Description("Fragment shader path")
	]
HBO_CLASS_DEFINITION_END

//----------------------------------------------------------------------------

CRHIMaterialShaders::CRHIMaterialShaders()
:	HBO_CONSTRUCT(CRHIMaterialShaders)
{
}

//----------------------------------------------------------------------------

CRHIMaterialShaders::~CRHIMaterialShaders()
{
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

