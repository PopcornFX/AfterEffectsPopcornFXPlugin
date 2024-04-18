//#define PROFILER_OLD_STYLE

void 	FragmentMain(IN(SFragmentInput) fInput, OUT(SFragmentOutput) fOutput FS_ARGS)
{
#if defined(HAS_DASHED_LINES)
#	if defined(PROFILER_OLD_STYLE)
	if (fInput.fragNodePixelCoord.x < 0.0f || fInput.fragNodePixelCoord.x > fInput.fragNodePixelCoord.z)
	{
		float 	lineWidth = 1.0f;
		float 	invCoordX = fInput.fragNodeExtendedPixelCoord.z - fInput.fragNodeExtendedPixelCoord.x;
		bool 	isLine = abs(invCoordX - fInput.fragNodeExtendedPixelCoord.y) < lineWidth;
		fOutput.outColor = isLine ? vec4(fInput.fragColor0.xyz, 1.0f / 3.0f) : vec4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	else
#	endif
	{
		// Draw dashed lines:
		// Dash:
		float 	dashSize = ceil(fInput.fragNodePixelCoord.w * 0.2f);
		int 	dashFrequency = 3;
		float 	coord = fInput.fragNodePixelCoord.x + fInput.fragNodePixelCoord.y;

		// Do a fake AA by supersampling before and after:
		vec4 	colDashCur = (int((coord + 0.0f) / dashSize) % dashFrequency == 0) ? fInput.fragColor0 : fInput.fragColor1;
		vec4 	colDashPos = (int((coord + 0.5f) / dashSize) % dashFrequency == 0) ? fInput.fragColor0 : fInput.fragColor1;
		vec4 	colDashNeg = (int((coord - 0.5f) / dashSize) % dashFrequency == 0) ? fInput.fragColor0 : fInput.fragColor1;
		fOutput.outColor = (colDashCur + colDashPos + colDashNeg) * (1.0f / 3.0f);
#	if defined(PROFILER_OLD_STYLE)
		// Border:
		vec2 	borderSize = vec2(0.0f, 2.0f);
		if (ANY_BOOL(VEC_LESS_THAN(fInput.fragNodePixelCoord.xy, borderSize)) ||
			ANY_BOOL(VEC_LESS_THAN((fInput.fragNodePixelCoord.zw - fInput.fragNodePixelCoord.xy), borderSize)))
			fOutput.outColor = fInput.fragColor2;
#	endif
	}
#else
	fOutput.outColor = fInput.fragColor0;
#endif
}
