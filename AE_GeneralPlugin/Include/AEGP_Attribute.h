//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef __FX_AAEATTRIBUTE_H__
#define __FX_AAEATTRIBUTE_H__

#include "AEGP_Define.h"

#include <AEConfig.h>

#include <entry.h>
#include <AE_GeneralPlug.h>
#include <A.h>
#include <SPSuites.h>
#include <AE_Macros.h>

#include <PopcornFX_Define.h>
#include <PopcornFX_Suite.h>

#include <pk_kernel/include/kr_string_id.h>
#include <pk_imaging/include/im_image.h>
#include <pk_particles/include/ps_samplers.h>

//----------------------------------------------------------------------------

namespace PopcornFX
{
	PK_FORWARD_DECLARE(ParticleSamplerDescriptor);
	PK_FORWARD_DECLARE(ShapeDescriptor);
	PK_FORWARD_DECLARE(ImageSampler);
	PK_FORWARD_DECLARE(ResourceMeshBatch);
	PK_FORWARD_DECLARE(ResourceMesh);

	class	CMeshSurfaceSamplerStructuresRandom;
	class	CMeshSurfaceSamplerStructuresFromUV;
	class	CMeshKdTree;
	class	CMeshVolumeSamplerStructuresRandom;
	class	CMeshProjection;

	struct	SDensitySamplerData;
}

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

struct SSamplerBase
{
	PParticleSamplerDescriptor	m_SamplerDescriptor;
	bool						m_Dirty = true;

			SSamplerBase();
	virtual ~SSamplerBase();
};

//----------------------------------------------------------------------------

struct SSamplerShape : public SSamplerBase
{
	ESamplerShapeType	m_Type;

	PShapeDescriptor	m_ShapeDesc;


	PResourceMeshBatch	m_MeshBatch;

	// Acceleration structures are now owned by the integration:
#if	(PK_GEOMETRICS_BUILD_MESH_SAMPLER_SURFACE != 0)
	CMeshSurfaceSamplerStructuresRandom	*m_SurfaceSamplingStructs;
	CMeshSurfaceSamplerStructuresFromUV	*m_SurfaceUVSamplingStructs;

	bool	CreateSurfaceSamplingStructs(const PResourceMeshBatch mesh);
	bool	CreateSurfaceUVSamplingStructs(const PResourceMeshBatch mesh);
#endif

#if (PK_GEOMETRICS_BUILD_MESH_SAMPLER_VOLUME != 0)
	CMeshVolumeSamplerStructuresRandom	*m_VolumeSamplingStructs;

	bool	CreateVolumeSamplingStructs(const PResourceMeshBatch mesh);
#endif

#if (PK_GEOMETRICS_BUILD_MESH_PROJECTION != 0) && 0
	CMeshProjection						*m_ProjectionStructs;

	bool	CreateProjectionStructs(const PResourceMeshBatch mesh);
#endif

#if (PK_GEOMETRICS_BUILD_KDTREE != 0)
	CMeshKdTree							*m_KdTree;

	bool	CreateKdTree(const PResourceMeshBatch mesh);
#endif

						SSamplerShape();
	virtual				~SSamplerShape();
	bool				UpdateShape(SShapeSamplerDescriptor *aeShapeDesc);
};

//----------------------------------------------------------------------------

struct SSamplerImage : public SSamplerBase
{
	PRefCountedMemoryBuffer	m_TextureData;
	u32						m_Width;
	u32						m_Height;
	u32						m_SizeInBytes;
	CImage::EFormat			m_PixelFormat;

	int						m_WrapMode;

	CImageSampler			*m_ImageDesc = null;
	SDensitySamplerData		*m_DensitySampler = null;

	SSamplerImage();
	virtual				~SSamplerImage();
	bool				UpdateImage(SImageSamplerDescriptor *aeTextDesc);
};

//----------------------------------------------------------------------------

struct SSamplerText : public SSamplerBase
{
	CString				m_Data;

						SSamplerText();
	virtual				~SSamplerText();
	bool				UpdateText(STextSamplerDescriptor *aeTextDesc);
};

//----------------------------------------------------------------------------

struct SSamplerAudio : public SSamplerBase
{
	enum SamplingType
	{
		SamplingType_Unknown = 0,
		SamplingType_WaveForm,
		SamplingType_Spectrum,
	};

	float				*m_Waveform;
	u32					m_InputSampleCount;
	u32					m_SampleCount;

	AEGP_SoundDataH		m_SoundData;

	SamplingType		m_SamplingType;
	float				*m_WaveformData;
	TArray<float*>		m_WaveformPyramid;

	CStringId			m_Name;

	Threads::CCriticalSection	m_Lock;
	bool						m_BuiltThisFrame;

	SSamplerAudio();
	virtual				~SSamplerAudio();
	bool				UpdateSound(SAudioSamplerDescriptor *aeTextDesc);

	bool				CleanAudioPyramid();
	bool				BuildAudioPyramidIFN();

	bool				ReleaseAEResources();
};

//----------------------------------------------------------------------------

struct SSamplerVectorField : public SSamplerBase
{
	SSamplerVectorField();
	virtual				~SSamplerVectorField();
	bool				UpdateVectorField(SVectorFieldSamplerDescriptor *aeShapeDesc);
};

//----------------------------------------------------------------------------

struct SPendingAttribute
{
	PF_ProgPtr			m_ParentEffectPtr;

	SAttributeBaseDesc	*m_Desc;

	SSamplerBase		*m_PKDesc;

	AEGP_EffectRefH		m_AttributeEffectRef;

	bool				m_Deleted = false;

	SPendingAttribute(PF_ProgPtr parent = null, SAttributeBaseDesc *desc = null, AEGP_EffectRefH effectRef = null)
		: m_ParentEffectPtr(parent)
		, m_Desc(desc)
		, m_PKDesc(null)
		, m_AttributeEffectRef(effectRef)
	{};

	SPendingAttribute(const SPendingAttribute &other)
		: m_ParentEffectPtr(other.m_ParentEffectPtr)
		, m_Desc(other.m_Desc)
		, m_PKDesc(other.m_PKDesc)
		, m_AttributeEffectRef(other.m_AttributeEffectRef)
	{};

	~SPendingAttribute()
	{
		PK_ASSERT(!m_AttributeEffectRef);
	};

};

//----------------------------------------------------------------------------

__AEGP_PK_END

#endif
