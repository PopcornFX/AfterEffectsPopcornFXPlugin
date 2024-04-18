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

#include "PKSample.h"

#include "MaterialToRHI.h"

#include "ShaderDefinitions/EditorShaderDefinitions.h"
#include "ShaderDefinitions/SampleLibShaderDefinitions.h"
#include "ShaderGenerator/ShaderGenerator.h"
#include "ApiContextConfig.h"
#include "pk_rhi/include/EnumHelper.h"

// Node (Alex) : this is temporary until we find a better way to handle built-in elements
#include "pk_particles/include/ps_nodegraph_frontend_renderers.h"
#include "pk_render_helpers/include/render_features/rh_features_basic.h"
#include "pk_render_helpers/include/draw_requests/rh_billboard.h"
#include "pk_render_helpers/include/draw_requests/rh_ribbon.h"


#include <pk_kernel/include/kr_buffer_parsing_utils.h> // for _SanitizeShaderVisibleName implementation

#include "SampleUtils.h"
#include "PK-MCPP/pk_preprocessor.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

namespace	MaterialToRHI
{

	static bool		_IsTriangleGeomGenerationFeatureProperty(const CString &fieldName)
	{
		return	fieldName == "TriangleCustomNormals_Normal1" ||
				fieldName == "TriangleCustomNormals_Normal2" ||
				fieldName == "TriangleCustomNormals_Normal3" ||
				fieldName == "TriangleCustomUVs_UV1" ||
				fieldName == "TriangleCustomUVs_UV2" ||
				fieldName == "TriangleCustomUVs_UV3";
	}

	static bool		_IsMeshLODProperty(const CString &fieldName)
	{
		return fieldName == "MeshLOD_LOD";
	}

	//----------------------------------------------------------------------------

#if (PK_MAT2RHI_CAN_COMPILE != 0)
	static u64		_GetShaderTimestamp(const CString &shaderPath, const CRHIShadersCompilationFileSystem &fs)
	{
		if (shaderPath.Empty())
			return 0;

		u64				timestamp = fs.FileLastModifiedTime(shaderPath);
		// Will need to gather timestamps on shader dependencies
		PFileStream		shader = fs.GetFileSystem()->OpenStream(shaderPath, IFileSystem::Access_Read, CFilePath::IsAbsolute(shaderPath));

		if (shader == null)
			return 0;

		const CString	shaderContent = shader->BufferizeToString();
		TArray<CString>	dependencies;

		if (!CPreprocessor::FindShaderDependencies(shaderContent, CFilePath::StripFilename(shaderPath), dependencies, fs.GetFileSystem()))
			return timestamp;

		for (const CString &path : dependencies)
			timestamp = PKMax(fs.FileLastModifiedTime(path), timestamp);
		return timestamp;
	}

	static bool		_IsGeometryGenerationFeature(const CString &featureName)
	{
#define	X_RENDERER_CLASSES(__name)	if (featureName == PK_STRINGIFY(Geometry ## __name)) return true;
		PK_EXEC_X_RENDERER_CLASSES()
#undef	X_RENDERER_CLASSES
		return false;
	}

	//----------------------------------------------------------------------------

	static bool		_IsMeshAtlasFeature(const CString &featureName)
	{
		return featureName == "MeshAtlas" || featureName == "MeshLOD";
	}

	//----------------------------------------------------------------------------

	SCompileArg::SCompileArg(const SCompileArg &oth)
	{
		m_SourceTimestamps = oth.m_SourceTimestamps;
		m_ShaderFolder = oth.m_ShaderFolder;
		m_Fields = oth.m_Fields;
		m_Properties = oth.m_Properties;
		m_FeaturesSettings = oth.m_FeaturesSettings;
		m_MaterialSettings = oth.m_MaterialSettings;
		m_RendererType = oth.m_RendererType;
		m_RenderPasses = oth.m_RenderPasses;
		m_OptionsOverride = oth.m_OptionsOverride;
		m_Material = oth.m_Material;
	}

	SCompileArg::SCompileArg(const ERendererClass rendererType, const CString &shaderFolder)
	:	m_ShaderFolder(shaderFolder)
	,	m_RendererType(rendererType)
	,	m_Material(null)
	{
	}

	bool		SCompileArg::InitFromRendererFeatures(const PCParticleRendererMaterial &material)
	{
		PK_ASSERT(!m_ShaderFolder.Empty());
		PK_ASSERT(material != null);

		m_Material = material;

		if (!PK_VERIFY(PopulateSettings(material, m_RendererType, null, &m_FeaturesSettings, &m_MaterialSettings)))
			return false;

		// Only remap fragment shader path, not generated
#if 0	// #5527: Materials: All .pkma files should have a valid .frag/.vert shader path
		if (m_MaterialSettings.m_FragmentShaderPath.Empty())
			m_MaterialSettings.m_FragmentShaderPath = m_ShaderFolder / m_MaterialSettings.m_MaterialName + ".frag";
#endif
		PK_ASSERT(!m_MaterialSettings.m_FragmentShaderPath.Empty());
		return true;
	}

	//----------------------------------------------------------------------------

	bool		SCompileArg::InitFromRendererData(	const PCRendererDataBase	&renderer,
													HBO::CContext				*context)
	{
		PK_ASSERT(!m_ShaderFolder.Empty());
		PK_ASSERT(context != null);
		PK_ASSERT(renderer != null && renderer->m_RendererType == m_RendererType);	// will crash above anyway

		m_Fields = renderer->m_Declaration.m_AdditionalFields;
		m_Properties = renderer->m_Declaration.m_Properties;

		// Here, retrieve shaders from .pkma
		const CString	&materialPath = renderer->m_Declaration.m_MaterialPath;
		bool			fileWasLoaded = false;
		PBaseObjectFile	materialFile = context->LoadFile(materialPath, false, &fileWasLoaded);
		if (materialFile != null)
		{
			PParticleRendererMaterial	material = materialFile->FindFirstOf<CParticleRendererMaterial>();
			if (PK_VERIFY(material != null) &&
				PK_VERIFY(material->Initialize()))
			{
				if (!PK_VERIFY(PopulateSettings(material, renderer->m_RendererType, renderer, &m_FeaturesSettings, &m_MaterialSettings)))
					return false;
			}

			if (!fileWasLoaded)
				materialFile->Unload();
			materialFile = null;
		}
		// Only remap fragment shader path, not generated
#if 0	// #5527: Materials: All .pkma files should have a valid .frag/.vert shader path
		if (m_MaterialSettings.m_FragmentShaderPath.Empty())
			m_MaterialSettings.m_FragmentShaderPath = m_ShaderFolder / m_MaterialSettings.m_MaterialName + ".frag";
#endif
		PK_ASSERT(!m_MaterialSettings.m_FragmentShaderPath.Empty());

		bool	success = true;

		{
			success &= m_OptionsOverride.PushBack(Option_VertexPassThrough).Valid();

			if (renderer->m_RendererType == ERendererClass::Renderer_Billboard)
			{
				// Geom billboarding shader options
				const PCRendererDataBillboard	rendererBillboard = static_cast<const CRendererDataBillboard*>(renderer.Get());

				const SRendererFeaturePropertyValue	*billboardingMode = rendererBillboard->m_Declaration.FindProperty(BasicRendererProperties::SID_BillboardingMode());
				EBillboardMode						modeBB = (billboardingMode != null) ? Drawers::SBillboard_BillboardingRequest::BillboardProperty_BillboardMode_ToInternal(billboardingMode->ValueI().x()) : BillboardMode_ScreenAligned;
				u32									shaderOptions_Geom = Option_VertexPassThrough | Option_GeomBillboarding;
				u32									shaderOptions_Vertex = Option_VertexPassThrough | Option_VertexBillboarding;
				switch (modeBB)
				{
				case BillboardMode_ScreenAligned:
				case BillboardMode_ViewposAligned:
					break;
				case BillboardMode_AxisAligned:
				case BillboardMode_AxisAlignedSpheroid:
					shaderOptions_Geom |= Option_Axis_C1;
					shaderOptions_Vertex |= Option_Axis_C1;
					break;
				case BillboardMode_AxisAlignedCapsule:
					shaderOptions_Geom |= Option_Axis_C1 | Option_Capsule;
					shaderOptions_Vertex |= Option_Axis_C1 | Option_Capsule;
					break;
				case BillboardMode_PlaneAligned:
					shaderOptions_Geom |= Option_Axis_C2;
					shaderOptions_Vertex |= Option_Axis_C2;
					break;
				default:
					PK_ASSERT_NOT_REACHED();
					break;
				}

				const SRendererFeaturePropertyValue	*size2Enable = rendererBillboard->m_Declaration.FindProperty(BasicRendererProperties::SID_EnableSize2D());
				const bool							size2Enabled = size2Enable != null ? size2Enable->ValueB() : false;
				if (size2Enabled) // must match with SBillboard_BillboardingRequest::m_SizeFloat2
				{
					shaderOptions_Geom |= Option_BillboardSizeFloat2;
					shaderOptions_Vertex |= Option_BillboardSizeFloat2;
				}

				// TODO: would be better to know if atlas would needed or not
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Geom)).Valid();
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Geom | Option_GPUStorage)).Valid();

				// Vertex billboarding shader options
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex)).Valid();
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex | Option_GPUStorage)).Valid();
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex | Option_GPUSort)).Valid();
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex | Option_GPUStorage | Option_GPUSort)).Valid();

			}
			else if (renderer->m_RendererType == ERendererClass::Renderer_Triangle)
			{
				const u32	shaderOptions_Vertex = Option_VertexPassThrough | Option_TriangleVertexBillboarding;
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex)).Valid();
			}
			else if (renderer->m_RendererType == ERendererClass::Renderer_Mesh)
			{
				const u32	shaderOptions_Vertex = Option_VertexPassThrough | Option_GPUMesh;
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex)).Valid();
			}
			else if (renderer->m_RendererType == ERendererClass::Renderer_Ribbon)
			{
				// Geom billboarding shader options
				const PCRendererDataRibbon	rendererRibbon = static_cast<const CRendererDataRibbon*>(renderer.Get());

				const SRendererFeaturePropertyValue	*billboardingMode = rendererRibbon->m_Declaration.FindProperty(BasicRendererProperties::SID_BillboardingMode());
				ERibbonMode							modeBB = (billboardingMode != null) ? Drawers::SRibbon_BillboardingRequest::RibbonProperty_BillboardMode_ToInternal(billboardingMode->ValueI().x()) : RibbonMode_ViewposAligned;

				u32	shaderOptions_Vertex = Option_VertexPassThrough | Option_RibbonVertexBillboarding;

				switch (modeBB)
				{
				case	RibbonMode_ViewposAligned:
					break;
				case	RibbonMode_NormalAxisAligned:
				case	RibbonMode_SideAxisAligned:
				case	RibbonMode_SideAxisAlignedTube: // Might be interesting to add shader options for tubes & multi-planes but it would break batching.
				case	RibbonMode_SideAxisAlignedMultiPlane:
					shaderOptions_Vertex |= Option_Axis_C1;
					break;
				default:
					PK_ASSERT_NOT_REACHED();
					break;
				}

				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex | Option_GPUStorage)).Valid();
				success &= m_OptionsOverride.PushBack(static_cast<EShaderOptions>(shaderOptions_Vertex | Option_GPUStorage | Option_GPUSort)).Valid();
			}
		}
		_FindParticleRenderPasses();
		return success;
	}

	bool	SCompileArg::ComputeSourceShadersTimestamps(const CString &materialPath, const CRHIShadersCompilationFileSystem &fs)
	{
		const u64	materialTimestamp = fs.FileLastModifiedTime(materialPath);
		const u64	vertexShaderTimestamp = _GetShaderTimestamp(m_MaterialSettings.m_VertexShaderPath, fs);
		const u64	fragmentShaderTimestamp = _GetShaderTimestamp(m_MaterialSettings.m_FragmentShaderPath, fs);

		m_SourceTimestamps.m_Fragment = PKMax(materialTimestamp, fragmentShaderTimestamp);
		if (!m_MaterialSettings.m_VertexShaderPath.Empty())
			m_SourceTimestamps.m_Vertex = PKMax(materialTimestamp, vertexShaderTimestamp);
		else
			m_SourceTimestamps.m_Vertex = materialTimestamp; // Generated
		return true;
	}

	//----------------------------------------------------------------------------

	bool	SCompileArg::SetShaderCombination(u32 combination)
	{
		if (!PK_VERIFY(m_Material != null))
			return false;
		// We enable the right options:
		u32		optionIdx = 0;
		for (u32 i = 0; i < m_FeaturesSettings.Count(); ++i)
		{
			if (!m_FeaturesSettings[i].m_Mandatory)
			{
				m_FeaturesSettings[i].m_Enabled = (combination & (1 << optionIdx)) != 0;
				++optionIdx;
			}
		}
		// Then we gather fields and properties:
		for (u32 i = 0; i < m_FeaturesSettings.Count(); ++i)
		{
			if (!m_FeaturesSettings[i].m_Enabled)
				continue;
			const CStringId				featureNameId = m_FeaturesSettings[i].m_FeatureName;
			const CString				featureName = featureNameId.ToString();
			PCParticleRendererFeature	feature = m_Material->GetFeature(featureName);
			if (!PK_VERIFY(feature != null))
				continue;

			// Skip all geometry generation features: These do not produce direct shader inputs as vertex streams.
			// We hardcode geom generation in the shader generation pipe.
			if (_IsGeometryGenerationFeature(featureName))
				continue;

			//	Temp: proper fix need to be done inside shader tool
			if (_IsMeshAtlasFeature(featureName))
				continue;

			for (u32 pidx = 0; pidx < feature->Properties().Count(); ++pidx)
			{
				const CParticleNodeTemplateExport	*exportNode = feature->Properties()[pidx];
				if (!PK_VERIFY(exportNode != null))
					continue;

				// This code matches the logic in 'CParticleNodeRendererBase::_FillMaterialDataInRendererFrontend'
				const Nodegraph::EDataType			exportedType = exportNode->ExportedType();
				const Nodegraph::SDataTypeTraits	&typeTraits = Nodegraph::SDataTypeTraits::Traits(exportedType);
				if (exportedType != Nodegraph::DataType_Bool1 &&
					exportedType != Nodegraph::DataType_Int1 &&
					exportedType != Nodegraph::DataType_Int2 &&
					exportedType != Nodegraph::DataType_Int3 &&
					exportedType != Nodegraph::DataType_Int4 &&
					exportedType != Nodegraph::DataType_Float1 &&
					exportedType != Nodegraph::DataType_Float2 &&
					exportedType != Nodegraph::DataType_Float3 &&
					exportedType != Nodegraph::DataType_Float4 &&
					exportedType != Nodegraph::DataType_Orientation &&
					exportedType != Nodegraph::DataType_DataImage &&
					exportedType != Nodegraph::DataType_DataImageAtlas &&
					exportedType != Nodegraph::DataType_DataGeometry &&
					exportedType != Nodegraph::DataType_DataAudio)
				{
					CLog::Log(PK_ERROR, "MaterialToRHI: rendering feature property '%s' of feature '%s' has an unsupported type '%s'.", exportNode->ExportedName().Data(), featureName.Data(), typeTraits.Name());
					return false;
				}

				// Gore Hack ! See next comment.
				const CStringView	kSortKey = "Transparent.SortKey";
				if (featureNameId == BasicRendererProperties::SID_Transparent() &&
					exportNode->ExportedName() == kSortKey)
					continue;

				// Note (Alex): FIXME, We can't base everything on link and property, or skip by feature.
				//   We need a clear distinction between Billboarding fields and Rendering fields (as additional shader input).
				if (exportNode->InputType() == InputType_Link_And_Property &&
					typeTraits.IsNative())	// it's a data field
				{
					const CStringId		pinName = CStringId(exportNode->ExportedName());

					// FIXME: Check for multiple identical fields?
					if (!PK_VERIFY(m_Fields.PushBack().Valid()))
						return false;
					m_Fields.Last().SetValueFrom(pinName, Nodegraph::SDataTypeTraits::Traits(exportNode->ExportedType()).BaseType());
					continue;
				}

				// FIXME: Check for multiple identical fields?
				if (!PK_VERIFY(m_Properties.PushBack().Valid()))
					return false;
				m_Properties.Last().SetupFrom(exportNode);
			}
		}
		// Finally we deduce the render passes from the enabled features:
		_FindParticleRenderPasses();
		return true;
	}

	//----------------------------------------------------------------------------

	bool	SCompileArg::_HasProperty(CStringId materialProperty) const
	{
		for (u32 i = 0; i < m_FeaturesSettings.Count(); ++i)
		{
			if (m_FeaturesSettings[i].m_FeatureName == materialProperty)
				return m_FeaturesSettings[i].m_Enabled;
		}
		return false;
	}

	//----------------------------------------------------------------------------

	void	SCompileArg::_FindParticleRenderPasses()
	{
		FindParticleRenderPasses(m_RenderPasses, m_RendererType, FastDelegate<bool(CStringId)>(this, &SCompileArg::_HasProperty));
	}
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)

	//----------------------------------------------------------------------------

	void	FindParticleRenderPasses(CRenderPassArray &renderPasses, ERendererClass renderer, const FastDelegate<bool(CStringId)> &hasProperty)
	{
		if (renderer == Renderer_Light)
		{
			renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_Lighting);
		}
		else if (renderer == Renderer_Decal)
		{
			renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_Decal);
		}
		else
		{
			const u32	renderPassCount = renderPasses.Count();
			const bool	hasDiffuseColor = hasProperty(BasicRendererProperties::SID_Diffuse());
			const bool	hasEmissiveColor = hasProperty(BasicRendererProperties::SID_Emissive());
			const bool	hasOpaque = hasProperty(BasicRendererProperties::SID_Opaque());
			const bool	hasLit = hasProperty(BasicRendererProperties::SID_Lit());
			const bool	hasTint = hasProperty(BasicRendererProperties::SID_Tint());
			const bool	hasDistortion = hasProperty(BasicRendererProperties::SID_Distortion());
			const bool	needTransparentLighting = hasLit && (hasTint || hasDistortion);
			const bool	hasTransparentColors = !hasOpaque && (hasDiffuseColor || hasEmissiveColor);
			const bool	hasTransparent = hasTransparentColors || needTransparentLighting || (!hasOpaque && !hasTint && !hasDistortion);

			if (hasTransparent)
			{
				if (hasTint || hasDistortion)
					renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_TransparentPostDisto);
				else
					renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_Transparent);
			}
			if (hasOpaque)
			{
				renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_Opaque);
				renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_OpaqueShadow);
			}
			if (hasTint)
				renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_Tint);
			if (hasDistortion)
				renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_Distortion);
			if (renderPassCount == renderPasses.Count())
			{
				// By default, materials are rendered in the transparent render pass:
				renderPasses.PushBack(ESampleLibGraphicResources_RenderPass::ParticlePass_Transparent);
			}
		}
	}

	//----------------------------------------------------------------------------

	bool	PopulateSettings(	const PCParticleRendererMaterial	&material,
								const ERendererClass				rendererType,
								const PCRendererDataBase			renderer /*= null*/,
								TArray<SToggledRenderingFeature>	*featuresSettings /*= null*/,
								SMaterialSettings					*materialSettings /*= null*/)
	{
		(void)rendererType;
		PK_ASSERT(material != null);
		if (featuresSettings != null)
		{
			PCRHIRenderingSettings		renderingSettings;
			PCBaseObjectFile			renderingInterfaceFile;

			for (u32 i = 0; i < material->RendererFeatures().Count(); ++i)
			{
				const CParticleRendererFeatureDesc	*featureDesc = material->RendererFeatures()[i];
				if (!PK_VERIFY(featureDesc != null))
					continue;
				const CString	&featureName = featureDesc->RendererFeatureName();
				bool			setFeatureMandatory = featureDesc->Mandatory();
				bool			addFeatureToList = setFeatureMandatory;
				bool			setFeatureEnabled = addFeatureToList;

				if (!addFeatureToList)
				{
					if (renderer != null)
					{
						// In the case where we have an actual renderer, we can add only the features that are actually enabled to the featuresSettings:
						PK_ASSERT(renderer->m_RendererType == rendererType);
						const CStringId	propertyKey = CStringId(featureName);
						const SRendererFeaturePropertyValue	*property = renderer->m_Declaration.FindProperty(propertyKey);
						// This happens when a feature in the pkma is not present in the baked pkfx. The renderer pins should be repopulated when baking...
						if (!PK_VERIFY(property != null))
							continue;
						PK_ASSERT(property->m_Type == PropertyType_Feature);
						setFeatureEnabled = addFeatureToList = property->ValueB();
					}
					else
					{
						addFeatureToList = true;
						setFeatureEnabled = false;
					}
				}

				if (addFeatureToList)
				{
					PCRHIRenderingFeature	renderingFeature = null;
					if (featureDesc->RendererInterfaceFile() != null)
					{
						// Avoids searching everytime the settings from HBO list of the same file
						if (featureDesc->RendererInterfaceFile() != renderingInterfaceFile)
						{
							renderingInterfaceFile = featureDesc->RendererInterfaceFile();
							renderingSettings = renderingInterfaceFile->FindFirstOf<CRHIRenderingSettings>();
						}
						renderingFeature = renderingSettings != null ? renderingSettings->FindFeature(featureName) : null;
					}
					if (!PK_VERIFY(featuresSettings->PushBack(SToggledRenderingFeature(CStringId(featureName), renderingFeature, setFeatureMandatory, setFeatureEnabled)).Valid()))
						return false;
				}
			}
		}
		if (materialSettings != null && material->File() != null)
		{
			materialSettings->m_MaterialName = CFilePath::StripExtension(CFilePath::ExtractFilename(CStringView(material->File()->Path()))).ToString();
			PCRHIMaterialShaders	materialDesc = material->File()->FindFirstOf<CRHIMaterialShaders>();
			if (materialDesc != null)
			{
				materialSettings->m_VertexShaderPath = materialDesc->VertexShader();
				materialSettings->m_FragmentShaderPath = materialDesc->FragmentShader();
			}
		}
		return true;
	}

	//----------------------------------------------------------------------------

#if defined(PK_LINUX) && ((defined(PK_COMPILER_GCC) && PK_COMPILER_VERSION >= 7000) || defined(PK_COMPILER_CLANG))
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

	static void	_PropertyTypeConversion(ERendererFeaturePropertyType inType, RHI::EVarType &outType, u32 &outSize)
	{
		outType = RHI::TypeUndefined;
		outSize = 0;

		switch (inType)
		{
#define X_CASE_TYPE(_type, _rhi_type) \
	case PK_NAMESPACE::PropertyType_ ## _type: \
		outType = RHI::Type ## _rhi_type; \
		outSize = sizeof(C ## _type); \
		break;

		case PK_NAMESPACE::PropertyType_Bool1:
			X_CASE_TYPE(Int1, Int);
			X_CASE_TYPE(Int2, Int2);
			X_CASE_TYPE(Int3, Int3);
			X_CASE_TYPE(Int4, Int4);
			X_CASE_TYPE(Float1, Float);
			X_CASE_TYPE(Float2, Float2);
			X_CASE_TYPE(Float3, Float3);
			X_CASE_TYPE(Float4, Float4);

		default:
			PK_ASSERT_NOT_REACHED();

#undef X_CASE_TYPE
		}

		PK_ASSERT(outType != RHI::TypeUndefined);
	}

#if defined(PK_LINUX) && ((defined(PK_COMPILER_GCC) && PK_COMPILER_VERSION >= 7000) || defined(PK_COMPILER_CLANG))
#	pragma GCC diagnostic pop
#endif

	//----------------------------------------------------------------------------

	// Replace '.' and other unrecognized tokens by '_'
	static CString	_SanitizeShaderVisibleName(const CString &baseName)
	{
		if (!PK_VERIFY(!baseName.Empty()))
			return null;

		// First, quickly scan through the string to see if we'll need to change it.
		// If we don't, there's no need to copy the string (calling 'RawDataForWriting()' will detach the string and make a copy)
		{
			const char	*readPtr = baseName.Data();
			PK_ASSERT(readPtr != null && *readPtr != '\0');	// '.Empty()' test at the beginning of the function
			bool	ok = !!KR_BUFFER_IS_IDST(*readPtr);
			++readPtr;
			while (*readPtr != '\0')
			{
				if (!KR_BUFFER_IS_IDSTNUM(*readPtr))
				{
					ok = false;
					break;
				}
				++readPtr;
			}
			if (ok)	// nothing to change here !
				return baseName;
		}
		CString		name = baseName;

		{
			char	*writePtr = name.RawDataForWriting();	// Detach
			PK_ASSERT(writePtr != null && *writePtr != '\0');	// '.Empty()' test at the beginning of the function

			const char	kReplace = '_';
			bool		prevIsOriginal = true;	// condense the '_' to keep the string readable.

			if (!KR_BUFFER_IS_IDST(*writePtr))
			{
				*writePtr = kReplace;
				prevIsOriginal = false;
			}
			writePtr++;

			const char	*readPtr = writePtr;
			while (*readPtr != '\0')
			{
				if (!KR_BUFFER_IS_IDSTNUM(*readPtr))
				{
					*writePtr = kReplace;
					if (prevIsOriginal)
						writePtr++;
					prevIsOriginal = false;
				}
				else
				{
					prevIsOriginal = true;
					*writePtr = *readPtr;
					writePtr++;
				}
				readPtr++;
			}
			*writePtr = '\0';
		}

		name.RebuildAfterRawModification();	// not needed, we didn't change the length
		return name;
	}

	//----------------------------------------------------------------------------
	//
	//	Front-end material to shader bindings, used in the integration to create the render state - Helpers:
	//
	//----------------------------------------------------------------------------

	// Helpers to create the inputs for the fields and the constants:
	static bool		_SetFields(	const SGenericArgs					&args,
								RHI::SShaderBindings				&outShaderBindings,
								RHI::SConstantSetLayout				*simDataConstantSet, // can be null
								RHI::SConstantSetLayout				*streamOffsetsConstantSet, // can be null
								TArray<RHI::SVertexInputBufferDesc>	&outVertexInputBuffer,
								RHI::SShaderDescription				*outShaderDesc,
								RHI::EVertexInputRate				inputRate, // outShaderDesc can be null
								EShaderOptions						options)
	{
		const bool	gpuStorage = (options & Option_GPUStorage) ||
								 (options & Option_GPUMesh);
		const bool	vertexBB =	(options & Option_VertexBillboarding) ||
								(options & Option_TriangleVertexBillboarding) ||
								(options & Option_RibbonVertexBillboarding);
		const bool	GPUMesh = options & Option_GPUMesh;
		PK_ASSERT(!vertexBB || simDataConstantSet != null);
		PK_ASSERT(!vertexBB || !gpuStorage || streamOffsetsConstantSet != null);

		bool	success = true;
		u32		attributeLocationBinding = 0;
		for (const RHI::SVertexAttributeDesc &inputAttribute : outShaderBindings.m_InputAttributes)
			attributeLocationBinding += RHI::VarType::GetRowNumber(inputAttribute.m_Type);

		for (u32 fidx = 0; fidx < args.m_FieldDefinitions.Count(); ++fidx)
		{
			const SRendererFeatureFieldDefinition	&fieldDef = args.m_FieldDefinitions[fidx];
			PK_ASSERT(fieldDef.m_Type != BaseType_Evolved);

			// Here, fieldDef.m_Name is in the 'General.Position' / 'Diffuse.Color' form
			// Replace '.' with '_'
			const CString		fieldnameSanitized = _SanitizeShaderVisibleName(fieldDef.m_Name.ToString());

			// If already here, don't add it again (added by hardcoded '_SetGeneratedInputs' function)
			PK_ONLY_IF_ASSERTS({
				for (u32 iidx = 0; iidx < outShaderBindings.m_InputAttributes.Count(); iidx++)
					PK_ASSERT(outShaderBindings.m_InputAttributes[iidx].m_Name != fieldnameSanitized);
			});

			if (fieldDef.m_Type == BaseType_Bool || fieldDef.m_Type == BaseType_Bool2 || fieldDef.m_Type == BaseType_Bool3 || fieldDef.m_Type == BaseType_Bool4)
				continue; // ignore boolean fields ...

			// TEMP: triangle custom normals / UVs only needed in vertex shader for vertexBB
			if (_IsTriangleGeomGenerationFeatureProperty(fieldnameSanitized) && !vertexBB)
				continue;

			// MeshLOD.LOD doesn't need to be visible in the VS shader (it's not a renderer builtin so it ends up here).
			if (_IsMeshLODProperty(fieldnameSanitized))
				continue;

			const RHI::EVarType	varType = RHI::VarType::RuntimeBaseTypeToVarType(fieldDef.m_Type);
			if (!vertexBB)
			{
				if (GPUMesh)
				{
					success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc(CString::Format("%ssOffsets", fieldnameSanitized.Data())));
				}
				else
				{
					success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc(fieldnameSanitized, attributeLocationBinding, varType, outShaderBindings.m_InputAttributes.Last().m_BufferIdx + 1)).Valid();
					success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(inputRate, RHI::VarType::GetTypeSize(varType))).Valid();
				}
			}
			else
			{
				if (gpuStorage)
					success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc(CString::Format("%ssOffsets", fieldnameSanitized.Data())));
				else
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc(CString::Format("%ss", fieldnameSanitized.Data())));
			}

			if (!_IsTriangleGeomGenerationFeatureProperty(fieldnameSanitized))
			{
				if (outShaderDesc != null)
				{
					const RHI::EInterpolation	interpolationMode =  (args.m_RendererType == Renderer_Ribbon) ? RHI::InterpolationSmooth : RHI::InterpolationFlat;
					CString	fragmentName = "frag" + fieldnameSanitized;
					if (outShaderDesc->m_Pipeline == RHI::VsGsPs)
					{
						success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput(fragmentName, varType, interpolationMode, outShaderDesc->m_VertexOutput.Count())).Valid();
						fragmentName = "geom" + fieldnameSanitized;
					}
					success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput(fragmentName, varType, interpolationMode, attributeLocationBinding)).Valid();
				}
			}

			attributeLocationBinding += RHI::VarType::GetRowNumber(varType);
		}
		return success;
	}

	//----------------------------------------------------------------------------

	const SRendererFeaturePropertyValue	*_FindProperty(const CStringId &name, const TMemoryView<const SRendererFeaturePropertyValue> &view)
	{
		for (u32 i = 0; i < view.Count(); ++i)
		{
			if (view[i].m_Name == name)
				return &view[i];
		}
		return null;
	}

	//----------------------------------------------------------------------------

	bool	CreateConstantSetLayout(const SGenericArgs								&args,
									const CString									&constantBuffName,
									RHI::SConstantSetLayout							&outConstSetLayout,
									TArray<STextureProperty>						*outTextureArray,
									TArray<SRendererFeaturePropertyValue>			*outConstantArray,
									CDigestMD5										*contentHash)
	{
		bool				success = true;
		CBufferDigesterMD5	digester;
		// Then we can add the constant inputs: we will have one constant set and one constant buffer per feature
		RHI::SConstantBufferDesc	constBufferDesc(constantBuffName);

		for (const SToggledRenderingFeature &feature : args.m_FeaturesSettings)
		{
			if (!feature.m_Enabled || feature.m_Settings == null)
				continue;
			for (const CString &propName : feature.m_Settings->PropertiesAsShaderConstants())
			{
				// Warning: This is inconsistent with renderer pins, which are CategoryName.PinName
				const CString						featureName = feature.m_FeatureName.ToString();
				const CString						propertyName = CString::Format("%s.%s", featureName.Data(), propName.Data());
				const CString						shaderVisibleName = CString::Format("%s_%s", featureName.Data(), propName.Data()); // Some properties have common names (ie. Blending)
				const SRendererFeaturePropertyValue	*value = _FindProperty(CStringId(propertyName), args.m_Properties);
				if (value != null)
				{
					if (value->m_Type == PropertyType_TexturePath)
					{
						outConstSetLayout.AddConstantsLayout(RHI::SConstantSamplerDesc(shaderVisibleName, RHI::SamplerTypeSingle));
						if (outTextureArray != null)
						{
							STextureProperty	texProp;
							texProp.m_Property = value;
							texProp.m_LoadAsSRGB = feature.m_Settings->TexturesUsedAsLookUp().Find(propName) == null;
							outTextureArray->PushBack(texProp);
						}
					}
					else
					{
						RHI::EVarType	varType = RHI::TypeUndefined;
						u32				varSize = 0;

						_PropertyTypeConversion(value->m_Type, varType, varSize);
						constBufferDesc.AddConstant(RHI::SConstantVarDesc(varType, shaderVisibleName));
						if (outConstantArray != null)
							outConstantArray->PushBack(*value);
						if (contentHash != null)
							digester.Append(&value->m_ValueI[0], varSize);
					}
				}
			}
		}

		if (!constBufferDesc.m_Constants.Empty())
			success &= outConstSetLayout.AddConstantsLayout(constBufferDesc);
		if (contentHash != null)
			digester.Finalize(*contentHash);

		return success;
	}

	//----------------------------------------------------------------------------
	//
	//	Create the constant set layout for the geom/vertex billboard pass:
	//
	//----------------------------------------------------------------------------

	bool	_SetGPUBillboardConstants(RHI::SShaderBindings &outShaderBindings, ERendererClass rendererType, bool vertexBB, bool gpuStorage)
	{
		if (gpuStorage)
		{
			RHI::SPushConstantBuffer	desc;
			desc.m_ShaderStagesMask = static_cast<RHI::EShaderStageMask>(RHI::VertexShaderMask | RHI::GeometryShaderMask);
			if (!SConstantDrawRequests::GetConstantBufferDesc(desc, 1, rendererType) ||
				!outShaderBindings.m_PushConstants.PushBack(desc).Valid())
				return false;
		}
		else
		{
			if (!outShaderBindings.m_ConstantSets.PushBack(SConstantDrawRequests::GetConstantSetLayout(rendererType)).Valid())
				return false;
		}
		if (vertexBB)
		{
			RHI::SPushConstantBuffer	desc;

			if (!SConstantVertexBillboarding::GetPushConstantBufferDesc(desc, gpuStorage) ||
				!outShaderBindings.m_PushConstants.PushBack(desc).Valid())
				return false;
		}
		return true;
	}

	//----------------------------------------------------------------------------
	//
	//	Create the shader inputs for the generated geometry:
	//
	//----------------------------------------------------------------------------

	bool	_CommonBillboardRibbonGeneratedInputs(	bool								uv,
													bool								atlas,
													bool								normal,
													bool								tangent,
													u32									&shaderLocationBinding,
													u32									&vBufferLocationBinding,
													RHI::SShaderBindings				&outShaderBindings,
													TArray<RHI::SVertexInputBufferDesc>	&outVertexInputBuffer,
													RHI::SShaderDescription				*outShaderDesc)
	{
		bool	success = true;
		if (normal)
		{
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Normal", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding)).Valid();
			if (outShaderDesc != null)
				success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragNormal", RHI::TypeFloat3, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
			++shaderLocationBinding;
			++vBufferLocationBinding;
			outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4)));
		}
		if (tangent)
		{
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Tangent", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
			if (outShaderDesc != null)
				success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragTangent", RHI::TypeFloat4, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
			++shaderLocationBinding;
			++vBufferLocationBinding;
			outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4)));
		}
		if (uv)
		{
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("UV0", shaderLocationBinding, RHI::TypeFloat2, vBufferLocationBinding)).Valid();
			if (outShaderDesc != null)
				success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV0", RHI::TypeFloat2, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
			++shaderLocationBinding;
			++vBufferLocationBinding;
			outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2)));
		}
		if (atlas)
		{
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("UV1", shaderLocationBinding, RHI::TypeFloat2, vBufferLocationBinding)).Valid();
			if (outShaderDesc != null)
				success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV1", RHI::TypeFloat2, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
			++shaderLocationBinding;
			++vBufferLocationBinding;
			outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2)));
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("AtlasID", shaderLocationBinding, RHI::TypeFloat, vBufferLocationBinding)).Valid();
			if (outShaderDesc != null)
				success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragAtlasID", RHI::TypeFloat, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
			++shaderLocationBinding;
			++vBufferLocationBinding;
			outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(float)));
		}

		return success;
	}

	//----------------------------------------------------------------------------

	static bool		_SetGeneratedInputs(SGenericArgs						&args,
										RHI::SShaderBindings				&outShaderBindings,
										u32									&outNeededConstants,
										RHI::SConstantSetLayout				*simDataConstantSet, // can be null
										RHI::SConstantSetLayout				*streamOffsetsConstantSet, // can be null
										TArray<RHI::SVertexInputBufferDesc>	&outVertexInputBuffer,
										RHI::SShaderDescription				*outShaderDesc,
										EShaderOptions						options) // outShaderDesc can be null
	{
		u32			shaderLocationBinding = 0;
		u32			vBufferLocationBinding = 0;
		bool		success = true;
		const bool	geomBB = options & Option_GeomBillboarding;
		const bool	vertexBB =	(options & Option_VertexBillboarding) ||
								(options & Option_TriangleVertexBillboarding) ||
								(options & Option_RibbonVertexBillboarding);
		const bool	gpuStorage = (options & Option_GPUStorage) ||
								 (options & Option_GPUMesh);
		const bool	gpuSortCamera = options & Option_GPUSort;
		const bool	gpuSortRibbon = (args.m_RendererType == Renderer_Ribbon) && vertexBB && gpuStorage;

		PK_ASSERT(!vertexBB || simDataConstantSet != null);
		PK_ASSERT(!vertexBB || !gpuStorage || streamOffsetsConstantSet != null);

		// TODO: Clean/split this function..

		// Get needed features
		bool	uv = false;
		bool	normal = false;
		bool	tangent = false;
		bool	uv1 = false;
		bool	atlas = false;
		bool	opaque = false;
		bool	vertexColor0 = false;
		bool	vertexColor1 = false;
		bool	vertexBonesInfo = false;
		bool	correctDeformation = false;
		bool	sampleDepth = false;
		bool	sampleNormalRoughMetal = false;
		bool	sampleDiffuse = false;
		bool	useLightingInfo = false;
//		bool	transformUVs = false;
		bool	customTextureU = false;

		const CStringId		strIdFeature_CorrectDeformation = BasicRendererProperties::SID_CorrectDeformation();
		const CStringId		strIdFeature_Atlas = BasicRendererProperties::SID_Atlas();
//		const CStringId		strIdFeature_TransformUVs = BasicRendererProperties::SID_TransformUVs();
		const CStringId		strIdFeature_CustomTextureU = BasicRendererProperties::SID_CustomTextureU();
		const CStringId		strIdFeature_Opaque = BasicRendererProperties::SID_Opaque();

		for (const SToggledRenderingFeature &settings : args.m_FeaturesSettings)
		{
			if (!settings.m_Enabled)
				continue;

			const PCRHIRenderingFeature	&featureSettings = settings.m_Settings;

			if (featureSettings == null)
				continue;

			correctDeformation |= settings.m_FeatureName == strIdFeature_CorrectDeformation;
			atlas |= settings.m_FeatureName == strIdFeature_Atlas;
			opaque |= settings.m_FeatureName == strIdFeature_Opaque;
//			transformUVs |= settings.m_FeatureName == strIdFeature_TransformUVs;
			customTextureU |= settings.m_FeatureName == strIdFeature_CustomTextureU;

			uv |= !!featureSettings->UseUV();
			normal |= !!featureSettings->UseNormal();
			tangent |= !!featureSettings->UseTangent();
			uv1 |= !!featureSettings->UseMeshUV1();
			vertexColor0 |= !!featureSettings->UseMeshVertexColor0();
			vertexColor1 |= !!featureSettings->UseMeshVertexColor1();
			vertexBonesInfo |= !!featureSettings->UseMeshVertexBonesIndicesAndWeights();
			sampleDepth |= !!featureSettings->SampleDepth();
			sampleNormalRoughMetal |= !!featureSettings->SampleNormalRoughMetal();
			sampleDiffuse |= !!featureSettings->SampleDiffuse();
			useLightingInfo |= !!featureSettings->UseSceneLightingInfo();
		}

		// Create the shader vertex inputs and outputs
		if (vertexBB)
		{
			if (gpuStorage)
			{
				PK_ASSERT(args.m_RendererType == Renderer_Billboard || args.m_RendererType == Renderer_Ribbon);

				// GPU sim: only a single raw buffer, offsets per stream, and define "BB_GPU_SIM"
				success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData"));
				success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_GPU_SIM")).Valid();
				// Mandatory stream offsets
				success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("EnabledsOffsets"));
				success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("PositionsOffsets"));
				// Ribbon mandatory stream offsets
				if (gpuSortRibbon)
					success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("ParentIDsOffsets"));

				// Sort indirection buffers
				if (gpuSortCamera)
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Indirection"));
				if (gpuSortRibbon)
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("RibbonIndirection"));

				// For ribbon we need the effective particle count contained in the indirect draw buffer.
				if (gpuSortRibbon)
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("IndirectDraw"));
			}
			else
			{
				success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Indices")); // Right now, always add the indices raw buffer. Later, shader permutation
				if (args.m_RendererType == Renderer_Billboard)
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Positions"));
				else if (args.m_RendererType == Renderer_Triangle)
				{
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("VertexPosition0"));
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("VertexPosition1"));
					success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("VertexPosition2"));
				}
			}
			if (args.m_RendererType != Renderer_Triangle)
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("TexCoords", shaderLocationBinding, RHI::TypeFloat2, vBufferLocationBinding)).Valid();
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2))).Valid();
			}
		}
		else
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Position", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding)).Valid();

		if (!geomBB && outShaderDesc != null)
		{
			success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragWorldPosition", RHI::TypeFloat3, RHI::InterpolationSmooth)).Valid();
			success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragViewProjPosition", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
		}

		if (!vertexBB)
		{
			if (geomBB && !gpuStorage)
			{
				++shaderLocationBinding;
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("DrawRequestID", shaderLocationBinding, RHI::TypeFloat, vBufferLocationBinding, sizeof(CFloat3))).Valid();
			}
			const bool	vertexPositionFloat3 = (args.m_RendererType == Renderer_Mesh || args.m_RendererType == Renderer_Light || args.m_RendererType == Renderer_Decal || gpuStorage);
			success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, vertexPositionFloat3 ? sizeof(CFloat3) : sizeof(CFloat4))).Valid();
		}
		++shaderLocationBinding;
		++vBufferLocationBinding;

		if (outShaderDesc != null)
		{
			PK_FOREACH(feature, args.m_FeaturesSettings)
			{
				if (feature->m_Enabled)
					outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::AllShaderMask, CString::Format("HAS_%s", feature->m_FeatureName.ToStringData())));
			}
		}

		if (args.m_RendererType == Renderer_Billboard)
		{
			u32	vOutLocationBinding = shaderLocationBinding;
			if (geomBB || vertexBB) // Lv0, all geom billboards have those inputs
			{
				const RHI::EShaderStage	shaderStage = geomBB ? RHI::GeometryShaderStage : RHI::VertexShaderStage;
				const bool				hasAtlas = atlas;// && (options & Option_BillboardAtlas);

				// Size (float) / SizeFloat2 (float2) -> geomSize (float2) -> POSITION
				RHI::EVarType	sizeType = RHI::TypeFloat;
				u32				sizeStride = sizeof(float);

				if (options & Option_BillboardSizeFloat2)
				{
					sizeType = RHI::TypeFloat2;
					sizeStride = sizeof(CFloat2);
					outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(shaderStage, "HAS_SizeFloat2"));
				}
				if (geomBB)
				{
					success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Size", shaderLocationBinding, sizeType, vBufferLocationBinding)).Valid();
					success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeStride)).Valid();
					++shaderLocationBinding;
					++vBufferLocationBinding;
					if (outShaderDesc != null)
					{
						success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("geomSize", sizeType, RHI::InterpolationSmooth, vOutLocationBinding)).Valid();
						++vOutLocationBinding;
					}
				}
				else
				{
					if (gpuStorage)
					{
						if (sizeType == RHI::TypeFloat2)
							success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Size2sOffsets"));
						else
							success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("SizesOffsets"));
					}
					else
					{
						if (sizeType == RHI::TypeFloat2)
							success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Size2s"));
						else
							success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Sizes"));
					}
				}
				// Rotation -> geomRotation -> POSITION
				// Warning: EShaderOptions operators & | are overloaded !
				const bool	isC0 = !(options & Option_Axis_C1) && !(options & Option_Axis_C2);
				const bool	isC2 = options & Option_Axis_C2;
				if (isC0 || isC2)
				{
					if (geomBB)
					{
						success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Rotation", shaderLocationBinding, RHI::TypeFloat, vBufferLocationBinding)).Valid();
						success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(float))).Valid();
						++shaderLocationBinding;
						++vBufferLocationBinding;
						if (outShaderDesc != null)
						{
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("geomRotation", RHI::TypeFloat, RHI::InterpolationSmooth, vOutLocationBinding)).Valid();
							++vOutLocationBinding;
						}
					}
					else
					{
						if (gpuStorage)
							success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("RotationsOffsets"));
						else
							success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Rotations"));
					}
				}
				if (uv && hasAtlas)
					success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(shaderStage, "BB_Feature_Atlas")).Valid();
				success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(shaderStage, "BB_FeatureC0")).Valid();
				if (outShaderDesc != null)
				{
					if (geomBB)
					{
						outShaderDesc->m_GeometryOutput.m_PrimitiveType = RHI::DrawModeTriangleStrip;
						outShaderDesc->m_GeometryOutput.m_MaxVertices = 6; // because of velocity capsule align
						success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragWorldPosition", RHI::TypeFloat3, RHI::InterpolationSmooth)).Valid();
						success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragViewProjPosition", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
						if (normal)
							success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragNormal", RHI::TypeFloat3, RHI::InterpolationSmooth)).Valid();
						if (tangent)
							success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragTangent", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
						if (uv)
							success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragUV0", RHI::TypeFloat2, RHI::InterpolationSmooth)).Valid();
						if (atlas)
						{
							success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragUV1", RHI::TypeFloat2, RHI::InterpolationSmooth)).Valid();
							success &= outShaderDesc->m_GeometryOutput.m_GeometryOutput.PushBack(RHI::SVertexOutput("fragAtlasID", RHI::TypeFloat, RHI::InterpolationSmooth)).Valid();
						}
					}
					else
					{
						if (normal)
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragNormal", RHI::TypeFloat3, RHI::InterpolationSmooth)).Valid();
						if (tangent)
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragTangent", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
						if (uv)
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV0", RHI::TypeFloat2, RHI::InterpolationSmooth)).Valid();
						if (atlas)
						{
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV1", RHI::TypeFloat2, RHI::InterpolationSmooth)).Valid();
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragAtlasID", RHI::TypeFloat, RHI::InterpolationSmooth)).Valid();
						}
					}
				}

				if ((options & Option_Axis_C1) != 0)
				{
					// Axis0 -> geomAxis0 -> POSITION
					if (geomBB)
					{
						success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Axis0", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding)).Valid();
						success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3))).Valid();
						++shaderLocationBinding;
						++vBufferLocationBinding;
						if (outShaderDesc != null)
						{
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("geomAxis0", RHI::TypeFloat3, RHI::InterpolationSmooth, vOutLocationBinding)).Valid();
							++vOutLocationBinding;
						}
					}
					else
					{
						if (gpuStorage)
							success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Axis0sOffsets"));
						else
							success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Axis0s"));
					}
					success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(shaderStage, "BB_FeatureC1")).Valid();
				}
				if ((options & Option_Axis_C1) && (options & Option_Capsule))
					outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(shaderStage, "BB_FeatureC1_Capsule"));
				if ((options & Option_Axis_C2) != 0)
				{
					// Axis1 -> geomAxis1 -> POSITION
					if (geomBB)
					{
						success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Axis1", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding)).Valid();
						success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3))).Valid();
						++shaderLocationBinding;
						++vBufferLocationBinding;
						if (outShaderDesc != null)
						{
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("geomAxis1", RHI::TypeFloat3, RHI::InterpolationSmooth, vOutLocationBinding)).Valid();
							++vOutLocationBinding;
						}
					}
					else
					{
						if (gpuStorage)
							success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Axis1sOffsets"));
						else
							success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Axis1s"));
					}
					success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(shaderStage, "BB_FeatureC2")).Valid();
				}

				PK_ASSERT(!gpuStorage || geomBB || vertexBB); // when gpuStorage is true, geomBB/vertexBB should be true.
				if (gpuStorage && geomBB)
				{
					success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Enabled", shaderLocationBinding, RHI::TypeUint, vBufferLocationBinding)).Valid();
					success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(u32))).Valid();
					++shaderLocationBinding;
					++vBufferLocationBinding;
				}
			}
			else // No geom billboard
			{
				success &= _CommonBillboardRibbonGeneratedInputs(uv, atlas, normal, tangent, shaderLocationBinding, vBufferLocationBinding, outShaderBindings, outVertexInputBuffer, outShaderDesc);
			}
		}
		else if (args.m_RendererType == Renderer_Ribbon)
		{
			if (vertexBB)
			{
				const bool	isC1 = options & Option_Axis_C1;
				success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_FeatureC0")).Valid();

				if (gpuStorage)
				{
					success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("SizesOffsets"));
					if (isC1)
					{
						success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Axis0sOffsets"));
						success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_FeatureC1")).Valid();
					}
					if (outShaderDesc != null)
					{
						if (normal)
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragNormal", RHI::TypeFloat3, RHI::InterpolationSmooth)).Valid();
						if (tangent)
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragTangent", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
						if (uv)
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV0", RHI::TypeFloat2, RHI::InterpolationSmooth)).Valid();
						if (atlas)
						{
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV1", RHI::TypeFloat2, RHI::InterpolationSmooth)).Valid();
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragAtlasID", RHI::TypeFloat, RHI::InterpolationSmooth)).Valid();
						}
						if (correctDeformation)
						{
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUVScaleAndOffset", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUVFactors", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
						}
					}
					if (uv && atlas)
						success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_Feature_Atlas")).Valid();
					if (customTextureU)
						success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_Feature_CustomTextureU")).Valid();
				}
				else
				{
					// Vertex ribbon billboarding not implemented for CPU simulation
				}
			}
			else
			{
				success &= _CommonBillboardRibbonGeneratedInputs(uv, atlas, normal, tangent, shaderLocationBinding, vBufferLocationBinding, outShaderBindings, outVertexInputBuffer, outShaderDesc);
				if (correctDeformation)
				{
					success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("UVScaleAndOffset", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
					if (outShaderDesc != null)
						success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUVScaleAndOffset", RHI::TypeFloat4, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
					++shaderLocationBinding;
					++vBufferLocationBinding;
					success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))).Valid();
					success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("UVFactors", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
					if (outShaderDesc != null)
						success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUVFactors", RHI::TypeFloat4, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
					++shaderLocationBinding;
					++vBufferLocationBinding;
					success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))).Valid();
				}
			}
		}
		else if (args.m_RendererType == Renderer_Mesh)
		{
			if (normal)
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Normal", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding)).Valid();
				if (outShaderDesc != null)
					success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragNormal", RHI::TypeFloat3, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat3))).Valid();
			}
			if (tangent)
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Tangent", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
				if (outShaderDesc != null)
					success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragTangent", RHI::TypeFloat4, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))).Valid();
			}
			if (vertexColor0)
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Color0", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
				if (outShaderDesc != null)
					success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragColor0", RHI::TypeFloat4, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))).Valid();
			}
			if (uv)
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("UV0", shaderLocationBinding, RHI::TypeFloat2, vBufferLocationBinding)).Valid();
				if (outShaderDesc != null)
					success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV0", RHI::TypeFloat2, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2))).Valid();
				if (atlas)
				{
					if (!uv1)
					{
						// Note: disable secondary uv set for meshes until this is properly implemented (#12340).
						// We could pack have fragUV0 be a float4 when atlas is enabled
						success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("UV1", shaderLocationBinding, RHI::TypeFloat2, vBufferLocationBinding)).Valid();
						if (outShaderDesc != null)
							success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV1", RHI::TypeFloat2, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
						++shaderLocationBinding;
						++vBufferLocationBinding;
						outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2)));
					}

					success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "BB_Feature_Atlas")).Valid();
				}
			}
			if (vertexColor1)
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("Color1", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
				if (outShaderDesc != null)
					success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragColor1", RHI::TypeFloat4, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))).Valid();
			}
			if (uv1)
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("UV1", shaderLocationBinding, RHI::TypeFloat2, vBufferLocationBinding)).Valid();
				if (outShaderDesc != null)
					success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV1", RHI::TypeFloat2, RHI::InterpolationSmooth, shaderLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat2))).Valid();

				// To remove once #12340 is fixed.
				success &= outShaderBindings.m_Defines.PushBack(RHI::SShaderDefine(RHI::VertexShaderStage, "MESH_USE_UV1")).Valid();
			}
			if (vertexBonesInfo)
			{
				// Indices:
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("BoneIds", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))).Valid();
				// Weights:
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("BoneWeights", shaderLocationBinding, RHI::TypeFloat4, vBufferLocationBinding)).Valid();
				++shaderLocationBinding;
				++vBufferLocationBinding;
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerVertexInput, sizeof(CFloat4))).Valid();
			}
			if (gpuStorage)
			{
				success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("GPUSimData"));
				success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("MeshTransforms"));
				success &= simDataConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("Indirection"));
				success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("MeshTransformsOffsets"));
				success &= streamOffsetsConstantSet->AddConstantsLayout(RHI::SRawBufferDesc("IndirectionOffsets"));
			}
			else
			{
				success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("MeshTransform", shaderLocationBinding, RHI::TypeFloat4x4, vBufferLocationBinding)).Valid();
				shaderLocationBinding += RHI::VarType::GetRowNumber(outShaderBindings.m_InputAttributes.Last().m_Type);
				success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(CFloat4x4))).Valid();
			}
		}
		else if (args.m_RendererType == Renderer_Triangle)
		{
			if (vertexBB)
			{
				if (outShaderDesc != null)
				{
					if (normal)
						success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragNormal", RHI::TypeFloat3, RHI::InterpolationSmooth)).Valid();
					if (tangent)
						success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragTangent", RHI::TypeFloat4, RHI::InterpolationSmooth)).Valid();
					if (uv)
						success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragUV0", RHI::TypeFloat2, RHI::InterpolationSmooth)).Valid();
				}
			}
			else
			{
				success &= _CommonBillboardRibbonGeneratedInputs(uv, atlas, normal, tangent, shaderLocationBinding, vBufferLocationBinding, outShaderBindings, outVertexInputBuffer, outShaderDesc);
			}
		}
		else if (args.m_RendererType == Renderer_Decal)
		{
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("DecalTransform", shaderLocationBinding, RHI::TypeFloat4x4, vBufferLocationBinding)).Valid();
			success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(CFloat4x4))).Valid();
			shaderLocationBinding += RHI::VarType::GetRowNumber(outShaderBindings.m_InputAttributes.Last().m_Type);
			++vBufferLocationBinding;
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("InverseDecalTransform", shaderLocationBinding, RHI::TypeFloat4x4, vBufferLocationBinding)).Valid();
			success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(CFloat4x4))).Valid();
			if (outShaderDesc != null)
				success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragInverseDecalTransform", RHI::TypeFloat4x4, RHI::InterpolationFlat, shaderLocationBinding)).Valid();
			shaderLocationBinding += RHI::VarType::GetRowNumber(outShaderBindings.m_InputAttributes.Last().m_Type);
			++vBufferLocationBinding;
		}
		else if (args.m_RendererType == Renderer_Light)
		{
			// We add the position for the bounding sphere vertices
			success &= outShaderBindings.m_InputAttributes.PushBack(RHI::SVertexAttributeDesc("LightPosition", shaderLocationBinding, RHI::TypeFloat3, vBufferLocationBinding)).Valid();
			if (outShaderDesc != null)
				success &= outShaderDesc->m_VertexOutput.PushBack(RHI::SVertexOutput("fragLightPosition", RHI::TypeFloat3, RHI::InterpolationFlat, shaderLocationBinding)).Valid();
			++shaderLocationBinding;
			++vBufferLocationBinding;
			success &= outVertexInputBuffer.PushBack(RHI::SVertexInputBufferDesc(RHI::PerInstanceInput, sizeof(CFloat3))).Valid();
		}
		else
		{
			PK_ASSERT_NOT_REACHED_MESSAGE("The material type is unknown");
			return false;
		}

		if (sampleDepth)
		{
			RHI::SConstantSetLayout		constSetLayout(RHI::FragmentShaderMask);
			constSetLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("DepthSampler", RHI::SamplerTypeSingle));
			if (!outShaderBindings.m_ConstantSets.PushBack(constSetLayout).Valid())
				return false;
			outNeededConstants |= NeedSampleDepth;
		}
		if (sampleNormalRoughMetal)
		{
			RHI::SConstantSetLayout		constSetLayout(RHI::FragmentShaderMask);
			constSetLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("NormalRoughMetalSampler", RHI::SamplerTypeSingle));
			if (!outShaderBindings.m_ConstantSets.PushBack(constSetLayout).Valid())
				return false;
			outNeededConstants |= NeedSampleNormalRoughMetal;
		}
		if (sampleDiffuse)
		{
			RHI::SConstantSetLayout		constSetLayout(RHI::FragmentShaderMask);
			constSetLayout.AddConstantsLayout(RHI::SConstantSamplerDesc("DiffuseSampler", RHI::SamplerTypeSingle));
			if (!outShaderBindings.m_ConstantSets.PushBack(constSetLayout).Valid())
				return false;
			outNeededConstants |= NeedSampleDiffuse;
		}
		if (useLightingInfo)
		{
			RHI::SConstantSetLayout		lightingInfo;
			RHI::SConstantSetLayout		shadowsInfo;
			RHI::SConstantSetLayout		brdfLut;
			RHI::SConstantSetLayout		envMap;
			CreateLightingSceneInfoConstantLayout(lightingInfo, shadowsInfo, brdfLut, envMap);
			if (!outShaderBindings.m_ConstantSets.PushBack(lightingInfo).Valid() ||
				!outShaderBindings.m_ConstantSets.PushBack(shadowsInfo).Valid() ||
				!outShaderBindings.m_ConstantSets.PushBack(brdfLut).Valid() ||
				!outShaderBindings.m_ConstantSets.PushBack(envMap).Valid())
				return false;
			outNeededConstants |= NeedLightingInfo;
		}
		if (atlas)
		{
			if (!outShaderBindings.m_ConstantSets.PushBack(SConstantAtlasKey::GetAtlasConstantSetLayout()).Valid())
				return false;
			outNeededConstants |= NeedAtlasInfo;
		}
		if (opaque)
		{
			if (!outShaderBindings.m_ConstantSets.PushBack(SConstantNoiseTextureKey::GetNoiseTextureConstantSetLayout()).Valid())
				return false;
			outNeededConstants |= NeedDitheringPattern;
		}
		return success;
	}

	//----------------------------------------------------------------------------
	//
	//	Create the shader inputs for the material interface:
	//
	//----------------------------------------------------------------------------

#if (PK_MAT2RHI_CAN_COMPILE != 0)
	// At runtime using pass description
	static bool		_SetOutputs(const SPassDescription			&pass,
								TArray<RHI::SFragmentOutput>	&shaderDescOutputs)
	{
		bool	success = true;
		// Gather sub pass def
		TMemoryView<const RHI::SSubPassDefinition>	subpasses = pass.m_RenderPass->GetSubPassDefinitions();
		if (!PK_VERIFY(pass.m_SubPassIdx < subpasses.Count()))
			return false;
		TMemoryView<const u32>	outputs = subpasses[pass.m_SubPassIdx].m_OutputRenderTargets.View();
		for (u32 i = 0; i < outputs.Count(); ++i)
		{
			// Get Format & extract components flags
			const RHI::EPixelFormat	format = pass.m_FrameBufferLayout[outputs[i]].m_Format;
			const RHI::EPixelFlags	flags = static_cast<RHI::EPixelFlags>(format & (RHI::FlagRGBA | RHI::FlagDS));
			success &= shaderDescOutputs.PushBack(RHI::SFragmentOutput(CString::Format("Output%u", i), flags)).Valid();
		}
		return success;
	}

	//----------------------------------------------------------------------------

	// At offline using static definition
	static bool		_SetOutputs(const TMemoryView<const RHI::EPixelFormat>	&outputs,
								TArray<RHI::SFragmentOutput>				&shaderDescOutputs)
	{
		bool	success = true;
		for (u32 i = 0; i < outputs.Count(); ++i)
		{
			// Get Format & extract components flags
			const RHI::EPixelFlags	flags = static_cast<RHI::EPixelFlags>(outputs[i] & (RHI::FlagRGBA | RHI::FlagDS));
			success &= shaderDescOutputs.PushBack(RHI::SFragmentOutput(CString::Format("Output%u", i), flags)).Valid();
		}
		return success;
	}

	//----------------------------------------------------------------------------

	bool		SetOutputs(	ESampleLibGraphicResources_RenderPass	renderPass,
							TArray<RHI::SFragmentOutput>			&shaderDescOutputs)
	{
		if (SPassDescription::s_PassDescriptions.Empty())
			return _SetOutputs(SPassDescription::s_PassOutputsFormats[renderPass], shaderDescOutputs);
		else
			return _SetOutputs(SPassDescription::s_PassDescriptions[renderPass], shaderDescOutputs);
	}
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)

	//----------------------------------------------------------------------------
	//
	//	Create the shader inputs for the material:
	//
	//----------------------------------------------------------------------------

	static bool		_SetMaterialInputsAndOutputs(	const SGenericArgs					&args,
													RHI::SShaderBindings				&outShaderBindings,
													RHI::SConstantSetLayout				*simDataConstantSet, // can be null
													RHI::SConstantSetLayout				*streamOffsetsConstantSet, // can be null
													TArray<RHI::SVertexInputBufferDesc>	&outVertexInputBuffer,
													RHI::SShaderDescription				*outShaderDesc, // outShaderDesc can be null
													EShaderOptions						options)
	{
		bool	rendererPropertiesPerInstance = args.m_RendererType == Renderer_Mesh || args.m_RendererType == Renderer_Light || args.m_RendererType == Renderer_Decal;

		if (!_SetFields(args, outShaderBindings, simDataConstantSet, streamOffsetsConstantSet, outVertexInputBuffer, outShaderDesc, rendererPropertiesPerInstance ? RHI::PerInstanceInput : RHI::PerVertexInput, options))
			return false;
		// Add constant set for scene infos
		{
			RHI::SConstantSetLayout		sceneInfoLayout;

			if (!CreateSceneInfoConstantLayout(sceneInfoLayout))
				return false;
			if (!outShaderBindings.m_ConstantSets.PushBack(sceneInfoLayout).Valid())
				return false;
		}
		// Material constant set
		{
			RHI::SConstantSetLayout		constSetLayout;

			if (!CreateConstantSetLayout(args, "Material", constSetLayout))
				return false;
			if (!constSetLayout.m_Constants.Empty() && !outShaderBindings.m_ConstantSets.PushBack(constSetLayout).Valid())
				return false;
		}
		// Should add constant set for billboarding
		const bool	vertexBB =	(options & Option_VertexBillboarding) ||
								(options & Option_TriangleVertexBillboarding) ||
								(options & Option_RibbonVertexBillboarding);
		if ((options & Option_GeomBillboarding) ||
			vertexBB ||
			(options & Option_GPUMesh))
		{
			const bool	gpuStorage =	(options & Option_GPUStorage) != 0 ||
										(options & Option_GPUMesh) != 0;
			if (!_SetGPUBillboardConstants(outShaderBindings, args.m_RendererType, vertexBB, gpuStorage))
				return false;
		}

		return true;
	}

	//----------------------------------------------------------------------------
	//
	//	Front-end material to shader bindings, used in the integration to create the render state:
	//
	//----------------------------------------------------------------------------

	bool	MaterialFrontendToShaderBindings(	const SPrepareArg					&args,
												RHI::SShaderBindings				&outShaderBindings,
												TArray<RHI::SVertexInputBufferDesc>	&outVertexInputBuffer,
												u32									&outNeededConstants,
												CDigestMD5							&outHash,
												EShaderOptions						options)
	{
		SGenericArgs	genArgs(args);

		//----------------------------------------------------------------------------
		// To avoid hashing the whole shader inputs, we combine all renderer features and hash:
		// - The renderer features
		// - The shader options
		// - The renderer type
		// - The render pass
		TSemiDynamicArray<u32, 32>	toHash;
		CBufferDigesterMD5			digester;

		if (!PK_VERIFY(toHash.PushBack(options).Valid()))
			return false;
		if (!PK_VERIFY(toHash.PushBack(genArgs.m_RendererType).Valid()))
			return false;
		for (const SToggledRenderingFeature &settings : genArgs.m_FeaturesSettings)
		{
			if (!PK_VERIFY(toHash.PushBack(settings.m_FeatureName.Id()).Valid()))
				return false;
		}
		digester.Append(toHash.RawDataPointer(), toHash.SizeInBytes());
		digester.Finalize(outHash);
		//----------------------------------------------------------------------------

		const bool					gpuStorage = (options & Option_GPUStorage) != 0;
		const bool					vertexBB =	(options & Option_VertexBillboarding) ||
												(options & Option_TriangleVertexBillboarding) ||
												(options & Option_RibbonVertexBillboarding);
		const bool					GPUMesh = (options & Option_GPUMesh) != 0;
		const bool					needSimDataConstantSet = vertexBB || GPUMesh;
		const bool					needStreamOffsetsConstantSet = (vertexBB && gpuStorage) || GPUMesh;

		RHI::SConstantSetLayout		_simDataConstantSet(RHI::VertexShaderMask);
		RHI::SConstantSetLayout		_streamOffsetsConstantSet(RHI::VertexShaderMask);
		RHI::SConstantSetLayout		*simDataConstantSet = needSimDataConstantSet ? &_simDataConstantSet : null;
		RHI::SConstantSetLayout		*streamOffsetsConstantSet = needStreamOffsetsConstantSet ? &_streamOffsetsConstantSet : null;

		if (!_SetGeneratedInputs(genArgs, outShaderBindings, outNeededConstants, simDataConstantSet, streamOffsetsConstantSet, outVertexInputBuffer, null, options) ||
			!_SetMaterialInputsAndOutputs(genArgs, outShaderBindings, simDataConstantSet, streamOffsetsConstantSet, outVertexInputBuffer, null, options))
			return false;

		// As we generate a single fragment shader no matter the billboarding mode (CPU/Vertex/Geom)
		// We add the vertex billboarding constant set layout last, making sure texture shader registers are not offset:
		// register indices are shared between textures and buffers.
		if (streamOffsetsConstantSet != null)
		{
			if (!PK_VERIFY(outShaderBindings.m_ConstantSets.PushBack(_streamOffsetsConstantSet).Valid()))
				return false;
		}
		if (simDataConstantSet != null)
		{
			if (!PK_VERIFY(outShaderBindings.m_ConstantSets.PushBack(_simDataConstantSet).Valid()))
				return false;
		}

		return true;
	}

	bool	MaterialFrontendToShaderBindings(	RHI::SShaderBindings				&outShaderBindings,
												EComputeShaderType					computeShaderType)
	{
		switch (computeShaderType)
		{
		case ComputeType_ComputeParticleCountPerMesh:
			FillComputeParticleCountPerMeshShaderBindings(outShaderBindings, false, false);
			break;
		case ComputeType_ComputeParticleCountPerMesh_MeshAtlas:
			FillComputeParticleCountPerMeshShaderBindings(outShaderBindings, true, false);
			break;
		case ComputeType_ComputeParticleCountPerMesh_LOD:
			FillComputeParticleCountPerMeshShaderBindings(outShaderBindings, false, true);
			break;
		case ComputeType_ComputeParticleCountPerMesh_LOD_MeshAtlas:
			FillComputeParticleCountPerMeshShaderBindings(outShaderBindings, true, true);
			break;
		case ComputeType_InitIndirectionOffsetsBuffer:
			FillInitIndirectionOffsetsBufferShaderBindings(outShaderBindings, false);
			break;
		case ComputeType_InitIndirectionOffsetsBuffer_LODNoAtlas:
			FillInitIndirectionOffsetsBufferShaderBindings(outShaderBindings, true);
			break;
		case ComputeType_ComputeMeshIndirectionBuffer:
			FillComputeMeshIndirectionBufferShaderBindings(outShaderBindings, false, false);
			break;
		case ComputeType_ComputeMeshIndirectionBuffer_MeshAtlas:
			FillComputeMeshIndirectionBufferShaderBindings(outShaderBindings, true, false);
			break;
		case ComputeType_ComputeMeshIndirectionBuffer_LOD:
			FillComputeMeshIndirectionBufferShaderBindings(outShaderBindings, false, true);
			break;
		case ComputeType_ComputeMeshIndirectionBuffer_LOD_MeshAtlas:
			FillComputeMeshIndirectionBufferShaderBindings(outShaderBindings, true, true);
			break;
		case ComputeType_ComputeMeshMatrices:
			FillComputeMeshMatricesShaderBindings(outShaderBindings);
			break;
		case ComputeType_ComputeSortKeys:
			FillComputeSortKeysShaderBindings(outShaderBindings, false, false);
			break;
		case ComputeType_ComputeSortKeys_CameraDistance:
			FillComputeSortKeysShaderBindings(outShaderBindings, true, false);
			break;
		case ComputeType_ComputeSortKeys_RibbonIndirection:
			FillComputeSortKeysShaderBindings(outShaderBindings, false, true);
			break;
		case ComputeType_ComputeSortKeys_CameraDistance_RibbonIndirection:
			FillComputeSortKeysShaderBindings(outShaderBindings, true, true);
			break;
		case ComputeType_SortUpSweep:
			FillSortUpSweepShaderBindings(outShaderBindings, false);
			break;
		case ComputeType_SortPrefixSum:
			FillSortPrefixSumShaderBindings(outShaderBindings);
			break;
		case ComputeType_SortDownSweep:
			FillSortDownSweepShaderBindings(outShaderBindings, false);
			break;
		case ComputeType_SortUpSweep_KeyStride64:
			FillSortUpSweepShaderBindings(outShaderBindings, true);
			break;
		case ComputeType_SortDownSweep_KeyStride64:
			FillSortDownSweepShaderBindings(outShaderBindings, true);
			break;
		case ComputeType_ComputeRibbonSortKeys:
			FillComputeRibbonSortKeysShaderBindings(outShaderBindings);
			break;
		default:
			PK_ASSERT_NOT_REACHED();
			break;
		}

		return true;
	}

	//----------------------------------------------------------------------------
	//
	// front-end material to shader desc, used offline for generating the shaders
	//
	//----------------------------------------------------------------------------

#if (PK_MAT2RHI_CAN_COMPILE != 0)
	bool	MaterialFrontendToShaderDesc(	const SCompileArg						&args,
											RHI::SShaderDescription					&outShaderDesc,
											EShaderOptions							options,
											ESampleLibGraphicResources_RenderPass	renderPass)
	{
		PK_ASSERT(args.m_RendererType != Renderer_Sound);
		// Set the shader stages used:
		outShaderDesc.m_Pipeline = ParticleShaderGenerator::GetShaderStagePipeline(options);
		if (options & Option_GeomBillboarding)
			outShaderDesc.m_DrawMode = RHI::DrawModePoint;

		TArray<RHI::SVertexInputBufferDesc>	tmp;

		const bool					gpuStorage = (options & Option_GPUStorage) != 0;
		const bool					vertexBB =	(options & Option_VertexBillboarding) ||
												(options & Option_TriangleVertexBillboarding) ||
												(options & Option_RibbonVertexBillboarding);
		const bool					GPUMesh = (options & Option_GPUMesh) != 0;
		const bool					needSimDataConstantSet = vertexBB || GPUMesh;
		const bool					needStreamOffsetsConstantSet = (vertexBB && gpuStorage) || GPUMesh;

		RHI::SConstantSetLayout		_simDataConstantSet(RHI::VertexShaderMask);
		RHI::SConstantSetLayout		_streamOffsetsConstantSet(RHI::VertexShaderMask);
		RHI::SConstantSetLayout		*simDataConstantSet = needSimDataConstantSet ? &_simDataConstantSet : null;
		RHI::SConstantSetLayout		*streamOffsetsConstantSet = needStreamOffsetsConstantSet ? &_streamOffsetsConstantSet : null;

		u32							dummyNeededConstants = 0;
		SGenericArgs				genArgs(args);
		// fill the other data:
		if (!_SetGeneratedInputs(genArgs, outShaderDesc.m_Bindings, dummyNeededConstants, simDataConstantSet, streamOffsetsConstantSet, tmp, &outShaderDesc, options) ||
			!_SetMaterialInputsAndOutputs(genArgs, outShaderDesc.m_Bindings, simDataConstantSet, streamOffsetsConstantSet, tmp, &outShaderDesc, options))
			return false;

		if (renderPass != __MaxParticlePass)
		{
			if (!SetOutputs(renderPass, outShaderDesc.m_FragmentOutput))
				return false;

			// We add the define for the current compiled render pass:
			if (!PK_VERIFY(outShaderDesc.m_Bindings.m_Defines.PushBack().Valid()))
				return false;

			const CStringView	renderPassName = s_ParticleRenderPassNames[renderPass];
			outShaderDesc.m_Bindings.m_Defines.Last() = RHI::SShaderDefine(RHI::AllShaderMask, renderPassName.ToString());
		}

		if (streamOffsetsConstantSet != null)
		{
			if (!PK_VERIFY(outShaderDesc.m_Bindings.m_ConstantSets.PushBack(_streamOffsetsConstantSet).Valid()))
				return false;
		}
		if (simDataConstantSet != null)
		{
			if (!PK_VERIFY(outShaderDesc.m_Bindings.m_ConstantSets.PushBack(_simDataConstantSet).Valid()))
				return false;
		}

		return true;
	}
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)

	//----------------------------------------------------------------------------
	//
	// Generate and compile the shaders from a HBO
	//
	//----------------------------------------------------------------------------

#if (PK_MAT2RHI_CAN_COMPILE != 0)
	bool	CompileParticleShadersFromMaterial(	const SCompileArg						&args,
												const SShaderCompilationSettings		&settings,
												HBO::CContext							*context)
	{
		PK_ASSERT(args.m_RendererType != Renderer_Sound);
		CMaterialCompilation	compilation(args, settings, context);
		if (compilation.BuildShaderFilesList())
			return compilation.Exec();
		return false;
	}
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)

	//----------------------------------------------------------------------------

	CString	GetHashedShaderName(const char											*pathPrefix,
								const char											*path,
								const CString										&materialName,
								const char											*specialization,
								const TMemoryView<const SToggledRenderingFeature>	&features,
								const CStringView									&permsSuffix,
								ESampleLibGraphicResources_RenderPass				renderPass)
	{
		CBufferDigesterMD5	digester;
		CDigestMD5			hashValue;

		if (pathPrefix != null)
			digester.Append(pathPrefix, SNativeStringUtils::Length(pathPrefix));
		if (path != null)
			digester.Append(path, SNativeStringUtils::Length(path));
		if (specialization != null)
			digester.Append(specialization, SNativeStringUtils::Length(specialization));

		digester.Append(materialName.Data(), materialName.Length()); // ie. Default_Billboard

		for (u32 i = 0; i < features.Count(); ++i)
		{
			if (features[i].m_Enabled)
			{
				const CString	&featureName = features[i].m_FeatureName.ToString();
				digester.Append(featureName.Data(), featureName.Length());
				digester.Append(0);
			}
		}

		digester.Append(permsSuffix.Data(), permsSuffix.Length()); // ie. Fwd_VC0..
		digester.Append(renderPass);

		digester.Finalize(hashValue);
		return RHI::ShaderHashToString(hashValue);
	}

	//----------------------------------------------------------------------------

	CString	RemapShaderPathNoExt(	const CString										&shaderRoot,
									const CString										&shaderPath,
									const CString										&materialName,
									RHI::EShaderStage									stage,
									ERendererClass										rendererClass,
									EShaderOptions										options,
									const TMemoryView<const SToggledRenderingFeature>	&features,
									ESampleLibGraphicResources_RenderPass				renderPass,
									CString												*outShaderPermutation)
	{
		const char		*pathPrefix = shaderPath.Empty() ? "Generated" : null;
		const char		*path = !shaderPath.Empty() ? CFilePath::ExtractFilename(shaderPath) : GetShaderExtensionStringFromStage(stage);
		const char		*specialization = null;
		if (rendererClass == ERendererClass::Renderer_Mesh && stage == RHI::VertexShaderStage)
			specialization = "Mesh_";

		char				storage[256];
		const CStringView	shaderName = ShaderOptionsUtils::GetShaderName(options, stage, storage);
		if (!PK_VERIFY(!shaderName.Empty()))
			return null;
		PK_ASSERT(shaderName.Data()[shaderName.Length()] == '\0');	// Must be null-terminated for the code below to work

		if (outShaderPermutation != null)
		{
			*outShaderPermutation = CString::Format("%s%s.%s_"
													"%s",
													pathPrefix != null ? pathPrefix : "", path == null ? "" : path, materialName.Data(),
													specialization != null ? specialization : "");

			for (const SToggledRenderingFeature &feature : features)
			{
				if (feature.m_Enabled)
				{
					*outShaderPermutation += feature.m_FeatureName.ToStringView();
					*outShaderPermutation += CStringView("_");
				}
			}
			*outShaderPermutation += s_ParticleRenderPassNames[renderPass];
			*outShaderPermutation += CStringView("_");
			*outShaderPermutation += shaderName;
		}

		return shaderRoot / GetHashedShaderName(pathPrefix, path, materialName, specialization, features, shaderName, renderPass);
	}

#if (PK_MAT2RHI_CAN_COMPILE != 0)
	//----------------------------------------------------------------------------
	//
	//	CMaterialCompilation
	//
	//----------------------------------------------------------------------------

	static const EShaderOptions	kShaderOptions_Regular[] =
	{
		Option_VertexPassThrough,
	};

	//----------------------------------------------------------------------------

	static const EShaderOptions	kShaderOptions_GeomBB[] =
	{
		Option_VertexPassThrough,

		// Basic geoms:
		Option_VertexPassThrough | Option_GeomBillboarding,
		Option_VertexPassThrough | Option_GeomBillboarding | Option_Axis_C1,
		Option_VertexPassThrough | Option_GeomBillboarding | Option_Axis_C1 | Option_Capsule,
		Option_VertexPassThrough | Option_GeomBillboarding | Option_Axis_C2,
		// Geoms size float2:
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding | Option_Axis_C1,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding | Option_Axis_C1 | Option_Capsule,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding | Option_Axis_C2,

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		// Basic geoms:
		Option_VertexPassThrough | Option_GeomBillboarding | Option_GPUStorage,
		Option_VertexPassThrough | Option_GeomBillboarding | Option_Axis_C1 | Option_GPUStorage,
		Option_VertexPassThrough | Option_GeomBillboarding | Option_Axis_C1 | Option_Capsule | Option_GPUStorage,
		Option_VertexPassThrough | Option_GeomBillboarding | Option_Axis_C2 | Option_GPUStorage,
		// Geoms size float2:
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding | Option_GPUStorage,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding | Option_Axis_C1 | Option_GPUStorage,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding | Option_Axis_C1 | Option_Capsule | Option_GPUStorage,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_GeomBillboarding | Option_Axis_C2 | Option_GPUStorage,
#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	};

	//----------------------------------------------------------------------------

	static const EShaderOptions	kShaderOptions_VertexBB[] =
	{
		Option_VertexPassThrough,

		// Basic geoms:
		Option_VertexPassThrough | Option_VertexBillboarding,
		Option_VertexPassThrough | Option_VertexBillboarding | Option_Axis_C1,
		Option_VertexPassThrough | Option_VertexBillboarding | Option_Axis_C1 | Option_Capsule,
		Option_VertexPassThrough | Option_VertexBillboarding | Option_Axis_C2,

		// Geoms size float2:
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C1,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C1 | Option_Capsule,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C2,

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		// Basic geoms:
		Option_VertexPassThrough | Option_VertexBillboarding | Option_GPUStorage,
		Option_VertexPassThrough | Option_VertexBillboarding | Option_Axis_C1 | Option_GPUStorage,
		Option_VertexPassThrough | Option_VertexBillboarding | Option_Axis_C1 | Option_Capsule | Option_GPUStorage,
		Option_VertexPassThrough | Option_VertexBillboarding | Option_Axis_C2 | Option_GPUStorage,
		// Geoms size float2:
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_GPUStorage,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C1 | Option_GPUStorage,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C1 | Option_Capsule | Option_GPUStorage,
		Option_VertexPassThrough | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C2 | Option_GPUStorage,

		// Basic geoms:
		Option_VertexPassThrough | Option_GPUSort | Option_VertexBillboarding | Option_GPUStorage,
		Option_VertexPassThrough | Option_GPUSort | Option_VertexBillboarding | Option_Axis_C1 | Option_GPUStorage,
		Option_VertexPassThrough | Option_GPUSort | Option_VertexBillboarding | Option_Axis_C1 | Option_Capsule | Option_GPUStorage,
		Option_VertexPassThrough | Option_GPUSort | Option_VertexBillboarding | Option_Axis_C2 | Option_GPUStorage,
		// Geoms size float2:
		Option_VertexPassThrough | Option_GPUSort | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_GPUStorage,
		Option_VertexPassThrough | Option_GPUSort | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C1 | Option_GPUStorage,
		Option_VertexPassThrough | Option_GPUSort | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C1 | Option_Capsule | Option_GPUStorage,
		Option_VertexPassThrough | Option_GPUSort | Option_BillboardSizeFloat2 | Option_VertexBillboarding | Option_Axis_C2 | Option_GPUStorage,

#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	};

	//----------------------------------------------------------------------------

	static const EShaderOptions	kShaderOptions_TriangleVertexBB[] =
	{
		Option_VertexPassThrough,

		Option_VertexPassThrough | Option_TriangleVertexBillboarding,
	};


	//----------------------------------------------------------------------------

	static const EShaderOptions	kShaderOptions_GPUmesh[] =
	{
		Option_VertexPassThrough,

		Option_VertexPassThrough | Option_GPUMesh,
	};

	//----------------------------------------------------------------------------

	static const EShaderOptions	kShaderOptions_RibbonVertexBB[] =
	{
		Option_VertexPassThrough,

#if (PK_PARTICLES_UPDATER_USE_GPU != 0)
		Option_VertexPassThrough | Option_GPUStorage,

		// Basic geoms:
		Option_VertexPassThrough | Option_GPUStorage | Option_RibbonVertexBillboarding,
		Option_VertexPassThrough | Option_GPUStorage | Option_RibbonVertexBillboarding | Option_Axis_C1,

		// GPU camera sort
		// Basic geoms:
		Option_VertexPassThrough | Option_GPUStorage | Option_RibbonVertexBillboarding | Option_GPUSort,
		Option_VertexPassThrough | Option_GPUStorage | Option_RibbonVertexBillboarding | Option_Axis_C1 | Option_GPUSort,

#endif // (PK_PARTICLES_UPDATER_USE_GPU != 0)
	};

	//----------------------------------------------------------------------------

	CMaterialCompilation::CMaterialCompilation(	const SCompileArg					&args,
												const SShaderCompilationSettings	&settings,
												HBO::CContext						*context)
	:	m_Args(args)
	,	m_Settings(settings)
	,	m_FSController(context->FileController())
	{
	}

	//----------------------------------------------------------------------------

	CMaterialCompilation::~CMaterialCompilation()
	{
	}

	//----------------------------------------------------------------------------

	bool	CMaterialCompilation::_AddShaderFileForCompilation(	const CString							&inputPath,
																const CString							&outputPath,
																const CString							&shaderPermutation,
																RHI::EShaderStage						stage,
																ESampleLibGraphicResources_RenderPass	renderPass,
																EShaderOptions							option,
																RHI::EGraphicalApi						api)
	{
		if (!PK_VERIFY(m_ShaderFiles.PushBack().Valid()))
			return false;
		SFileCompilation	&compilation = m_ShaderFiles.Last();
		compilation.m_InputPath = inputPath;
		compilation.m_OutputPath = outputPath;
		compilation.m_ShaderPermutation = shaderPermutation;
		compilation.m_Stage = stage;
		compilation.m_Option = option;
		compilation.m_Api = api;
		compilation.m_RenderPass = renderPass;
		return true;
	}

	//----------------------------------------------------------------------------

	bool	CMaterialCompilation::_FileRequiresCompilation(const CString &compiledFilePath, u64 srcFileTimestamp)
	{
		SFileTimes		fileTimes;
		if (!m_FSController->Timestamps(compiledFilePath, fileTimes))
			return true;
		return fileTimes.m_LastWriteTime < srcFileTimestamp;
	}

	//----------------------------------------------------------------------------

	bool	CMaterialCompilation::_BuildShaderFilesListForOptions(TMemoryView<const EShaderOptions> options, ESampleLibGraphicResources_RenderPass renderPass)
	{
		PK_ASSERT(!m_Settings.m_Targets.Empty());

		TArray<CString>		compiledShaders;
		const u32			shaderCount = options.Count() * m_Settings.m_Targets.Count();
		if (!PK_VERIFY(compiledShaders.Reserve(shaderCount)) ||
			!PK_VERIFY(m_ShaderFiles.Reserve(shaderCount)))
			return false;

		const CString	&materialName = m_Args.m_MaterialSettings.m_MaterialName;
		const CString	&vertexShaderPath = m_Args.m_MaterialSettings.m_VertexShaderPath;
		const CString	&fragmentShaderPath = m_Args.m_MaterialSettings.m_FragmentShaderPath;

		const bool		forceRebuild = m_Settings.m_ForceRecreate;
		for (EShaderOptions option : options)
		{
			const RHI::EShaderStagePipeline	stagesForOption = ParticleShaderGenerator::GetShaderStagePipeline(option);

			// .meta file content (full desc of shader options)
			CString			vertexPermutation;
			CString			geomPermutation;
			CString			fragmentPermutation;

			// Full paths (api independent)
			const CString	vertexShaderNoExt = RemapShaderPathNoExt(m_Settings.m_OutputFolder, vertexShaderPath, materialName, RHI::VertexShaderStage, m_Args.m_RendererType, option, m_Args.m_FeaturesSettings.View(), renderPass, &vertexPermutation);
			const CString	geomShaderNoExt = RemapShaderPathNoExt(m_Settings.m_OutputFolder, null, materialName, RHI::GeometryShaderStage, m_Args.m_RendererType, option, m_Args.m_FeaturesSettings.View(), renderPass, &geomPermutation);
			const CString	fragmentShaderNoExt = RemapShaderPathNoExt(m_Settings.m_OutputFolder, fragmentShaderPath, materialName, RHI::FragmentShaderStage, m_Args.m_RendererType, option, m_Args.m_FeaturesSettings.View(), renderPass, &fragmentPermutation);

			// We remove the already compiled stages:
			u32				stagesToCompile = stagesForOption;
			const bool		containsVertexShader = !forceRebuild && compiledShaders.Contains(vertexShaderNoExt);
			const bool		containsGeomShader = !forceRebuild && compiledShaders.Contains(geomShaderNoExt);
			const bool		containsFragmentShader = !forceRebuild && compiledShaders.Contains(fragmentShaderNoExt);

			stagesToCompile &= ~(containsVertexShader ? RHI::VertexShaderMask : 0);
			stagesToCompile &= ~(containsGeomShader ? RHI::GeometryShaderMask : 0);
			stagesToCompile &= ~(containsFragmentShader ? RHI::FragmentShaderMask : 0);

			if ((stagesToCompile & RHI::VertexShaderMask) != 0)
				PK_VERIFY(compiledShaders.PushBack(vertexShaderNoExt).Valid());
			if ((stagesToCompile & RHI::GeometryShaderMask) != 0)
				PK_VERIFY(compiledShaders.PushBack(geomShaderNoExt).Valid());
			if ((stagesToCompile & RHI::FragmentShaderMask) != 0)
				PK_VERIFY(compiledShaders.PushBack(fragmentShaderNoExt).Valid());

			for (u32 i = 0; i < m_Settings.m_Targets.Count(); ++i)
			{
				const RHI::EGraphicalApi	api = m_Settings.m_Targets[i].m_Target;
				const char					*shadersExt = GetShaderExtensionStringFromApi(api);

				if ((stagesForOption & RHI::GeometryShaderMask) != 0)
				{
					// unsupported geometry shaders on orbis/metal
					if (api == RHI::EGraphicalApi::GApi_Orbis ||
						api == RHI::EGraphicalApi::GApi_UNKNOWN2 ||
						api == RHI::EGraphicalApi::GApi_Metal)
						continue;
				}

				if ((stagesToCompile & RHI::VertexShaderMask) != 0)
				{
					const CString	fileDst = vertexShaderNoExt + shadersExt;
					if (forceRebuild || _FileRequiresCompilation(fileDst, m_Args.m_SourceTimestamps.m_Vertex))
					{
						if (!_AddShaderFileForCompilation(vertexShaderPath, fileDst, vertexPermutation, RHI::VertexShaderStage, renderPass, option, api))
							return false;
					}
				}
				if ((stagesToCompile & RHI::GeometryShaderMask) != 0)
				{
					const CString	fileDst = geomShaderNoExt + shadersExt;
					if (forceRebuild || _FileRequiresCompilation(fileDst, m_Args.m_SourceTimestamps.m_Vertex))
					{
						if (!_AddShaderFileForCompilation(vertexShaderPath, fileDst, geomPermutation, RHI::GeometryShaderStage, renderPass, option, api))
							return false;
					}
				}
				if ((stagesToCompile & RHI::FragmentShaderMask) != 0)
				{
					const CString	fileDst = fragmentShaderNoExt + shadersExt;
					if (forceRebuild || _FileRequiresCompilation(fileDst, m_Args.m_SourceTimestamps.m_Fragment))
					{
						if (!_AddShaderFileForCompilation(fragmentShaderPath, fileDst, fragmentPermutation, RHI::FragmentShaderStage, renderPass, option, api))
							return false;
					}
				}
			}
		}
		return true;
	}

	//----------------------------------------------------------------------------

	bool	CMaterialCompilation::BuildShaderFilesList()
	{
		for (u32 i = 0; i < m_Args.m_RenderPasses.Count(); ++i)
		{
			ESampleLibGraphicResources_RenderPass	renderPass = m_Args.m_RenderPasses[i];

			// Generate with different options needed
			const bool	geomBB = m_Settings.m_GenerateGeometryBBShaders;
			const bool	vertexBB = m_Settings.m_GenerateVertexBBShaders;

			if (m_Settings.m_Targets.Empty())
				continue;

			if (!m_Args.m_OptionsOverride.Empty())
			{
				if (!_BuildShaderFilesListForOptions(m_Args.m_OptionsOverride, renderPass))
					return false;
			}
			else if (m_Args.m_RendererType == Renderer_Billboard &&
					(vertexBB || geomBB))
			{
				bool	success = true;
				if (geomBB)
					success &= _BuildShaderFilesListForOptions(kShaderOptions_GeomBB, renderPass);
				if (vertexBB)
					success &= _BuildShaderFilesListForOptions(kShaderOptions_VertexBB, renderPass);
				if (!success)
					return false;
			}
			else if (m_Args.m_RendererType == Renderer_Triangle &&
					(vertexBB || geomBB))
			{
				bool	success = true;
				// TODO: Geom billboarding
				if (vertexBB)
					success &= _BuildShaderFilesListForOptions(kShaderOptions_TriangleVertexBB, renderPass);
				if (!success)
					return false;
			}
			else if (m_Args.m_RendererType == Renderer_Mesh)
			{
				bool	success = true;
				success &= _BuildShaderFilesListForOptions(kShaderOptions_GPUmesh, renderPass);
				if (!success)
					return false;
			}
			else if (m_Args.m_RendererType == Renderer_Ribbon)
			{
				bool	success = true;
				success &= _BuildShaderFilesListForOptions(kShaderOptions_RibbonVertexBB, renderPass);
				if (!success)
					return false;
			}
			if (!_BuildShaderFilesListForOptions(kShaderOptions_Regular, renderPass))
				return false;
		}
		return true;
	}

	//----------------------------------------------------------------------------

	bool	CMaterialCompilation::Partial(u32 taskNb) const
	{
#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)
		if (!PK_VERIFY(taskNb < m_ShaderFiles.Count()))
			return false;

		bool					result = true;
		const SFileCompilation	&compilation = m_ShaderFiles[taskNb];
		RHI::SShaderDescription	shaderDesc;

		if (!MaterialFrontendToShaderDesc(m_Args, shaderDesc, compilation.m_Option, compilation.m_RenderPass))
		{
			CLog::Log(PK_ERROR, "Cannot create the shader description for this material instance");
			return false;
		}
		CShaderCompilation	shaderCompilation(	compilation.m_InputPath,
												compilation.m_OutputPath,
												compilation.m_ShaderPermutation,
												compilation.m_Stage,
												compilation.m_Option,
												shaderDesc,
												m_Settings);
		result &= shaderCompilation.Exec(compilation.m_Api, m_FSController);
		if (!result)
			CLog::Log(PK_ERROR, "Compilation failed for shader %s", compilation.m_OutputPath.Data());
		return result;
#else
		CLog::Log(PK_ERROR, "Cannot compile shaders at runtime on this platform");
		return result;
#endif	// (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)
	}

	//----------------------------------------------------------------------------

	bool	CMaterialCompilation::Exec() const
	{
		bool	success = true;
		for (u32 i = 0; i < ShaderCount(); ++i)
		{
			success &= Partial(i);
		}
		return success;
	}
#endif // (PK_MAT2RHI_CAN_COMPILE != 0)

}	// namespace MaterialToRHI

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
