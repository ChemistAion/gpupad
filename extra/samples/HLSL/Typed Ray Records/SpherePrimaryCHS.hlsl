#include "Payload.hlsl"
#include "SphereVisibility.hlsl"

[shader("closesthit")]
void SpherePrimaryCHS(inout TestPayload payload, in SphereAttributes attributes)
{
    payload.Marker = 21u;
    payload.AppId = InstanceID();
}
