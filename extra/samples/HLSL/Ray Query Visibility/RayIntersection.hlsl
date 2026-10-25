#include "SphereVisibility.hlsl"

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
    if (found) {
        SphereAttributes attributes;
        attributes.PrimitiveId = PrimitiveIndex();
        ReportHit(t, 0, attributes);
    }
}
