#include "Payload.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);
RWStructuredBuffer<uint4> Results : register(u1);

[shader("raygeneration")]
void RayGeneration()
{
    const uint2 cell = DispatchRaysIndex().xy;
    const uint index = cell.y * 3u + cell.x;
    const uint rayType = cell.y;
    RayDesc ray;
    ray.Origin = float3(cell.x == 0u ? -2.0 : (cell.x == 1u ? 2.0 : 5.0), 0, -3);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.01;
    ray.TMax = 10.0;
    TestPayload payload;
    payload.Marker = 0u;
    payload.AppId = 0xffffffffu;
    TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0xff, rayType, 2u, rayType, ray, payload);

    // Fixed geometric/ray-type expectations, not another tracing implementation.
    static const uint expected[6] = { 11u, 21u, 31u, 12u, 22u, 32u };
    const uint expectedID = cell.x == 0u ? 17u : (cell.x == 1u ? 42u : 0xffffffffu);
    const bool match = payload.Marker == expected[index] && payload.AppId == expectedID;
    Results[index] = uint4(payload.Marker, expected[index], payload.AppId, uint(match));

    [loop] for (uint y = 0u; y < 96u; ++y) {
        const float3 color = y < 64u
            ? (match ? float3(0.1, 0.8, 0.2) : float3(1, 0, 0))
            : (payload.AppId != 0xffffffffu ? float3(0.08, 0.08, 0.12) : float3(0.9, 0.9, 0.9));
        [loop] for (uint x = 0u; x < 128u; ++x)
            OutputImage[cell * uint2(128, 96) + uint2(x, y)] = float4(
                x == 0u || y == 0u || y == 64u ? float3(0, 0, 0) : color, 1.0);
    }
}
