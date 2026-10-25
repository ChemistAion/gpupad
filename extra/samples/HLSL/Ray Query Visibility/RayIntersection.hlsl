#include "SphereVisibility.hlsl"

StructuredBuffer<float4> Spheres : register(t1);

[shader("intersection")]
void RayIntersection()
{
    float t;
    if (SphereHit(Spheres[PrimitiveIndex()], ObjectRayOrigin(), ObjectRayDirection(),
            RayTMin(), RayTCurrent(), t)) {
        SphereAttributes attributes;
        attributes.PrimitiveId = PrimitiveIndex();
        ReportHit(t, 0, attributes);
    }
}
