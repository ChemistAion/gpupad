#include "Payload.hlsl"

[shader("closesthit")]
void TriangleSecondaryCHS(inout TestPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    payload.Marker = 12u;
    payload.AppId = InstanceID();
}
