#include "Payload.hlsl"
#include "SphereVisibility.hlsl"

[shader("closesthit")]
void SphereSecondaryCHS(inout TestPayload payload, in SphereAttributes attributes)
{
    payload.Marker = 22u;
    payload.AppId = InstanceID();
}
