#if BB_FeatureC0
#	define BB_ScreenAligned				1U
#	define BB_ViewposAligned			2U
#	if !defined(GINPUT_geomSize)
#		error missing input
#	endif
#else
#	error config error
#endif

#if BB_FeatureC1
#	define NORMALIZER_EPSILON		1.0e-8f
#	define BB_AxisAligned			3U
#	define BB_AxisAlignedSpheroid	4U
#	if !defined(GINPUT_geomAxis0)
#		error missing input
#	endif
#endif

#if BB_FeatureC1_Capsule
#	define BB_AxisAlignedCapsule	5U
#endif

#if BB_FeatureC2
#	define BB_PlaneAligned			6U
#	if !defined(GINPUT_geomAxis1)
#		error missing input
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

void	swap(inout vec2 a, inout vec2 b)
{
	vec2	s = a;
	a = b;
	b = s;
}

void	swap(inout vec4 a, inout vec4 b)
{
	vec4	s = a;
	a = b;
	b = s;
}

vec2	get_radius(in SPrimitives gInput)
{
#if		defined(HAS_SizeFloat2)
	return gInput.geomSize;
#else
	return vec2(gInput.geomSize, gInput.geomSize);
#endif
}

#if defined(BB_ScreenAligned) || defined(BB_ViewposAligned) || defined(BB_PlaneAligned)
void	rotate_axis(in SPrimitives gInput, inout vec3 xAxis, inout vec3 yAxis)
{
#if		defined(GINPUT_geomRotation)
	float	c = cos(gInput.geomRotation);
	float	s = sin(gInput.geomRotation);
	vec3	xa = xAxis;
	xAxis = yAxis * s + xAxis * c;
	yAxis = yAxis * c - xa * s;
#endif
}
#endif

#if	BB_ScreenAligned
void	bb_ScreenAlignedQuad(in SPrimitives gInput, out vec3 xAxis, out vec3 yAxis, out vec3 nAxis)
{
#if		!defined(CONST_SceneInfo_BillboardingView)
#error "Missing View matrix in SceneInfo"
#endif
	xAxis = GET_MATRIX_X_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
	yAxis = GET_MATRIX_Y_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
	rotate_axis(gInput, xAxis, yAxis);
	vec2	radius = get_radius(gInput);
	xAxis *= radius.x;
	yAxis *= radius.y;
	nAxis = GET_MATRIX_Z_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
}
#endif

#if	BB_ViewposAligned
void	bb_ViewposAlignedQuad(in SPrimitives gInput, out vec3 xAxis, out vec3 yAxis, out vec3 nAxis)
{
#if		!defined(CONST_SceneInfo_BillboardingView)
#error "Missing View matrix in SceneInfo"
#endif
	vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
	vec3	viewUpAxis = GET_MATRIX_Y_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;
	vec3	camToParticle = normalize(gInput.VertexPosition.xyz - viewPos);
	vec3	upVectorInterm = viewUpAxis - 1.e-5f * camToParticle.xzy;

	xAxis = normalize(myCross(camToParticle, upVectorInterm));
	yAxis = myCross(xAxis, camToParticle);
	rotate_axis(gInput, xAxis, yAxis);

	vec2	radius = get_radius(gInput);
	xAxis *= radius.x;
	yAxis *= radius.y;
	nAxis = -camToParticle;
}
#endif

#if	BB_AxisAligned
void	bb_VelocityAxisAligned(in SPrimitives gInput, out vec3 xAxis, out vec3 yAxis, out vec3 nAxis)
{
#if		!defined(CONST_SceneInfo_BillboardingView)
#error "Missing View matrix in SceneInfo"
#endif
	vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;

	vec3	camToParticle = normalize(gInput.VertexPosition.xyz - viewPos);
	vec2	radius = get_radius(gInput);
	vec3	side = GET_CONSTANT(SceneInfo, SideVector).xyz;
	vec3	depth = GET_CONSTANT(SceneInfo, DepthVector).xyz;

	xAxis = myCross(camToParticle, gInput.geomAxis0);
	if (dot(xAxis, xAxis) > NORMALIZER_EPSILON)
		xAxis = normalize(xAxis);
	else
	{
		vec3	v = -side * dot(camToParticle, depth) + depth * (dot(camToParticle, side) + 0.01f);
		xAxis = normalize(myCross(camToParticle, v));
		// This is consistent with CPU but we still flip the axis
		// to cancel GPU bb flip, due to FLIP_BILLBOARDING_AXIS.
		xAxis = -xAxis;
	}
	xAxis *= radius.x;

	yAxis = gInput.geomAxis0 * 0.5f;
	nAxis = -camToParticle;
}
#endif

#if	BB_AxisAlignedSpheroid
void	bb_VelocitySpheroidalAlign(in SPrimitives gInput, out vec3 xAxis, out vec3 yAxis, out vec3 nAxis)
{
#if		!defined(CONST_SceneInfo_BillboardingView)
#error "Missing View matrix in SceneInfo"
#endif
	vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;

	vec3	camToParticle = normalize(gInput.VertexPosition.xyz - viewPos);
	vec2	radius = get_radius(gInput);
	vec3	side = GET_CONSTANT(SceneInfo, SideVector).xyz;
	vec3	depth = GET_CONSTANT(SceneInfo, DepthVector).xyz;

	vec3	sideVec = myCross(camToParticle, gInput.geomAxis0);
	if (dot(sideVec, sideVec) > NORMALIZER_EPSILON)
		sideVec = normalize(sideVec);
	else
	{
		vec3	v = -side * dot(camToParticle, depth) + depth * (dot(camToParticle, side) + 0.01f);
		sideVec = normalize(myCross(camToParticle, v));
		// This computation is consistent with CPU BUT we flip the axis
		// to cancel GPU bb flip, due to FLIP_BILLBOARDING_AXIS.
		sideVec = -sideVec;
	}
	sideVec *= radius.x;

	xAxis = sideVec;
	yAxis = gInput.geomAxis0 * 0.5f + myCross(sideVec, camToParticle);
	// Warning: xAxis and yAxis are not orthogonal.
	nAxis = normalize(myCross(xAxis, yAxis));
}
#endif

#if	BB_PlaneAligned
void	bb_PlanarAlignedQuad(in SPrimitives gInput, out vec3 xAxis, out vec3 yAxis, out vec3 nAxis)
{
	vec3	center = gInput.VertexPosition.xyz;
	vec3	axis_fwd = gInput.geomAxis0;
	vec3	axis_nrm = gInput.geomAxis1;
	vec3	side = GET_CONSTANT(SceneInfo, SideVector).xyz;
	vec3	depth = GET_CONSTANT(SceneInfo, DepthVector).xyz;

	xAxis = myCross(axis_nrm, axis_fwd);
	if (dot(xAxis, xAxis) > NORMALIZER_EPSILON)
		xAxis = normalize(xAxis);
	else
	{
		vec3	v = -side * dot(axis_nrm, depth) + depth * (dot(axis_nrm, side) + 0.01f);
		// cross(), not myCross(): in billboards_planar_quad.cpp, fallback axis ignores the handedness
		xAxis = normalize(cross(axis_nrm, v));
		// This computation is consistent with CPU BUT we flip the axis
		// to cancel GPU bb flip, due to FLIP_BILLBOARDING_AXIS.
		xAxis = -xAxis;
	}
	yAxis = myCross(xAxis, axis_nrm);
	rotate_axis(gInput, xAxis, yAxis);

	vec2	radius = get_radius(gInput);
	xAxis *= radius.x;
	yAxis *= radius.y;
	nAxis = axis_nrm;

	// Specific to planar aligned quads, flip X
	xAxis = -xAxis;
}
#endif

#if	BB_AxisAlignedCapsule
void	bb_VelocityCapsuleAlign(in SPrimitives gInput, out vec3 xAxis, out vec3 yAxis, out vec3 upVec, out vec3 nAxis)
{
#if		!defined(CONST_SceneInfo_BillboardingView)
#error "Missing View matrix in SceneInfo"
#endif
	vec3	viewPos = GET_MATRIX_W_AXIS(GET_CONSTANT(SceneInfo, BillboardingView)).xyz;

	vec3	camToParticle = normalize(gInput.VertexPosition.xyz - viewPos);
	vec2	radius = get_radius(gInput);
	vec3	side = GET_CONSTANT(SceneInfo, SideVector).xyz;
	vec3	depth = GET_CONSTANT(SceneInfo, DepthVector).xyz;

	vec3	sideVec = myCross(camToParticle, gInput.geomAxis0);
	if (dot(sideVec, sideVec) > NORMALIZER_EPSILON)
		sideVec = normalize(sideVec);
	else
	{
		vec3	v = -side * dot(camToParticle, depth) + depth * (dot(camToParticle, side) + 0.01f);
		sideVec = normalize(myCross(camToParticle, v));
		// This computation is consistent with CPU BUT we flip the axis
		// to cancel GPU bb flip, due to FLIP_BILLBOARDING_AXIS.
		sideVec = -sideVec;
	}
	sideVec *= sqrt(2.f) * radius.x;

	upVec = myCross(sideVec, camToParticle);
	xAxis = sideVec;
#if FLIP_BILLBOARDING_AXIS
	xAxis *= -1.0f;
#endif
	yAxis = gInput.geomAxis0 * 0.5f;
	nAxis = normalize(myCross(sideVec, yAxis));
}
#endif

void	billboard_quad(in SPrimitives gInput, uint bb, out vec3 xAxis, out vec3 yAxis, out vec3 nAxis)
{
#if 0
	bb_ScreenAlignedQuad(gInput, xAxis, yAxis, nAxis);
#else
	switch (bb)
	{
#if BB_ScreenAligned
	case BB_ScreenAligned:
		bb_ScreenAlignedQuad(gInput, xAxis, yAxis, nAxis);
		break;
#endif
#if BB_ViewposAligned
	case BB_ViewposAligned:
		bb_ViewposAlignedQuad(gInput, xAxis, yAxis, nAxis);
		break;
#endif
#if BB_AxisAligned
	case BB_AxisAligned:
		bb_VelocityAxisAligned(gInput, xAxis, yAxis, nAxis);
		break;
#endif
#if BB_AxisAlignedSpheroid
	case BB_AxisAlignedSpheroid:
		bb_VelocitySpheroidalAlign(gInput, xAxis, yAxis, nAxis);
		break;
#endif
#if BB_PlaneAligned
	case BB_PlaneAligned:
		bb_PlanarAlignedQuad(gInput, xAxis, yAxis, nAxis);
		break;
#endif
	default:
		bb_ScreenAlignedQuad(gInput, xAxis, yAxis, nAxis);
		break;
	}
#endif
#if FLIP_BILLBOARDING_AXIS
	yAxis *= -1.0f; // We need to flip the y axis to get the same result as the CPU billboarders
#endif
}

vec4	proj_position(vec3 position)
{
#if		defined(CONST_SceneInfo_ViewProj)
	return mul(GET_CONSTANT(SceneInfo, ViewProj), vec4(position, 1.0f));
#else
	return vec4(position, 1.0f);
#endif
}

#if 	defined(GOUTPUT_fragUV0) && defined(GOUTPUT_fragUV1)
void	bb_billboardUV(in SPrimitives gInput, inout vec4 c00, inout vec4 c01, inout vec4 c10, inout vec4 c11, inout float atlasId, inout vec4 rectA, inout vec4 rectB)
#else
void	bb_billboardUV(in SPrimitives gInput, inout vec2 c00, inout vec2 c01, inout vec2 c10, inout vec2 c11, inout vec4 rectA)
#endif
{
	uint	drId = asuint(gInput.VertexPosition.w);
	uint	flags = asuint(GET_CONSTANT(BillboardInfo, DrawRequest)[drId].x);
#if BB_Feature_Atlas
	float	idf = gInput.geomAtlas_TextureID;
	uint	maxAtlasIdx = LOADU(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(0)) - 1U;
	uint	idA = min(uint(floor(idf)), maxAtlasIdx);
	rectA = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(idA * 4 + 1));
#	if defined(GOUTPUT_fragUV1)
	uint	idB = min(idA + 1U, maxAtlasIdx);
	rectB = LOADF4(GET_RAW_BUFFER(Atlas), RAW_BUFFER_INDEX(idB * 4 + 1));
	vec4	maddm = vec4(rectA.xy, rectB.xy);
	vec4	madda = vec4(rectA.zw, rectB.zw);
	float	blendWeight = (flags & BB_Flag_SoftAnimBlend) != 0U ? 1.0f : 0.0f;
	atlasId = fract(idf) * blendWeight;
#	else
	vec2	maddm = rectA.xy;
	vec2	madda = rectA.zw;
#	endif
	c00 = c00 * maddm + madda;
	c01 = c01 * maddm + madda;
	c10 = c10 * maddm + madda;
	c11 = c11 * maddm + madda;
#endif

	if ((flags & BB_Flag_FlipV) != 0U)
	{
		swap(c00, c01);
		swap(c10, c11);
	}
	if ((flags & BB_Flag_FlipU) != 0U)
	{
		swap(c00, c10);
		swap(c01, c11);
	}
}

void	bb_billboardNormal(
	float nFactor,
	out vec3 n0, out vec3 n1, out vec3 n2, out vec3 n3,
	#if BB_AxisAlignedCapsule
	out vec3 n4, out vec3 n5,
	#endif
	in vec3 xAxis, in vec3 yAxis, in vec3 nAxis)
{
	float	nw = (1.0f - nFactor); // weight
	vec3	xAxisNorm = normalize(xAxis);
	vec3	yAxisNorm = normalize(yAxis);
	vec3	n = nAxis * nw; // normal weighted

	#if BB_AxisAlignedCapsule
	float	rlen = rsqrt(nw * nw + nFactor * nFactor);
	xAxisNorm *= nFactor * rlen;
	yAxisNorm *= nFactor * rlen;
	n *= rlen;
	n0 = n + xAxisNorm;
	n1 = n - xAxisNorm;
	n2 = n + xAxisNorm;
	n3 = n - xAxisNorm;
	n4 = n - yAxisNorm;
	n5 = n + yAxisNorm;
	#else
	vec3	xpy = (xAxisNorm + yAxisNorm) * nFactor;
	vec3	xmy = (xAxisNorm - yAxisNorm) * nFactor;
	n0 = normalize(n + xpy);
	n1 = normalize(n + xmy);
	n2 = normalize(n - xmy);
	n3 = normalize(n - xpy);
	#endif
}

void	bb_billboardTangent(
	float nFactor,
	out vec4 t0, out vec4 t1, out vec4 t2, out vec4 t3,
	#if BB_AxisAlignedCapsule
	out vec4 t4, out vec4 t5,
	#endif
	in vec3 xAxis, in vec3 yAxis, in vec3 nAxis, in bool flipU, in bool flipV)
{
	float	nw = (1.0f - nFactor); // weight
	vec3	xAxisNorm = normalize(xAxis);
	vec3	yAxisNorm = normalize(yAxis);
	vec3	n = nAxis; // normal

	#if BB_AxisAlignedCapsule
	vec3	t = normalize(xAxisNorm + yAxisNorm) * nw;
	float	rlen = rsqrt(nw * nw + 2.0f * nFactor * nFactor);
	t *= rlen;
	n *= nFactor * rlen;
	xAxisNorm *= nFactor * rlen;
	yAxisNorm *= nFactor * rlen;
	float	tangentW = flipU != flipV ? -1.0f : 1.0f;
	float	tangentDir = flipU ? -1.0f : 1.0f;
	t0 = vec4(t - n + yAxisNorm * tangentDir, tangentW);
	t2 = t0;
	t1 = vec4(t + n + yAxisNorm * tangentDir, tangentW);
	t3 = t1;
	t4 = vec4(t + n + xAxisNorm * tangentDir, tangentW);
	t5 = vec4(t - n + xAxisNorm * tangentDir, tangentW);
	#else
	vec3	t = xAxisNorm * nw;
	n *= nFactor;
	xAxisNorm *= nFactor;
	yAxisNorm *= nFactor;
	float	tangentW = flipU != flipV ? 1.0f : -1.0f;
	float	tangentDir = flipU ? -1.0f : 1.0f;
	t0 = vec4(normalize(t - n + xAxisNorm - yAxisNorm) * tangentDir, tangentW);
	t1 = vec4(normalize(t - n + xAxisNorm + yAxisNorm) * tangentDir, tangentW);
	t2 = vec4(normalize(t + n + xAxisNorm + yAxisNorm) * tangentDir, tangentW);
	t3 = vec4(normalize(t + n + xAxisNorm - yAxisNorm) * tangentDir, tangentW);
	#endif
}

void 	GeometryBillboard(in SGeometryInput gInput, SGeometryOutput gOutput GS_ARGS)
{
	uint	drId = asuint(gInput.Primitives[0].VertexPosition.w);
	uint	flags = asuint(GET_CONSTANT(BillboardInfo, DrawRequest)[drId].x);
	
	bool	flipU = (flags & BB_Flag_FlipU) != 0U;
	bool	flipV = (flags & BB_Flag_FlipV) != 0U;
	float	tangentW = 1.0f;
	// UV
#if 	defined(GOUTPUT_fragUV0) && defined(GOUTPUT_fragUV1)
# define	setUV0(_uv, _value)	_uv = _value.xy;
# define	setUV1(_uv, _value)	_uv = _value.zw;
	vec4	c00 = vec4(0, 0, 0, 0);
	vec4	c01 = vec4(0, 1, 0, 1);
	vec4	c10 = vec4(1, 0, 1, 0);
	vec4	c11 = vec4(1, 1, 1, 1);
	vec4	rect0 = vec4(0, 0, 0, 0);
	vec4	rect1 = vec4(0, 0, 0, 0);;
	bb_billboardUV(gInput.Primitives[0], c00, c01, c10, c11, gOutput.fragAtlasID, rect0, rect1);
#elif 	defined(GOUTPUT_fragUV0)
# define	setUV0(_uv, _value)	_uv = _value;
# define	setUV1(_uv, _value)
	vec2	c00 = vec2(0, 0);
	vec2	c01 = vec2(0, 1);
	vec2	c10 = vec2(1, 0);
	vec2	c11 = vec2(1, 1);
	vec4	rect0 = vec4(0, 0, 0, 0);
	bb_billboardUV(gInput.Primitives[0], c00, c01, c10, c11, rect0);
#else
# define	setUV0(_uv, _value)
# define	setUV1(_uv, _value)
#endif

	// Axis y/x to create the quad + normal
	vec3	center = gInput.Primitives[0].VertexPosition.xyz;
	vec3	xAxis = vec3(0, 0, 0);
	vec3	yAxis = vec3(0, 0, 0);
	vec3	nAxis = vec3(0, 0, 0);

#if BB_AxisAlignedCapsule
	vec3	upVec = vec3(0, 0, 0);
	bb_VelocityCapsuleAlign(gInput.Primitives[0], xAxis, yAxis, upVec, nAxis);
#else
	billboard_quad(gInput.Primitives[0], flags & BB_Flag_BillboardMask, xAxis, yAxis, nAxis);
#endif

	vec3	xpy = xAxis + yAxis;
	vec3	xmy = xAxis - yAxis;

	// fragViewProjPosition
#if		defined(GOUTPUT_fragViewProjPosition)
	#define	setViewProjPosition(_position, _value)	_position = _value;
#else
	#define	setViewProjPosition(_position, _value)
#endif

	// fragWorldPosition
#if		defined(GOUTPUT_fragWorldPosition)
	#define	setWorldPosition(_position, _value)		_position = _value;
#else
	#define	setWorldPosition(_position, _value)
#endif

#if		defined(GOUTPUT_fragNormal) || defined(GOUTPUT_fragTangent)
	float	nFactor = GET_CONSTANT(BillboardInfo, DrawRequest)[drId].y;
#endif

	// Normals
#if		defined(GOUTPUT_fragNormal)
#define	setNormal(_normal, _value)	_normal = _value;
	vec3	n0, n1, n2, n3;
	#if BB_AxisAlignedCapsule
		vec3	n4, n5;
		bb_billboardNormal(nFactor, n0, n1, n2, n3, n4, n5, xAxis, yAxis, nAxis);
	#else
		bb_billboardNormal(nFactor, n0, n1, n2, n3, xAxis, yAxis, nAxis);
	#endif
#else
#define	setNormal(_normal, _value)
#endif

	// Tangent
#if		defined(GOUTPUT_fragTangent)
#define	setTangent(_tangent, _value)	_tangent = _value;
	vec4	t0, t1, t2, t3;
	#if BB_AxisAlignedCapsule
		vec4	t4, t5;
		bb_billboardTangent(nFactor, t0, t1, t2, t3, t4, t5, xAxis, yAxis, nAxis, flipU, flipV);
	#else
		bb_billboardTangent(nFactor, t0, t1, t2, t3, xAxis, yAxis, nAxis, flipU, flipV);
	#endif
#else
#define	setTangent(_tangent, _value)
#endif

	// Emit 4 / 6 vertex

#define emittr(pos, uv, normal, tangent)	\
	gOutput.VertexPosition = proj_position(pos); \
	setViewProjPosition(gOutput.fragViewProjPosition, gOutput.VertexPosition); \
	setWorldPosition(gOutput.fragWorldPosition, pos); \
	setUV0(gOutput.fragUV0, uv); \
	setUV1(gOutput.fragUV1, uv); \
	setNormal(gOutput.fragNormal, normal); \
	setTangent(gOutput.fragTangent, tangent); \
	AppendVertex(gOutput GS_PARAMS);

#if BB_AxisAlignedCapsule
	emittr(center + yAxis + upVec, c10, n5, t5);
	emittr(center + xpy, c11, n0, t0); // +x+y
	emittr(center - xmy, c00, n1, t1); // -x+y
	emittr(center + xmy, c11, n2, t2); // +x-y
	emittr(center - xpy, c00, n3, t3); // -x-y
	emittr(center - yAxis - upVec, c01, n4, t4);
#else
	emittr(center + xpy, c11, n0, t0); // +x+y
	emittr(center + xmy, c10, n1, t1); // +x-y
	emittr(center - xmy, c01, n2, t2); // -x+y
	emittr(center - xpy, c00, n3, t3); // -x-y
#endif
}
