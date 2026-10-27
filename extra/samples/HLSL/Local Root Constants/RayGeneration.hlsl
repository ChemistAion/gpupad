#include "Payload.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);
RWStructuredBuffer<uint4> Results : register(u1);

[shader("raygeneration")]
void RayGeneration()
{
    const uint index = DispatchRaysIndex().x;
    RayDesc ray;
    ray.Origin = float3(index == 2u ? 3.0 : 0.0, 0, -2);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.01;
    ray.TMax = 10.0;
    TestPayload payload;
    payload.Marker = 0xffffffffu;
    payload.AppId = 0xffffffffu;
    TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0xff, index == 1u ? 1u : 0u, 2u, 0u, ray, payload);

    // Independent authored expectations; both hits select the same group/export.
    static const uint expected[3] = { 17u, 42u, 0u };
    const uint expectedID = index == 2u ? 0xffffffffu : 17u;
    const bool match = payload.Marker == expected[index] && payload.AppId == expectedID;
    Results[index] = uint4(payload.Marker, expected[index], payload.AppId, uint(match));
    [loop] for (uint y = 0u; y < 96u; ++y) {
        const float3 color = y < 64u
            ? (match ? float3(0.1, 0.8, 0.2) : float3(1, 0, 0))
            : (payload.AppId != 0xffffffffu ? float3(0.08, 0.08, 0.12) : float3(0.9, 0.9, 0.9));
        [loop] for (uint x = 0u; x < 128u; ++x)
            OutputImage[uint2(index * 128u + x, y)] = float4(
                x == 0u || y == 0u || y == 64u ? float3(0, 0, 0) : color, 1.0);
    }
}
