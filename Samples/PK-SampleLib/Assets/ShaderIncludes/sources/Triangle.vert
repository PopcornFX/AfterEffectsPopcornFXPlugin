#if (!defined(VRESOURCE_VertexPosition0) || !defined(VRESOURCE_VertexPosition1) || !defined(VRESOURCE_VertexPosition2)) && !defined(BB_GPU_SIM)
#		error missing Positions SRV
#	endif

#if !defined(VRESOURCE_Indices) && !defined(BB_GPU_SIM)
#	error missing Indices SRV	// Mandatory for now
#endif

#if defined(CONST_TriangleInfo_DrawRequest)
#	define	BB_Flag_FlipU			1U	// 1st bit, must match values in STriangleDrawRequest::EFlags
#	define	BB_Flag_FlipV			2U	// 2nd bit
#endif

vec4	proj_position(IN(vec3) position VS_ARGS)
{
#if		defined(CONST_SceneInfo_ViewProj)
	return mul(GET_CONSTANT(SceneInfo, ViewProj), vec4(position, 1.0f));
#else
	return vec4(position, 1.0f);
#endif
}

void    VertexBillboard(IN(SVertexInput) vInput, INOUT(SVertexOutput) vOutput, uint particleID VS_ARGS)
{
#if BB_GPU_SIM
	const uint		storageId = GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex);
	const vec3		pos0 = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Position0Offsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
	const vec3		pos1 = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Position1Offsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
	const vec3		pos2 = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Position2Offsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
	const uint		drId = 0; // No batching yet for GPU particles
#else
	const vec4	worldPosDrId = LOADF4(GET_RAW_BUFFER(VertexPosition0), RAW_BUFFER_INDEX(particleID * 4));
	const vec3	pos0 = worldPosDrId.xyz;
	const vec3	pos1 = LOADF3(GET_RAW_BUFFER(VertexPosition1), RAW_BUFFER_INDEX(particleID * 3));
	const vec3	pos2 = LOADF3(GET_RAW_BUFFER(VertexPosition2), RAW_BUFFER_INDEX(particleID * 3));
	const uint	drId = asuint(worldPosDrId.w);
#endif // BB_GPU_SIM

	vec3	worldPos = vec3(0.0, 0.0, 0.0);
	if (vInput.VertexIndex == 0)
		worldPos = pos0;
	else if (vInput.VertexIndex == 1)
		worldPos = pos1;
	else if (vInput.VertexIndex == 2)
		worldPos = pos2;

	const vec2 dr = GET_CONSTANT(TriangleInfo, DrawRequest)[drId];

    vOutput.VertexPosition = proj_position(worldPos VS_PARAMS);

#if defined(VOUTPUT_fragTangent) || defined(VOUTPUT_fragUV0)
#if defined(HAS_TriangleCustomUVs)
#if BB_GPU_SIM
	const vec2	uv0 = LOADF2(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(TriangleCustomUVs_UV1sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 2));
	const vec2	uv1 = LOADF2(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(TriangleCustomUVs_UV2sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 2));
	const vec2	uv2 = LOADF2(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(TriangleCustomUVs_UV3sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 2));
#else
	const vec2	uv0 = LOADF2(GET_RAW_BUFFER(TriangleCustomUVs_UV1s), RAW_BUFFER_INDEX(particleID * 2));
	const vec2	uv1 = LOADF2(GET_RAW_BUFFER(TriangleCustomUVs_UV2s), RAW_BUFFER_INDEX(particleID * 2));
	const vec2	uv2 = LOADF2(GET_RAW_BUFFER(TriangleCustomUVs_UV3s), RAW_BUFFER_INDEX(particleID * 2));
#endif
#else
	const vec2	uv0 = vec2(0.0, 0.0);
	const vec2 	uv1 = vec2(1.0, 0.0);
	const vec2 	uv2 = vec2(0.0, 1.0);
#endif // defined(HAS_TriangleCustomUVs)
#endif // defined(VOUTPUT_fragTangent) || defined(VOUTPUT_fragUV0)

#if defined(VOUTPUT_fragUV0)
#if defined(HAS_TriangleCustomUVs)
	vec2	uv = vec2(0.0, 0.0);
	if (vInput.VertexIndex == 0)
		uv = uv0;
	else if (vInput.VertexIndex == 1)
		uv = uv1;
	else if (vInput.VertexIndex == 2)
		uv = uv2;
#else
	/*
	VertexIndex 0 -> x = 0, y = 0
	VertexIndex 1 -> x = 1, y = 0
	VertexIndex 2 -> x = 0, y = 1
	*/
	vec2	uv = vec2(vInput.VertexIndex & 1, (vInput.VertexIndex & 2) >> 1);
#endif // defined(HAS_TriangleCustomUVs)

#if defined(HAS_BasicTransformUVs)
	const uint 		flipMask = asuint(dr.y);
	const bool		flipU = (flipMask & BB_Flag_FlipU) != 0U;
	const bool		flipV = (flipMask & BB_Flag_FlipV) != 0U;
	if (flipU)
		uv.x = 1.0f - uv.x;
	if (flipV)
		uv.y = 1.0f - uv.y;
#endif

	vOutput.fragUV0 = uv;
#endif // defined(VOUTPUT_fragUV0)

#if defined(VOUTPUT_fragNormal) || defined(VOUTPUT_fragTangent)
	vec3	normal = vec3(0.0, 0.0, 0.0);
	const vec3	edge0 = pos1 - pos0;
	const vec3	edge1 = pos2 - pos0;

#if defined(HAS_TriangleCustomNormals)
#if BB_GPU_SIM
	if (vInput.VertexIndex == 0)
		normal = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(TriangleCustomNormals_Normal1sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
	else if (vInput.VertexIndex == 1)
		normal = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(TriangleCustomNormals_Normal2sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
	else if (vInput.VertexIndex == 2)
		normal = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(TriangleCustomNormals_Normal3sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
#else
	if (vInput.VertexIndex == 0)
		normal = LOADF3(GET_RAW_BUFFER(TriangleCustomNormals_Normal1s), RAW_BUFFER_INDEX(particleID * 3));
	else if (vInput.VertexIndex == 1)
		normal = LOADF3(GET_RAW_BUFFER(TriangleCustomNormals_Normal2s), RAW_BUFFER_INDEX(particleID * 3));
	else if (vInput.VertexIndex == 2)
		normal = LOADF3(GET_RAW_BUFFER(TriangleCustomNormals_Normal3s), RAW_BUFFER_INDEX(particleID * 3));
#endif // BB_GPU_SIM
#else
	normal = normalize(cross(edge0, edge1));
#endif // defined(HAS_TriangleCustomNormals)

#if defined(HAS_NormalBend) && !defined(HAS_TriangleCustomNormals)

	const float normalBendingFactor = dr.x;
	if (normalBendingFactor != 0.0f)
	{
		const vec3	barycenter = (pos0 + pos1 + pos2) / 3.0;
		const vec3	maxBentNormal = normalize(worldPos - barycenter);
		normal = normalize(mix(normal, maxBentNormal, normalBendingFactor));
	}
#endif // defined(HAS_NormalBend) && !defined(HAS_TriangleCustomNormals)

#if defined(VOUTPUT_fragNormal)
    vOutput.fragNormal = normal;
#endif // defined(VOUTPUT_fragNormal)

#if defined(VOUTPUT_fragTangent)
	vec2	edge0UV = uv1 - uv0;
	vec2 	edge1UV = uv2 - uv0;
	float	det = edge0UV.x * edge1UV.y - edge0UV.y * edge1UV.x;
	if (abs(det) <= 1e-6)
	{
		edge0UV = vec2(1.0, 0.0);
		edge1UV = vec2(0.0, 1.0);
		det = edge0UV.x * edge1UV.y - edge0UV.y * edge1UV.x;
	}
	const float	r = 1.0 / det;
	const vec3	tangent = (edge0 * edge1UV.y - edge1 * edge0UV.y) * r;
	const vec3	bitangent = (edge1 * edge0UV.x - edge0 * edge1UV.x) * r;
	const float	crossSign = GET_CONSTANT(SceneInfo, Handedness);
	const vec3	outBitangent = cross(normal, tangent) * crossSign;
	const float	tangentW = (dot(bitangent, outBitangent) > 0) ? 1.0 : -1.0;

	// compute vertex tangent based on triangle uvs & positions
    vOutput.fragTangent = vec4(normalize(cross(outBitangent, normal)) * crossSign, tangentW);
#endif // defined(VOUTPUT_fragTangent)
#endif // defined(VOUTPUT_fragNormal) || defined(VOUTPUT_fragTangent)

#if defined(VOUTPUT_fragWorldPosition)
    vOutput.fragWorldPosition = worldPos;
#endif
}