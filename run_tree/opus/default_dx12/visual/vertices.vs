// #shader_type vs_5_0
// #entry_point vs_main

/*
struct VS_INPUT
{
    float4 a_position : POSITION;
    float3 a_normal   : NORMAL;
    float2 a_texcoord : TEXCOORD0;
};
*/

struct VS_OUTPUT
{
    float4 v_position : SV_POSITION;
    float3 v_normal   : NORMAL;
    float2 v_texcoord : TEXCOORD0;
};

VS_OUTPUT vs_main(uint id : SV_VERTEXID)
{
    VS_OUTPUT Out;
    Out.v_position.x = (float)(id / 2) * 4.0 - 1.0;
    Out.v_position.y = (float)(id % 2) * 4.0 - 1.0;
    Out.v_position.z = 0.0;
    Out.v_position.w = 1.0;

    Out.v_texcoord.x = (float)(id / 2) * 2.0;
    Out.v_texcoord.y = 1.0 - (float)(id % 2) * 2.0;

    Out.v_normal.x = 0.0;
    Out.v_normal.y = 0.0;
    Out.v_normal.z = -1.0;

    return Out;
}

/*
float time;
float2 resolution;
float2 mouse;
float3 spectrum;
float4x4 mvp;
*/

/*
VS_OUTPUT vs_main(in VS_INPUT In)
{
    VS_OUTPUT Out;
    Out.v_position = mul(mvp, In.a_position);
    Out.v_normal   = In.a_normal;
    Out.v_texcoord = In.a_texcoord;
    return Out;
}
*/

