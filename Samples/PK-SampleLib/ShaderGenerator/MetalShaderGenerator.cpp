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
#include "MetalShaderGenerator.h"

#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)

#include "pk_rhi/include/EnumHelper.h"
#include "PK-MCPP/pk_preprocessor.h"

#include <pk_kernel/include/kr_buffer_parsing_utils.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	CMetalShaderGenerator::GatherShaderInfo(const CString &shaderContent, const CString &shaderDir, IFileSystem *fs)
{
	// Find all the shader dependencies for parsing:
	TArray<CString>		dependencies;

	if (!PK_VERIFY(CPreprocessor::FindShaderDependencies(shaderContent, shaderDir, dependencies, fs)))
	{
		CLog::Log(PK_ERROR, "Could not find the shader dependencies");
		return false;
	}
	if (!_FindAllTextureFunctionParameters(shaderContent))
		return false;
	PK_FOREACH(dep, dependencies)
	{
		const CString	fileContent = fs->BufferizeToString(*dep, CFilePath::IsAbsolute(*dep));
		if (!fileContent.Empty())
		{
			if (!_FindAllTextureFunctionParameters(fileContent))
				return false;
		}
	}
	return true;
}

//----------------------------------------------------------------------------

CString CMetalShaderGenerator::GenDefines(const RHI::SShaderDescription &description) const
{
	(void)description;
	return	"\n"
			"#define	METAL_API\n"
			"\n"
			"#define	GLUE(_a, _b)			_a ## _b\n"
			"\n"
			"#define	VEC4_ZERO				float4(0,0,0,0)\n"
			"#define	VEC4_ONE				float4(1,1,1,1)\n"
			"#define	VEC3_ZERO				float3(0,0,0)\n"
			"#define	VEC3_ONE				float3(1,1,1)\n"
			"#define	VEC2_ZERO				float2(0,0)\n"
			"#define	VEC2_ONE				float2(1,1)\n"
			"\n"
			"#define	VEC_GREATER_THAN(_vec1, _vec2)		((_vec1) > (_vec2))\n"
			"#define	VEC_GREAT_THAN_EQ(_vec1, _vec2)		((_vec1) >= (_vec2))\n"
			"#define	VEC_LESS_THAN(_vec1, _vec2)			((_vec1) < (_vec2))\n"
			"#define	VEC_LESS_THAN_EQ(_vec1, _vec2)		((_vec1) <= (_vec2))\n"
			"#define	VEC_EQ(_vec1, _vec2)				((_vec1) == (_vec2))\n"
			"#define	VEC_NOT_EQ(_vec1, _vec2)			((_vec1) != (_vec2))\n"
			"#define	ALL_BOOL(_bvec)						all(_bvec)\n"
			"#define	ANY_BOOL(_bvec)						any(_bvec)\n"
			"\n"
			"#define	vec2					float2\n"
			"#define	vec3					float3\n"
			"#define	vec4					float4\n"
			"\n"
			"#define	mat2					float2x2\n"
			"#define	mat3					float3x3\n"
			"#define	mat4					float4x4\n"
			"\n"
			"#define	dvec2					double2\n"
			"#define	dvec3					double3\n"
			"#define	dvec4					double4\n"
			"\n"
			"#define	ivec2					int2\n"
			"#define	ivec3					int3\n"
			"#define	ivec4					int4\n"
			"\n"
			"#define	uvec2					uint2\n"
			"#define	uvec3					uint3\n"
			"#define	uvec4					uint4\n"
			"\n"
			"#define	bvec2					bool2\n"
			"#define	bvec3					bool3\n"
			"#define	bvec4					bool4\n"
			"\n"
			"#define	BUILD_MAT2(_axisX, _axisY)							float2x2(_axisX, _axisY)\n"
			"#define	BUILD_MAT3(_axisX, _axisY, _axisZ)					float3x3(_axisX, _axisY, _axisZ)\n"
			"#define	BUILD_MAT4(_axisX, _axisY, _axisZ, _axisW)			float4x4(_axisX, _axisY, _axisZ, _axisW)\n"
			"\n"
			"#define	MTL_DEREF_SYM(_name)								GLUE(__DEREF_SYM_,_name)\n"			// '->' OR '.' depending on if the constant is a push constant or a buffer
			"#define	MTL_SET_NAME(_name)									GLUE(__CONSTANT_SET_,_name)\n"		// Constant set name from constant var name
			"#define	MTL_TEXTURE_NAME(_name)								MTL_SET_NAME(_name)GLUE(Tex_,_name)\n"
			"#define	MTL_SAMPLER_NAME(_name)								MTL_SET_NAME(_name)GLUE(Sampler_,_name)\n"
			"\n"
			"#define	GET_CONSTANT(_constant, _var)						MTL_SET_NAME(_constant) _constant MTL_DEREF_SYM(_constant) _var\n"
			"#define	GET_RAW_BUFFER(_buffer)								MTL_SET_NAME(_buffer) _buffer\n"
			"\n"
			"#define	TEXTURE_DIMENSIONS(_sampler, _lod, _dimensions)		{ _dimensions = int2(MTL_TEXTURE_NAME(_sampler).get_width(), MTL_TEXTURE_NAME(_sampler).get_height()); }\n"
			"#define	SAMPLE(_sampler, _uv)								MTL_TEXTURE_NAME(_sampler).sample(MTL_SAMPLER_NAME(_sampler), float2((_uv).x, 1.0f - (_uv).y))\n"
			"#define	SAMPLE_CUBE(_sampler, _uv)							MTL_TEXTURE_NAME(_sampler).sample(MTL_SAMPLER_NAME(_sampler), _uv)\n"
			"# define	FETCH(_sampler, _uv)								MTL_TEXTURE_NAME(_sampler).read(uint2((_uv).x, MTL_TEXTURE_NAME(_sampler).get_height() - (_uv).y), uint((_uv).z))\n"	// uv is int3 (z = lod)
			"# define	SAMPLEMS(_sampler, _uv, _sampleID)					MTL_TEXTURE_NAME(_sampler).read(_uv, _sampleID)\n"
			"#define	SAMPLELOD(_sampler, _uv, _lod)						MTL_TEXTURE_NAME(_sampler).sample(MTL_SAMPLER_NAME(_sampler), float2((_uv).x, 1.0f - (_uv).y), level(_lod))\n"
			"#define	SAMPLELOD_CUBE(_sampler, _uv, _lod)					MTL_TEXTURE_NAME(_sampler).sample(MTL_SAMPLER_NAME(_sampler), _uv, level(_lod))\n"
			"#define	SAMPLEGRAD(_sampler, _uv, _ddx, _ddy)				MTL_TEXTURE_NAME(_sampler).sample(MTL_SAMPLER_NAME(_sampler), _uv, gradient2d(_ddx, _ddy), 0.0f)\n"
			"\n"
			"#define	RAW_BUFFER_INDEX(_index)							(_index)\n"
			"\n"
			"#define	LOADU(_name,  _index)								_name[_index]\n"
			"#define	LOADU2(_name, _index)								uvec2(_name[_index], _name[(_index) + 1])\n"
			"#define	LOADU3(_name, _index)								uvec3(_name[_index], _name[(_index) + 1], _name[(_index) + 2])\n"
			"#define	LOADU4(_name, _index)								uvec4(_name[_index], _name[(_index) + 1], _name[(_index) + 2], _name[(_index) + 3])\n"
			"\n"
			"#define	LOADI(_name,  _index)								asint(_name[_index])\n"
			"#define	LOADI2(_name, _index)								ivec2(asint(_name[_index]), asint(_name[(_index) + 1]))\n"
			"#define	LOADI3(_name, _index)								ivec3(asint(_name[_index]), asint(_name[(_index) + 1]), asint(_name[(_index) + 2]))\n"
			"#define	LOADI4(_name, _index)								ivec4(asint(_name[_index]), asint(_name[(_index) + 1]), asint(_name[(_index) + 2]), asint(_name[(_index) + 3]))\n"
			"\n"
			"#define	LOADF(_name,  _index)								asfloat(_name[_index])\n"
			"#define	LOADF2(_name, _index)								vec2(asfloat(_name[_index]), asfloat(_name[(_index) + 1]))\n"
			"#define	LOADF3(_name, _index)								vec3(asfloat(_name[_index]), asfloat(_name[(_index) + 1]), asfloat(_name[(_index) + 2]))\n"
			"#define	LOADF4(_name, _index)								vec4(asfloat(_name[_index]), asfloat(_name[(_index) + 1]), asfloat(_name[(_index) + 2]), asfloat(_name[(_index) + 3]))\n"
			"\n"
			"#define	STOREU(_name,  _index, _value)						_name[_index] = _value;\n"
			"#define	STOREU2(_name, _index, _value)						_name[_index] = (_value).x; _name[(_index) + 1] = (_value).y;\n"
			"#define	STOREU3(_name, _index, _value)						_name[_index] = (_value).x; _name[(_index) + 1] = (_value).y; _name[(_index) + 2] = (_value).z;\n"
			"#define	STOREU4(_name, _index, _value)						_name[_index] = (_value).x; _name[(_index) + 1] = (_value).y; _name[(_index) + 2] = (_value).z; _name[(_index) + 3] = (_value).w;\n"
			"\n"
			"#define	STOREF(_name,  _index, _value)						_name[_index] = asuint(_value);\n"
			"#define	STOREF2(_name, _index, _value)						_name[_index] = asuint((_value).x); _name[(_index) + 1] = asuint((_value).y);\n"
			"#define	STOREF3(_name, _index, _value)						_name[_index] = asuint((_value).x); _name[(_index) + 1] = asuint((_value).y); _name[(_index) + 2] = asuint((_value).z);\n"
			"#define	STOREF4(_name, _index, _value)						_name[_index] = asuint((_value).x); _name[(_index) + 1] = asuint((_value).y); _name[(_index) + 2] = asuint((_value).z); _name[(_index) + 3] = asuint((_value).w);\n"
			"\n"
			"#define	F32TOF16(_x)										((uint)as_type<ushort>((half)(_x)))\n"
			"\n"
			"#define	ATOMICADD(_name, _index, _value, _original)			_original = atomic_fetch_add_explicit((device _atomic<uint>*)&_name[_index], _value, memory_order_relaxed)\n"
			"\n"
			"#define	GROUPMEMORYBARRIERWITHGROUPSYNC()					threadgroup_barrier(mem_flags::mem_threadgroup)\n"
			"\n"
			"#define	IMAGE_LOAD(_name, _uv)								MTL_TEXTURE_NAME(_name).read(CAST(uint2, _uv))\n"
			"#define	IMAGE_STORE(_name, _uv, _value)						MTL_TEXTURE_NAME(_name).write(_value, CAST(uint2, _uv))\n"
			"\n"
			"#define	asint(x)											as_type<int>(x)\n"
			"#define	asuint(x)											as_type<uint>(x)\n"
			"#define	asfloat(x)											as_type<float>(x)\n"
			"\n"
			"#define	mul(_a, _b)											((_a) * (_b))\n"
			"#define	mod(_a, _b)											fmod(_a, _b)\n"
			"\n"
			"#define	atan(_a, _b)										atan2(_a, _b)"
			"\n"
			"#define	ARRAY_BEGIN(_type)									{\n"
			"#define	ARRAY_END()											}\n"
			"\n"
			"#define	SAMPLER2D_DCL_ARG(_sampler)							texture2d<float> MTL_TEXTURE_NAME(_sampler), sampler MTL_SAMPLER_NAME(_sampler)\n"
			"#define	SAMPLER2DMS_DCL_ARG(_sampler)						texture2d_ms<float> MTL_TEXTURE_NAME(_sampler)\n"
			"\n"
			"#define	SAMPLER_ARG(_sampler)								MTL_TEXTURE_NAME(_sampler), MTL_SAMPLER_NAME(_sampler)\n"
			"#define	SAMPLERMS_ARG(_sampler)								MTL_TEXTURE_NAME(_sampler)\n"
			"\n"
			"#define	SELECT(_x, _y, _a)									select(_x, _y, _a)\n"
			"#define	SATURATE(_value)									saturate(_value)\n"
			"#define	CAST(_type, _value)									static_cast<_type>(_value)\n"
			"\n"
			"#define	CROSS(_a, _b)										cross(_a, _b)\n"
			"\n"
			"#define	IN(_type)											_type \n"
			"#define	OUT(_type)											thread _type &\n"
			"#define	INOUT(_type)										thread _type &\n"
			"\n"
			"#define	discard												discard_fragment()\n"
			"\n"
			"#define	rsqrt(x)											(1.0 / sqrt(x))\n"
			"#define	GET_MATRIX_X_AXIS(_mat)								(_mat)[0]\n"
			"#define	GET_MATRIX_Y_AXIS(_mat)								(_mat)[1]\n"
			"#define	GET_MATRIX_Z_AXIS(_mat)								(_mat)[2]\n"
			"#define	GET_MATRIX_W_AXIS(_mat)								(_mat)[3]\n"
			"\n"
			"#define	METAL_INCLUDE										#include <metal_stdlib>\n"
			"\n"
			"#define	GET_GROUPSHARED(_var)								Groupshared._var\n"
			"\n"
			"#define	dFdx(_arg)											dfdx(_arg)\n"
			"#define	dFdy(_arg)											dfdy(_arg)\n"
			"\n"
			"#define	UNROLL\n"
			"\n";
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenHeader(RHI::EShaderStage stage) const
{
	(void)stage;
	return "METAL_INCLUDE\nusing namespace metal;\n";
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenVertexInputs(const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs)
{
	CString	vertexInputsStr;

	// real input struct
	vertexInputsStr += "\nstruct\t__SVertexInput\n{\n";
	PK_FOREACH(attribute, vertexInputs)
	{
		if (RHI::VarType::GetRowNumber(attribute->m_Type) == 1)
		{
			vertexInputsStr += CString::Format("\t%s %s [[attribute(%u)]];\n",
												RHI::HlslShaderTypes::GetTypeString(attribute->m_Type).Data(),
												attribute->m_Name.Data(),
												attribute->m_ShaderLocationBinding);
		}
		else
		{
			for (u32 i = 0; i < RHI::VarType::GetRowNumber(attribute->m_Type); ++i)
			{
				RHI::EVarType	rowType = RHI::VarType::GetRowType(attribute->m_Type);

				vertexInputsStr += CString::Format("\t%s %s_%u [[attribute(%u)]];\n",
													RHI::HlslShaderTypes::GetTypeString(rowType).Data(),
													attribute->m_Name.Data(),
													i,
													attribute->m_ShaderLocationBinding + i);
			}
		}
	}
	vertexInputsStr += "};\n";
	// struct
	vertexInputsStr += "\nstruct\tSVertexInput\n{\n";
	PK_FOREACH(attribute, vertexInputs)
	{
		vertexInputsStr += CString::Format("\t%s %s;\n", RHI::HlslShaderTypes::GetTypeString(attribute->m_Type).Data(), attribute->m_Name.Data());
	}
	vertexInputsStr += "\tuint VertexIndex;\n";
	vertexInputsStr += "\tuint InstanceId;\n";
	vertexInputsStr += "};\n";
	return vertexInputsStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenVertexOutputs(const TMemoryView<const RHI::SVertexOutput> &vertexOutputs, bool toFragment)
{
	(void)toFragment;
	CString	vertexOutputsStr;
	u32		outputLocation = 0;

	// real input struct
	vertexOutputsStr += "\nstruct\tSVertexOutput\n{\n";
	PK_FOREACH(interpolated, vertexOutputs)
	{
		vertexOutputsStr += CString::Format("\t%s %s [[user(attribute%u)]];\n",
											RHI::HlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data(),
											outputLocation);
		++outputLocation;
	}
	vertexOutputsStr += "\tfloat4 VertexPosition [[position]];\n";
	vertexOutputsStr += "};\n";
	return vertexOutputsStr;
}

//-----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenGeometryInputs(const TMemoryView<const RHI::SVertexOutput> &geometryInputs, const RHI::EDrawMode drawMode)
{
	(void)geometryInputs; (void)drawMode;
	PK_RELEASE_ASSERT_NOT_REACHED_MESSAGE("Metal does not handle geometry shaders");
	return "";
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenGeometryOutputs(const RHI::SGeometryOutput &geometryOutputs)
{
	(void)geometryOutputs;
	PK_RELEASE_ASSERT_NOT_REACHED_MESSAGE("Metal does not handle geometry shaders");
	return "";
}

//-----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenFragmentInputs(const TMemoryView<const RHI::SVertexOutput> &fragmentInputs)
{
	CString	fragInputsStr;
	u32		outputLocation = 0;

	// struct
	fragInputsStr += "\nstruct\tSFragmentInput\n{\n";
	PK_FOREACH(interpolated, fragmentInputs)
	{
		fragInputsStr += CString::Format("\t%s %s [[user(attribute%u)]];\n",
										RHI::HlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
										interpolated->m_Name.Data(),
										outputLocation);
		++outputLocation;
	}
	fragInputsStr += "\tfloat4 VertexPosition [[position]];\n";
	fragInputsStr += "\tbool IsFrontFace[[front_facing]];\n";
	fragInputsStr += "};\n";
	return fragInputsStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenFragmentOutputs(const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs)
{
	CString	fragmentOutputsStr;
	u32		outputLocation = 0;

	fragmentOutputsStr += "\nstruct\tSFragmentOutput\n{\n";
	PK_FOREACH(outBuffer, fragmentOutputs)
	{
		bool			isDepth = false;
		RHI::EVarType	outType = RHI::VarType::PixelFormatToType(outBuffer->m_Type, isDepth);
		if (isDepth == false)
		{
			fragmentOutputsStr += CString::Format("\t%s %s [[color(%u)]];\n",
													RHI::HlslShaderTypes::GetTypeString(outType).Data(),
													outBuffer->m_Name.Data(),
													outputLocation++);
		}
		else
		{
			fragmentOutputsStr += CString::Format("\t%s %s [[depth(any)]];\n",
													RHI::HlslShaderTypes::GetTypeString(outType).Data(),
													outBuffer->m_Name.Data());
		}
	}
	fragmentOutputsStr += "};\n";
	return fragmentOutputsStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenConstantSets(const TMemoryView<const RHI::SConstantSetLayout> &constSet, RHI::EShaderStage stage)
{
	CString		mainArgs;
	CString		stageArgs;
	CString		stageParams;
	CString		constantToSetNames;

	if (stage == RHI::VertexShaderStage)
	{
		mainArgs = "#define __VS_MAIN_ARGS\t";
		stageArgs = "#define __VS_ARGS\t";
		stageParams = "#define __VS_PARAMS\t";
	}
	else if (stage == RHI::FragmentShaderStage)
	{
		mainArgs = "#define __FS_MAIN_ARGS\t";
		stageArgs = "#define __FS_ARGS\t";
		stageParams = "#define __FS_PARAMS\t";
	}
	else if (stage == RHI::ComputeShaderStage)
	{
		mainArgs = "#define __CS_MAIN_ARGS\t";
		stageArgs = "#define __CS_ARGS\t";
		stageParams = "#define __CS_PARAMS\t";
	}

	CString		constantsStr;

	PK_FOREACH(constantSet, constSet)
	{
		if (constantSet->m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage))
		{
			u32				bindIdx = 0;
			CString			constSetGeneratedName = CString::Format("ConstantSet%u", constantSet->m_PerSetBinding.Get());
			const char 		*constSetName = constSetGeneratedName.Data();
			CString 		constSetContent;
			CString 		constantStructures;

			PK_FOREACH(constant, constantSet->m_Constants)
			{
				if (constant->m_Type == RHI::TypeConstantBuffer)
				{
					const char	*structName = constant->m_ConstantBuffer.m_Name.Data();

					constantToSetNames += CString::Format("#define __CONSTANT_SET_%s\t%s.\n", structName, constSetName);
					constantToSetNames += CString::Format("#define __DEREF_SYM_%s\t->\n", structName);

					constantStructures += CString::Format("struct\tS%s{\n", structName);
					PK_FOREACH(var, constant->m_ConstantBuffer.m_Constants)
					{
						constantStructures += CString::Format("\t%s %s", RHI::HlslShaderTypes::GetTypeString(var->m_Type).Data(), var->m_Name.Data());
						if (var->m_ArraySize >= 1)
							constantStructures += CString::Format("[%u];\n", var->m_ArraySize);
						else
							constantStructures += ";\n";
					}
					constantStructures += CString::Format("};\n\n");

					constSetContent += CString::Format("\tconstant S%s *%s [[id(%u)]];\n", structName, structName, bindIdx++);

				}
				else if (constant->m_Type == RHI::TypeRawBuffer)
				{
					const char	*bufferName = constant->m_RawBuffer.m_Name.Data();

					constantToSetNames += CString::Format("#define __CONSTANT_SET_%s\t%s.\n", bufferName, constSetName);
					const char	*addrSpace = constant->m_RawBuffer.m_ReadOnly ? "constant" : "device";
					constSetContent += CString::Format("\t%s uint *%s [[id(%u)]];\n", addrSpace, bufferName, bindIdx++);
				}
				else if (constant->m_Type == RHI::TypeTextureStorage)
				{
					const char	*texName = constant->m_TextureStorage.m_Name.Data();

					constantToSetNames += CString::Format("#define __CONSTANT_SET_%s\t%s.\n", texName, constSetName);
					const char	*access = constant->m_TextureStorage.m_ReadOnly ? "access::read" : "access::read_write";
					constSetContent += CString::Format("\ttexture2d<float, %s> Tex_%s [[id(%u)]];\n", access, texName, bindIdx++);
				}
				else if (constant->m_Type == RHI::TypeConstantSampler)
				{
					const char	*texName = constant->m_ConstantSampler.m_Name.Data();

					constantToSetNames += CString::Format("#define __CONSTANT_SET_%s\t%s.\n", texName, constSetName);

					if (constant->m_ConstantSampler.m_Type == RHI::SamplerTypeCube)
					{
						constSetContent += CString::Format("\ttexturecube<float> Tex_%s [[id(%u)]];\n", texName, bindIdx++);
						constSetContent += CString::Format("\tsampler Sampler_%s [[id(%u)]];\n", texName, bindIdx++);
					}
					else if (constant->m_ConstantSampler.m_Type == RHI::SamplerTypeMulti)
					{
						constSetContent += CString::Format("\ttexture2d_ms<float> Tex_%s [[id(%u)]];\n", texName, bindIdx++);
						constSetContent += CString::Format("\tsampler Sampler_%s [[id(%u)]];\n", texName, bindIdx++);
					}
					else
					{
						constSetContent += CString::Format("\ttexture2d<float> Tex_%s [[id(%u)]];\n", texName, bindIdx++);
						constSetContent += CString::Format("\tsampler Sampler_%s [[id(%u)]];\n", texName, bindIdx++);
					}
				}
			}

			constantsStr += constantStructures;
			constantsStr += CString::Format("struct\tS%s\n{\n", constSetName);
			constantsStr += constSetContent;
			constantsStr += "};\n";

			mainArgs += CString::Format(", constant S%s &%s [[buffer(%u)]]", constSetName, constSetName, static_cast<u32>(constantSet->m_PerSetBinding));
			stageArgs += CString::Format(", constant S%s &%s", constSetName, constSetName);
			stageParams += CString::Format(", %s", constSetName);
		}
	}

	PK_FOREACH(identifier, m_TextureParameters)
	{
		constantToSetNames += CString::Format("#define __CONSTANT_SET_%s\n", identifier->Data());
	}

	constantsStr += "\n" + mainArgs;
	constantsStr += "\n" + stageArgs;
	constantsStr += "\n" + stageParams;
	constantsStr += "\n" + constantToSetNames;

	return constantsStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenPushConstants(const TMemoryView<const RHI::SPushConstantBuffer> &pushConstants, RHI::EShaderStage stage)
{
	CString		mainArgs;
	CString		stageArgs;
	CString		stageParams;
	CString		constantToSetNames;

	if (stage == RHI::VertexShaderStage)
	{
		mainArgs = "#define VS_MAIN_ARGS __VS_MAIN_ARGS\t";
		stageArgs = "#define VS_ARGS __VS_ARGS\t";
		stageParams = "#define VS_PARAMS __VS_PARAMS\t";
	}
	else if (stage == RHI::FragmentShaderStage)
	{
		mainArgs = "#define FS_MAIN_ARGS __FS_MAIN_ARGS\t";
		stageArgs = "#define FS_ARGS __FS_ARGS\t";
		stageParams = "#define FS_PARAMS __FS_PARAMS\t";
	}
	else if (stage == RHI::ComputeShaderStage)
	{
		mainArgs = "#define _CS_MAIN_ARGS __CS_MAIN_ARGS\t";
		stageArgs = "#define _CS_ARGS __CS_ARGS\t";
		stageParams = "#define _CS_PARAMS __CS_PARAMS\t";
	}

	CString		pushConstantsStr;

	PK_FOREACH(pushConstant, pushConstants)
	{
		if ((pushConstant->m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			u32			constantIdx = 0;
			const char	*structName = pushConstant->m_Name.Data();

			constantToSetNames += CString::Format("#define __CONSTANT_SET_%s\n", structName);
			constantToSetNames += CString::Format("#define __DEREF_SYM_%s\t.\n", structName);

			pushConstantsStr += CString::Format("struct\tS%s\n{\n", structName);
			PK_FOREACH(var, pushConstant->m_Constants)
			{
				pushConstantsStr += CString::Format("\t%s %s", RHI::HlslShaderTypes::GetTypeString(var->m_Type).Data(), var->m_Name.Data());
				if (var->m_ArraySize >= 1)
					pushConstantsStr += CString::Format(" [[id(%u)]] [%u];\n", constantIdx++, var->m_ArraySize);
				else
					pushConstantsStr += CString::Format(" [[id(%u)]];\n", constantIdx++);
			}
			pushConstantsStr += CString::Format("};\n");

			mainArgs += CString::Format(", constant S%s &%s [[buffer(%u)]]", structName, structName, static_cast<u32>(pushConstant->m_PerBlockBinding));
			stageArgs += CString::Format(", constant S%s &%s", structName, structName);
			stageParams += CString::Format(", %s", structName);
		}
	}

	pushConstantsStr += "\n" + mainArgs;
	pushConstantsStr += "\n" + stageArgs;
	pushConstantsStr += "\n" + stageParams;
	pushConstantsStr += "\n" + constantToSetNames;

	return pushConstantsStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenGroupsharedVariables(const TMemoryView<const RHI::SGroupsharedVariable> &groupsharedVariables)
{
	CString	groupsharedStr;
	CString	mainArgs = "#define CS_MAIN_ARGS _CS_MAIN_ARGS\t";
	CString	stageArgs = "#define CS_ARGS _CS_ARGS\t";
	CString	stageParams = "#define CS_PARAMS _CS_PARAMS\t";

	if (!groupsharedVariables.Empty())
	{
		groupsharedStr += CString::Format("struct\tSGroupshared\n{\n");
		for (auto &variable : groupsharedVariables)
		{
			groupsharedStr += CString::Format(	"\t%s %s[%u];\n",
												RHI::HlslShaderTypes::GetTypeString(variable.m_Type).Data(),
												variable.m_Name.Data(),
												variable.m_ArraySize);
		}
		groupsharedStr += CString::Format("};\n");

		mainArgs += ", threadgroup SGroupshared &Groupshared [[threadgroup(0)]]";
		stageArgs += ", threadgroup SGroupshared &Groupshared";
		stageParams += ", Groupshared";
	}

	groupsharedStr += "\n" + mainArgs;
	groupsharedStr += "\n" + stageArgs;
	groupsharedStr += "\n" + stageParams;

	return groupsharedStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenVertexMain(	const TMemoryView<const RHI::SVertexAttributeDesc>	&vertexInputs,
													const TMemoryView<const RHI::SVertexOutput>			&vertexOutputs,
													const TMemoryView<const CString>					&funcToCall,
													bool												outputClipspacePosition)
{
	(void)vertexOutputs; (void)outputClipspacePosition;
	CString	mainStr;

	// Shader main
	mainStr += "vertex SVertexOutput\tvert_main(__SVertexInput _vInput [[stage_in]], uint vertexID [[vertex_id]], uint instanceID [[instance_id]] VS_MAIN_ARGS)\n{\n";
	mainStr += "\tSVertexInput	vInput;\n";
	mainStr += "\tSVertexOutput	vOutput;\n\n";

	PK_FOREACH(attribute, vertexInputs)
	{
		if (RHI::VarType::GetRowNumber(attribute->m_Type) == 1)
		{
			mainStr += CString::Format("\tvInput.%s = _vInput.%s;\n", attribute->m_Name.Data(), attribute->m_Name.Data());
		}
		else
		{
			for (u32 i = 0; i < RHI::VarType::GetRowNumber(attribute->m_Type); ++i)
			{
				mainStr += CString::Format("\tvInput.%s[%u] = _vInput.%s_%u;\n", attribute->m_Name.Data(), i, attribute->m_Name.Data(), i);
			}
		}
	}
	mainStr += "\tvInput.VertexIndex = vertexID;\n";
	mainStr += "\tvInput.InstanceId = instanceID;\n\n";
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "\n	" + *func + "(vInput, vOutput VS_PARAMS);\n";
	}
	mainStr += "\treturn vOutput;\n";
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenGeometryMain(const TMemoryView<const RHI::SVertexOutput>	&geometryInputs,
													const RHI::SGeometryOutput					&geometryOutputs,
													const TMemoryView<const CString>			&funcToCall,
													const RHI::EDrawMode						primitiveType)
{
	(void)geometryInputs; (void)geometryOutputs; (void)funcToCall; (void)primitiveType;
	PK_RELEASE_ASSERT_NOT_REACHED_MESSAGE("Metal does not handle geometry shaders");
	return "";
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenFragmentMain(const TMemoryView<const RHI::SVertexOutput>		&vertexOutputs,
													const TMemoryView<const RHI::SFragmentOutput>	&fragmentOutputs,
													const TMemoryView<const CString>				&funcToCall)
{
	(void)vertexOutputs; (void)fragmentOutputs; (void)funcToCall;
	CString	mainStr;

	// Shader main
	mainStr += "fragment SFragmentOutput\tfrag_main(SFragmentInput fInput [[stage_in]] FS_MAIN_ARGS)\n{\n";
	mainStr += "\tSFragmentOutput	fOutput;\n\n";
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "	" + *func + "(fInput, fOutput FS_PARAMS);\n";
	}
	mainStr += "\treturn fOutput;\n";
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenGeometryEmitVertex(const RHI::SGeometryOutput &geometryOutputs, bool outputClipspacePosition)
{
	(void)geometryOutputs; (void)outputClipspacePosition;
	PK_RELEASE_ASSERT_NOT_REACHED_MESSAGE("Metal does not handle geometry shaders");
	return "";
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenGeometryEndPrimitive(const RHI::SGeometryOutput &geometryOutputs)
{
	(void)geometryOutputs;
	PK_RELEASE_ASSERT_NOT_REACHED_MESSAGE("Metal does not handle geometry shaders");
	return "";
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenComputeInputs()
{
	return	"\nstruct SComputeInput\n"
			"{\n"
			"	uint3 GlobalThreadID;\n"
			"	uint3 LocalThreadID;\n"
			"	uint3 GroupID;\n"
			"};\n";
}

//----------------------------------------------------------------------------

CString		CMetalShaderGenerator::GenComputeMain(	const CUint3						dispatchSize,
													const TMemoryView<const CString>	&funcToCall)
{
	(void)dispatchSize;
	CString	mainStr;

	// Shader main
	mainStr +=	"kernel void\tcomp_main(uint3 globalThreadID [[thread_position_in_grid]], uint3 localThreadID [[thread_position_in_threadgroup]], uint3 groupID [[threadgroup_position_in_grid]] CS_MAIN_ARGS)\n"
				"{\n"
				"	SComputeInput	cInput;\n"
				"	cInput.GlobalThreadID = globalThreadID;\n"
				"	cInput.LocalThreadID = localThreadID;\n"
				"	cInput.GroupID = groupID;\n";
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "\n	" + *func + "(cInput CS_PARAMS);\n";
	}
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

bool	CMetalShaderGenerator::_FindAllTextureFunctionParameters(const CString &shaderCode)
{
	_FindMacroParametersInShader(shaderCode, "SAMPLER2D_DCL_ARG");
	_FindMacroParametersInShader(shaderCode, "SAMPLER2DMS_DCL_ARG");
	return true;
}

//----------------------------------------------------------------------------

bool	CMetalShaderGenerator::_FindMacroParametersInShader(const CString &shaderCode, const CStringView &macro)
{
	const char	*shaderCodePtr = shaderCode.Data();
	const u32	shaderCodeLength = shaderCode.Length();
	u32			shaderCharIdx = 0;

	while (shaderCharIdx < shaderCodeLength)
	{
		// Skip comment '//':
		if (shaderCharIdx + 1 < shaderCodeLength &&
			shaderCodePtr[shaderCharIdx] == '/' &&
			shaderCodePtr[shaderCharIdx + 1] == '/')
		{
			shaderCharIdx += 2;
			if (shaderCharIdx < shaderCodeLength)
			{
				// Jump to EOL:
				KR_BUFFER_JUMP2EOL(shaderCodePtr, shaderCharIdx);
			}
		}
		// Skip comment '/* */'
		if (shaderCharIdx + 1 < shaderCodeLength &&
			shaderCodePtr[shaderCharIdx] == '/' &&
			shaderCodePtr[shaderCharIdx + 1] == '*')
		{
			shaderCharIdx += 2;
			while (shaderCharIdx + 1 < shaderCodeLength)
			{
				if (shaderCodePtr[shaderCharIdx] == '*' &&
					shaderCodePtr[shaderCharIdx + 1] == '/')
				{
					shaderCharIdx += 2;
					break;
				}
				else if (shaderCodePtr[shaderCharIdx] == '\\')
					shaderCharIdx += 2;
				else
					shaderCharIdx += 1;
			}
		}
		// Search for macro:
		u32		cmpLength = 0;
		while (cmpLength < macro.Length() && shaderCharIdx + cmpLength < shaderCodeLength)
		{
			if (shaderCodePtr[shaderCharIdx + cmpLength] != macro[cmpLength])
				break;
			++cmpLength;
		}
		if (cmpLength == macro.Length()) // We found the macro
		{
			shaderCharIdx += cmpLength;
			// Skip the spaces IFN:
			while (KR_BUFFER_IS_SPACE(shaderCodePtr[shaderCharIdx]))
				++shaderCharIdx;
			// Check the open parenthesis:
			if (shaderCodePtr[shaderCharIdx] == '(')
			{
				++shaderCharIdx;
				// Skip the spaces IFN:
				while (KR_BUFFER_IS_SPACE(shaderCodePtr[shaderCharIdx]))
					++shaderCharIdx;
				// Get the identifier name:
				u32		identifierStart = shaderCharIdx;
				if (KR_BUFFER_IS_IDST(shaderCodePtr[shaderCharIdx])) // Needs to start with an non-digit character
				{
					++shaderCharIdx;
					while (KR_BUFFER_IS_IDSTNUM(shaderCodePtr[shaderCharIdx]))
						++shaderCharIdx;
					u32		identifierEnd = shaderCharIdx;
					// Skip the spaces IFN:
					while (KR_BUFFER_IS_SPACE(shaderCodePtr[shaderCharIdx]))
						++shaderCharIdx;
					// Check the closing parenthesis:
					if (shaderCodePtr[shaderCharIdx] == ')')
					{
						++shaderCharIdx;
						CString		identifierFound = CString(shaderCodePtr + identifierStart, identifierEnd - identifierStart);
						CLog::Log(PK_INFO, "Found identifier '%s'", identifierFound.Data());
						if (!m_TextureParameters.Contains(identifierFound))
							m_TextureParameters.PushBack(identifierFound);
					}
				}
			}
		}
		else if (cmpLength == 0)
			shaderCharIdx += 1;
		else
			shaderCharIdx += cmpLength;
	}
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)
