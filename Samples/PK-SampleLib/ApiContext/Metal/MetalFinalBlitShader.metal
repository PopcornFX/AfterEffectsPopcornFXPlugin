#include <metal_stdlib>
using namespace metal;

struct SVertexOutput
{
	float2 TexCoords [[user(attribute0)]];
	float4 VertexPosition [[position]];
};

vertex SVertexOutput vert_main(uint vertexID [[vertex_id]])
{
    uint    indices[6] = {
        0, 1, 2,
        1, 3, 2
    };
    float2  positions[4] =  {
	    float2 (-1.0, -1.0),
	    float2 (-1.0, 1.0),
	    float2 (1.0, -1.0),
	    float2 (1.0, 1.0),
    };
    float2  uvs[4] =  {
	    float2 (0.0, 0.0),
	    float2 (0.0, 1.0),
	    float2 (1.0, 0.0),
	    float2 (1.0, 1.0),
    };

    SVertexOutput   out;

    uint     curIdx = indices[vertexID];

    out.TexCoords = uvs[curIdx];
    out.VertexPosition = float4(positions[curIdx], 0, 1);
    return out;
}

struct  SFragmentOutput
{
	float4 outColor [[color(0)]];
};

fragment SFragmentOutput    frag_main(SVertexOutput fInput [[stage_in]], texture2d<float> finalRendering [[texture(0)]])
{
    SFragmentOutput     out;
    constexpr sampler   linearSampler ( mip_filter::linear,
                                        mag_filter::linear,
                                        min_filter::linear);

    out.outColor = finalRendering.sample(linearSampler, fInput.TexCoords);
    return out;
}