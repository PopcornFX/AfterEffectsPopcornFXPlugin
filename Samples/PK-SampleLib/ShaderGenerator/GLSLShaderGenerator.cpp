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
#include "GLSLShaderGenerator.h"

#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)

#include "pk_rhi/include/EnumHelper.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

namespace
{

	const char	*_GetGlslPrimitiveLayoutType(const RHI::EDrawMode primitiveType)
	{
		switch (primitiveType)
		{
		case RHI::EDrawMode::DrawModePoint:
			return "points";
		case RHI::EDrawMode::DrawModeLine:
		case RHI::EDrawMode::DrawModeLineStrip:
			return "lines";
		case RHI::EDrawMode::DrawModeTriangle:
		case RHI::EDrawMode::DrawModeTriangleStrip:
			return "triangles";
		default:
			PK_ASSERT_NOT_REACHED();
			return null;
		}
	}

	//----------------------------------------------------------------------------

	const char	*_GetGlslOutType(const RHI::EDrawMode primitiveType)
	{
		switch (primitiveType)
		{
		case RHI::EDrawMode::DrawModePoint:
			return "points";
		case RHI::EDrawMode::DrawModeLine:
		case RHI::EDrawMode::DrawModeLineStrip:
			return "line_strip";
		case RHI::EDrawMode::DrawModeTriangle:
		case RHI::EDrawMode::DrawModeTriangleStrip:
			return "triangle_strip";
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

CString CGLSLShaderGenerator::GenDefines(const RHI::SShaderDescription &description) const
{
	(void)description;
	const CString		defines = "\n"
			"#define	OPENGL_API\n"
			"\n"
			"#define	GLUE(_a, _b)						_a ## _b\n"
			"\n"
			"#define	VEC4_ZERO							vec4(0,0,0,0)\n"
			"#define	VEC4_ONE							vec4(1,1,1,1)\n"
			"#define	VEC3_ZERO							vec3(0,0,0)\n"
			"#define	VEC3_ONE							vec3(1,1,1)\n"
			"#define	VEC2_ZERO							vec2(0,0)\n"
			"#define	VEC2_ONE							vec2(1,1)\n"
			"\n"
			"#define	VEC_GREATER_THAN(_vec1, _vec2)		greaterThan(_vec1, _vec2)\n"
			"#define	VEC_GREAT_THAN_EQ(_vec1, _vec2)		greaterThanEqual(_vec1, _vec2)\n"
			"#define	VEC_LESS_THAN(_vec1, _vec2)			lessThan(_vec1, _vec2)\n"
			"#define	VEC_LESS_THAN_EQ(_vec1, _vec2)		lessThanEqual(_vec1, _vec2)\n"
			"#define	VEC_EQ(_vec1, _vec2)				equal(_vec1, _vec2)\n"
			"#define	VEC_NOT_EQ(_vec1, _vec2)			notEqual(_vec1, _vec2)\n"
			"#define	ALL_BOOL(_bvec)						all(_bvec)\n"
			"#define	ANY_BOOL(_bvec)						any(_bvec)\n"
			"\n"
			"#define	BUILD_MAT2(_axisX, _axisY)						mat2(_axisX, _axisY)\n"
			"#define	BUILD_MAT3(_axisX, _axisY, _axisZ)				mat3(_axisX, _axisY, _axisZ)\n"
			"#define	BUILD_MAT4(_axisX, _axisY, _axisZ, _axisW)		mat4(_axisX, _axisY, _axisZ, _axisW)\n"
			"\n"
			"#define	GET_CONSTANT(_constant, _var)					_constant._var\n"
			"#define	GET_RAW_BUFFER(_buffer)							_buffer._buffer\n"
			"\n"
			"#define	TEXTURE_DIMENSIONS(_sampler, _lod, _dimensions)		_dimensions = textureSize(_sampler, _lod);\n"
			"#define	SAMPLE(_sampler, _uv)								texture(_sampler, _uv)\n"
			"#define	SAMPLE_CUBE(_sampler, _uv)							SAMPLE(_sampler, _uv)\n"
			"#define	FETCH(_sampler, _uv)								texelFetch(_sampler, (_uv).xy, (_uv).z)\n" // uv is int3 (z = lod)
			"#define	SAMPLEMS(_sampler, _uv, _sampleID)					texelFetch(_sampler, _uv, _sampleID)\n"
			"#define	SAMPLELOD(_sampler, _uv, _lod)						textureLod(_sampler, _uv, _lod)\n"
			"#define	SAMPLELOD_CUBE(_sampler, _uv, _lod)					SAMPLELOD(_sampler, _uv, _lod)\n"
			"#define	SAMPLEGRAD(_sampler, _uv, _ddx, _ddy)				textureGrad(_sampler, _uv, _ddx, _ddy)\n"
			"\n"
			"#define	RAW_BUFFER_INDEX(_index)					(_index)\n"
			"\n"
			"#define	LOADU(_name,  _index)						_name[_index]\n"
			"#define	LOADU2(_name, _index)						uvec2(_name[_index], _name[(_index) + 1])\n"
			"#define	LOADU3(_name, _index)						uvec3(_name[_index], _name[(_index) + 1], _name[(_index) + 2])\n"
			"#define	LOADU4(_name, _index)						uvec4(_name[_index], _name[(_index) + 1], _name[(_index) + 2], _name[(_index) + 3])\n"
			"\n"
			"#define	LOADI(_name,  _index)						_name[_index]\n"
			"#define	LOADI2(_name, _index)						ivec2(_name[_index], _name[(_index) + 1])\n"
			"#define	LOADI3(_name, _index)						ivec3(_name[_index], _name[(_index) + 1], _name[(_index) + 2])\n"
			"#define	LOADI4(_name, _index)						ivec4(_name[_index], _name[(_index) + 1], _name[(_index) + 2], _name[(_index) + 3])\n"
			"\n"
			"#define	LOADF(_name,  _index)						asfloat(_name[_index])\n"
			"#define	LOADF2(_name, _index)						vec2(asfloat(_name[_index]), asfloat(_name[(_index) + 1]))\n"
			"#define	LOADF3(_name, _index)						vec3(asfloat(_name[_index]), asfloat(_name[(_index) + 1]), asfloat(_name[(_index) + 2]))\n"
			"#define	LOADF4(_name, _index)						vec4(asfloat(_name[_index]), asfloat(_name[(_index) + 1]), asfloat(_name[(_index) + 2]), asfloat(_name[(_index) + 3]))\n"
			"\n"
			"#define	STOREU(_name,  _index, _value)				_name[_index] = _value;\n"
			"#define	STOREU2(_name, _index, _value)				_name[_index] = (_value).x; _name[(_index) + 1] = (_value).y\n"
			"#define	STOREU3(_name, _index, _value)				_name[_index] = (_value).x; _name[(_index) + 1] = (_value).y; _name[(_index) + 2] = (_value).z;\n"
			"#define	STOREU4(_name, _index, _value)				_name[_index] = (_value).x; _name[(_index) + 1] = (_value).y; _name[(_index) + 2] = (_value).z; _name[(_index) + 3] = (_value).w;\n"
			"\n"
			"#define	STOREF(_name,  _index, _value)				_name[_index] = asuint(_value);\n"
			"#define	STOREF2(_name, _index, _value)				_name[_index] = asuint((_value).x); _name[(_index) + 1] = asuint((_value).y)\n"
			"#define	STOREF3(_name, _index, _value)				_name[_index] = asuint((_value).x); _name[(_index) + 1] = asuint((_value).y); _name[(_index) + 2] = asuint((_value).z);\n"
			"#define	STOREF4(_name, _index, _value)				_name[_index] = asuint((_value).x); _name[(_index) + 1] = asuint((_value).y); _name[(_index) + 2] = asuint((_value).z); _name[(_index) + 3] = asuint((_value).w);\n"
			"\n"
			"#define	F32TOF16(_x)								packHalf2x16(vec2 (_x, 0))\n"
			"\n"
			"#define	ATOMICADD(_name, _index, _value, _original)	_original = atomicAdd(_name[_index], asuint(_value))\n"
			"\n"
			"#define	GROUPMEMORYBARRIERWITHGROUPSYNC()			groupMemoryBarrier(); barrier();\n"
			"\n"
			"#define	IMAGE_LOAD(_name, _uv)						imageLoad(_name, ivec2(_uv))\n"
			"#define	IMAGE_STORE(_name, _uv, _value)				imageStore(_name, ivec2(_uv), _value)\n"
			"\n"
			"#define	mul(_a,_b)									((_a) * (_b))\n"
			"\n"
			"#define	asuint(x)									floatBitsToUint(x)\n"
			"#define	asfloat(x)									uintBitsToFloat(x)\n"
			"\n"
			"#define	ARRAY_BEGIN(_type)							_type[] (\n"
			"#define	ARRAY_END()									)\n"
			"\n"
			"#define	SAMPLER2D_DCL_ARG(_sampler)					sampler2D _sampler\n"
			"#define	SAMPLER2DMS_DCL_ARG(_sampler)				sampler2DMS _sampler\n"
			"\n"
			"#define	SAMPLER_ARG(_sampler)						_sampler\n"
			"#define	SAMPLERMS_ARG(_sampler)						_sampler\n"
			"\n"
			"#define	SELECT(_x, _y, _a)							mix(_x, _y, _a)\n"
			"#define	SATURATE(_value)							clamp(_value, 0.0, 1.0)\n"
			"#define	CAST(_type, _value)							_type(_value)\n"
			"\n"
			"#define	CROSS(_a, _b)								cross(_a, _b)\n"
			"\n"
			"#define	IN(_type)									in _type\n"
			"#define	OUT(_type)									out _type\n"
			"#define	INOUT(_type)								inout _type\n"
			"\n"
			"#define	GS_PARAMS\n"
			"#define	GS_ARGS\n"
			"#define	VS_PARAMS\n"
			"#define	VS_ARGS\n"
			"#define	FS_PARAMS\n"
			"#define	FS_ARGS\n"
			"#define	CS_PARAMS\n"
			"#define	CS_ARGS\n"
			"\n"
			"#define	rsqrt(x)							(1.0 / sqrt(x))\n"
			"#define	GET_MATRIX_X_AXIS(_mat)				(_mat)[0]\n"
			"#define	GET_MATRIX_Y_AXIS(_mat)				(_mat)[1]\n"
			"#define	GET_MATRIX_Z_AXIS(_mat)				(_mat)[2]\n"
			"#define	GET_MATRIX_W_AXIS(_mat)				(_mat)[3]\n"
			"\n"
			"#define	GET_GROUPSHARED(_var)				_var\n"
			"\n"
			"#define	UNROLL\n"
			"\n";

	CString			precisions;

	if (m_UseMediumPrecision)
	{
		precisions =	"#define FLOAT_PRECISION		precision mediump float;\n"
						"#define SAMPLER2D_PRECISION	precision mediump sampler2D;\n"
						"#define SAMPLER2DMS_PRECISION	precision mediump sampler2DMS;\n"
						"#define SAMPLERCUBE_PRECISION	precision mediump samplerCube;\n";
	}
	else
	{
		precisions =	"#define FLOAT_PRECISION\n"
						"#define SAMPLER2D_PRECISION\n"
						"#define SAMPLER2DMS_PRECISION\n"
						"#define SAMPLERCUBE_PRECISION\n";
	}

	CString		version;

	if (m_IsOpenGLES)
		version = "#define VERSION			#version 310 es\n";
	else
	{
		bool	useStorageResource = false;
		// We check if we are using a storage buffer:
		for (u32 i = 0; i < description.m_Bindings.m_ConstantSets.Count() && !useStorageResource; ++i)
		{
			for (u32 j = 0; j < description.m_Bindings.m_ConstantSets[i].m_Constants.Count() && !useStorageResource; ++j)
			{
				useStorageResource =	(description.m_Bindings.m_ConstantSets[i].m_Constants[j].m_Type == RHI::TypeRawBuffer) ||
										(description.m_Bindings.m_ConstantSets[i].m_Constants[j].m_Type == RHI::TypeTextureStorage);
			}
		}
		if (useStorageResource)
			version = "#define VERSION			#version 430\n";
		else
			version = "#define VERSION			#version 330\n";
	}
	CString		extensions;

	if (m_IsOpenGLES)
		extensions = "#define	EXTENSION1\n";
	else
		extensions = "#define	EXTENSION1	#extension GL_ARB_separate_shader_objects : enable\n";

	return defines + precisions + version + extensions;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenHeader(RHI::EShaderStage stage) const
{
	(void)stage;
	return "VERSION\nEXTENSION1\nFLOAT_PRECISION\nSAMPLER2D_PRECISION\nSAMPLER2DMS_PRECISION\nSAMPLERCUBE_PRECISION\n";
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenVertexInputs(const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs)
{
	CString	vertexInputsStr;

	// Vertex Inputs
	PK_FOREACH(attribute, vertexInputs)
	{
		vertexInputsStr += CString::Format(	"layout(location = %d) in %s _vin_%s;\n",
											attribute->m_ShaderLocationBinding,
											RHI::GlslShaderTypes::GetTypeString(attribute->m_Type).Data(),
											attribute->m_Name.Data());
	}

	// struct
	vertexInputsStr += "\nstruct	SVertexInput\n{\n";
	PK_FOREACH(attribute, vertexInputs)
	{
		vertexInputsStr += CString::Format(	"	%s %s;\n",
											RHI::GlslShaderTypes::GetTypeString(attribute->m_Type).Data(),
											attribute->m_Name.Data());
	}
	vertexInputsStr += "	int VertexIndex;\n";
	vertexInputsStr += "	uint InstanceId;\n";
	vertexInputsStr += "};\n";
	return vertexInputsStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenVertexOutputs(const TMemoryView<const RHI::SVertexOutput> &vertexOutputs, bool toFragment)
{
	(void)toFragment;
	CString	vertexOutputsStr;
	u32		outputLocation = 0;

	// Vertex Outputs
	PK_FOREACH(interpolated, vertexOutputs)
	{
#if	defined(PK_MACOSX)
		CString	format;

		if (toFragment)
			format = "layout(location = %d) %s out %s _fin_%s;\n";
		else
			format = "layout(location = %d) %s out %s _gin_%s;\n";

		vertexOutputsStr += CString::Format(format.Data(),
											outputLocation,
											RHI::GlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
											RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data());
#else
		vertexOutputsStr += CString::Format("layout(location = %d) %s out %s _vout_%s;\n",
											outputLocation,
											RHI::GlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
											RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data());
#endif
		outputLocation += RHI::VarType::GetRowNumber(interpolated->m_Type);
	}
	// struct
	vertexOutputsStr += "\nstruct	SVertexOutput\n{\n";
	PK_FOREACH(interpolated, vertexOutputs)
	{
		vertexOutputsStr += CString::Format("	%s %s;\n",
											RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data());
	}
	vertexOutputsStr += "	vec4 VertexPosition;\n";
	vertexOutputsStr += "};\n";
	return vertexOutputsStr;
}

//-----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenGeometryInputs(const TMemoryView<const RHI::SVertexOutput>& geometryInputs, const RHI::EDrawMode drawMode)
{
	PK_ASSERT_MESSAGE(drawMode != RHI::DrawModeInvalid, "Geometry shader input primitive type is not set");


	CString	geomInputsStr;
	geomInputsStr += CString::Format("layout(%s) in;\n", _GetGlslPrimitiveLayoutType(drawMode));

	const u32	pointNumberInput = _GetPointNumberInPrimitive(drawMode);
	u32			inputLocation = 0;
	// Geometry Inputs
	PK_FOREACH(attribute, geometryInputs)
	{
		geomInputsStr += CString::Format(	"layout(location = %d) %s in %s _gin_%s[%d];\n",
											inputLocation,
											RHI::GlslShaderTypes::GetInterpolationString(attribute->m_Interpolation).Data(),
											RHI::GlslShaderTypes::GetTypeString(attribute->m_Type).Data(),
											attribute->m_Name.Data(),
											pointNumberInput);
		inputLocation += RHI::VarType::GetRowNumber(attribute->m_Type);
	}

	geomInputsStr += "\nstruct	SPrimitives\n{\n";
	PK_FOREACH(attribute, geometryInputs) {
		geomInputsStr += CString::Format(	"	%s %s;\n",
											RHI::GlslShaderTypes::GetTypeString(attribute->m_Type).Data(),
											attribute->m_Name.Data());
	}
	//geomInputsStr += CString::Format("	vec4 VertexPosition[%d];\n", pointNumberInput);
	geomInputsStr += "\tvec4 VertexPosition;\n";
	geomInputsStr += "};\n";

	// struct
	geomInputsStr += "\nstruct	SGeometryInput\n{\n";
	geomInputsStr += CString::Format("\tSPrimitives Primitives[%d];\n", pointNumberInput);
	geomInputsStr += "\tint PrimitiveId;\n";
	geomInputsStr += "};\n";
	return geomInputsStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenGeometryOutputs(const RHI::SGeometryOutput &geometryOutputs)
{
	CString	geometryOutputsStr;
	u32		outputLocation = 0;

	PK_ASSERT_MESSAGE(geometryOutputs.m_MaxVertices != 0, "Geometry can emmit a maximum of 0 vertices... Set the value to something appropriate");

	geometryOutputsStr += CString::Format(	"layout(%s, max_vertices = %d) out;\n",
											_GetGlslOutType(geometryOutputs.m_PrimitiveType),
											geometryOutputs.m_MaxVertices);

	// Vertex Outputs
	PK_FOREACH(interpolated, geometryOutputs.m_GeometryOutput)
	{
#if    defined(PK_MACOSX)
		geometryOutputsStr += CString::Format("layout(location = %d) %s out %s _fin_%s;\n",
											  outputLocation,
											  RHI::GlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
											  RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											  interpolated->m_Name.Data());
#else
		geometryOutputsStr += CString::Format("layout(location = %d) %s out %s _gout_%s;\n",
											outputLocation,
											RHI::GlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
											RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data());
#endif
		outputLocation += RHI::VarType::GetRowNumber(interpolated->m_Type);
	}
	// struct
	geometryOutputsStr += "\nstruct	SGeometryOutput\n{\n";
	PK_FOREACH(interpolated, geometryOutputs.m_GeometryOutput)
	{
		geometryOutputsStr += CString::Format("	%s %s;\n",
											RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data());
	}
	geometryOutputsStr += "	vec4 VertexPosition;\n";
	geometryOutputsStr += "};\n";
	return geometryOutputsStr;
}

//-----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenFragmentInputs(const TMemoryView<const RHI::SVertexOutput> &fragmentInputs)
{
	CString	vertexOutputsStr;
	u32		inputLocation = 0;

	// Vertex Outputs
	PK_FOREACH(interpolated, fragmentInputs)
	{
		vertexOutputsStr += CString::Format("layout(location = %d) %s in %s _fin_%s;\n",
											inputLocation,
											RHI::GlslShaderTypes::GetInterpolationString(interpolated->m_Interpolation).Data(),
											RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data());
		inputLocation += RHI::VarType::GetRowNumber(interpolated->m_Type);
	}
	// struct
	vertexOutputsStr += "\nstruct	SFragmentInput\n{\n";
	PK_FOREACH(interpolated, fragmentInputs)
	{
		vertexOutputsStr += CString::Format("	%s %s;\n",
											RHI::GlslShaderTypes::GetTypeString(interpolated->m_Type).Data(),
											interpolated->m_Name.Data());
	}
	vertexOutputsStr += "	bool IsFrontFace;\n";
	vertexOutputsStr += "};\n";
	return vertexOutputsStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenFragmentOutputs(const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs)
{
	CString	fragmentOutputsStr;
	u32		outputLocation = 0;
	bool	hasDepthOutput = false;

	PK_FOREACH(outBuffer, fragmentOutputs)
	{
		bool			isDepth = false;
		RHI::EVarType	outType = RHI::VarType::PixelFormatToType(outBuffer->m_Type, isDepth);

		if (isDepth == false)
		{
			fragmentOutputsStr += CString::Format("layout(location = %d) out %s _fout_%s;\n",
													outputLocation++,
													RHI::GlslShaderTypes::GetTypeString(outType).Data(),
													outBuffer->m_Name.Data());
		}
		else
		{
			hasDepthOutput = true;
		}
	}
	fragmentOutputsStr += "\nstruct	SFragmentOutput\n{\n";
	PK_FOREACH(outBuffer, fragmentOutputs)
	{
		bool				isDepth;
		RHI::EVarType		outType;

		outType = RHI::VarType::PixelFormatToType(outBuffer->m_Type, isDepth);
		if (isDepth == false)
		{
			fragmentOutputsStr += CString::Format("	%s %s;\n",
				RHI::GlslShaderTypes::GetTypeString(outType).Data(),
				outBuffer->m_Name.Data());
		}
	}
	if (hasDepthOutput)
	{
		fragmentOutputsStr += "	float DepthValue;\n";
	}
	fragmentOutputsStr += "};\n";
	return fragmentOutputsStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenConstantSets(const TMemoryView<const RHI::SConstantSetLayout> &constSet, RHI::EShaderStage stage)
{
	CString	constantsStr;

	PK_FOREACH(constantSet, constSet)
	{
		if ((constantSet->m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			PK_FOREACH(constant, constantSet->m_Constants)
			{
				if (constant->m_Type == RHI::TypeConstantBuffer)
				{
					constantsStr += CString::Format("layout(/*binding = %d, */std140) uniform U%s\n", constant->m_ConstantBuffer.m_PerBlockBinding.Get(), constant->m_ConstantBuffer.m_Name.Data());
					constantsStr += "{\n";
					PK_FOREACH(var, constant->m_ConstantBuffer.m_Constants)
					{
						constantsStr += CString::Format("	%s %s", RHI::GlslShaderTypes::GetTypeString(var->m_Type).Data(), var->m_Name.Data());
						if (var->m_ArraySize >= 1)
							constantsStr += CString::Format("[%u];\n", var->m_ArraySize);
						else
							constantsStr += ";\n";
					}
					constantsStr += CString::Format("}		%s;\n", constant->m_ConstantBuffer.m_Name.Data());
				}
				else if (constant->m_Type == RHI::TypeRawBuffer)
				{
					const char	*ro = constant->m_RawBuffer.m_ReadOnly ? "readonly" : "";
					const char	*name = constant->m_RawBuffer.m_Name.Data();
					constantsStr += CString::Format("layout(std430/*, binding = %d*/) %s buffer B%s\n"
													"{\n"
													"	uint %s[];\n"
													"} %s;\n", constant->m_RawBuffer.m_PerBlockBinding.Get(), ro, name, name, name);
				}
				else if (constant->m_Type == RHI::TypeTextureStorage)
				{
					const bool		hasFormat = constant->m_TextureStorage.m_Format != RHI::FormatUnknown;
					constantsStr += CString::Format("/*layout(binding = %d)*/ %s uniform %s image2D %s;\n",
													constant->m_TextureStorage.m_PerBlockBinding.Get(),
													hasFormat ? ("layout(" + RHI::GlslShaderTypes::GetGLSLLayoutQualifierString(constant->m_TextureStorage.m_Format) + ")").Data() : "",
													constant->m_TextureStorage.m_ReadOnly ? "readonly" : hasFormat ? "" : "writeonly",
													constant->m_TextureStorage.m_Name.Data());
				}
				else if (constant->m_Type == RHI::TypeConstantSampler)
				{
					constantsStr += CString::Format("/*layout(binding = %d) */uniform %s %s;\n",
													constant->m_ConstantSampler.m_PerBlockBinding.Get(),
													constant->m_ConstantSampler.m_Type == RHI::SamplerTypeMulti ? "highp sampler2DMS" : constant->m_ConstantSampler.m_Type == RHI::SamplerTypeCube ? "samplerCube" : "sampler2D",
													constant->m_ConstantSampler.m_Name.Data());
				}
			}
		}
	}
	return constantsStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenPushConstants(const TMemoryView<const RHI::SPushConstantBuffer> &pushConstants, RHI::EShaderStage stage)
{
	CString	pushConstantsStr;

	PK_FOREACH(pushConstant, pushConstants)
	{
		if ((pushConstant->m_ShaderStagesMask & RHI::EnumConversion::ShaderStageToMask(stage)) != 0)
		{
			pushConstantsStr += CString::Format("layout(/*binding = %d, */std140) uniform U%s\n",
												pushConstant->m_PerBlockBinding.Get(),
												pushConstant->m_Name.Data());
			pushConstantsStr += "{\n";
			PK_FOREACH(var, pushConstant->m_Constants)
			{
				pushConstantsStr += CString::Format("	%s %s",
													RHI::GlslShaderTypes::GetTypeString(var->m_Type).Data(),
													var->m_Name.Data());
				if (var->m_ArraySize >= 1)
					pushConstantsStr += CString::Format("[%u];\n", var->m_ArraySize);
				else
					pushConstantsStr += ";\n";
			}
			pushConstantsStr += CString::Format("}		%s;\n", pushConstant->m_Name.Data());
		}
	}
	return pushConstantsStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenGroupsharedVariables(const TMemoryView<const RHI::SGroupsharedVariable> &groupsharedVariables)
{
	CString	groupsharedStr;

	for (auto &variable : groupsharedVariables)
	{
		groupsharedStr += CString::Format(	"shared %s	%s[%u];\n",
											RHI::GlslShaderTypes::GetTypeString(variable.m_Type).Data(),
											variable.m_Name.Data(),
											variable.m_ArraySize);
	}

	return groupsharedStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenVertexMain(	const TMemoryView<const RHI::SVertexAttributeDesc> &vertexInputs,
												const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
												const TMemoryView<const CString> &funcToCall,
												bool outputClipspacePosition)
{
	(void)outputClipspacePosition;
	CString	mainStr;

	// Shader main
	mainStr += "void	main()\n{\n";
	mainStr += "	SVertexInput	vInput;\n";
	mainStr += "	SVertexOutput	vOutput;\n\n";
	PK_FOREACH(attribute, vertexInputs)
	{
		mainStr += CString::Format("	vInput.%s = _vin_%s;\n", attribute->m_Name.Data(), attribute->m_Name.Data());
	}
	mainStr += "	vInput.VertexIndex = gl_VertexID;\n";
	mainStr += "	vInput.InstanceId = uint(gl_InstanceID);\n";

	// Initialize output struct to zero
	PK_FOREACH(vertOutput, vertexOutputs)
	{
		mainStr += CString::Format("	vOutput.%s = %s(0", vertOutput->m_Name.Data(), RHI::GlslShaderTypes::GetTypeString(vertOutput->m_Type).Data());
		for (u32 i = 1; i < RHI::VarType::VarTypeToComponentSize(vertOutput->m_Type); ++i)
			mainStr += ", 0";
		mainStr += ");\n";
	}
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "	" + *func + "(vInput, vOutput);\n";
	}
	PK_FOREACH(interpolated, vertexOutputs)
	{
#if	defined(PK_MACOSX)
		CString	format;

		if (outputClipspacePosition)
			mainStr += CString::Format("	_fin_%s = vOutput.%s;\n", interpolated->m_Name.Data(), interpolated->m_Name.Data());
		else
			mainStr += CString::Format("	_gin_%s = vOutput.%s;\n", interpolated->m_Name.Data(), interpolated->m_Name.Data());
#else
		mainStr += CString::Format("	_vout_%s = vOutput.%s;\n", interpolated->m_Name.Data(), interpolated->m_Name.Data());
#endif
	}
	mainStr += "	gl_Position = vOutput.VertexPosition;\n";
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenGeometryMain(	const TMemoryView<const RHI::SVertexOutput> & geometryInputs,
													const RHI::SGeometryOutput &geometryOutputs,
													const TMemoryView<const CString> &funcToCall,
													const RHI::EDrawMode primitiveType)
{
	const u32	pointNumberInput = _GetPointNumberInPrimitive(primitiveType);
//	const u32	pointNumberOutput = _GetPointNumberInPrimitive(geometryOutputs.m_PrimitiveType);

	// Shader main
	CString	mainStr;
	mainStr += "void	main()\n{\n";
	mainStr += "	SGeometryInput	gInput;\n";
	mainStr += CString::Format("	SGeometryOutput	gOutput;\n\n");

	PK_FOREACH(attribute, geometryInputs)
	{
		for (u32 i = 0 ; i < pointNumberInput ; ++i)
			mainStr += CString::Format("	gInput.Primitives[%d].%s = _gin_%s[%d];\n", i, attribute->m_Name.Data(), attribute->m_Name.Data(), i);
	}
	for (u32 i = 0 ; i < pointNumberInput ; ++i)
		mainStr += CString::Format("	gInput.Primitives[%d].VertexPosition = gl_in[%d].gl_Position;\n", i, i);
	mainStr += "	gInput.PrimitiveId = gl_PrimitiveIDIn;\n";
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
			mainStr += "	" + *func + "(gInput, gOutput);\n";
	}
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenFragmentMain(	const TMemoryView<const RHI::SVertexOutput> &vertexOutputs,
													const TMemoryView<const RHI::SFragmentOutput> &fragmentOutputs,
													const TMemoryView<const CString> &funcToCall)
{
	bool	hasDepth = false;
	CString	mainStr;

	// Shader main
	mainStr += "void	main()\n{\n";
	mainStr += "	SFragmentInput	fInput;\n";
	mainStr += "	SFragmentOutput	fOutput;\n\n";
	PK_FOREACH(interpolated, vertexOutputs)
	{
		mainStr += CString::Format("	fInput.%s = _fin_%s;\n", interpolated->m_Name.Data(), interpolated->m_Name.Data());
	}
	mainStr += "	fInput.IsFrontFace = gl_FrontFacing;\n";
	PK_FOREACH(func, funcToCall)
	{
		if (!func->Empty())
			mainStr += "	" + *func + "(fInput, fOutput);\n";
	}
	PK_FOREACH(outBuffer, fragmentOutputs)
	{
		if (outBuffer->m_Type == RHI::FlagD)
		{
			hasDepth = true;
		}
		else
		{
			mainStr += CString::Format("	_fout_%s = fOutput.%s;\n", outBuffer->m_Name.Data(), outBuffer->m_Name.Data());
		}
	}
	if (hasDepth)
	{
		mainStr += "	gl_FragDepth = fOutput.DepthValue;\n";
	}
	mainStr += "}\n";
	return mainStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenGeometryEmitVertex(const RHI::SGeometryOutput &geometryOutputs, bool outputClipspacePosition)
{
	(void)outputClipspacePosition;
//	const u32 pointNumberOutput = _GetPointNumberInPrimitive(geometryOutputs.m_PrimitiveType);

	CString	emitVertexStr;
	emitVertexStr += "void	AppendVertex(in SGeometryOutput outputData)\n{\n";

	PK_FOREACH(attribute, geometryOutputs.m_GeometryOutput)
	{
#if	defined(PK_MACOSX)
		emitVertexStr += CString::Format("	_fin_%s = outputData.%s;\n", attribute->m_Name.Data(), attribute->m_Name.Data());
#else
		emitVertexStr += CString::Format("	_gout_%s = outputData.%s;\n", attribute->m_Name.Data(), attribute->m_Name.Data());
#endif
	}
	emitVertexStr += "	gl_Position = outputData.VertexPosition;\n";
	emitVertexStr += "	EmitVertex();\n";

	emitVertexStr += "}";
	return emitVertexStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenGeometryEndPrimitive(const RHI::SGeometryOutput &geometryOutputs)
{
	(void)geometryOutputs;
	CString	emitEndStr;
	emitEndStr += "void	FinishPrimitive(in SGeometryOutput outputData)\n{\n";
	emitEndStr += "	EndPrimitive();\n";
	emitEndStr += "}\n";
	return emitEndStr;
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenComputeInputs()
{
	return	"\n"
			"\nstruct SComputeInput\n"
			"{\n"
			"	uvec3 GlobalThreadID;"
			"	uvec3 LocalThreadID;"
			"	uvec3 GroupID;"
			"};\n";
}

//----------------------------------------------------------------------------

CString		CGLSLShaderGenerator::GenComputeMain(	const CUint3						dispatchSize,
													const TMemoryView<const CString>	&funcToCall)
{
	// Shader main
	CString	mainStr;
	mainStr += CString::Format("layout(local_size_x = %u, local_size_y = %u, local_size_z = %u) in;\n", dispatchSize.x(), dispatchSize.y(), dispatchSize.z());
	mainStr +=	"void	main()\n"
				"{\n"
				"	SComputeInput	cInput;\n"
				"	cInput.GlobalThreadID = gl_GlobalInvocationID;\n"
				"	cInput.LocalThreadID = gl_LocalInvocationID;\n"
				"	cInput.GroupID = gl_WorkGroupID;\n";
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
