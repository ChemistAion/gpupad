#include "SphereVisibility.hlsl"
#include "PipelineDiagnostics.hlsl"

StructuredBuffer<float4> Spheres : register(t1);

[shader("intersection")]
void RayIntersection()
{
    const float4 sphere = Spheres[PrimitiveIndex()];
    const float3 origin = ObjectRayOrigin();
    const float3 direction = ObjectRayDirection();
    const float tMin = RayTMin();
    const float tMax = RayTCurrent();
    float t;
    const bool found = SphereHit(sphere, origin, direction, tMin, tMax, t);
    const uint index = DispatchRaysIndex().y * 6u + DispatchRaysIndex().x;
    PipelineState diagnostic = (PipelineState)0;
    diagnostic.OriginHitT = float4(origin, t);
    diagnostic.DirectionMin = float4(direction, tMin);
    diagnostic.Sphere = sphere;
    diagnostic.State = float4(1.0, found ? 1.0 : 0.0, tMax, -1.0);
    PipelineDiagnostics[index] = diagnostic;
    if (found) {
        SphereAttributes attributes;
        attributes.PrimitiveId = PrimitiveIndex();
        const bool accepted = ReportHit(t, 0, attributes);
        // First-hit termination can leave this at -1 after an accepted report.
        PipelineDiagnostics[index].State.w = accepted ? 1.0 : 0.0;
    }
}
