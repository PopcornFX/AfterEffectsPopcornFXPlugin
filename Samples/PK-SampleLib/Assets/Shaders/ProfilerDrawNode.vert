//#define PROFILER_OLD_STYLE

void 	VertexMain(IN(SVertexInput) vInput, OUT(SVertexOutput) vOutput VS_ARGS)
{
	vec2 	curVtxUV;

	switch (vInput.VertexIndex)
	{
		default:
		case 0:
			curVtxUV = vec2(0, 0);
			break;
		case 1:
			curVtxUV = vec2(1, 0);
			break;
		case 2:
			curVtxUV = vec2(1, 1);
			break;
		case 3:
			curVtxUV = vec2(0, 1);
			break;
	}

	vec2 	minPos = vInput.Position.xy;
	vec2 	maxPos = vInput.Position.zw;

	// color
#if defined(HAS_DASHED_LINES) && defined(PROFILER_OLD_STYLE)
	float 	currentNodeWidth = maxPos.x - minPos.x;
	float 	extendNodeSize = 5.0f - currentNodeWidth;
	if (extendNodeSize > 0.0f)
	{
		maxPos.x += extendNodeSize;
		minPos.x -= extendNodeSize;
	}
#endif

	// build vertex position:
	vec2 	vtxPosition = (vec2(1.0f, 1.0f) - curVtxUV) * minPos + curVtxUV * maxPos;
	vOutput.VertexPosition = mul(GET_CONSTANT(ProfilerConstant, Transform), vec4(vtxPosition, 0.0f, 1.0f));

#if defined(HAS_DASHED_LINES)
	vOutput.fragColor0 = vInput.Color0;
	vOutput.fragColor1 = vInput.Color1;
	vOutput.fragColor2 = vInput.Color2;

	vOutput.fragNodePixelCoord.xy = vtxPosition - vInput.Position.xy;
	vOutput.fragNodePixelCoord.zw = vInput.Position.zw - vInput.Position.xy;

	vOutput.fragNodeExtendedPixelCoord.xy = vtxPosition - minPos;
	vOutput.fragNodeExtendedPixelCoord.zw = maxPos - minPos;
#else
	vOutput.fragColor0 = curVtxUV.y == 0.0f ? vInput.Color0 : vInput.Color1;
#endif

}
