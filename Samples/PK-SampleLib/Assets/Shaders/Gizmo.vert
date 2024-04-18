
void 	VertexMain(IN(SVertexInput) vInput, OUT(SVertexOutput) vOutput VS_ARGS)
{
	vec4 	grabbedColor = GET_CONSTANT(GizmoConstant, GrabbedColor);
	vec4 	hoveredColor = GET_CONSTANT(GizmoConstant, HoveredColor);

	vOutput.VertexPosition = mul(GET_CONSTANT(GizmoConstant, ModelViewProj), vec4(vInput.Position, 1.));
	if (hoveredColor.a == 1.)
		vOutput.fragColor = hoveredColor.rgb;
	else if (grabbedColor.a == 1.)
		vOutput.fragColor = grabbedColor.rgb;
	else
		vOutput.fragColor = vInput.Color;
}
