
void 	FragmentMain(IN(SFragmentInput) fInput, OUT(SFragmentOutput) fOutput FS_ARGS)
{
	fOutput.outColor = fInput.fragColor * SAMPLE(Texture, fInput.fragTexCoord).r;
}
