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

#include "ParticleShaderGenerator.h"

#include "Assets/ShaderIncludes/generated/Billboard.vert.h"
#include "Assets/ShaderIncludes/generated/Billboard.geom.h"
#include "Assets/ShaderIncludes/generated/Triangle.vert.h"
#include "Assets/ShaderIncludes/generated/Ribbon.vert.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

SPerStageShaderName			g_OptionsShaderNames[] =
{
	{ Option_None, null, null, null },
	{ Option_VertexPassThrough, "Fwd", null, null },
	{ Option_GeomBillboarding, "Geom", "Geom", "Geom" },
	{ Option_VertexBillboarding, "Vertex", null, "Vertex" },
	{ Option_Axis_C1, "C1", "C1", "C1" },
	{ Option_Axis_C2, "C2", "C2", "C2" },
	{ Option_Capsule, "Caps", "Caps", null },
	{ Option_BillboardSizeFloat2, "Size2", "Size2", null },
	{ Option_TriangleVertexBillboarding, "Tri", null, "VB" },
	{ Option_RibbonVertexBillboarding, "Ribbon", null, "VB" },
	{ Option_GPUStorage, "GPU", "GPU", "GPU" },
	{ Option_GPUMesh, "GPUMesh", null, "GPUMesh" },
	{ Option_GPUSort, "GPUSort", null, "GPUSort" },
};

// FIXME:
// We cannot share the same fragment between CPUBB/GeomBB and VertexBB because shader bindings are not taken into account
// for generating shaders. If sampleDepth is enabled, its binding will be offset by all vertex stage bindings (Positions, indices, ..),
// And the depth sampler binding will end up being different than the vertex passthrough option the shader is generated with
// Same issue when Option_VertexBillboarding_GPU is enabled, generated texture registers differ
// ! We can get rid of some of those fragment shader specializations once we have a single GPU buffer as for the GPU sim billboarded in the vertex shader !

//----------------------------------------------------------------------------

CStringView		ShaderOptionsUtils::GetShaderName(EShaderOptions options, RHI::EShaderStage stage, const TMemoryView<char> &outStorage)
{
	u32		writeId = 0;

	for (u32 i = 0; i < PK_ARRAY_COUNT(g_OptionsShaderNames); ++i)
	{
		if (options & g_OptionsShaderNames[i].m_ShaderOption)
		{
			if (stage == RHI::VertexShaderStage && g_OptionsShaderNames[i].m_Vertex != null)
			{
				const u32	len = static_cast<u32>(strlen(g_OptionsShaderNames[i].m_Vertex));
				if (!PK_VERIFY(writeId + 1 + len + 1 < outStorage.Count()))
					return CStringView();
				if (writeId != 0)
					outStorage[writeId++] = '_';
				memcpy(&outStorage[writeId], g_OptionsShaderNames[i].m_Vertex, len);
				writeId += len;
			}
			else if (stage == RHI::GeometryShaderStage && g_OptionsShaderNames[i].m_Geom != null)
			{
				const u32	len = static_cast<u32>(strlen(g_OptionsShaderNames[i].m_Geom));
				if (!PK_VERIFY(writeId + 1 + len + 1 < outStorage.Count()))
					return CStringView();
				if (writeId != 0)
					outStorage[writeId++] = '_';
				memcpy(&outStorage[writeId], g_OptionsShaderNames[i].m_Geom, len);
				writeId += len;
			}
			else if (stage == RHI::FragmentShaderStage && g_OptionsShaderNames[i].m_Fragment != null)
			{
				const u32	len = static_cast<u32>(strlen(g_OptionsShaderNames[i].m_Fragment));
				if (!PK_VERIFY(writeId + 1 + len + 1 < outStorage.Count()))
					return CStringView();
				if (writeId != 0)
					outStorage[writeId++] = '_';
				memcpy(&outStorage[writeId], g_OptionsShaderNames[i].m_Fragment, len);
				writeId += len;
			}
		}
	}

	if (writeId == 0)
	{
		const char	kDefault[] = "User";
		const u32	len = PK_ARRAY_COUNT(kDefault)-1;
		if (!PK_VERIFY(len + 1 < outStorage.Count()))
			return CStringView();
		memcpy(&outStorage[0], kDefault, len);
		writeId += len;
	}
	outStorage[writeId] = '\0';

	return CStringView(outStorage.Data(), writeId);
}

//----------------------------------------------------------------------------

namespace
{
	CString		GenVertexBBBody(const RHI::SShaderDescription &description, EShaderOptions options)
	{
		CString		shaderCode;
		if (options & Option_VertexBillboarding)
			shaderCode = CString(g_Billboard_vert_data, sizeof(g_Billboard_vert_data));
		else if (options & Option_TriangleVertexBillboarding)
			shaderCode = CString(g_Triangle_vert_data, sizeof(g_Triangle_vert_data));
		else if (options & Option_RibbonVertexBillboarding)
			shaderCode = CString(g_Ribbon_vert_data, sizeof(g_Ribbon_vert_data));
		else
 			PK_ASSERT_NOT_REACHED();
		shaderCode +=	"\nvoid " + ParticleShaderGenerator::GetVertexPassThroughFunctionName() + "(IN(SVertexInput) vInput, OUT(SVertexOutput) vOutput VS_ARGS)\n"
						"{\n";

		// Right now, indices are always generated
		shaderCode += "	const uint	indicesOffset = GET_CONSTANT(GPUBillboardPushConstants, IndicesOffset);\n";

		// We'll generate VertexBillboard callee without #if BB_GPU_SIM as we have that info here
		const bool	gpuStorage = (options & Option_GPUStorage) != 0;
		if (gpuStorage)
		{
			shaderCode += "	const uint	storageId = GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex);\n";
			shaderCode += "	uint	particleID = vInput.InstanceId;\n";
			if (options & Option_GPUSort)
			{
				shaderCode += "#if	defined(VRESOURCE_Indirection)\n";
				shaderCode += "	particleID = LOADU(GET_RAW_BUFFER(Indirection), RAW_BUFFER_INDEX(particleID));\n";
				shaderCode += "#endif\n";
			}
		}
		else
			shaderCode += "	const uint	particleID = LOADU(GET_RAW_BUFFER(Indices), RAW_BUFFER_INDEX(indicesOffset) + RAW_BUFFER_INDEX(vInput.InstanceId));\n";

		// Initialize vertex outputs to 0
		for (const RHI::SVertexOutput &vOutput : description.m_VertexOutput)
		{
			shaderCode += CString::Format("	vOutput.%s = %s(0", vOutput.m_Name.Data(), RHI::GlslShaderTypes::GetTypeString(vOutput.m_Type).Data());
			for (u32 i = 1; i < RHI::VarType::VarTypeToComponentSize(vOutput.m_Type); ++i)
				shaderCode += ", 0";
			shaderCode += ");\n";
		}

		// Do billboarding (write position / normal / uv vertex outputs). If ribbon, it also
		// returns updated particle ID for the current vertex (a ribbon quad being made of 2 particles)
		if (options & Option_RibbonVertexBillboarding)
			shaderCode += "	particleID = VertexBillboard(vInput, vOutput, particleID VS_PARAMS);\n";
		else
			shaderCode += "	VertexBillboard(vInput, vOutput, particleID VS_PARAMS);\n";

		shaderCode +=	"#if	defined(VOUTPUT_fragViewProjPosition)\n"
						"vOutput.fragViewProjPosition = vOutput.VertexPosition;\n"
						"#endif\n";

		// Setup others vertex outputs (color and others additionnal per particle parameters)
		for (const RHI::SVertexOutput &vOutput : description.m_VertexOutput)
		{
			if (vOutput.m_InputRelated.Valid())
			{
				const u32			dim = RHI::VarType::VarTypeToComponentSize(vOutput.m_Type);
				const EBaseTypeID	baseType = RHI::VarType::VarTypeToRuntimeBaseType(vOutput.m_Type);

				if (PK_VERIFY(dim >= 1 && dim <= 4))
				{
					const CString	bindingName = vOutput.m_Name.Replace("frag", ""); // TODO: not happy with that

					CString		loadString;
					if (CBaseTypeTraits::Traits(baseType).IsFp)
						loadString = dim > 1 ? CString::Format("LOADF%d", dim) : "LOADF";
					else
						loadString = dim > 1 ? CString::Format("LOADU%d", dim) : "LOADU";

				if (gpuStorage)
					if (vOutput.m_Type == RHI::TypeInt) // Gorefix for #13391. If more implicit cast errors happen, refactor to force explicit cast. 
						shaderCode += CString::Format("	vOutput.%s = int(%s(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(%ssOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * %d)));\n", vOutput.m_Name.Data(), loadString.Data(), bindingName.Data(), dim);
					else
						shaderCode += CString::Format("	vOutput.%s = %s(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(%ssOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * %d));\n", vOutput.m_Name.Data(), loadString.Data(), bindingName.Data(), dim);
				else
					shaderCode += CString::Format("	vOutput.%s = %s(GET_RAW_BUFFER(%ss), RAW_BUFFER_INDEX(particleID * %d));\n", vOutput.m_Name.Data(), loadString.Data(), bindingName.Data(), dim);
				}
				else
					PK_ASSERT_NOT_REACHED();
			}
		}

		const CString	sizeZero = (options & Option_BillboardSizeFloat2) ? CString("vec2(0.f, 0.f)") : CString("0.f");

		shaderCode += "}\n";

		return shaderCode;
	}

//----------------------------------------------------------------------------

	CString		_GenGPUMeshVertexPassthrough(const RHI::SShaderDescription &description, CGuid positionIdx)
	{
		CString		shaderCode = "void " + ParticleShaderGenerator::GetVertexPassThroughFunctionName() + "(IN(SVertexInput) vInput, OUT(SVertexOutput) vOutput VS_ARGS)\n"
			"{\n";

		const auto	&inputs = description.m_Bindings.m_InputAttributes;
		shaderCode += "	const uint	storageID = GET_CONSTANT(GPUMeshPushConstants, DrawRequest);\n";
		shaderCode += "	const uint	indirectionOffsetsID = GET_CONSTANT(GPUMeshPushConstants, IndirectionOffsetsIndex);\n";
		shaderCode += "	const uint	indirectionOffset = LOADU(GET_RAW_BUFFER(IndirectionOffsets), RAW_BUFFER_INDEX(indirectionOffsetsID));\n";
		shaderCode += "	const uint	particleID = LOADU(GET_RAW_BUFFER(Indirection), indirectionOffset + RAW_BUFFER_INDEX(vInput.InstanceId));\n";
		shaderCode += "	const mat4	modelMatrix = GetMeshMatrix(vInput VS_PARAMS);\n";

		if (positionIdx.Valid())
		{
			for (const RHI::SVertexAttributeDesc &field : inputs)
			{
				if (field.m_ShaderLocationBinding == positionIdx)
				{
					CString	swizzle;
					switch (inputs[positionIdx].m_Type)
					{
					case RHI::TypeFloat2:
						swizzle = ".xy, 0.0";
						break;
					case RHI::TypeFloat3:
					case RHI::TypeFloat4:
						swizzle = ".xyz";
						break;
					default:
						break;
					}
					if (swizzle.Empty())
						break;
					shaderCode += "\n#if		defined(CONST_SceneInfo_ViewProj)\n";
					shaderCode += CString::Format("	vec4 pos = mul(modelMatrix, vec4(vInput.%s%s, 1.0));\n", inputs[positionIdx].m_Name.Data(), swizzle.Data());
					shaderCode += CString::Format("	vOutput.VertexPosition = mul(GET_CONSTANT(SceneInfo, ViewProj), pos);\n");
					shaderCode += "#else\n";
					shaderCode += CString::Format("	vOutput.VertexPosition = vec4(vInput.%s%s, 1.0);\n", inputs[positionIdx].m_Name.Data(), swizzle.Data());
					shaderCode += "#endif\n";
				}
			}
		}

		for (const RHI::SVertexOutput &vOutput : description.m_VertexOutput)
		{
			CGuid	idx = vOutput.m_InputRelated;
			if (idx.Valid())
			{
				bool	hasVertexInput = false;
				for (const RHI::SVertexAttributeDesc &field : inputs)
				{
					if (field.m_ShaderLocationBinding == idx)
					{
						shaderCode += CString::Format("	vOutput.%s = vInput.%s;\n", vOutput.m_Name.Data(), field.m_Name.Data());
						hasVertexInput = true;
					}
				}
				if (!hasVertexInput)
				{
					const u32			dim = RHI::VarType::VarTypeToComponentSize(vOutput.m_Type);
					const EBaseTypeID	baseType = RHI::VarType::VarTypeToRuntimeBaseType(vOutput.m_Type);

					if (PK_VERIFY(dim >= 1 && dim <= 4))
					{
						const CString	bindingName = vOutput.m_Name.Replace("frag", ""); // TODO: not happy with that

						CString		loadString;
						if (CBaseTypeTraits::Traits(baseType).IsFp)
							loadString = dim > 1 ? CString::Format("LOADF%d", dim) : "LOADF";
						else
							loadString = dim > 1 ? CString::Format("LOADU%d", dim) : "LOADU";
						if (vOutput.m_Type == RHI::TypeInt) // Gorefix for #13391. If more implicit cast errors happen, refactor to force explicit cast. 
						{
							shaderCode += CString::Format(
						"	vOutput.%s = int(%s(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(%ssOffsets), RAW_BUFFER_INDEX(storageID)) + RAW_BUFFER_INDEX(particleID * %d)));\n",
							vOutput.m_Name.Data(), loadString.Data(), bindingName.Data(), dim);
						}
						else
						{
							shaderCode += CString::Format(
						"	vOutput.%s = %s(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(%ssOffsets), RAW_BUFFER_INDEX(storageID)) + RAW_BUFFER_INDEX(particleID * %d));\n",
							vOutput.m_Name.Data(), loadString.Data(), bindingName.Data(), dim);
						}
						
					}
					else
						PK_ASSERT_NOT_REACHED();
				}
			}
			else
			{
				shaderCode += CString::Format("	vOutput.%s = %s(0", vOutput.m_Name.Data(), RHI::GlslShaderTypes::GetTypeString(vOutput.m_Type).Data());
				for (u32 i = 1; i < RHI::VarType::VarTypeToComponentSize(vOutput.m_Type); ++i)
					shaderCode += ", 0";
				shaderCode += ");\n";
			}
		}

		shaderCode += "#if	defined(VOUTPUT_fragWorldPosition)\n"
			"	vOutput.fragWorldPosition = mul(modelMatrix, vec4(vInput.Position, 1.0f)).xyz;\n"
			"#endif\n"
			"#if	defined(VINPUT_Normal)\n"
			"	vOutput.fragNormal = mul(modelMatrix, vec4(vInput.Normal.xyz, 0)).xyz;\n"
			"#endif\n"
			"#if	defined(VINPUT_Tangent)\n"
			"	vOutput.fragTangent = vec4(mul(modelMatrix, vec4(vInput.Tangent.xyz, 0)).xyz, vInput.Tangent.w);\n"
			"#endif\n"
			"#if	defined(VOUTPUT_fragViewProjPosition)\n"
			"	vOutput.fragViewProjPosition = vOutput.VertexPosition;\n"
			"#endif\n";

		// Atlas feature, mesh particles
		shaderCode +=	"#if	defined(BB_Feature_Atlas) && defined(VINPUT_UV0)\n"
						"#	if !defined(VRESOURCE_Atlas_TextureIDsOffsets)\n"
						"#		error \"config error\"\n"
						"#	endif\n"
						"	const uint	maxAtlasID = LOADU(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(0)) - 1U;\n"
						"	const float	textureID = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Atlas_TextureIDsOffsets), RAW_BUFFER_INDEX(storageID)) + RAW_BUFFER_INDEX(particleID));\n"
						"	const uint	atlasID0 = min(uint(textureID), maxAtlasID);\n"
						"	const uint	atlasID1 = min(uint(textureID + 1), maxAtlasID);\n"
						"	const vec4	rect0 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID0 * 4 + 1));\n"
						"	const vec4	rect1 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID1 * 4 + 1));\n"
						"	vOutput.fragUV0 = vInput.UV0 * rect0.xy + rect0.zw;\n"
						"#	if !defined(MESH_USE_UV1)\n" // #12340
						"	vOutput.fragUV1 = vInput.UV0 * rect1.xy + rect1.zw;\n"
						"#	endif // !defined(MESH_USE_UV1)\n"
						"#endif\n";

		shaderCode += "";

		shaderCode += "}\n";

		return shaderCode;
	}
}

//----------------------------------------------------------------------------

CString		ParticleShaderGenerator::GenGetMeshTransformHelper()
{
	CString		getMeshMatrixHelper = "mat4		GetMeshMatrix(IN(SVertexInput) vInput VS_ARGS)\n";
	getMeshMatrixHelper += "{\n";
	getMeshMatrixHelper += "#if		defined(VRESOURCE_MeshTransforms)\n";
	getMeshMatrixHelper += "	const uint	storageID = GET_CONSTANT(GPUMeshPushConstants, DrawRequest);\n";
	getMeshMatrixHelper += "	const uint	indirectionOffsetsID = GET_CONSTANT(GPUMeshPushConstants, IndirectionOffsetsIndex);\n";
	getMeshMatrixHelper += "	const uint	indirectionOffset = LOADU(GET_RAW_BUFFER(IndirectionOffsets), RAW_BUFFER_INDEX(indirectionOffsetsID));\n";
	getMeshMatrixHelper += "	const uint	transformsOffset = LOADU(GET_RAW_BUFFER(MeshTransformsOffsets), RAW_BUFFER_INDEX(storageID));\n";
	getMeshMatrixHelper += "	const uint	particleID = LOADU(GET_RAW_BUFFER(Indirection), indirectionOffset + RAW_BUFFER_INDEX(vInput.InstanceId));\n";
	getMeshMatrixHelper += "	const vec4	m0 = LOADF4(GET_RAW_BUFFER(MeshTransforms), transformsOffset + RAW_BUFFER_INDEX(particleID * 16 + 0 * 4));\n";
	getMeshMatrixHelper += "	const vec4	m1 = LOADF4(GET_RAW_BUFFER(MeshTransforms), transformsOffset + RAW_BUFFER_INDEX(particleID * 16 + 1 * 4));\n";
	getMeshMatrixHelper += "	const vec4	m2 = LOADF4(GET_RAW_BUFFER(MeshTransforms), transformsOffset + RAW_BUFFER_INDEX(particleID * 16 + 2 * 4));\n";
	getMeshMatrixHelper += "	const vec4	m3 = LOADF4(GET_RAW_BUFFER(MeshTransforms), transformsOffset + RAW_BUFFER_INDEX(particleID * 16 + 3 * 4));\n";
	getMeshMatrixHelper += "	return BUILD_MAT4(m0, m1, m2, m3);\n";
	getMeshMatrixHelper += "#elif		defined(VINPUT_MeshTransform)\n";
	getMeshMatrixHelper += "	return vInput.MeshTransform;\n";
	getMeshMatrixHelper += "#else\n";
	getMeshMatrixHelper += "	return BUILD_MAT4(vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1));\n";
	getMeshMatrixHelper += "#endif\n";
	getMeshMatrixHelper += "}\n\n";
	return getMeshMatrixHelper;
}

//----------------------------------------------------------------------------

//	Generate Passthrough for Particle shaders
CString		ParticleShaderGenerator::GenVertexPassThrough(const RHI::SShaderDescription &description, CGuid positionIdx, EShaderOptions options)
{
	const bool	vertexBB =	options & Option_VertexBillboarding ||
							options & Option_TriangleVertexBillboarding ||
							options & Option_RibbonVertexBillboarding;
	if (vertexBB)
		return GenVertexBBBody(description, options);

	if (options & Option_GPUMesh)
		return _GenGPUMeshVertexPassthrough(description, positionIdx);

	CString		shaderCode =	"void " + GetVertexPassThroughFunctionName() + "(IN(SVertexInput) vInput, OUT(SVertexOutput) vOutput VS_ARGS)\n"
								"{\n";

	const auto	&inputs = description.m_Bindings.m_InputAttributes;
	const bool	geomBB = options & Option_GeomBillboarding;
	if (positionIdx.Valid())
	{
		for (const RHI::SVertexAttributeDesc &field : inputs)
		{
			if (field.m_ShaderLocationBinding == positionIdx)
			{
				CString	swizzle;
				switch (inputs[positionIdx].m_Type)
				{
				case RHI::TypeFloat2:
					swizzle = ".xy, 0.0";
					break;
				case RHI::TypeFloat3:
				case RHI::TypeFloat4:
					swizzle = ".xyz";
					break;
				default:
					break;
				}
				if (swizzle.Empty())
					break;
				if (!geomBB)
				{
					shaderCode += "#if		defined(CONST_SceneInfo_ViewProj)\n";
					shaderCode += " #if		defined(VINPUT_MeshTransform)\n";
					shaderCode += CString::Format("	vec4 pos = mul(vInput.MeshTransform, vec4(vInput.%s%s, 1.0));\n", inputs[positionIdx].m_Name.Data(), swizzle.Data());
					shaderCode += CString::Format("	vOutput.VertexPosition = mul(GET_CONSTANT(SceneInfo, ViewProj), pos);\n");
					shaderCode += " #else\n";
					shaderCode += CString::Format("	vOutput.VertexPosition = mul(GET_CONSTANT(SceneInfo, ViewProj), vec4(vInput.%s%s, 1.0));\n", inputs[positionIdx].m_Name.Data(), swizzle.Data());
					shaderCode += " #endif\n";
					shaderCode += "#else\n";
					shaderCode += CString::Format("	vOutput.VertexPosition = vec4(vInput.%s%s, 1.0);\n", inputs[positionIdx].m_Name.Data(), swizzle.Data());
					shaderCode += "#endif\n";
				}
				else
				{
					// For geometry shaders, DrawRequestID is in Position's W component
					if (options & Option_GPUStorage)
						shaderCode += CString::Format("	vOutput.VertexPosition = vec4(vInput.%s%s, 0.0);\n", inputs[positionIdx].m_Name.Data(), swizzle.Data());
					else if (geomBB)
					{
						PK_ASSERT(inputs.Count() > positionIdx + 1);
						shaderCode += CString::Format("	vOutput.VertexPosition = vec4(vInput.%s%s, vInput.%s);\n", inputs[positionIdx].m_Name.Data(), swizzle.Data(), inputs[positionIdx + 1].m_Name.Data());
					}
				}
			}
		}
	}

	for (const RHI::SVertexOutput &vOutput : description.m_VertexOutput)
	{
		CGuid	idx = vOutput.m_InputRelated;
		if (idx.Valid())
		{
			for (const RHI::SVertexAttributeDesc &field : inputs)
			{
				if (field.m_ShaderLocationBinding == idx)
					shaderCode += CString::Format("	vOutput.%s = vInput.%s;\n", vOutput.m_Name.Data(), field.m_Name.Data());
			}
		}
		else
		{
			shaderCode += CString::Format("	vOutput.%s = %s(0", vOutput.m_Name.Data(), RHI::GlslShaderTypes::GetTypeString(vOutput.m_Type).Data());
			for (u32 i = 1; i < RHI::VarType::VarTypeToComponentSize(vOutput.m_Type); ++i)
				shaderCode += ", 0";
			shaderCode += ");\n";
		}
	}

	shaderCode +=	"#if	defined(VINPUT_MeshTransform)\n"
					"	mat4 modelMatrix = vInput.MeshTransform;\n"
					"#endif\n"
					"#if	defined(VOUTPUT_fragWorldPosition)\n"
					"#	if		defined(VINPUT_MeshTransform)\n"
					"	vOutput.fragWorldPosition = mul(modelMatrix, vec4(vInput.Position, 1.0f)).xyz;\n"
					"#	else\n"
					"	vOutput.fragWorldPosition = vInput.Position;\n"
					"#	endif\n"
					"#endif\n"
					"#if	defined(VINPUT_Normal) && defined(VINPUT_MeshTransform)\n"
					"	vOutput.fragNormal = mul(modelMatrix, vec4(vInput.Normal.xyz, 0)).xyz;\n"
					"#endif\n"
					"#if	defined(VINPUT_Tangent) && defined(VINPUT_MeshTransform)\n"
					"	vOutput.fragTangent = vec4(mul(modelMatrix, vec4(vInput.Tangent.xyz, 0)).xyz, vInput.Tangent.w);\n"
					"#endif\n"
					"#if	defined(VOUTPUT_fragViewProjPosition)\n"
					"	vOutput.fragViewProjPosition = vOutput.VertexPosition;\n"
					"#endif\n";

	// Atlas feature, mesh particles
	shaderCode +=	"#if	defined(BB_Feature_Atlas) && defined(VINPUT_UV0)\n"
					"#	if !defined(VINPUT_Atlas_TextureID)\n"
					"#		error \"config error\"\n"
					"#	endif\n"
					"	const uint	maxAtlasID = LOADU(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(0)) - 1U;\n"
					"	const float	textureID = vInput.Atlas_TextureID;\n"
					"	const uint	atlasID0 = min(uint(textureID), maxAtlasID);\n"
					"	const uint	atlasID1 = min(uint(textureID + 1), maxAtlasID);\n"
					"	const vec4	rect0 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID0 * 4 + 1));\n"
					"	const vec4	rect1 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID1 * 4 + 1));\n"
					"	vOutput.fragUV0 = vInput.UV0 * rect0.xy + rect0.zw;\n"
					"#	if !defined(MESH_USE_UV1)\n" // #12340
					"	vOutput.fragUV1 = vInput.UV0 * rect1.xy + rect1.zw;\n"
					"#	endif // !defined(MESH_USE_UV1)\n"
					"#endif\n";

	shaderCode += "";

	const CString	sizeZero = (options & Option_BillboardSizeFloat2) ? CString("vec2(0.f, 0.f)") : CString("0.f");

	shaderCode +=	"#if	defined(VINPUT_Enabled)\n"
					"	if (vInput.Enabled == 0u)\n";
	shaderCode +=	CString::Format("		vOutput.geomSize = %s;\n", sizeZero.Data());
	shaderCode +=	"#endif\n";

	shaderCode +=	"}\n";

	return shaderCode;
}

//----------------------------------------------------------------------------

CString		ParticleShaderGenerator::GetVertexPassThroughFunctionName()
{
	return "VertexGenerated";
}

//----------------------------------------------------------------------------

CString		ParticleShaderGenerator::GenGeomBillboarding(const RHI::SShaderDescription &description)
{
	CString	shaderCode =	CString(g_Billboard_geom_data, sizeof(g_Billboard_geom_data)) +
							"\nvoid " + GetGeomBillboardingFunctionName() + "(IN(SGeometryInput) gInput, SGeometryOutput gOutput GS_ARGS)\n"
							"{\n";
	PK_FOREACH(vOuput, description.m_GeometryOutput.m_GeometryOutput)
	{
		CGuid	idx = vOuput->m_InputRelated;
		if (idx.Valid())
		{
			if (idx < description.m_VertexOutput.Count())
				shaderCode += CString::Format("	gOutput.%s = gInput.Primitives[0].%s;\n", vOuput->m_Name.Data(), description.m_VertexOutput[idx].m_Name.Data());
		}
	}

	shaderCode +=	"	GeometryBillboard(gInput, gOutput GS_PARAMS);\n"
					"}\n";

	return shaderCode;
}

//----------------------------------------------------------------------------

CString		ParticleShaderGenerator::GetGeomBillboardingFunctionName()
{
	return "GeometryGenerated";
}

//----------------------------------------------------------------------------

CString		ParticleShaderGenerator::GenAdditionalFunction(const RHI::SShaderDescription &description, EShaderOptions options, TArray<CString> &funcToCall, RHI::EShaderStage stage)
{
	CString	genStr;
	bool	geomBB = options & Option_GeomBillboarding;
	if (options & Option_VertexPassThrough && stage == RHI::VertexShaderStage)
	{
		genStr += GenVertexPassThrough(description, 0, options); // 0 is hard coded ... Bad
		funcToCall.PushFront(GetVertexPassThroughFunctionName());
	}
	if (geomBB && stage == RHI::GeometryShaderStage)
	{
		genStr += GenGeomBillboarding(description);
		funcToCall.PushFront(GetGeomBillboardingFunctionName());
	}
	return genStr;
}

//----------------------------------------------------------------------------

RHI::EShaderStagePipeline	ParticleShaderGenerator::GetShaderStagePipeline(EShaderOptions options)
{
	if (options & Option_GeomBillboarding)
		return RHI::VsGsPs;
	return RHI::VsPs;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
