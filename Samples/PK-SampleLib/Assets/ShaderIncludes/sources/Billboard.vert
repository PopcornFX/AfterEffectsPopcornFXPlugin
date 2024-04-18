#if BB_FeatureC0
#	define BB_ScreenAligned				1U
#	define BB_ViewposAligned			2U
#	if !defined(VRESOURCE_PositionsOffsets) && defined(BB_GPU_SIM)
#		error missing PositionsOffsets SRV
#	endif
#	if !defined(VRESOURCE_Positions) && !defined(BB_GPU_SIM)
#		error missing Positions SRV
#	endif
#	if !defined(VRESOURCE_Indices) && !defined(BB_GPU_SIM)
#		error missing Indices SRV	// Mandatory for now
#	endif
#	if !defined(VRESOURCE_SizesOffsets) && !defined(VRESOURCE_Size2sOffsets) && defined(BB_GPU_SIM)
#		error missing SizesOffsets or Size2sOffsets SRV
#	endif
#	if !defined(VRESOURCE_Sizes) && !defined(VRESOURCE_Size2s) && !defined(BB_GPU_SIM)
#		error missing Sizes SRV
#	endif
#else
#	error config error
#endif

#if defined(VOUTPUT_fragUV1)
#	if !defined(VOUTPUT_fragAtlasID)
#		error "config error"
#	endif
#else
#	if defined(VOUTPUT_fragAtlasID)
#		error "config error"
#	endif
#endif
#if BB_Feature_Atlas
#	if !defined(VRESOURCE_Atlas_TextureIDsOffsets) && defined(BB_GPU_SIM)
#		error missing TextureIDsOffsets SRV
#	endif
#	if !defined(VRESOURCE_Atlas_TextureIDs) && !defined(BB_GPU_SIM)
#		error "config error"
#	endif
#endif

#if BB_FeatureC1
#	define NORMALIZER_EPSILON				1.0e-8f
#	define BB_AxisAligned			3U
#	define BB_AxisAlignedSpheroid	4U
#	if !defined(VRESOURCE_Axis0sOffsets) && defined(BB_GPU_SIM)
#		error missing Axis0sOffsets SRV
#	endif
#	if !defined(VRESOURCE_Axis0s) && !defined(BB_GPU_SIM)
#		error missing Axis0s SRV
#	endif
#endif

#if BB_FeatureC1_Capsule
#	define BB_AxisAlignedCapsule	5U
#endif

#if BB_FeatureC2
#	define BB_PlaneAligned			6U
#	if !defined(VRESOURCE_Axis1sOffsets) && defined(BB_GPU_SIM)
#		error missing Axis1sOffsets SRV
#	endif
#	if !defined(VRESOURCE_Axis1s) && !defined(BB_GPU_SIM)
#		error missing Axis1s SRV
#	endif
#endif

#define	FLIP_BILLBOARDING_AXIS		1

#if	!defined(CONST_BillboardInfo_DrawRequest)
#error "Missing constant with infos for billboard"
#endif

#if defined(CONST_BillboardInfo_DrawRequest)
#	define	BB_Flag_BillboardMask	7U	// 3 firsts bits
#	define	BB_Flag_FlipV			8U	// 4th bit
#	define	BB_Flag_SoftAnimBlend	16U // 5th bit
#	define	BB_Flag_FlipU			32U	// 6th bit
#endif

//	DrawRequest:
// x: Flags
// y: NormalsBendingFactor

// RightHanded/LeftHanded:
#define	myCross(a, b)  cross(a, b) * GET_CONSTANT(SceneInfo, Handedness)

vec2	get_radius(uint particleID VS_ARGS)
{
	float	enabled = 1.0f;

#if BB_GPU_SIM
#	if	 defined(VRESOURCE_EnabledsOffsets)
	const uint	e = LOADU(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(EnabledsOffsets), RAW_BUFFER_INDEX(GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex))) + RAW_BUFFER_INDEX(particleID));
	enabled = e != 0 ? 1.0f : 0.0f;
#	endif

#	if		defined(HAS_SizeFloat2)
	return LOADF2(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Size2sOffsets), RAW_BUFFER_INDEX(GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex))) + RAW_BUFFER_INDEX(particleID * 2)) * enabled;
#	else
	float	_radius = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(SizesOffsets), RAW_BUFFER_INDEX(GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex))) + RAW_BUFFER_INDEX(particleID));
	return vec2(_radius, _radius) * enabled;
#	endif
#else
#	if		defined(HAS_SizeFloat2)
	return LOADF2(GET_RAW_BUFFER(Size2s), RAW_BUFFER_INDEX(particleID * 2)) * enabled;
#	else
	float	_radius = LOADF(GET_RAW_BUFFER(Sizes), RAW_BUFFER_INDEX(particleID));
	return vec2(_radius, _radius) * enabled;
#	endif
#endif // BB_GPU_SIM
}

#if defined(BB_ScreenAligned) || defined(BB_ViewposAligned) || defined(BB_PlaneAligned)
void	RotateTangents(uint particleID, INOUT(vec3) tangent0, INOUT(vec3) tangent1 VS_ARGS)
{
#if defined(VRESOURCE_Rotations) || defined(VRESOURCE_RotationsOffsets)
#	if BB_GPU_SIM
#		if !defined(VRESOURCE_RotationsOffsets)
#			error missing RotationsOffsets SRV
#		endif // !defined(VRESOURCE_Rotations)
	const float	rotation = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(RotationsOffsets), RAW_BUFFER_INDEX(GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex))) + RAW_BUFFER_INDEX(particleID));
#	else
#		if !defined(VRESOURCE_Rotations)
#			error missing Rotations SRV
#		endif // !defined(VRESOURCE_Rotations)
	const float	rotation = LOADF(GET_RAW_BUFFER(Rotations), RAW_BUFFER_INDEX(particleID));
#	endif // BB_GPU_SIM

	const float	rotCos = cos(rotation);
	const float	rotSin = sin(rotation);
	const vec3	_tangent0 = tangent0;

	tangent0 = tangent1 * rotSin + tangent0 * rotCos;
	tangent1 = tangent1 * rotCos - _tangent0 * rotSin;
#endif // defined(VRESOURCE_Rotations) || defined(VRESOURCE_RotationsOffsets)
}
#endif

vec4	proj_position(IN(vec3) position VS_ARGS)
{
#if		defined(CONST_SceneInfo_ViewProj)
	return mul(GET_CONSTANT(SceneInfo, ViewProj), vec4(position, 1.0f));
#else
	return vec4(position, 1.0f);
#endif
}

#if BB_FeatureC1
vec3	ComputeTangent1(IN(vec3) nrm, IN(vec3) tangent0 VS_ARGS)
{
	vec3	side = GET_CONSTANT(SceneInfo, SideVector).xyz;
	vec3	depth = GET_CONSTANT(SceneInfo, DepthVector).xyz;

	vec3	tangent1 = myCross(nrm, tangent0);
	if (dot(tangent1, tangent1) > NORMALIZER_EPSILON)
		tangent1 = normalize(tangent1);
	else
	{
		const vec3	v = -side * dot(nrm, depth) + depth * (dot(nrm, side) + 0.01f);
		// cross(), not myCross(): in billboards_planar_quad.cpp, fallback axis ignores the handedness
		tangent1 = normalize(cross(nrm, v));
		// This computation is consistent with CPU BUT we flip the axis
		// to cancel GPU bb flip, due to FLIP_BILLBOARDING_AXIS.
		tangent1 = -tangent1;
	}
	return tangent1;
}
#endif

void 	VertexBillboard(IN(SVertexInput) vInput, INOUT(SVertexOutput) vOutput, uint particleID VS_ARGS)
{
#if BB_GPU_SIM
	const uint		storageId = GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex);
	const vec3		worldPos = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(PositionsOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
	const uint		drId = 0; // No batching yet for GPU particles
#else
	const vec4		worldPosDrId = LOADF4(GET_RAW_BUFFER(Positions), RAW_BUFFER_INDEX(particleID * 4));
	const vec3		worldPos = worldPosDrId.xyz;
	const uint		drId = asuint(worldPosDrId.w);
#endif


	const vec4		currentDr = GET_CONSTANT(BillboardInfo, DrawRequest)[drId];
	const uint		flags = asuint(currentDr.x);
	const uint		billboarderType = flags & BB_Flag_BillboardMask;
	const bool		flipU = (flags & BB_Flag_FlipU) != 0U;
	const bool		flipV = (flags & BB_Flag_FlipV) != 0U;

	const vec2		radius = get_radius(particleID VS_PARAMS);
	vec3			tangent0 = vec3(0, 0, 0); // billboard side vector
	vec3			tangent1 = vec3(0, 0, 0); // billboard fwd/stretch vector
	vec3			planeNormal = vec3(0, 0, 0); // billboard normal vector

#if		!defined(CONST_SceneInfo_BillboardingView)
#	error "Missing BillboardingView matrix in SceneInfo"
#endif
#if		!defined(CONST_SceneInfo_SideVector)
#	error "Missing View side vector in SceneInfo"
#endif
#if		!defined(CONST_SceneInfo_DepthVector)
#	error "Missing View depth vector in SceneInfo"
#endif

	const vec2		cornerCoords = vInput.TexCoords;
	vec2			texCoords = cornerCoords;
	switch (billboarderType)
	{
#if BB_ScreenAligned
	case BB_ScreenAligned:
	{
		tangent0 = GET_MATRIX_X_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
		tangent1 = GET_MATRIX_Y_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
		planeNormal = GET_MATRIX_Z_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;

		RotateTangents(particleID, tangent0, tangent1 VS_PARAMS);

		// Apply radius after rotation, otherwise float2 sizes are incorrect
		tangent0 *= radius.x;
		tangent1 *= radius.y;
		break;
	}
#endif
#if BB_ViewposAligned
	case BB_ViewposAligned:
	{
		const vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
		const vec3	viewUpAxis = GET_MATRIX_Y_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
		const vec3	camToParticle = normalize(worldPos - viewPos);
		const vec3	upVectorInterm = viewUpAxis - 1.e-5f * camToParticle.xzy;

		tangent0 = normalize(myCross(camToParticle, upVectorInterm));
		tangent1 = myCross(tangent0, camToParticle);
		planeNormal = -camToParticle;
		RotateTangents(particleID, tangent0, tangent1 VS_PARAMS);
		
		// Apply radius after rotation, otherwise float2 sizes are incorrect
		tangent0 *= radius.x;
		tangent1 *= radius.y;
		break;
	}
#endif
#if BB_AxisAligned
	case BB_AxisAligned:
	{
		const vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
		const vec3	camToParticle = normalize(worldPos - viewPos);

#	if BB_GPU_SIM
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Axis0sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
#	else
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(Axis0s), RAW_BUFFER_INDEX(particleID * 3));
#	endif // BB_GPU_SIM

		tangent0 = ComputeTangent1(camToParticle, axis_fwd VS_PARAMS) * radius.x;
		tangent1 = axis_fwd * 0.5f;
		planeNormal = -camToParticle;
		break;
	}
#endif
#if BB_AxisAlignedSpheroid
	case BB_AxisAlignedSpheroid:
	{
		const vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
		const vec3	camToParticle = normalize(worldPos - viewPos);

#	if BB_GPU_SIM
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Axis0sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
#	else
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(Axis0s), RAW_BUFFER_INDEX(particleID * 3));
#	endif // BB_GPU_SIM

		tangent0 = ComputeTangent1(camToParticle, axis_fwd VS_PARAMS) * radius.x;
		tangent1 = axis_fwd * 0.5f + myCross(tangent0, camToParticle);
		// Warning: tangent0 and tangent1 are not orthogonal.
		planeNormal = normalize(myCross(tangent0, tangent1));
		break;
	}
#endif
#if BB_AxisAlignedCapsule
	case BB_AxisAlignedCapsule:
	{
		const vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
		const vec3	camToParticle = normalize(worldPos - viewPos);

#	if BB_GPU_SIM
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Axis0sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
#	else
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(Axis0s), RAW_BUFFER_INDEX(particleID * 3));
#	endif // BB_GPU_SIM

		tangent0 = ComputeTangent1(camToParticle, axis_fwd VS_PARAMS) * sqrt(2.f) * radius.x;

		tangent1 = axis_fwd * 0.5f;
		planeNormal = normalize(myCross(tangent0, tangent1));

#if FLIP_BILLBOARDING_AXIS
		tangent0 *= -1.0f;
#endif

		const float	upAxisStretch = abs(texCoords.y) - 1.0f;
		tangent1 -= myCross(tangent0, camToParticle) * upAxisStretch;
		texCoords.y = clamp(texCoords.y, -1.0f, 1.0f);

		break;
	}
#endif
#if BB_PlaneAligned
	case BB_PlaneAligned:
	{
#	if BB_GPU_SIM
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Axis0sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
		const vec3	axis_nrm = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Axis1sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
#	else
		const vec3	axis_fwd = LOADF3(GET_RAW_BUFFER(Axis0s), RAW_BUFFER_INDEX(particleID * 3));
		const vec3	axis_nrm = LOADF3(GET_RAW_BUFFER(Axis1s), RAW_BUFFER_INDEX(particleID * 3));
#	endif // BB_GPU_SIM

		tangent0 = ComputeTangent1(axis_nrm, axis_fwd VS_PARAMS);
		tangent1 = myCross(tangent0, axis_nrm);
		planeNormal = axis_nrm;

		// Specific to planar aligned quads, flip X
		RotateTangents(particleID, tangent0, tangent1 VS_PARAMS);
		
		// Apply radius after rotation, otherwise float2 sizes are incorrect
		tangent0 *= radius.x;
		tangent1 *= radius.y;
		tangent0 = -tangent0;

		break;
	}
#endif
	default:
		break;
	}

#if BB_AxisAlignedCapsule
	// Flip not needed
#elif FLIP_BILLBOARDING_AXIS
	tangent1 *= -1.0f; // We need to flip the y axis to get the same result as the CPU billboarders
#endif

	const vec3	bbCorner = tangent0 * texCoords.x + tangent1 * texCoords.y;
	const vec3	vertexWorldPosition = worldPos + bbCorner;

#if BB_AxisAlignedCapsule
	/*	We want to remap the input texcoords that indicates the expand direction, into the texcoords:
	*
	*			2		  3
	*		   1,1		 1,-1					   1,1		 1,1
	*		   ,.---------+						   ,+---------+
	*	 	 ,' |       . | `.					 ,' |       . | `.
	*	0,2	+   |    .    |   + 0,-2	->	0,1	+   |    .    |   + 1,0
	*	 4	 '. | .       | .'   5				 '. | .       | .'
	*		   +.---------+'					   '+---------+'
	*		 -1,1		-1,-1					  0,0		 0,0
	*		   1		  0
	*
	*		Indices: 1 3 0, 2 3 1, 3 5 0, 1 4 2
	*/
	const float		xMax = max(cornerCoords.x, 0.0f);
	texCoords = vec2(xMax + max(cornerCoords.y - 1.0f, 0.0f), xMax + max(-cornerCoords.y - 1.0f, 0.0f));
#else
	/*	We want to remap the input texcoords that indicates the expand direction, into the texcoords:
	*
	*		1		  2
	*	  -1,1		 1,1	   0,1		 1,1
	*		+---------+			+---------+
	*		|`._      |			|`._      |
	*		|   `._   |		->	|   `._   |
	*		|      `. |			|      `. |
	*		+--------`+			+--------`+
	*	 -1,-1		 1,-1	   0,0		 0,1
	*	   0		  3
	*
	*	Indices: 1 3 0, 2 3 1
	*/
	texCoords = texCoords * 0.5f + 0.5f; // Remap corners from -1,1 to 0,1
#endif

	if (flipU)
		texCoords.x = 1.0f - texCoords.x;
	if (flipV)
		texCoords.y = 1.0f - texCoords.y;

#if	defined(VOUTPUT_fragUV0)
#	if BB_Feature_Atlas
	const uint	maxAtlasID = LOADU(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(0)) - 1U;
	
#		if BB_GPU_SIM
	const float	textureID = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Atlas_TextureIDsOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID));
#		else
	const float	textureID = LOADF(GET_RAW_BUFFER(Atlas_TextureIDs), RAW_BUFFER_INDEX(particleID));
#		endif // BB_GPU_SIM

	const uint	atlasID0 = min(uint(textureID), maxAtlasID);
	const vec4	rect0 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID0 * 4 + 1));

	vOutput.fragUV0 = texCoords * rect0.xy + rect0.zw;

#		if defined (VOUTPUT_fragUV1)
	const uint	atlasID1 = min(atlasID0 + 1U, maxAtlasID);
	const vec4	rect1 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID1 * 4 + 1));

	vOutput.fragUV1 = texCoords * rect1.xy + rect1.zw;

	// To be removed in future PopcornFX versions. We already output vOutput.fragAtlas_TextureID generated
	// from additional inputs. This line generates a driver bug on nvidia cards if the "+ 0.00001f" isn't added.
	vOutput.fragAtlasID = textureID + 0.00001f;

#		endif
#	else
		vOutput.fragUV0 = texCoords;
#	endif
#endif

#if	defined(VOUTPUT_fragNormal) || defined(VOUTPUT_fragTangent)
	const float	normalBendingFactor = currentDr.y;
	if (normalBendingFactor > 0.0f)
	{
		const vec3	tangent0Norm = normalize(tangent0);
		const vec3	tangent1Norm = normalize(tangent1);
		const float	normalWeight = (1.0f - normalBendingFactor);

#	if	defined (VOUTPUT_fragNormal)
#		if BB_AxisAlignedCapsule
		const float	upAxisStretch = abs(cornerCoords.y) - 1.0f;
		const vec3	bentNormal = (tangent0Norm * cornerCoords.x * (1.0f - upAxisStretch) + tangent1Norm * sign(cornerCoords.y) * upAxisStretch) * normalBendingFactor;
#		else
		const vec3	bentNormal = (tangent0Norm * cornerCoords.x + tangent1Norm * cornerCoords.y) * normalBendingFactor;
#		endif

		vOutput.fragNormal = normalize(planeNormal * normalWeight + bentNormal);
#	endif

#	if	defined(VOUTPUT_fragTangent)
#		if BB_AxisAlignedCapsule
		const float	normalSign = sign(cornerCoords.y) - cornerCoords.y - cornerCoords.x;
		const vec3	planeNormalForTangent = planeNormal * normalSign * normalBendingFactor;
		const vec3	bentTangent = planeNormalForTangent + (tangent0Norm * upAxisStretch + tangent1Norm * (1.0f - upAxisStretch)) * normalBendingFactor;
		const vec3	planeTangentWeighted = normalize(tangent0Norm + tangent1Norm) * normalWeight;
		const float	tangentDir = flipU ? -1.0f : 1.0f;
		const float	bitangentDir = flipU != flipV ? -1.0f : 1.0f;
		vOutput.fragTangent = vec4(normalize(planeTangentWeighted + bentTangent) * tangentDir, bitangentDir);
#		else
		const vec2	tangentTexCoords = vec2(-cornerCoords.x, cornerCoords.y * sign(-cornerCoords.x));
		const vec3	planeNormalForTangent = planeNormal * tangentTexCoords.x * normalBendingFactor;
		const vec3	bentTangent = planeNormalForTangent + (tangent0Norm + tangent1Norm * tangentTexCoords.y) * normalBendingFactor;
		const vec3	planeTangentWeighted = tangent0Norm * normalWeight;
		const float tangentDir = flipU ? -1.0f : 1.0f;
		const float	bitangentDir = flipU != flipV ? 1.0f : -1.0f;
		vOutput.fragTangent = vec4(normalize(planeTangentWeighted + bentTangent) * tangentDir, bitangentDir);
#		endif
#	endif
	}
	else
	{
#	if	defined (VOUTPUT_fragNormal)
		vOutput.fragNormal = planeNormal;
#	endif
#	if	defined(VOUTPUT_fragTangent)
#		if BB_AxisAlignedCapsule
		const vec3	tangent0Norm = normalize(tangent0);
		const vec3	tangent1Norm = normalize(tangent1);
		vOutput.fragTangent = vec4(normalize(tangent0Norm + tangent1Norm) * (flipU ? -1.0f : 1.0f), flipU != flipV ? -1.0f : 1.0f);
#		else
		vOutput.fragTangent = vec4(normalize(tangent0) * (flipU ? -1.0f : 1.0f), flipU != flipV ? 1.0f : -1.0f);
#endif
#	endif
	}
#endif

	vOutput.VertexPosition = proj_position(vertexWorldPosition VS_PARAMS);
#if defined(VOUTPUT_fragWorldPosition)
	vOutput.fragWorldPosition = vertexWorldPosition;
#endif
}