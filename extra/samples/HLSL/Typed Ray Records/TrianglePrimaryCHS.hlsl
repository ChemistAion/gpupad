#include "Payload.hlsl"

[shader("closesthit")]
void TrianglePrimaryCHS(inout TestPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    payload.Marker = 11u;
    payload.AppId = InstanceID();
}
