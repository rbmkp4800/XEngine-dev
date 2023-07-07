struct ViewConstants
{
    float4 dummy;
};

[[xe::binding(VIEW_CONSTANTS)]] ConstantBuffer<ViewConstants> c_viewConstants;

float4 main() : SV_Target0
{
    return float4(0, frac(c_viewConstants.dummy.x).xx, 1);
}
