cbuffer HyperspaceStreakVS : register(b0, space1)
{
    row_major float4x4 view_proj;
};

struct VSIn
{
    float3 position : POSITION;
    float4 color : COLOR0;
};

struct VSOut
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

VSOut main(VSIn input)
{
    VSOut output;
    output.position = mul(view_proj, float4(input.position, 1.0f));
    output.color = input.color;
    return output;
}
