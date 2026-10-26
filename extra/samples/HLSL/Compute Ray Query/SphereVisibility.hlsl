#ifndef SPHERE_VISIBILITY_HLSL
#define SPHERE_VISIBILITY_HLSL

struct VisibilityPayload { uint Blocked; };
struct SphereAttributes { uint PrimitiveId; };

// Shifted-origin/direct-root sphere solver shared by pipeline and inline paths.
// Keep the two strict upper-bound checks and restore the original ray parameter.
bool SphereHit(float4 sphere, float3 origin, float3 direction,
    float tMin, float tMax, out float t)
{
    t = 0.0;
    const float3 originalOc = origin - sphere.xyz;
    const float a = dot(direction, direction);
    const float originalB = dot(originalOc, direction);
    const float shift = -originalB / a;
    const float3 shiftedOrigin = origin + shift * direction;
    const float3 oc = shiftedOrigin - sphere.xyz;
    const float localMin = tMin - shift;
    const float localMax = tMax - shift;
    const float b = dot(oc, direction);
    const float c = dot(oc, oc) - sphere.w * sphere.w;
    const float discriminant = b * b - a * c;
    if (discriminant >= 0.0) {
        const float root = sqrt(discriminant);
        const float nearT = (-b - root) / a;
        const float farT = (-b + root) / a;
        const float localT = nearT >= localMin ? nearT : farT;
        if (localT >= localMin && localT < localMax) {
            const float restoredT = localT + shift;
            if (restoredT >= tMin && restoredT < tMax) {
                t = restoredT;
                return true;
            }
        }
    }
    return false;
}

#endif
