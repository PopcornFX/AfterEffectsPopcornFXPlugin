//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"

#include "AEGP_Attribute.h"
#include "AEGP_AEPKConversion.h"

#include <PopcornFX_Suite.h>
#include <AEGP_SuiteHandler.h>

#include "AEGP_FileWatcher.h"
#include "pk_kernel/include/kr_log_listeners_file.h"

#include <pk_particles/include/ps_system.h>
#include <pk_particles/include/ps_effect.h>
#include <pk_particles/include/ps_stats.h>

#include <pk_particles/include/ps_mediums.h>
#include <pk_particles/include/ps_samplers.h>
#include <pk_particles/include/ps_samplers_image.h>
#include <pk_particles/include/ps_samplers_text.h>
#include <pk_particles/include/ps_samplers_curve.h>
#include <pk_particles/include/ps_samplers_shape.h>
#include <pk_particles/include/ps_samplers_audio.h>
#include <pk_particles/include/ps_samplers_vectorfield.h>
#include <pk_particles/include/ps_attributes.h>

#include <pk_particles/include/ps_vectorfield_resource.h>

#include <pk_geometrics/include/ge_matrix_tools.h>

#include <pk_geometrics/include/ge_mesh.h>
#include <pk_geometrics/include/ge_mesh_resource.h>
#include <pk_particles_toolbox/include/pt_mesh_deformers_skin.h>
#include <pk_geometrics/include/ge_mesh_kdtree.h>
#include <pk_geometrics/include/ge_mesh_sampler_accel.h>
#include <pk_geometrics/include/ge_mesh_projection.h>

#include <pk_maths/include/pk_numeric_tools_int.h>

#include "AEGP_World.h"

#include <complex>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

SSamplerBase::SSamplerBase()
	: m_SamplerDescriptor(null)
{

}

//----------------------------------------------------------------------------

SSamplerBase::~SSamplerBase()
{
	m_SamplerDescriptor = null;
}

//----------------------------------------------------------------------------

SSamplerShape::SSamplerShape()
	: m_Type(SamplerShapeType_None)
	, m_ShapeDesc(null)
	, m_MeshBatch(null)
{

}

//----------------------------------------------------------------------------

SSamplerShape::~SSamplerShape()
{
	m_ShapeDesc = null;

}

//----------------------------------------------------------------------------

#if	(PK_GEOMETRICS_BUILD_MESH_SAMPLER_SURFACE != 0)

bool	SSamplerShape::CreateSurfaceSamplingStructs(const PResourceMeshBatch mesh)
{
	m_SurfaceSamplingStructs = PK_NEW(CMeshSurfaceSamplerStructuresRandom);
	if (!PK_VERIFY(m_SurfaceSamplingStructs != null))
		return false;
	return m_SurfaceSamplingStructs->Build(mesh->RawMesh()->TriangleBatch().m_IStream, mesh->RawMesh()->TriangleBatch().m_VStream.Positions());
}

//----------------------------------------------------------------------------

bool	SSamplerShape::CreateSurfaceUVSamplingStructs(const PResourceMeshBatch mesh)
{
	m_SurfaceUVSamplingStructs = PK_NEW(CMeshSurfaceSamplerStructuresFromUV);
	if (!PK_VERIFY(m_SurfaceUVSamplingStructs != null))
		return false;
	return m_SurfaceUVSamplingStructs->Build(SMeshUV2PCBuildConfig(), mesh->RawMesh()->TriangleBatch().m_VStream, mesh->RawMesh()->TriangleBatch().m_IStream);
}

#endif

//----------------------------------------------------------------------------

#if (PK_GEOMETRICS_BUILD_MESH_SAMPLER_VOLUME != 0)
bool	SSamplerShape::CreateVolumeSamplingStructs(const PResourceMeshBatch mesh)
{
	m_VolumeSamplingStructs = PK_NEW(CMeshVolumeSamplerStructuresRandom);
	if (!PK_VERIFY(m_VolumeSamplingStructs != null))
		return false;

	if (!mesh->RawMesh()->HasTetrahedralMeshing())
		return false;

	return m_VolumeSamplingStructs->Build(mesh->RawMesh()->TriangleBatch().m_VStream.Positions(), mesh->RawMesh()->TetrahedralOtherPositions(), mesh->RawMesh()->TetrahedralIndices(), mesh->RawMesh()->TetrahedralIndicesCount());
}
#endif

//----------------------------------------------------------------------------

#if (PK_GEOMETRICS_BUILD_MESH_PROJECTION) && 0
bool	SSamplerShape::CreateProjectionStructs(const PResourceMeshBatch mesh)
{
	m_ProjectionStructs = PK_NEW(CMeshProjection);
	if (!PK_VERIFY(m_ProjectionStructs != null))
		return false;
	return m_ProjectionStructs->Build(mesh->RawMesh()->TriangleBatch());
}
#endif

//----------------------------------------------------------------------------

#if (PK_GEOMETRICS_BUILD_KDTREE != 0)
bool	SSamplerShape::CreateKdTree(const PResourceMeshBatch mesh)
{
	m_KdTree = PK_NEW(CMeshKdTree);
	if (!PK_VERIFY(m_KdTree != null))
		return false;
	SMeshKdTreeBuildConfig	buildConfig;
	buildConfig.m_Flags |= SMeshKdTreeBuildConfig::LowQualityButFasterBuild;
	return m_KdTree->Build(mesh->RawMesh()->TriangleBatch(), buildConfig);
}
#endif

//----------------------------------------------------------------------------

bool	SSamplerShape::UpdateShape(SShapeSamplerDescriptor *aeShapeDesc)
{
	CParticleSamplerDescriptor_Shape_Default	*desc = static_cast<CParticleSamplerDescriptor_Shape_Default*>(m_SamplerDescriptor.Get());

	if (m_ShapeDesc == null ||
		m_Type != aeShapeDesc->m_Type)
	{
		m_Type = aeShapeDesc->m_Type;
		// Create new shape descriptor:
		switch (m_Type)
		{
		case SamplerShapeType_Box:
			m_ShapeDesc = PK_NEW(CShapeDescriptor_Box(CFloat3(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[1], aeShapeDesc->m_Dimension[2])));
			break;
		case SamplerShapeType_Sphere:
			m_ShapeDesc = PK_NEW(CShapeDescriptor_Sphere(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[1]));
			break;
		case SamplerShapeType_Ellipsoid:
			m_ShapeDesc = PK_NEW(CShapeDescriptor_Ellipsoid(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[1]));
			break;
		case SamplerShapeType_Cylinder:
			m_ShapeDesc = PK_NEW(CShapeDescriptor_Cylinder(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[1], aeShapeDesc->m_Dimension[2]));
			break;
		case SamplerShapeType_Capsule:
			m_ShapeDesc = PK_NEW(CShapeDescriptor_Capsule(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[1], aeShapeDesc->m_Dimension[2]));
			break;
		case SamplerShapeType_Cone:
			m_ShapeDesc = PK_NEW(CShapeDescriptor_Cone(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[1]));
			break;
		case SamplerShapeType_Mesh:
			m_ShapeDesc = PK_NEW(CShapeDescriptor_Mesh());
			break;
		default:
			break;
		}
	}
	if (aeShapeDesc->m_Type == m_Type)
	{
		// Update shape descriptor:
		if (m_Type == SamplerShapeType_Box)
		{
			static_cast<CShapeDescriptor_Box*>(m_ShapeDesc.Get())->SetDimensions(CFloat3(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[1], aeShapeDesc->m_Dimension[2]));
		}
		else if (m_Type == SamplerShapeType_Sphere)
		{
			CShapeDescriptor_Sphere	*sphereDesc = static_cast<CShapeDescriptor_Sphere*>(m_ShapeDesc.Get());
			sphereDesc->SetRadius(aeShapeDesc->m_Dimension[0]);
			sphereDesc->SetInnerRadius(aeShapeDesc->m_Dimension[1]);
		}
		else if (m_Type == SamplerShapeType_Ellipsoid)
		{
			CShapeDescriptor_Ellipsoid	*ellipsoidDesc = static_cast<CShapeDescriptor_Ellipsoid*>(m_ShapeDesc.Get());
			ellipsoidDesc->SetRadius(aeShapeDesc->m_Dimension[0]);
			ellipsoidDesc->SetInnerRadius(aeShapeDesc->m_Dimension[1]);
		}
		else if (m_Type == SamplerShapeType_Cylinder)
		{
			CShapeDescriptor_Cylinder	*cylinderDesc = static_cast<CShapeDescriptor_Cylinder*>(m_ShapeDesc.Get());
			cylinderDesc->SetRadius(aeShapeDesc->m_Dimension[0]);
			cylinderDesc->SetHeight(aeShapeDesc->m_Dimension[1]);
			cylinderDesc->SetInnerRadius(aeShapeDesc->m_Dimension[2]);
		}
		else if (m_Type == SamplerShapeType_Capsule)
		{
			CShapeDescriptor_Capsule	*capsuleDesc = static_cast<CShapeDescriptor_Capsule*>(m_ShapeDesc.Get());
			capsuleDesc->SetRadius(aeShapeDesc->m_Dimension[0]);
			capsuleDesc->SetHeight(aeShapeDesc->m_Dimension[1]);
			capsuleDesc->SetInnerRadius(aeShapeDesc->m_Dimension[2]);
		}
		else if (m_Type == SamplerShapeType_Cone)
		{
			CShapeDescriptor_Cone	*coneDesc = static_cast<CShapeDescriptor_Cone*>(m_ShapeDesc.Get());
			coneDesc->SetRadius(aeShapeDesc->m_Dimension[0]);
			coneDesc->SetHeight(aeShapeDesc->m_Dimension[1]);
		}
		else if (m_Type == SamplerShapeType_Mesh)
		{
			CShapeDescriptor_Mesh	*meshDesc = static_cast<CShapeDescriptor_Mesh*>(m_ShapeDesc.Get());

			if (aeShapeDesc->m_Path.length() == 0)
				return true;

			//Todo do it with the resource manager
			CFilePackPath			filePackPath = CFilePackPath::FromPhysicalPath(aeShapeDesc->m_Path.data(), File::DefaultFileSystem());

			if (!filePackPath.Empty())
			{
				CMessageStream	loadReport;
				PResourceMesh	mesh = CResourceMesh::Load(File::DefaultFileSystem(), filePackPath, loadReport);
				loadReport.Log();
				if (!PK_VERIFY(mesh != null))
				{
					CLog::Log(PK_ERROR, "Fail loading the CMeshResource from the pkmm content");
					return false;
				}
				const u32		uSubMeshId = static_cast<u32>(0);
				const u32		batchCount = mesh->BatchList().Count();
				if (!PK_VERIFY(uSubMeshId < batchCount))
				{
					CLog::Log(PK_ERROR, "Cannot use the submesh ID %d: the mesh only has %d submeshes", uSubMeshId, batchCount);
					return false;
				}
				m_MeshBatch = mesh->BatchList()[uSubMeshId];

				// ------------------------------------------
				// Build KD Tree IFN
				if ((aeShapeDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Mesh_Intersect) != 0 ||
					(aeShapeDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Mesh_Project) != 0 ||
					(aeShapeDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Mesh_Contains) != 0 ||
					(aeShapeDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Mesh_DistanceField) != 0)
				{
#if	(PK_GEOMETRICS_BUILD_KDTREE != 0)
					if (!CreateKdTree(m_MeshBatch))
						CLog::Log(PK_WARN, "Failed building mesh kdTree acceleration structure");
					else
						meshDesc->SetKdTree(m_KdTree);
#endif
				}

#if	(PK_GEOMETRICS_BUILD_MESH_SAMPLER_SURFACE != 0)
				// Build sampling info IFN
				if ((aeShapeDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Mesh_Sample) != 0)
				{
					if (!CreateSurfaceSamplingStructs(m_MeshBatch))
						CLog::Log(PK_WARN, "Failed building mesh surface-sampling acceleration structure");
					else
						meshDesc->SetSamplingStructs(m_SurfaceSamplingStructs, null);
				}

				// Build UV 2 PCoords info IFN
				if ((aeShapeDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Mesh_SampleFromUV) != 0)
				{
					if (!CreateSurfaceUVSamplingStructs(m_MeshBatch))
						CLog::Log(PK_WARN, "Failed building mesh uv-to-pcoords acceleration structure");
					else
						meshDesc->SetUVSamplingStructs(m_SurfaceUVSamplingStructs, 0);
				}
#endif
#if (PK_GEOMETRICS_BUILD_MESH_SAMPLER_VOLUME)
				if ((aeShapeDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Mesh_Sample) != 0)
				{
					if (!CreateVolumeSamplingStructs(m_MeshBatch))
						CLog::Log(PK_WARN, "Failed building mesh surface-sampling acceleration structure");
				}
#endif
				meshDesc->SetMesh(m_MeshBatch->RawMesh());
				meshDesc->SetScale(CFloat3(aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[0], aeShapeDesc->m_Dimension[0]));
			}
		}
	}

	if (desc == null && m_ShapeDesc != null)
	{
		desc = PK_NEW(CParticleSamplerDescriptor_Shape_Default(m_ShapeDesc.Get()));
		m_SamplerDescriptor = desc;
	}
	else if (m_ShapeDesc != null)
	{
		desc->m_Shape = m_ShapeDesc.Get();
	}

	return true;
}

//----------------------------------------------------------------------------

SSamplerImage::SSamplerImage()
	: m_ImageDesc(null)
	, m_TextureData(null)
	, m_Width(0)
	, m_Height(0)
	, m_SizeInBytes(0)
	, m_PixelFormat(CImage::EFormat::Format_Invalid)
	, m_DensitySampler(null)

{
}

//----------------------------------------------------------------------------

SSamplerImage::~SSamplerImage()
{
	PK_SAFE_DELETE(m_DensitySampler);
	PK_SAFE_DELETE(m_ImageDesc);
}

//----------------------------------------------------------------------------

bool	SSamplerImage::UpdateImage(SImageSamplerDescriptor *aeImageDesc)
{
	const u32				width = m_Width;
	const u32				height = m_Height;
	const u32				sizeInBytes = m_SizeInBytes;
	const CImage::EFormat	format = m_PixelFormat;

	CParticleSamplerDescriptor_Image_Default	*desc = static_cast<CParticleSamplerDescriptor_Image_Default*>(m_SamplerDescriptor.Get());

	if (width == 0 || height == 0)
		return false;

	CImageMap		map;

	map.m_RawBuffer = m_TextureData;
	map.m_Dimensions = CUint3(width, height, 1);

	CImageSampler	*imageDesc = m_ImageDesc;
	CImageSurface	surface(map, format);

	if (imageDesc == null)
	{
		imageDesc = PK_NEW(CImageSamplerBilinear);
		m_ImageDesc = imageDesc;
	}

	if (!PK_VERIFY(m_ImageDesc != null))
	{
		CLog::Log(PK_ERROR, "Could not create the image sampler");
		return false;
	}

	if (!imageDesc->SetupFromSurface(surface))
	{
		surface.Convert(CImage::Format_BGRA8);
		if (!PK_VERIFY(imageDesc->SetupFromSurface(surface)))
		{
			CLog::Log(PK_ERROR, "Could not setup the image sampler");
			return false;
		}
	}

	if (desc == null)
	{
		desc = PK_NEW(CParticleSamplerDescriptor_Image_Default(m_ImageDesc));
		m_SamplerDescriptor = desc;
	}
	else
	{
		desc->m_Sampler = m_ImageDesc;
		desc->m_ImageDimensions = m_ImageDesc->Dimensions();
	}

	if (aeImageDesc->m_UsageFlags & SParticleDeclaration::SSampler::UsageFlags_Image_Density)
	{
		if (m_DensitySampler == null)
		{
			m_DensitySampler = PK_NEW(SDensitySamplerData);

			PK_ASSERT(m_DensitySampler != null);
		}
		SDensitySamplerBuildSettings	densityBuildSettings;

		if (!m_DensitySampler->Build(surface, densityBuildSettings))
		{
			CLog::Log(PK_ERROR, "Could not build the density sampler");
			return false;
		}
		if (!desc->SetupDensity(m_DensitySampler))
		{
			CLog::Log(PK_ERROR, "Could not setup the density image sampler");
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

SSamplerText::SSamplerText()
{
}

//----------------------------------------------------------------------------

SSamplerText::~SSamplerText()
{
}

//----------------------------------------------------------------------------

bool	SSamplerText::UpdateText(STextSamplerDescriptor *aeTextDesc)
{
	m_Data = aeTextDesc->m_Data.c_str();

	CParticleSamplerDescriptor_Text_Default	*desc = static_cast<CParticleSamplerDescriptor_Text_Default*>(m_SamplerDescriptor.Get());

	const CFontMetrics	*fontKerning = null;
	bool				useKerning = false;

	if (desc == null)
	{
		desc = PK_NEW(CParticleSamplerDescriptor_Text_Default());
		m_SamplerDescriptor = desc;
	}
	if (desc != null)
	{
		if (!desc->_Setup(m_Data, fontKerning, useKerning))
		{
			CLog::Log(PK_ERROR, "Could not setup the text descriptor");
			return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

SSamplerAudio::SSamplerAudio()
	: m_Waveform(null)
	, m_InputSampleCount(0)
	, m_SampleCount(0)
	, m_Name(null)
	, m_SoundData(null)
	, m_SamplingType(SamplingType_Unknown)
	, m_WaveformData(null)
	, m_BuiltThisFrame(false)
{
}

//----------------------------------------------------------------------------

SSamplerAudio::~SSamplerAudio()
{
	CleanAudioPyramid();
}

//----------------------------------------------------------------------------

bool	SSamplerAudio::UpdateSound(SAudioSamplerDescriptor *aeSoundDesc)
{
	(void)aeSoundDesc;
	CParticleSamplerDescriptor_Audio_Default	*desc = static_cast<CParticleSamplerDescriptor_Audio_Default*>(m_SamplerDescriptor.Get());

	if (desc == null)
	{
		desc = PK_NEW(CParticleSamplerDescriptor_Audio_Default());
		m_SamplerDescriptor = desc;
	}
	m_Name = desc->m_ChannelGroupNameID;
	return true;
}

//----------------------------------------------------------------------------

bool	SSamplerAudio::CleanAudioPyramid()
{
	for (u32 i = 0; i < m_WaveformPyramid.Count(); i++)
	{
		PK_FREE(m_WaveformPyramid[i]);
	}
	m_WaveformPyramid.Clean();
	return true;
}

//----------------------------------------------------------------------------

unsigned int BitReverse(u32 x, int log2n)
{
	return IntegerTools::ReverseBits(x) >> (32 - log2n);
}

void	BasicFFT(std::complex<float> *inputs, std::complex<float> *outputs, int log2n)
{
	const float					kPI = 3.1415926536f;
	const std::complex<float>	kImag(0, 1);
	u32							n = 1 << log2n;
	for (u32 i = 0; i < n; ++i)
	{
		outputs[BitReverse(i, log2n)] = inputs[i];
	}
	for (s32 s = 1; s <= log2n; ++s)
	{
		u32 m = 1 << s;
		u32 m2 = m >> 1;
		std::complex<float> w(1, 0);
		std::complex<float>	wm = exp(-kImag * (kPI / m2));
		for (u32 j = 0; j < m2; ++j)
		{
			for (u32 k = j; k < n; k += m)
			{
				std::complex<float>	t = w * outputs[k + m2];
				std::complex<float>	u = outputs[k];
				outputs[k] = u + t;
				outputs[k + m2] = u - t;
			}
			w *= wm;
        }
    }
}

//----------------------------------------------------------------------------

bool	SSamplerAudio::BuildAudioPyramidIFN()
{
	PK_SCOPEDLOCK(m_Lock);

	if (m_InputSampleCount == 0 ||
		m_Waveform == null)
		return false;

	m_SampleCount = 1024;
	PK_ASSERT(m_InputSampleCount >= m_SampleCount);
	if (m_SamplingType == SamplingType_Spectrum)
	{
		const u32				frequencyCount = m_SampleCount * 2;
		PK_ASSERT(m_InputSampleCount >= frequencyCount);
		TArray<std::complex<float> >	inData;
		TArray<std::complex<float> >	outData;

		if (!PK_VERIFY(inData.Resize(m_InputSampleCount) && outData.Resize(frequencyCount)))
			return false;

		for (u32 i = 0; i < m_InputSampleCount; ++i)
		{
			inData[i].real(m_Waveform[i]);
			inData[i].imag(0);
		}
		BasicFFT(inData.RawDataPointer(), outData.RawDataPointer(), IntegerTools::Log2(frequencyCount));
		for (u32 j = 0; j < m_SampleCount; ++j)
		{
			m_Waveform[j] = std::sqrt(PKSquared(outData[j].real()) + PKSquared(outData[j].imag())) / static_cast<float>(m_SampleCount);
		}
	}

	// lazy-allocation
	if (m_WaveformData == null)
	{
		CleanAudioPyramid();

		const u32	baseAllocSize = m_SampleCount;
		// allocate two double borders to avoid checking for overflow during sampling w/ Cubic or Linear filters
		const u32	baseByteCount = (2 + baseAllocSize + 2) * sizeof(*m_WaveformData);
		m_WaveformData = (float*)PK_MALLOC_ALIGNED(baseByteCount, 0x80);
		if (m_WaveformData == null)
			return false;
		Mem::Clear(m_WaveformData, baseByteCount);

		bool		success = true;
		const u32	pyramidSize = IntegerTools::Log2(m_SampleCount) + 1;
		if (m_WaveformPyramid.Resize(pyramidSize))
		{
			u32	currentCount = m_SampleCount;
			for (u32 i = 1; i < pyramidSize; i++)
			{
				currentCount >>= 1;
				PK_ASSERT(currentCount != 0);
				const u32	mipByteCount = (2 + currentCount + 2) * sizeof(float);
				m_WaveformPyramid[i] = (float*)PK_MALLOC_ALIGNED(mipByteCount, 0x10);
				if (m_WaveformPyramid[i] != null)
					Mem::Clear(m_WaveformPyramid[i], mipByteCount);
				success &= (m_WaveformPyramid[i] != null);
			}
		}
		m_WaveformPyramid[0] = m_WaveformData;

		if (!success)
		{
			m_WaveformData = null;
			CleanAudioPyramid();
			return false;
		}
	}

	PK_ASSERT(m_WaveformData != null);
	float	*realDataPtr = m_WaveformData + 2;	// ptr to the first real element, skipping the two-element border
	memcpy(realDataPtr, m_Waveform, m_SampleCount * sizeof(*m_WaveformData));

	{
		const float	firstEntry = realDataPtr[0];
		const float	lastEntry = realDataPtr[m_SampleCount - 1];
		realDataPtr[-1] = firstEntry; // duplicate the first entry in the two start borders
		realDataPtr[-2] = firstEntry;
		realDataPtr[m_SampleCount + 0] = lastEntry; // duplicate the last entry in the two end borders
		realDataPtr[m_SampleCount + 1] = lastEntry;
	}

	// right, rebuild the spectrum pyramid:
	if (!m_WaveformPyramid.Empty())
	{
		u32	currentCount = m_SampleCount;
		for (u32 i = 1; i < m_WaveformPyramid.Count(); i++)
		{
			const float	* __restrict src = 2 + m_WaveformPyramid[i - 1];
			float		* __restrict dst = 2 + m_WaveformPyramid[i];

			currentCount >>= 1;

			// downsample
			for (u32 j = 0; j < currentCount; j++)
			{
				dst[j] = 0.5f * (src[j * 2 + 0] + src[j * 2 + 1]);
			}

			const float	firstEntry = dst[0];
			const float	lastEntry = dst[currentCount - 1];
			dst[-1] = firstEntry;				// duplicate the first entry in the two start borders
			dst[-2] = firstEntry;
			dst[currentCount + 0] = lastEntry;	// duplicate the last entry in the two end borders
			dst[currentCount + 1] = lastEntry;
		}
	}
	m_BuiltThisFrame = true;
	return true;
}

//----------------------------------------------------------------------------

bool SSamplerAudio::ReleaseAEResources()
{
	m_Waveform = null;
	if (m_SoundData != null)
	{
		CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();
		A_Err					result = A_Err_NONE;
		AEGP_SuiteHandler		suites(instance.GetAESuites());

		result |= suites.SoundDataSuite1()->AEGP_UnlockSoundDataSamples(m_SoundData);
		result |= suites.SoundDataSuite1()->AEGP_DisposeSoundData(m_SoundData);

		m_SoundData = null;
		if (result != A_Err_NONE)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

SSamplerVectorField::SSamplerVectorField()
{
}

//----------------------------------------------------------------------------

SSamplerVectorField::~SSamplerVectorField()
{
}

//----------------------------------------------------------------------------

bool SSamplerVectorField::UpdateVectorField(SVectorFieldSamplerDescriptor * aeTurbulenceDesc)
{
	CParticleSamplerDescriptor_VectorField_Grid	*desc = static_cast<CParticleSamplerDescriptor_VectorField_Grid*>(m_SamplerDescriptor.Get());

	if (m_Dirty == false)
		return true;

	m_Dirty = false;

	if (desc == null)
	{
		desc = PK_NEW(CParticleSamplerDescriptor_VectorField_Grid());
		m_SamplerDescriptor = desc;
	}


	if (aeTurbulenceDesc->m_ResourceUpdate)
	{
		if (aeTurbulenceDesc->m_Path.length() == 0)
			return true;

		CFilePackPath				filePackPath = CFilePackPath::FromPhysicalPath(aeTurbulenceDesc->m_Path.data(), File::DefaultFileSystem());
		TResourcePtr<CVectorField>	rscVf = Resource::DefaultManager()->Load<CVectorField>(filePackPath);

		CParticleSamplerDescriptor_VectorField_Grid::EDataType	dataType = CParticleSamplerDescriptor_VectorField_Grid::DataType_Void;
		switch (rscVf->m_DataType)
		{
		case VFDataType_Fp32:
			dataType = CParticleSamplerDescriptor_VectorField_Grid::DataType_Fp32;
			break;
		case VFDataType_Fp16:
			dataType = CParticleSamplerDescriptor_VectorField_Grid::DataType_Fp16;
			break;
		case VFDataType_U8SN:
			dataType = CParticleSamplerDescriptor_VectorField_Grid::DataType_U8SN;
			break;
		default:
			break;
		}
		CFloat4x4	transform = CFloat4x4::IDENTITY;

		transform.StrippedTranslations() = AAEToPK(aeTurbulenceDesc->m_Position);

		if (!(desc->Setup(	rscVf->m_Dimensions,
							rscVf->m_IntensityMultiplier,
							rscVf->m_BoundsMin,
							rscVf->m_BoundsMax,
							rscVf->m_Data,
							dataType,
							aeTurbulenceDesc->m_Strength,						//Strength
							transform,											//Xforms
							0,													//Flags
							AAEToPK(aeTurbulenceDesc->m_Interpolation))))
			return false;
	}
	else
	{
		CFloat4x4	transform = CFloat4x4::IDENTITY;

		transform.StrippedTranslations() = AAEToPK(aeTurbulenceDesc->m_Position);

		desc->SetTransforms(transform);
		desc->SetStrength(aeTurbulenceDesc->m_Strength);
	}
	return true;
}

//----------------------------------------------------------------------------

__AEGP_PK_END

