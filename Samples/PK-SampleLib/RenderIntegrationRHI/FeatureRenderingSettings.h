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

#include "PK-SampleLib/PKSample.h"
#include <pk_base_object/include/hbo_helpers.h>
#include <pk_particles/include/ps_project_settings.h>
#include <pk_particles/include/Renderers/ps_renderer_feature_properties.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

PK_FORWARD_DECLARE(RHIRenderingSettings);
PK_FORWARD_DECLARE(RHIRenderingFeature);
PK_FORWARD_DECLARE(RHIMaterialShaders);

//----------------------------------------------------------------------------
// Keep this object definition in-sync with 'AssetBaker_EditorMaterialProxy.h'

class	HBO_CLASS(CRHIRenderingSettings), public CBaseSettings
{
	HBO_FIELD(TArray<CRHIRenderingFeature*>,	RenderingFeatures);

public:
	CRHIRenderingSettings();
	~CRHIRenderingSettings();

	virtual const char						*DisplayName() const override;

	CRHIRenderingFeature					*FindFeature(const CString &name);
	PCRHIRenderingFeature					FindFeature(const CString &name) const;

public:
	HBO_CLASS_DECLARATION();
};

//----------------------------------------------------------------------------
// Keep this object definition in-sync with 'AssetBaker_EditorMaterialProxy.h'

class	HBO_CLASS(CRHIRenderingFeature), public CBaseObject
{
	HBO_FIELD(CString,	FeatureName);
	HBO_FIELD(bool,		UseUV);
	HBO_FIELD(bool,		UseNormal);
	HBO_FIELD(bool,		UseTangent);
	HBO_FIELD(bool,		UseMeshUV1);
	HBO_FIELD(bool,		UseMeshVertexColor0);
	HBO_FIELD(bool,		UseMeshVertexColor1);
	HBO_FIELD(bool,		UseMeshVertexBonesIndicesAndWeights);
	HBO_FIELD(bool,		SampleDepth);
	HBO_FIELD(bool,		SampleNormalRoughMetal);
	HBO_FIELD(bool,		SampleDiffuse);
	HBO_FIELD(bool,		UseSceneLightingInfo);

	HBO_FIELD(TArray<CString>,		PropertiesAsShaderConstants);
	HBO_FIELD(TArray<CString>,		TexturesUsedAsLookUp);

public:
	CRHIRenderingFeature();
	~CRHIRenderingFeature();

public:
	HBO_CLASS_DECLARATION();
};

//----------------------------------------------------------------------------

struct	SRenderingFeature
{
	CStringId				m_FeatureName;
	PCRHIRenderingFeature	m_Settings;

	SRenderingFeature(const CStringId &name, PCRHIRenderingFeature settings)
	:	m_FeatureName(name)
	,	m_Settings(settings)
	{
	}
};

//----------------------------------------------------------------------------
// This is a helper to gather all rendering features and depending on the shader combination used
// we can enable or disable some features:

struct	SToggledRenderingFeature : public SRenderingFeature
{
	bool		m_Mandatory;
	bool		m_Enabled;

	SToggledRenderingFeature(const SRenderingFeature &feature, bool mandatory, bool enabled)
	:	SRenderingFeature(feature.m_FeatureName, feature.m_Settings)
	,	m_Mandatory(mandatory)
	,	m_Enabled(enabled)
	{
	}

	SToggledRenderingFeature(const CStringId &name, PCRHIRenderingFeature settings, bool mandatory, bool enabled)
	:	SRenderingFeature(name, settings)
	,	m_Mandatory(mandatory)
	,	m_Enabled(enabled)
	{
	}
};

//----------------------------------------------------------------------------
// Keep this object definition in-sync with 'AssetBaker_EditorMaterialProxy.h'

class	HBO_CLASS(CRHIMaterialShaders), public CBaseObject
{
	HBO_FIELD(CString,	VertexShader);
	HBO_FIELD(CString,	FragmentShader);

public:
	CRHIMaterialShaders();
	~CRHIMaterialShaders();

public:
	HBO_CLASS_DECLARATION();
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
