struct ViewConstants
{
    float4 dummy;
};

ConstantBuffer<ViewConstants> c_viewConstants : @binding(VIEW_CONSTANTS);

float4 main() : SV_Target0
{
    return float4(0, frac(c_viewConstants.dummy.x).xx, 1);
}
