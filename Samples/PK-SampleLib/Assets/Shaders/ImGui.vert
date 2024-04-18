
void 	VertexMain(IN(SVertexInput) vInput, OUT(SVertexOutput) vOutput VS_ARGS)
{
	vOutput.fragColor = vInput.Color;
    vOutput.fragTexCoord = vInput.TexCoord;
    vOutput.VertexPosition = vec4(vInput.Position * GET_CONSTANT(ImGui, Scale) + GET_CONSTANT(ImGui, Translate), 0, 1);
}
