#if BB_FeatureC0
#	define RB_ViewposAligned			1U
#	if !defined(VRESOURCE_PositionsOffsets) && defined(BB_GPU_SIM)
#		error missing PositionsOffsets SRV
#	endif
#	if !defined(VRESOURCE_Positions) && !defined(BB_GPU_SIM)
#		error missing Positions SRV
#	endif
#	if !defined(VRESOURCE_SizesOffsets) && defined(BB_GPU_SIM)
#		error missing SizesOffsets
#	endif
#	if !defined(VRESOURCE_Sizes) && !defined(BB_GPU_SIM)
#		error missing Sizes SRV
#	endif
#else
#	error config error
#endif

#if BB_FeatureC1
#	define NORMALIZER_EPSILON				1.0e-8f
#	define RB_NormalAxisAligned				2U
#	define RB_SideAxisAligned				3U
#	define RB_SideAxisAlignedTube			4U
#	define RB_SideAxisAlignedMultiPlane		5U
#	if !defined(VRESOURCE_Axis0sOffsets) && defined(BB_GPU_SIM)
#		error missing Axis0sOffsets SRV
#	endif
#	if !defined(VRESOURCE_Axis0s) && !defined(BB_GPU_SIM)
#		error missing Axis0s SRV
#	endif
#endif

#define	FLIP_BILLBOARDING_AXIS		1

#if	!defined(CONST_BillboardInfo_DrawRequest)
#	error "Missing constant with infos for billboard"
#endif

#if defined(CONST_BillboardInfo_DrawRequest)
#	define	BB_Flag_BillboardMask		7U	// 3 firsts bits
#	define	BB_Flag_VFlipU				8U	// 4th bit
#	define	BB_Flag_VFlipV				16U	// 5th bit
#	define	BB_Flag_VRotateTexture		32U	// 5th bit
#endif

#if	!defined(CONST_SceneInfo_BillboardingView)
#	error "Missing BillboardingView matrix in SceneInfo"
#endif

// DrawRequest:
//	x: Flags
//	y: NormalsBendingFactor

// RightHanded/LeftHanded:
#define	myCross(a, b)  cross(a, b) * GET_CONSTANT(SceneInfo, Handedness)

//----------------------------------------------------------------------------

vec4	proj_position(IN(vec3) position VS_ARGS)
{
#if	defined(CONST_SceneInfo_ViewProj)
	return mul(GET_CONSTANT(SceneInfo, ViewProj), vec4(position, 1.0f));
#else
	return vec4(position, 1.0f);
#endif
}

//----------------------------------------------------------------------------

uint	VertexBillboard(IN(SVertexInput) vInput, INOUT(SVertexOutput) vOutput, uint inParticleID VS_ARGS)
{
#if defined(BB_GPU_SIM)
	const uint	storageId = GET_CONSTANT(GPUBillboardPushConstants, StreamOffsetsIndex);
	const uint	particleCount = LOADU(GET_RAW_BUFFER(IndirectDraw), RAW_BUFFER_INDEX(storageId * 5 + 1));
	const uint	maxID = particleCount - 1;
#else
	const uint	maxID = -1;
#endif

	// Vertex texcoords are in range [-1 +1] and used to build vertex position
	const bool	isLeftSide =  vInput.TexCoords.x < 0.0f;

	// Get ribbon indirection
#if defined(BB_GPU_SIM)
	// FIXME: Even with "maxID", there is out-of-bound access of the next particle because of "inParticleIDOffsetted".
	//        Do like in the CPU-sim.
	const uint	inParticleIDOffsetted = inParticleID + (isLeftSide ? 0 : 1); // Offset particle ID for the right side vertices
	const uint	particleID = LOADU(GET_RAW_BUFFER(RibbonIndirection), RAW_BUFFER_INDEX(inParticleIDOffsetted));
	uint		particleIDPrev = LOADU(GET_RAW_BUFFER(RibbonIndirection), RAW_BUFFER_INDEX(uint(max(int(inParticleIDOffsetted) - 1, 0))));
	uint		particleIDNext = LOADU(GET_RAW_BUFFER(RibbonIndirection), RAW_BUFFER_INDEX(min(inParticleIDOffsetted + 1, maxID)));
#else
	const uvec4	connect = LOADU4(GET_RAW_BUFFER(Connectivities), RAW_BUFFER_INDEX(inParticleID * 4));
	const uint	particleIDPrev = isLeftSide ? connect.x : connect.y;
	const uint	particleID     = isLeftSide ? connect.y : connect.z;
	const uint	particleIDNext = isLeftSide ? connect.z : connect.w;
#endif	// defined(BB_GPU_SIM)

	// ParentIDs
#if defined(BB_GPU_SIM)
	const uint	parentIDOffset = LOADU(GET_RAW_BUFFER(ParentIDsOffsets), RAW_BUFFER_INDEX(storageId));
	const uint	parentID = LOADU2(GET_RAW_BUFFER(GPUSimData), parentIDOffset + RAW_BUFFER_INDEX(particleID * 2)).x;
	const uint	parentIDPrev = LOADU2(GET_RAW_BUFFER(GPUSimData), parentIDOffset + RAW_BUFFER_INDEX(particleIDPrev * 2)).x;
	const uint	parentIDNext = LOADU2(GET_RAW_BUFFER(GPUSimData), parentIDOffset + RAW_BUFFER_INDEX(particleIDNext * 2)).x;
	// If prev or next particles are from another ribbon, use middle particleID.
	if (parentIDPrev != parentID)
		particleIDPrev = particleID;
	if (parentIDNext != parentID)
		particleIDNext = particleID;
#endif

#if defined(BB_GPU_SIM)
	const uint		drId = 0; // No batching yet for GPU particles
#else
	const vec4		worldPosDrId = LOADF4(GET_RAW_BUFFER(Positions), RAW_BUFFER_INDEX(particleID * 4));
	const uint		drId = asuint(worldPosDrId.w);
#endif	// defined(BB_GPU_SIM)
	const vec4	currentDr = GET_CONSTANT(BillboardInfo, DrawRequest)[drId];
	const uint	flags = asuint(currentDr.x);
	const uint	billboarderType = flags & BB_Flag_BillboardMask;
	const float	quadCount = currentDr.z;

	// Positions
#if defined(BB_GPU_SIM)
	const uint	posOffset = LOADU(GET_RAW_BUFFER(PositionsOffsets), RAW_BUFFER_INDEX(storageId));
	const vec3	pos = LOADF3(GET_RAW_BUFFER(GPUSimData), posOffset + RAW_BUFFER_INDEX(particleID * 3));
	const vec3	posPrev = LOADF3(GET_RAW_BUFFER(GPUSimData), posOffset + RAW_BUFFER_INDEX(particleIDPrev * 3));
	const vec3	posNext = LOADF3(GET_RAW_BUFFER(GPUSimData), posOffset + RAW_BUFFER_INDEX(particleIDNext * 3));
#else
	const vec3	pos = LOADF3(GET_RAW_BUFFER(Positions), RAW_BUFFER_INDEX(particleID * 4));
	const vec3	posPrev = LOADF3(GET_RAW_BUFFER(Positions), RAW_BUFFER_INDEX(particleIDPrev * 4));
	const vec3	posNext = LOADF3(GET_RAW_BUFFER(Positions), RAW_BUFFER_INDEX(particleIDNext * 4));
#endif
	const vec3	center = 0.5f * (pos + (isLeftSide ? posNext : posPrev));

	// Do not render if...
	const bool	isLast =	(isLeftSide && (particleID == particleIDNext)) ||
							(!isLeftSide && (particleID == particleIDPrev)) ||
							(inParticleID == maxID);
	const float	doRender = isLast ? 0.0f : 1.0f;

	// Size
#if defined(BB_GPU_SIM)
	const uint	sizesOffset = LOADU(GET_RAW_BUFFER(SizesOffsets), RAW_BUFFER_INDEX(storageId));
	const float	size = LOADF(GET_RAW_BUFFER(GPUSimData), sizesOffset + RAW_BUFFER_INDEX(particleID));
#else
	const float	size = LOADF(GET_RAW_BUFFER(Sizes), RAW_BUFFER_INDEX(particleID));
#endif

	const vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;

	// Billboarding will write those: 
	vec3		tangent0 = vec3(0, 0, 0); // Ribbon side axis, unnormalized
	vec3		tangent1 = vec3(0, 0, 0); // Ribbon longitudinal axis, unnormalized
	vec3		normal = vec3(0, 0, 0); // Vertex normal vector

	float		quadId = 1.0f;

#if RB_SideAxisAlignedMultiPlane && RB_SideAxisAlignedTube

	// The texcoords u value are multiplied by the current planeId.
	if (billboarderType == RB_SideAxisAlignedMultiPlane || billboarderType == RB_SideAxisAlignedTube)
	{
		quadId = abs(vInput.TexCoords.x);

	}
#endif

	// Axis 0
#if BB_FeatureC1
#if defined(BB_GPU_SIM)
	const vec3 axis0 = LOADF3(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Axis0sOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID * 3));
#else
	const vec3 axis0 = LOADF3(GET_RAW_BUFFER(Axis0s), RAW_BUFFER_INDEX(particleID * 3));
#	endif
#endif

	switch (billboarderType)
	{
#if RB_ViewposAligned
	case RB_ViewposAligned:
		{
			const vec3	prevToNext = posNext - posPrev;
			normal = pos - viewPos;
			tangent1 = isLeftSide ? (posNext - pos) : (pos - posPrev);
			tangent0 = normalize(myCross(normal, prevToNext));
			normal = normalize(myCross(tangent0, prevToNext));
			tangent0 *= size * doRender;
		}
		break;
#endif	// RB_ViewposAligned
#if RB_NormalAxisAligned
	case RB_NormalAxisAligned:
		{
			const vec3	prevToNext = posNext - posPrev;
			normal = -axis0;
			tangent1 = isLeftSide ? (posNext - pos) : (pos - posPrev);
			tangent0 = normalize(myCross(normal, prevToNext));
			normal = normalize(myCross(tangent0, prevToNext));
			tangent0 *= size * doRender;
		}
		break;
#endif	// RB_NormalAxisAligned
#if RB_SideAxisAligned
	case RB_SideAxisAligned:
		{
			const vec3	prevToNext = posNext - posPrev;
			tangent1 = isLeftSide ? (posNext - pos) : (pos - posPrev);
			tangent0 = -axis0;
			normal = normalize(myCross(tangent0, prevToNext));
			tangent0 *= size * doRender;
		}
		break;
#endif	// RB_SideAxisAligned
#if RB_SideAxisAlignedTube
	case RB_SideAxisAlignedTube:
		{
			const vec3	prevToNext = posNext - posPrev;
			tangent1 = isLeftSide ? (posNext - pos) : (pos - posPrev);
			tangent0 = -normalize(axis0);
			normal = normalize(myCross(tangent0, prevToNext));
			tangent0 *= size * doRender;

			// Quaternion rotation of the normal vector around the prevToNext axis with an angle of quadId / quadCount * 2.0f * PI radiant (even rotation of each segment around 360 degrees).
			const float	angle = 3.14159265358f * (quadId - 1) / quadCount;
			const float	sintheta = sin(angle);
			const float	r = cos(angle);
			const vec3	u = normalize(prevToNext) * sintheta;
			const vec3	uv = myCross(u, normal);
			const vec3	uuv = myCross(u, uv);
			normal = uv * (r + r) + (normal + (uuv + uuv));
		}
		break;
#endif	// RB_SideAxisAlignedTube
#if RB_SideAxisAlignedMultiPlane
	case RB_SideAxisAlignedMultiPlane:
		{
			const vec3	prevToNext = posNext - posPrev;
			tangent1 = isLeftSide ? (posNext - pos) : (pos - posPrev);
			tangent0 = -normalize(axis0);

			// Quaternion rotation of the tangent0 vector around the prevToNext axis with an angle of quadId / quadCount * PI radiant (even rotation of each plane tangent dir around 180 degrees).
			const float	angle = 3.14159265358f * (quadId - 1) / quadCount * 0.5f;
			const float	sintheta = sin(angle);
			const float	r = cos(angle);
			const vec3	u = normalize(prevToNext) * sintheta;
			const vec3	uv = myCross(u, tangent0);
			const vec3	uuv = myCross(u, uv);
			tangent0 = uv * (r + r) + (tangent0 + (uuv + uuv));

			tangent0 *= size * doRender;

			normal = normalize(myCross(tangent0, prevToNext));
		}
		break;
#endif	// RB_SideAxisAlignedMultiPlane
	default:
		break;
	}

	// We use tangents to transform [-1 +1] coords to world space position
	const vec2	cornerCoords = vInput.TexCoords / vec2(quadId, 1.0f);

	vec3	bbCorner = tangent1 * cornerCoords.x * 0.5f + tangent0 * cornerCoords.y;

#if RB_SideAxisAlignedTube
	if (billboarderType == RB_SideAxisAlignedTube)
		bbCorner = tangent1 * cornerCoords.x * 0.5f + normal * size * doRender;
#endif

	const vec3	vertexWorldPosition = center + bbCorner;

	// Remap corners from [-1 +1] to [0 1] texCoords
	vec2		texCoords = cornerCoords * 0.5f + 0.5f; 

#if BB_Feature_CustomTextureU && !BB_Feature_CorrectDeformation
#if defined(BB_GPU_SIM)
	texCoords.x = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(CustomTextureU_TextureUsOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleID));
#else
	texCoords.x = LOADF(GET_RAW_BUFFER(CustomTextureU_TextureUs), RAW_BUFFER_INDEX(particleID));
#endif
#endif

	// Flip and rotate UVs
	const bool	rotateUVs = (flags & BB_Flag_VRotateTexture) != 0U;
	const bool	flipU = ((flags & BB_Flag_VFlipU) != 0U);
	const bool	flipV = ((flags & BB_Flag_VFlipV) != 0U) != rotateUVs;

	// If we have correct deformation, rotation is handled in fragment shader.
#if !BB_Feature_CorrectDeformation
	if (rotateUVs)
		texCoords.xy = texCoords.yx;
	if (flipU)
		texCoords.x = 1.0f - texCoords.x;
	if (flipV)
		texCoords.y = 1.0f - texCoords.y;
#endif

#if defined(VOUTPUT_fragRawUV0)
	vOutput.fragRawUV0 = texCoords;
#endif

	// UVs output 
#if	defined(VOUTPUT_fragUV0)
#	if BB_Feature_CorrectDeformation
#if defined(BB_GPU_SIM)
	const float	otherSize = LOADF(GET_RAW_BUFFER(GPUSimData), sizesOffset + RAW_BUFFER_INDEX(isLeftSide ? particleIDNext : particleIDPrev));
#else
	const float	otherSize = LOADF(GET_RAW_BUFFER(Sizes), RAW_BUFFER_INDEX(isLeftSide ? particleIDNext : particleIDPrev));
#endif
	float	ratio = size / otherSize;

	const bool	isDownSide =  vInput.TexCoords.y < 0.0f;

	vec4	outUVFactors = vec4(1, 1, 1, 1);

	if (!isDownSide && isLeftSide)
		outUVFactors.w = ratio;
	if (isDownSide && !isLeftSide)
		outUVFactors.y = ratio;
	vOutput.fragUVFactors = outUVFactors;

	vec2	flipscale = vec2(flipU ? -1.f : 1.f, flipV ? -1.f : 1.f);
	vec2	flipoffset = vec2 (flipU ? 1.f : 0.f, flipV ? 1.f : 0.f);
#if BB_Feature_CustomTextureU
#if defined(BB_GPU_SIM)
	float	uoffset = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(CustomTextureU_TextureUsOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(isLeftSide ? particleID : particleIDPrev));
	float	uoffsetNext = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(CustomTextureU_TextureUsOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(isLeftSide ? particleIDNext : particleID));
#else
	float	uoffset = LOADF(GET_RAW_BUFFER(CustomTextureU_TextureUs), RAW_BUFFER_INDEX(isLeftSide ? particleID : particleIDPrev));
	float	uoffsetNext = LOADF(GET_RAW_BUFFER(CustomTextureU_TextureUs), RAW_BUFFER_INDEX(isLeftSide ? particleIDNext : particleID));
#endif
	flipoffset += (rotateUVs ? vec2(0.f, uoffset) : vec2(uoffset, 0.f)) * flipscale;
	flipscale *= rotateUVs ? vec2(1.f, uoffsetNext - uoffset) : vec2(uoffsetNext - uoffset, 1.f);
#endif // BB_Feature_CustomTextureU
#endif // BB_Feature_CorrectDeformation

#	if BB_Feature_Atlas
	// particleIDInner: constant-ID of the current ribbon's quad (constant in between both particles)
#if BB_Feature_CustomTextureU
	const uint	particleIDInner = isLeftSide ? particleIDNext : particleID; // with custom-U, it seems inverted ?!
#else
	const uint	particleIDInner = isLeftSide ? particleID : particleIDPrev;
#endif // BB_Feature_CustomTextureU
	const uint	maxAtlasID = LOADU(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(0)) - 1U;
#if defined(BB_GPU_SIM)
	const float	textureID = LOADF(GET_RAW_BUFFER(GPUSimData), LOADU(GET_RAW_BUFFER(Atlas_TextureIDsOffsets), RAW_BUFFER_INDEX(storageId)) + RAW_BUFFER_INDEX(particleIDInner));
#else
	const float	textureID = LOADF(GET_RAW_BUFFER(Atlas_TextureIDs), RAW_BUFFER_INDEX(particleIDInner));
#endif
	const uint	atlasID0 = min(uint(textureID), maxAtlasID);
	const vec4	rect0 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID0 * 4 + 4));
#		if defined (VOUTPUT_fragUV1) || defined (VOUTPUT_fragUV1ScaleAndOffset)
	const uint	atlasID1 = min(atlasID0 + 1U, maxAtlasID);
	const vec4	rect1 = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(atlasID1 * 4 + 4));
#		endif // defined (VOUTPUT_fragUV1) || defined (VOUTPUT_fragUV1ScaleAndOffset)
#	endif // BB_Feature_Atlas

#	if BB_Feature_Atlas && !BB_Feature_CorrectDeformation
	vOutput.fragUV0 = texCoords * rect0.xy + rect0.zw;
#		if defined (VOUTPUT_fragUV1)
	vOutput.fragUV1 = texCoords * rect1.xy + rect1.zw;
#		endif // defined (VOUTPUT_fragUV1)
#	else
	vOutput.fragUV0 = texCoords;

#		if BB_Feature_CorrectDeformation
#			if BB_Feature_Atlas
	vOutput.fragUVScaleAndOffset = vec4(flipscale * rect0.xy, flipoffset * rect0.xy + rect0.zw);
#				if defined (VOUTPUT_fragUV1ScaleAndOffset)
	vOutput.fragUV1ScaleAndOffset = vec4(flipscale * rect1.xy, flipoffset * rect1.xy + rect1.zw);
#				endif // defined (VOUTPUT_fragUV1ScaleAndOffset)
#				if defined (VOUTPUT_fragRawUVScaleAndOffset)
	vOutput.fragRawUVScaleAndOffset = vec4(flipscale, flipoffset);
#				endif // defined (VOUTPUT_fragRawUVScaleAndOffset)
#			else
	vOutput.fragUVScaleAndOffset = vec4(flipscale, flipoffset);
#			endif // BB_Feature_Atlas
#		endif // BB_Feature_CorrectDeformation
#	endif // BB_Feature_Atlas && !BB_Feature_CorrectDeformation
#endif // defined(VOUTPUT_fragUV0)

	// Normal output
#if defined(VOUTPUT_fragNormal)
	const float	normalBendingFactor = currentDr.y;
	if (normalBendingFactor > 0.0f)
	{
		const vec3	tangent0Norm = normalize(tangent0);
		const vec3	tangent1Norm = normalize(tangent1);
		const float	oneMinusNormalBendingFactor = (1.0f - normalBendingFactor);

		const vec3	bentNormal = (tangent0Norm * cornerCoords.y);
		vOutput.fragNormal = normalize(normal * oneMinusNormalBendingFactor + bentNormal * normalBendingFactor);
	}
	else
		vOutput.fragNormal = normalize(normal);
#endif	// defined(VOUTPUT_fragNormal)

	// Tangent output
#if defined(VOUTPUT_fragTangent)
	// With ribbon, bent normals are pushed along tangent0 axis only (ribbon side aixs)
	// This means that tangent1, which is ribbon longitudinal axis, is not impacted.
	// w = -1.0f will work with default ribbon, but won't adapt to
	// transforming the UVs / custom U. Fix that in CPU bb, then here. 
	vOutput.fragTangent = vec4(normalize(tangent1), -1.0f);
#endif	// defined(VOUTPUT_fragTangent)

	vOutput.VertexPosition = proj_position(vertexWorldPosition VS_PARAMS);
#if defined(VOUTPUT_fragWorldPosition)
	vOutput.fragWorldPosition = vertexWorldPosition;
#endif

	return particleID;
}
