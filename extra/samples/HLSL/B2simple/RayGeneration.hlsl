RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

struct Result
{
    uint Hit;
    uint Expected;
    uint Match;
    float HitT;
};
RWStructuredBuffer<Result> Results : register(u1);

[shader("raygeneration")]
void RayGeneration()
{
    const uint index = DispatchRaysIndex().x;
    RayDesc ray;
    ray.Origin = float3(index == 1u ? 2.0 : 0.0, 0.0, -2.0);
    ray.Direction = float3(0.0, 0.0, index == 3u ? -1.0 : 1.0);
    ray.TMin = 0.01;
    ray.TMax = index == 2u ? 1.0 : 10.0;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed()) {
        // Opaque native triangles commit in hardware; no candidate handler needed.
    }

    Result result;
    result.Hit = uint(query.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
    // Analytically specified answers, independent of any other GPU tracing path.
    static const uint expected[4] = { 1u, 0u, 0u, 0u };
    result.Expected = expected[index];
    result.Match = uint(result.Hit == result.Expected);
    result.HitT = result.Hit != 0u ? query.CommittedRayT() : -1.0;
    Results[index] = result;

    // Four tiles, left to right: green/red agreement above, hit/miss below.
    [loop] for (uint y = 0u; y < 96u; ++y) {
        const float3 color = y < 64u
            ? (result.Match != 0u ? float3(0.1, 0.8, 0.2) : float3(1, 0, 0))
            : (result.Hit != 0u ? float3(0.08, 0.08, 0.12) : float3(0.9, 0.9, 0.9));
        [loop] for (uint x = 0u; x < 96u; ++x)
            OutputImage[uint2(index * 96u + x, y)] = float4(
                x == 0u || y == 0u || y == 64u ? float3(0, 0, 0) : color, 1.0);
    }
}
