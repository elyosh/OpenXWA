float4 main(float4 position : SV_Position, float4 color : COLOR0) : SV_Target0
{
    return color + position.x * 0.0f;
}
