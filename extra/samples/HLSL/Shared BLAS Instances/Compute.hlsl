RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);
RWStructuredBuffer<uint4> Results : register(u1);

[numthreads(8, 1, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    const uint index = thread.x;
    static const float origins[8] = { -2, -2, 2, 2, -2, 2, -2, 5 };
    static const uint masks[8] = { 1, 2, 2, 1, 3, 3, 0, 3 };
    // Independently known fixture answers; 0xffffffff marks a miss.
    static const uint expectedID[8] = {
        17u, 0xffffffffu, 42u, 0xffffffffu, 17u, 42u, 0xffffffffu, 0xffffffffu
    };

    RayDesc ray;
    ray.Origin = float3(origins[index], 0, -2);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.01;
    ray.TMax = 10.0;
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, masks[index], ray);
    while (query.Proceed()) {
        // Opaque native triangles need no candidate callback.
    }

    const bool hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
    const uint instanceID = hit ? query.CommittedInstanceID() : 0xffffffffu;
    const uint instanceIndex = hit ? query.CommittedInstanceIndex() : 0xffffffffu;
    const uint expectedIndex = expectedID[index] == 17u ? 0u
        : (expectedID[index] == 42u ? 1u : 0xffffffffu);
    const bool match = uint(hit) == uint(expectedID[index] != 0xffffffffu)
        && instanceID == expectedID[index] && instanceIndex == expectedIndex;
    Results[index] = uint4(uint(hit), instanceID, instanceIndex, uint(match));

    const uint2 cell = uint2(index % 4u, index / 4u);
    [loop] for (uint y = 0u; y < 96u; ++y) {
        const float3 color = y < 64u
            ? (match ? float3(0.1, 0.8, 0.2) : float3(1, 0, 0))
            : (hit ? float3(0.08, 0.08, 0.12) : float3(0.9, 0.9, 0.9));
        [loop] for (uint x = 0u; x < 128u; ++x)
            OutputImage[cell * uint2(128, 96) + uint2(x, y)] = float4(
                x == 0u || y == 0u || y == 64u ? float3(0, 0, 0) : color, 1.0);
    }
}
