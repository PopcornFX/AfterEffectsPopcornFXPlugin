#if (!defined(VRESOURCE_VertexPosition0) || !defined(VRESOURCE_VertexPosition1) || !defined(VRESOURCE_VertexPosition2)) && !defined(BB_GPU_SIM)
#		error missing Positions SRV
#	endif

#if !defined(VRESOURCE_Indices) && !defined(BB_GPU_SIM)
#	error missing Indices SRV	// Mandatory for now
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
#if BB_GPU_SIM // Not implemented
    const vec3  worldPos = vec3(0.0, 0.0, 0.0);
	const vec4 	worldPosDrId = vec4(0.0, 0.0, 0.0, 0.0);
#else
	const vec4	worldPosDrId = LOADF4(GET_RAW_BUFFER(VertexPosition0), RAW_BUFFER_INDEX(particleID * 4));
	const vec3	pos0 = worldPosDrId.xyz;
	const vec3	pos1 = LOADF3(GET_RAW_BUFFER(VertexPosition1), RAW_BUFFER_INDEX(particleID * 3));
	const vec3	pos2 = LOADF3(GET_RAW_BUFFER(VertexPosition2), RAW_BUFFER_INDEX(particleID * 3));

	vec3	worldPos = vec3(0.0, 0.0, 0.0);
	if (vInput.VertexIndex == 0)
		worldPos = pos0;
	else if (vInput.VertexIndex == 1)
		worldPos = pos1;
	else if (vInput.VertexIndex == 2)
		worldPos = pos2;
#endif // BB_GPU_SIM

    vOutput.VertexPosition = proj_position(worldPos VS_PARAMS);

#if defined(VOUTPUT_fragTangent) || defined(VOUTPUT_fragUV0)
#if defined(HAS_TriangleCustomUVs)
	const vec2	uv0 = LOADF2(GET_RAW_BUFFER(TriangleCustomUVs_UV1s), RAW_BUFFER_INDEX(particleID * 2));
	const vec2	uv1 = LOADF2(GET_RAW_BUFFER(TriangleCustomUVs_UV2s), RAW_BUFFER_INDEX(particleID * 2));
	const vec2	uv2 = LOADF2(GET_RAW_BUFFER(TriangleCustomUVs_UV3s), RAW_BUFFER_INDEX(particleID * 2));
#else
	const vec2	uv0 = vec2(0.0, 0.0);
	const vec2 	uv1 = vec2(1.0, 0.0);
	const vec2 	uv2 = vec2(0.0, 1.0);
#endif // defined(HAS_TriangleCustomUVs)
#endif // defined(VOUTPUT_fragTangent) || defined(VOUTPUT_fragUV0)

#if defined(VOUTPUT_fragUV0)
#if BB_GPU_SIM // Not implemented
	const vec2	uv = vec2(0.0, 0.0);
#else
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
	const vec2	uv = vec2(vInput.VertexIndex & 1, (vInput.VertexIndex & 2) >> 1);
#endif // defined(HAS_TriangleCustomUVs)
	vOutput.fragUV0 = uv;
#endif // BB_GPU_SIM
#endif // defined(VOUTPUT_fragUV0)

#if defined(VOUTPUT_fragNormal) || defined(VOUTPUT_fragTangent)
#if BB_GPU_SIM // Not implemented
	const vec3	normal = vec3(0.0, 0.0, 0.0);
#else
	vec3	normal = vec3(0.0, 0.0, 0.0);

	const vec3	edge0 = pos1 - pos0;
	const vec3	edge1 = pos2 - pos0;

#if defined(HAS_TriangleCustomNormals)
	if (vInput.VertexIndex == 0)
		normal = LOADF3(GET_RAW_BUFFER(TriangleCustomNormals_Normal1s), RAW_BUFFER_INDEX(particleID * 3));
	else if (vInput.VertexIndex == 1)
		normal = LOADF3(GET_RAW_BUFFER(TriangleCustomNormals_Normal2s), RAW_BUFFER_INDEX(particleID * 3));
	else if (vInput.VertexIndex == 2)
		normal = LOADF3(GET_RAW_BUFFER(TriangleCustomNormals_Normal3s), RAW_BUFFER_INDEX(particleID * 3));
#else
	normal = normalize(cross(edge0, edge1));
#endif // defined(HAS_TriangleCustomNormals)

#if defined(HAS_NormalBend) && !defined(HAS_TriangleCustomNormals)
	const uint	drId = asuint(worldPosDrId.w);
	const float normalBendingFactor = GET_CONSTANT(TriangleInfo, DrawRequest)[drId / 4][drId % 4];
	if (normalBendingFactor != 0.0f)
	{
		const vec3	barycenter = (pos0 + pos1 + pos2) / 3.0;
		const vec3	maxBentNormal = normalize(worldPos - barycenter);
		normal = normalize(mix(normal, maxBentNormal, normalBendingFactor));
	}
#endif // defined(HAS_NormalBend) && !defined(HAS_TriangleCustomNormals)
#endif // BB_GPU_SIM

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