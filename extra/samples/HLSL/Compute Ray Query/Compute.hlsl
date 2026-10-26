#include "SphereVisibility.hlsl"
#include "RayInputs.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
StructuredBuffer<float4> Spheres : register(t1);
RWTexture2D<float4> OutputImage : register(u0);
RWStructuredBuffer<uint4> Results : register(u1);

[numthreads(8, 1, 1)]
void ComputeVisibility(uint3 dispatchId : SV_DispatchThreadID)
{
    const uint index = dispatchId.x;
    if (index >= 36u)
        return;

    const float4 originMin = asfloat(RayOrigins[index]);
    const float4 directionMax = asfloat(RayDirections[index]);
    RayDesc ray;
    ray.Origin = originMin.xyz;
    ray.Direction = directionMax.xyz;
    ray.TMin = originMin.w;
    ray.TMax = directionMax.w;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed()) {
        if (query.CandidateType() == CANDIDATE_PROCEDURAL_PRIMITIVE) {
            // Same identity instance, finite interval and solver as the raygen sample.
            float t;
            if (SphereHit(Spheres[query.CandidatePrimitiveIndex()],
                    query.CandidateObjectRayOrigin(), query.CandidateObjectRayDirection(),
                    query.RayTMin(), query.CommittedRayT(), t))
                query.CommitProceduralPrimitiveHit(t);
        }
    }
    const uint status = query.CommittedStatus();
    const bool blocked = status != COMMITTED_NOTHING;
    static const uint expected[8] = { 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u };
    uint expectedBlocked = 0xffffffffu;
    if (index < 8u)
        expectedBlocked = expected[index];
    const bool referenceOK = expectedBlocked == 0xffffffffu
        || uint(blocked) == expectedBlocked;
    Results[index] = uint4(uint(blocked), uint(referenceOK), expectedBlocked, status);

    // First two bands show compute visibility; bottom checks the eight references.
    // Full 36-case comparison with the unchanged pipeline/inline sample is external.
    const uint2 cell = uint2(index % 6u, index / 6u);
    [loop] for (uint y = 0u; y < 96u; ++y) {
        const uint band = y / 32u;
        const float3 color = band == 2u
            ? (referenceOK ? float3(0.1, 0.8, 0.2) : float3(1, 0, 0))
            : (blocked ? float3(0.08, 0.08, 0.12) : float3(0.9, 0.9, 0.9));
        [loop] for (uint x = 0u; x < 128u; ++x)
            OutputImage[cell * uint2(128, 96) + uint2(x, y)] =
                float4(x == 0u || y % 32u == 0u ? float3(0, 0, 0) : color, 1.0);
    }
}
