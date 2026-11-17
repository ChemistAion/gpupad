#include "RayPayload.hlsl"

RaytracingAccelerationStructure Scene : register(t0);
RWStructuredBuffer<uint2> Results : register(u0);

cbuffer Settings : register(b0)
{
    uint requestReorder;
};

[shader("raygeneration")]
void RayGeneration()
{
    uint index = DispatchRaysIndex().x;
    float x = index == 0 ? 0.0 : index == 1 ? 3.0 : index == 2 ? 0.25 : -3.0;

    RayDesc ray;
    ray.Origin = float3(x, 0.0, -2.0);
    ray.TMin = 0.001;
    ray.Direction = float3(0.0, 0.0, 1.0);
    ray.TMax = 10.0;

    RayPayload payload = { 0xffffffff, 0xffffffff };
    dx::HitObject hit = dx::HitObject::TraceRay(Scene,
        RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);
    if (requestReorder != 0)
        dx::MaybeReorderThread(hit);
    dx::HitObject::Invoke(hit, payload);
    if (payload.Hit != uint(hit.IsHit()))
        payload.Invoked = 0xffffffff;
    Results[index] = uint2(payload.Hit, payload.Invoked);
}
