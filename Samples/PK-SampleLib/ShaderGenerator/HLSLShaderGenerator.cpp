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
#include "HLSLShaderGenerator.h"

#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)

#include <pk_rhi/include/EnumHelper.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

namespace
{

	const char	*_GetHlslPrimitiveLayoutInputType(const RHI::EDrawMode primitiveType)
	{
		switch (primitiveType)
		{
		case RHI::EDrawMode::DrawModePoint:
			return "point";
		case RHI::EDrawMode::DrawModeLine:
		case RHI::EDrawMode::DrawModeLineStrip:
			return "line";
		case RHI::EDrawMode::DrawModeTriangle:
		case RHI::EDrawMode::DrawModeTriangleStrip:
			return "triangle";
		default:
			PK_ASSERT_NOT_REACHED();
			return null;
		}
	}

	//----------------------------------------------------------------------------

	const char	*_GetHlslPrimitiveLayoutOutputType(const RHI::EDrawMode primitiveType)
	{
		switch (primitiveType)
		{
		case RHI::EDrawMode::DrawModePoint:
			return "PointStream";
		case RHI::EDrawMode::DrawModeLine:
		case RHI::EDrawMode::DrawModeLineStrip:
			return "LineStream";
		case RHI::EDrawMode::DrawModeTriangle:
		case RHI::EDrawMode::DrawModeTriangleStrip:
			return "TriangleStream";
		default:
			PK_ASSERT_NOT_REACHED();
			return null;
		}
	}

	//----------------------------------------------------------------------------

	u32		_GetPointNumberInPrimitive(const RHI::EDrawMode primitiveType)
	{
		switch (primitiveType)
		{
		case RHI::EDrawMode::DrawModePoint:
			return 1;
		case RHI::EDrawMode::DrawModeLine:
		case RHI::EDrawMode::DrawModeLineStrip:
			return 2;
		case RHI::EDrawMode::DrawModeTriangle:
		case RHI::EDrawMode::DrawModeTriangleStrip:
			return 3;
		default:
			PK_ASSERT_NOT_REACHED();
			return 0;
		}
	}
}

//----------------------------------------------------------------------------

CString CHLSLShaderGenerator::GenDefines(const RHI::SShaderDescription &description) const
{
	CString	defines =	"\n"
						"#define	DIRECTX_API											\n"
						"\n"
						"#define	GLUE(_a, _b)										_a ## _b\n"
						"\n"
						"#define	VEC4_ZERO											vec4(0,0,0,0)\n"
						"#define	VEC4_ONE											vec4(1,1,1,1)\n"
						"#define	VEC3_ZERO											vec3(0,0,0)\n"
						"#define	VEC3_ONE											vec3(1,1,1)\n"
						"#define	VEC2_ZERO											vec2(0,0)\n"
						"#define	VEC2_ONE											vec2(1,1)\n"
						"\n"
						"#define	VEC_GREATER_THAN(_vec1, _vec2)						((_vec1) > (_vec2))\n"
						"#define	VEC_GREAT_THAN_EQ(_vec1, _vec2)						((_vec1) >= (_vec2))\n"
						"#define	VEC_LESS_THAN(_vec1, _vec2)							((_vec1) < (_vec2))\n"
						"#define	VEC_LESS_THAN_EQ(_vec1, _vec2)						((_vec1) <= (_vec2))\n"
						"#define	VEC_EQ(_vec1, _vec2)								((_vec1) == (_vec2))\n"
						"#define	VEC_NOT_EQ(_vec1, _vec2)							((_vec1) != (_vec2))\n"
						"#define	ALL_BOOL(_bvec)										all(_bvec)\n"
						"#define	ANY_BOOL(_bvec)										any(_bvec)\n"
						"\n"
						"#define	vec2												float2\n"
						"#define	vec3												float3\n"
						"#define	vec4												float4\n"
						"\n"
						"#define	mat2												float2x2\n"
						"#define	mat3												float3x3\n"
						"#define	mat4												float4x4\n"
						"\n"
						"#define	dvec2												double2\n"
						"#define	dvec3												double3\n"
						"#define	dvec4												double4\n"
						"\n"
						"#define	ivec2												int2\n"
						"#define	ivec3												int3\n"
						"#define	ivec4												int4\n"
						"\n"
						"#define	uvec2												uint2\n"
						"#define	uvec3												uint3\n"
						"#define	uvec4												uint4\n"
						"\n"
						"#define	BUILD_MAT2(_axisX, _axisY)							transpose(mat2(_axisX, _axisY))\n"
						"#define	BUILD_MAT3(_axisX, _axisY, _axisZ)					transpose(mat3(_axisX, _axisY, _axisZ))\n"
						"#define	BUILD_MAT4(_axisX, _axisY, _axisZ, _axisW)			transpose(mat4(_axisX, _axisY, _axisZ, _axisW))\n"
						"\n"
						"#define	GET_CONSTANT(_constant, _var)						_var\n"
						"#define	GET_RAW_BUFFER(_buffer)								_buffer\n"
						"\n"
						"#define	CROSS(_a, _b)										cross(_a, _b)\n"
						"\n"
						"#define	DX_TEXTURE_NAME(_name)								GLUE(Tex_,_name)\n"
						"#define	DX_SAMPLER_NAME(_name)								GLUE(Sampler_,_name)\n"
						"\n"
						"#define	TEXTURE_DIMENSIONS(_sampler, _lod, _dimensions)		{ uint _levelCount; DX_TEXTURE_NAME(_sampler).GetDimensions(_lod, (_dimensions).x, (_dimensions).y, _levelCount); }\n"
						"#define	SAMPLE(_sampler, _uv)								DX_TEXTURE_NAME(_sampler).Sample(DX_SAMPLER_NAME(_sampler), _uv)\n"
						"#define	SAMPLE_CUBE(_sampler, _uv)							SAMPLE(_sampler, _uv)\n"
						"#define	FETCH(_sampler, _uv)								DX_TEXTURE_NAME(_sampler).Load(_uv)\n" // uv is int3 (z = lod)
						"#define	SAMPLEMS(_samplerMS, _uv, _sampleID)				DX_TEXTURE_NAME(_samplerMS).Load(_uv, _sampleID)\n"
						"#define	SAMPLELOD(_sampler, _uv, _lod)						DX_TEXTURE_NAME(_sampler).SampleLevel(DX_SAMPLER_NAME(_sampler), _uv, _lod)\n"
						"#define	SAMPLELOD_CUBE(_sampler, _uv, _lod)					SAMPLELOD(_sampler, _uv, _lod)\n"
						"#define	SAMPLEGRAD(_sampler, _uv, _ddx, _ddy)				DX_TEXTURE_NAME(_sampler).SampleGrad(DX_SAMPLER_NAME(_sampler), _uv, _ddx, _ddy)\n"
						"\n"
						"#define	RAW_BUFFER_INDEX(_index)							((_index) * 4)\n" // u32 aligned
						"\n"
						"#define	LOADU(_name,  _index)								_name.Load(_index)\n"
						"#define	LOADU2(_name, _index)								_name.Load2(_index)\n"
						"#define	LOADU3(_name, _index)								_name.Load3(_index)\n"
						"#define	LOADU4(_name, _index)								_name.Load4(_index)\n"
						"\n"
						"#define	LOADI(_name,  _index)								asint(_name.Load(_index))\n"
						"#define	LOADI2(_name, _index)								asint(_name.Load2(_index))\n"
						"#define	LOADI3(_name, _index)								asint(_name.Load3(_index))\n"
						"#define	LOADI4(_name, _index)								asint(_name.Load4(_index))\n"
						"\n"
						"#define	LOADF(_name,  _index)								asfloat(_name.Load(_index))\n"
						"#define	LOADF2(_name, _index)								asfloat(_name.Load2(_index))\n"
						"#define	LOADF3(_name, _index)								asfloat(_name.Load3(_index))\n"
						"#define	LOADF4(_name, _index)								asfloat(_name.Load4(_index))\n"
						"\n"
						"#define	STOREU(_name,  _index, _value)						_name.Store(_index, _value)\n"
						"#define	STOREU2(_name, _index, _value)						_name.Store2(_index, _value)\n"
						"#define	STOREU3(_name, _index, _value)						_name.Store3(_index, _value)\n"
						"#define	STOREU4(_name, _index, _value)						_name.Store4(_index, _value)\n"
						"\n"
						"#define	STOREF(_name,  _index, _value)						_name.Store(_index, asuint(_value))\n"
						"#define	STOREF2(_name, _index, _value)						_name.Store2(_index, asuint(_value))\n"
						"#define	STOREF3(_name, _index, _value)						_name.Store3(_index, asuint(_value))\n"
						"#define	STOREF4(_name, _index, _value)						_name.Store4(_index, asuint(_value))\n"
						"\n"
						"#define	F32TOF16(_x)										f32tof16(_x)\n"
						"\n"
						"#define	ATOMICADD(_name, _index, _value, _original)			_name.InterlockedAdd(_index, asuint(_value), _original)\n"
						"\n"
						"#define	GROUPMEMORYBARRIERWITHGROUPSYNC()					GroupMemoryBarrierWithGroupSync()\n"
						"\n"
						"#define	IMAGE_LOAD(_name, _uv)								_name[_uv]\n"
						"#define	IMAGE_STORE(_name, _uv, _value)						_name[_uv] = _value\n"
						"\n"
						"#define	ARRAY_BEGIN(_type)									{\n"
						"#define	ARRAY_END()											}\n"
						"\n"
						"#define	SAMPLER2D_DCL_ARG(_sampler)							Texture2D DX_TEXTURE_NAME(_sampler), SamplerState DX_SAMPLER_NAME(_sampler)\n"
						"#define	SAMPLER2DMS_DCL_ARG(_sampler)						Texture2DMS<float4> DX_TEXTURE_NAME(_sampler)\n"
						"\n"
						"#define	SAMPLER_ARG(_sampler)								DX_TEXTURE_NAME(_sampler), DX_SAMPLER_NAME(_sampler)\n"
						"#define	SAMPLERMS_ARG(_sampler)								DX_TEXTURE_NAME(_sampler)\n"
						"\n"
						"#define	SATURATE(_value)									saturate(_value)\n"
						"#define	CAST(_type, _value)									((_type) (_value))\n"
						"\n"
						"#define	mod(_x, _y)											fmod(_x, _y)\n"
						"#define	fract(_x)											frac(_x)\n"
						"#define	mix(_x, _y, _s)										lerp(_x, _y, _s)\n"
						"#define	atan(_x, _y)										atan2(_x, _y)\n"
						"\n"
						"#define	IN(_type)											in _type\n"
						"#define	OUT(_type)											out _type\n"
						"#define	INOUT(_type)										inout _type\n"
						"\n"
						"#define	GS_PARAMS											, _outputStream\n"
						"#define	VS_PARAMS\n"
						"#define	VS_ARGS\n"
						"#define	FS_PARAMS\n"
						"#define	FS_ARGS\n"
						"#define	CS_PARAMS\n"
						"#define	CS_ARGS\n"
						"\n"
						"#define	GET_MATRIX_X_AXIS(_mat)								float4((_mat)[0].x, (_mat)[1].x, (_mat)[2].x, (_mat)[3].x)\n"
						"#define	GET_MATRIX_Y_AXIS(_mat)								float4((_mat)[0].y, (_mat)[1].y, (_mat)[2].y, (_mat)[3].y)\n"
						"#define	GET_MATRIX_Z_AXIS(_mat)								float4((_mat)[0].z, (_mat)[1].z, (_mat)[2].z, (_mat)[3].z)\n"
						"#define	GET_MATRIX_W_AXIS(_mat)								float4((_mat)[0].w, (_mat)[1].w, (_mat)[2].w, (_mat)[3].w)\n"
						"\n"
						"#define	dFdx(_arg)											ddx(_arg)\n"
						"#define	dFdy(_arg)											ddy(_arg)\n"
						"\n"
						"#define	GET_GROUPSHARED(_var)								_var\n"
						"\n"
						"#define	UNROLL												[unroll]\n"
						"\n";

	if (description.m_Pipeline == RHI::VsGsPs)
	{
		switch (description.m_GeometryOutput.m_PrimitiveType)
		{
		case RHI::DrawModePoint:
			defines += "#define		GS_ARGS					, inout PointStream<SGeometryOutput> _outputStream\n";
			break;
		case RHI::DrawModeLine:
		case RHI::DrawModeLineStrip:
			defines += "#define		GS_ARGS					, inout LineStream<SGeometryOutput> _outputStream\n";
			break;
		case RHI::DrawModeTriangle:
		case RHI::DrawModeTriangleStrip:
			defines += "#define		GS_ARGS					, inout TriangleStream<SGeometryOutput> _outputStream\n";
			break;
		default:
			PK_ASSERT_NOT_REACHED();
		}
	}

	return defines;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenHeader(RHI::EShaderStage stage) const
{
	(void)stage;
	return "";
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenVertexInputs(const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs)
{
	CString	vertexInputsStr;

	// struct VtxInput
	vertexInputsStr += "\nstruct	SVertexInput\n{\n";
	PK_FOREACH(attribute, vertexInputs)
	{
		vertexInputsStr += CString::Format(	"	%s %s : TEXCOORD%u;\n",
											RHI::HlslShaderTypes::GetTypeString(attribute->m_Type).Data(),
											attribute->m_Name.Data(),
											attribute->m_ShaderLocationBinding);
	}
	vertexInputsStr += "	uint VertexIndex : SV_VertexID;\n";
	vertexInputsStr += "	uint InstanceId : SV_InstanceID;\n";
	vertexInputsStr += "};\n";
	return vertexInputsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenVertexOutputs(const TMemoryView<const RHI::SVertexOutput> &vertexOutputs, bool toFragment)
{
	(void)toFragment;
	CString	vertexOutputsStr;
	u32		idx = 0;

	// struct VtxOutput
	vertexOutputsStr += "\nstruct	SVertexOutput\n{\n";
	PK_FOREACH(interpolated, vertexOutputs)
	{
		vertexOutputsStr += CString::Format("	%s %s %s : TEXCOORD%u;\n",
											RHI::HlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
											RHI::HlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data(),
											idx);
		idx += RHI::VarType::GetRowNumber(interpolated->m_Type);
	}
	vertexOutputsStr += "	float4 VertexPosition : SV_POSITION;\n";
	vertexOutputsStr += "};\n";
	return vertexOutputsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenGeometryInputs(const TMemoryView<const RHI::SVertexOutput>& geometryInputs, const RHI::EDrawMode drawMode)
{
	PK_ASSERT_MESSAGE(drawMode != RHI::DrawModeInvalid, "Geometry shader input primitive type is not set");

	const u32	pointNumberInput = _GetPointNumberInPrimitive(drawMode);
	u32			inputLocation = 0;

	CString		geomInputsStr;
	geomInputsStr += "\nstruct	SPrimitives\n{\n";
	PK_FOREACH(attribute, geometryInputs)
	{
		geomInputsStr += CString::Format(	"	%s %s %s : TEXCOORD%u;\n",
											RHI::HlslShaderTypes::GetInterpolationString(attribute->m_Interpolation).Data(),
											RHI::GlslShaderTypes::GetTypeString(attribute->m_Type).Data(),
											attribute->m_Name.Data(),
											inputLocation);
		inputLocation += RHI::VarType::GetRowNumber(attribute->m_Type);
	}
	geomInputsStr += "	float4 VertexPosition : SV_POSITION;\n";
	geomInputsStr += "};\n";

	geomInputsStr += "\nstruct	SGeometryInput\n{\n";
	geomInputsStr += CString::Format(	"	SPrimitives Primitives[%d];\n", pointNumberInput);
	geomInputsStr += "	int PrimitiveId;\n";
	geomInputsStr += "};\n";

	return geomInputsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenGeometryOutputs(const RHI::SGeometryOutput& geometryOutputs)
{
	CString	geometryOutputsStr;
	u32		outputLocation = 0;

	PK_ASSERT_MESSAGE(geometryOutputs.m_MaxVertices != 0, "Geometry can emmit a maximum of 0 vertices... Set the value to something appropriate");

	// struct
	geometryOutputsStr += "\nstruct	SGeometryOutput\n{\n";
	PK_FOREACH(interpolated, geometryOutputs.m_GeometryOutput)
	{
		geometryOutputsStr += CString::Format(	"	%s %s %s : TEXCOORD%u;\n",
												RHI::HlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
												RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
												interpolated->m_Name.Data(),
												outputLocation);
		outputLocation += RHI::VarType::GetRowNumber(interpolated->m_Type);
	}
	geometryOutputsStr += "	float4 VertexPosition : SV_POSITION;\n";
	geometryOutputsStr += "};\n";

	return geometryOutputsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenFragmentInputs(const TMemoryView<const RHI::SVertexOutput> &fragmentInputs)
{
	CString	fragInputsStr;
	u32		idx = 0;

	// struct VtxInput
	fragInputsStr += "\nstruct	SFragmentInput\n{\n";
	PK_FOREACH(interpolated, fragmentInputs)
	{
		fragInputsStr += CString::Format(	"	%s %s %s : TEXCOORD%u;\n",
											RHI::HlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
											RHI::HlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data(),
											idx);
		idx += RHI::VarType::GetRowNumber(interpolated->m_Type);
	}
	fragInputsStr += "	bool IsFrontFace : SV_IsFrontFace;\n";
	fragInputsStr += "};\n";
	return fragInputsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenFragmentOutputs(const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs)
{
	CString	fragOutputsStr;
	u32		idx = 0;
	bool	hasDepthOutput = false;

	// struct VtxInput
	fragOutputsStr += "\nstruct	SFragmentOutput\n{\n";
	PK_FOREACH(interpolated, fragmentOutputs)
	{
		bool				isDepth;
		RHI::EVarType		outType;

		outType = RHI::VarType::PixelFormatToType(interpolated->m_Type, isDepth);
		if (isDepth == false)
		{
			fragOutputsStr += CString::Format(	"	%s %s : SV_Target%u;\n",
												RHI::HlslShaderTypes::GetTypeString(outType).Data(),
												interpolated->m_Name.Data(),
												idx++);
		}
		hasDepthOutput |= isDepth;
	}
	if (hasDepthOutput)
		fragOutputsStr += "	float DepthValue : SV_Depth;\n";
	fragOutputsStr += "};\n";
	return fragOutputsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenConstantSets(const TMemoryView<const RHI::SConstantSetLayout> &constSet, RHI::EShaderStage stage)
{
	CString	constantsStr;

	PK_FOREACH(cSet, constSet)
	{
		if ((cSet->m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			PK_FOREACH(constant, cSet->m_Constants)
			{
				if (constant->m_Type == RHI::TypeConstantBuffer)
				{
					const RHI::SConstantBufferDesc & cbuffer = constant->m_ConstantBuffer;
					//constantsStr = CString::Format("cbuffer	%s : register(%s, b%u)\n{\n",
					constantsStr += CString::Format("cbuffer	%s : register(b%u)\n{\n",
													cbuffer.m_Name.Data(),
													cbuffer.m_PerBlockBinding.Get());
					PK_FOREACH(field, cbuffer.m_Constants)
					{
						constantsStr += CString::Format("	%s %s", RHI::HlslShaderTypes::GetTypeString(field->m_Type).Data(), field->m_Name.Data());
						if (field->m_ArraySize >= 1)
							constantsStr += CString::Format("[%u]", field->m_ArraySize);

						const u32	cidx = field->m_OffsetInBuffer / 16;
						const u32	sidx = (field->m_OffsetInBuffer % 16) / 4;
						const char	*swizzle[] = { "x", "y", "z", "w" };
						constantsStr += CString::Format(" : packoffset(c%d.%s);\n", cidx, swizzle[sidx]);
					}
					constantsStr += "};\n";
				}
				else if (constant->m_Type == RHI::TypeRawBuffer)
				{
					const RHI::SRawBufferDesc	&buffer = constant->m_RawBuffer;

					if (buffer.m_ReadOnly)
						constantsStr += CString::Format("ByteAddressBuffer	%s : register(t%u);\n", buffer.m_Name.Data(), buffer.m_PerBlockBinding.Get());
					else
						constantsStr += CString::Format("RWByteAddressBuffer	%s : register(u%u);\n", buffer.m_Name.Data(), buffer.m_PerBlockBinding.Get());
				}
				else if (constant->m_Type == RHI::TypeTextureStorage)
				{
					// TODO: might need to rework and add format in STextureStorageDesc, avoid float4 conversion and could handle correctly uint/int texture
					const RHI::STextureStorageDesc	&texStorage = constant->m_TextureStorage;

					if (texStorage.m_ReadOnly)
						constantsStr += CString::Format("Texture2D	%s : register(t%u);\n", texStorage.m_Name.Data(), texStorage.m_PerBlockBinding.Get());
					else
						constantsStr += CString::Format("RWTexture2D<float4>	%s : register(u%u);\n", texStorage.m_Name.Data(), texStorage.m_PerBlockBinding.Get());
				}
				else if (constant->m_Type == RHI::TypeConstantSampler)
				{
					const RHI::SConstantSamplerDesc	&csampler = constant->m_ConstantSampler;

					constantsStr += CString::Format("%s Tex_%s",
													csampler.m_Type == RHI::SamplerTypeMulti ? "Texture2DMS<float4>" : csampler.m_Type == RHI::SamplerTypeCube ? "TextureCube" : "Texture2D",
													csampler.m_Name.Data());
					constantsStr += CString::Format(" : register(%s, t%u);\n",
													RHI::HlslShaderTypes::GetShaderStageProfileString(stage).Data(),
													csampler.m_PerBlockBinding.Get());

					if (csampler.m_Type != RHI::SamplerTypeMulti)
					{
						constantsStr += CString::Format("SamplerState Sampler_%s", csampler.m_Name.Data());
						constantsStr += CString::Format(" : register(%s, s%u);\n",
														RHI::HlslShaderTypes::GetShaderStageProfileString(stage).Data(),
														csampler.m_PerStageSamplerBinding.Get());
					}
				}
			}
		}
	}

	return constantsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenPushConstants(const TMemoryView<const RHI::SPushConstantBuffer> &pushConstants, RHI::EShaderStage stage)
{
	CString	pushConstantsStr;

	PK_FOREACH(pushConstant, pushConstants)
	{
		if ((pushConstant->m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			//constantsStr = CString::Format("cbuffer	%s : register(%s, b%u)\n{\n",
			pushConstantsStr += CString::Format(	"cbuffer	%s : register(b%u)\n{\n",
												pushConstant->m_Name.Data(),
												//RHI::HlslShaderTypes::GetShaderStageProfileString(stage).Data(),
												pushConstant->m_PerBlockBinding.Get());

			PK_FOREACH(field, pushConstant->m_Constants)
			{
				pushConstantsStr += CString::Format("	%s %s", RHI::HlslShaderTypes::GetTypeString(field->m_Type).Data(), field->m_Name.Data());
				if (field->m_ArraySize >= 1)
					pushConstantsStr += CString::Format("[%u];\n", field->m_ArraySize);
				else
					pushConstantsStr += ";\n";
			}
			pushConstantsStr += "};\n";

		}
	}
	return pushConstantsStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenGroupsharedVariables(const TMemoryView<const RHI::SGroupsharedVariable> &groupsharedVariables)
{
	CString	groupsharedStr;

	for (auto &variable : groupsharedVariables)
	{
		groupsharedStr += CString::Format(	"groupshared %s	%s[%u];\n",
											RHI::HlslShaderTypes::GetTypeString(variable.m_Type).Data(),
											variable.m_Name.Data(),
											variable.m_ArraySize);
	}

	return groupsharedStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenVertexMain(const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs,
												const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
												const TMemoryView<const CString> &funcToCall,
												bool outputClipspacePosition)
{
	(void)vertexInputs; (void)vertexOutputs;
	CString		mainStr;

	// Shader main
	mainStr +=	"void	main(in SVertexInput vInput, out SVertexOutput vOutput)\n"
				"{\n";
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "	" + *func + "(vInput, vOutput);\n";
	}
	if (outputClipspacePosition)
	{
		mainStr += "	vOutput.VertexPosition.y *= -1.0;\n"; // We invert the y in clip space
	}
	mainStr +=	"}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenGeometryMain(	const TMemoryView<const RHI::SVertexOutput> &geometryInputs,
													const RHI::SGeometryOutput &geometryOutputs,
													const TMemoryView<const CString> &funcToCall,
													const RHI::EDrawMode primitiveType)
{
	(void)geometryInputs;
	const u32	pointNumberInput = _GetPointNumberInPrimitive(primitiveType);

	// Shader main
	CString	mainStr;
	mainStr += CString::Format("[maxvertexcount(%d)]\n", geometryOutputs.m_MaxVertices);
	mainStr += CString::Format(	"void	main(%s SPrimitives _input[%d], in uint PrimitiveId : SV_PrimitiveID, inout %s<SGeometryOutput> _output)\n{\n",
								_GetHlslPrimitiveLayoutInputType(primitiveType),
								pointNumberInput,
								_GetHlslPrimitiveLayoutOutputType(geometryOutputs.m_PrimitiveType));
	mainStr +=	"	SGeometryInput	gInput;\n"
				"	SGeometryOutput	gOutput;\n\n"
				"	gInput.Primitives = _input;\n\n"
				"	gInput.PrimitiveId = PrimitiveId;\n\n";
	// Initialize output struct to zero
	PK_FOREACH(geomOutput, geometryOutputs.m_GeometryOutput)
	{
		mainStr += CString::Format("	gOutput.%s = %s(0", geomOutput->m_Name.Data(), RHI::GlslShaderTypes::GetTypeString(geomOutput->m_Type).Data());
		for (u32 i = 1; i < RHI::VarType::VarTypeToComponentSize(geomOutput->m_Type); ++i)
			mainStr += ", 0";
		mainStr += ");\n";
	}
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "	" + *func + "(gInput, gOutput, _output);\n";
	}
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenFragmentMain(	const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
													const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs,
													const TMemoryView<const CString> &funcToCall)
{
	(void)vertexOutputs; (void)fragmentOutputs;
	CString mainStr;

	// Shader main
	mainStr +=	"void	main(in SFragmentInput fInput, out SFragmentOutput fOutput)\n"
				"{\n";
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "	" + *func + "(fInput, fOutput);\n";
	}
	mainStr +=	"}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenGeometryEmitVertex(const RHI::SGeometryOutput& geometryOutputs, bool outputClipspacePosition)
{
//	const u32	pointNumberOutput = _GetPointNumberInPrimitive(geometryOutputs.m_PrimitiveType);

	CString	emitVertexStr;
	emitVertexStr += CString::Format(	"void	AppendVertex(in SGeometryOutput outputData, inout %s<SGeometryOutput> _outputStream)\n{\n",
										_GetHlslPrimitiveLayoutOutputType(geometryOutputs.m_PrimitiveType));

	emitVertexStr += "	SGeometryOutput output;\n";

	PK_FOREACH(attribute, geometryOutputs.m_GeometryOutput)
	{
		emitVertexStr += CString::Format("	output.%s = outputData.%s;\n", attribute->m_Name.Data(), attribute->m_Name.Data());
	}
	emitVertexStr += "	output.VertexPosition = outputData.VertexPosition;\n";
	if (outputClipspacePosition)
	{
		emitVertexStr += "	output.VertexPosition.y *= -1.0;\n";
	}
	emitVertexStr +=	"	_outputStream.Append(output);\n"
						"}";
	return emitVertexStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenGeometryEndPrimitive(const RHI::SGeometryOutput& geometryOutputs)
{
	CString	emitEndStr;
	emitEndStr += CString::Format(	"void	FinishPrimitive(in SGeometryOutput outputData, inout %s<SGeometryOutput> _outputStream)\n{\n",
									_GetHlslPrimitiveLayoutOutputType(geometryOutputs.m_PrimitiveType));
	emitEndStr += "\t_outputStream.RestartStrip();\n";
	emitEndStr += "}\n";
	return emitEndStr;
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenComputeInputs()
{
	return	"\nstruct SComputeInput\n"
			"{\n"
			"	uint3 GlobalThreadID : SV_DispatchThreadID;\n"
			"	uint3 LocalThreadID : SV_GroupThreadID;\n"
			"	uint3 GroupID : SV_GroupID;\n"
			"};\n";
}

//----------------------------------------------------------------------------

CString		CHLSLShaderGenerator::GenComputeMain(	const CUint3						dispatchSize,
													const TMemoryView<const CString>	&funcToCall)
{
	// Shader main
	CString	mainStr;
	mainStr += CString::Format("[numthreads(%u, %u, %u)]\n", dispatchSize.x(), dispatchSize.y(), dispatchSize.z());
	mainStr +=	"void	main(in SComputeInput cInput)\n"
				"{\n";
	for(const CString &func : funcToCall)
	{
		if (!func.Empty())
			mainStr += "	" + func + "(cInput);\n";
	}
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)
