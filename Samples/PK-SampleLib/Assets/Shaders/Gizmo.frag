
void 	FragmentMain(IN(SFragmentInput) fInput, OUT(SFragmentOutput) fOutput FS_ARGS)
{
	fOutput.outColor = vec4(fInput.fragColor, 1);
}
