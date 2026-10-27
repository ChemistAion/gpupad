#include "Payload.hlsl"

[shader("miss")]
void Miss(inout TestPayload payload)
{
    payload.Marker = 0u;
    payload.AppId = 0xffffffffu;
}
