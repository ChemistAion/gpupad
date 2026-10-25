#include "SphereVisibility.hlsl"
#include "RayInputs.hlsl"
#include "PipelineDiagnostics.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
StructuredBuffer<float4> Spheres : register(t1);
RWTexture2D<float4> OutputImage : register(u0);
RWStructuredBuffer<uint4> Results : register(u1);

bool PipelineBlocked(RayDesc ray)
{
    VisibilityPayload payload;
    payload.Blocked = 1u;
    TraceRay(Scene, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
        | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xff, 0, 0, 0, ray, payload);
    return payload.Blocked != 0u;
}

bool InlineBlocked(RayDesc ray)
{
    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed()) {
        if (query.CandidateType() == CANDIDATE_PROCEDURAL_PRIMITIVE) {
            // One identity instance/geometry: primitive IDs index Spheres directly.
            // A procedural AABB remains only a candidate, including FORCE_OPAQUE.
            float t;
            if (SphereHit(Spheres[query.CandidatePrimitiveIndex()],
                    query.CandidateObjectRayOrigin(), query.CandidateObjectRayDirection(),
                    query.RayTMin(), query.CommittedRayT(), t))
                query.CommitProceduralPrimitiveHit(t);
        }
    }
    return query.CommittedStatus() != COMMITTED_NOTHING;
}

[shader("raygeneration")]
void RayGeneration()
{
    const uint2 cell = DispatchRaysIndex().xy;
    const uint index = cell.y * 6u + cell.x;
    const float4 originMin = asfloat(RayOrigins[index]);
    const float4 directionMax = asfloat(RayDirections[index]);
    RayDesc ray;
    ray.Origin = originMin.xyz;
    ray.Direction = directionMax.xyz;
    ray.TMin = originMin.w;
    ray.TMax = directionMax.w;
    PipelineState initialState = (PipelineState)0;
    initialState.State.w = -1.0;
    PipelineDiagnostics[index] = initialState;
    DeviceMemoryBarrier();
    const bool pipeline = PipelineBlocked(ray);
    const bool inlineHit = InlineBlocked(ray);

    // Simple reference cases also catch a broken all-miss setup.
    static const uint expected[8] = { 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u };
    const bool match = pipeline == inlineHit;
    uint expectedBlocked = 0xffffffffu;
    if (index < 8u)
        expectedBlocked = expected[index];
    const bool referenceOK = expectedBlocked == 0xffffffffu
        || uint(pipeline) == expectedBlocked;
    Results[index] = uint4(uint(pipeline), uint(inlineHit), uint(match),
        expectedBlocked);

    // One dispatch invocation per fixed ray; three bands in each image tile.
    [loop] for (uint y = 0u; y < 96u; ++y) {
        const uint band = y / 32u;
        const bool blocked = band == 0u ? pipeline : inlineHit;
        const float3 color = band == 2u
            ? (match && referenceOK ? float3(0.1, 0.8, 0.2) : float3(1, 0, 0))
            : (blocked ? float3(0.08, 0.08, 0.12) : float3(0.9, 0.9, 0.9));
        [loop] for (uint x = 0u; x < 128u; ++x)
            OutputImage[cell * uint2(128, 96) + uint2(x, y)] =
                float4(x == 0u || y % 32u == 0u ? float3(0, 0, 0) : color, 1.0);
    }
}
