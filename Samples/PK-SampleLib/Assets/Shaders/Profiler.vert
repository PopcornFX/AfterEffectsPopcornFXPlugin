
void 	VertexMain(IN(SVertexInput) vInput, OUT(SVertexOutput) vOutput VS_ARGS)
{
	vOutput.fragColor = vInput.Color;
	vOutput.VertexPosition = mul(GET_CONSTANT(ProfilerConstant, Transform), vec4(vInput.Position.xy, 0.0, 1.0));
}
