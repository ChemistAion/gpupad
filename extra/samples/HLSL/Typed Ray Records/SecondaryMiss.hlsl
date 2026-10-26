#include "Payload.hlsl"

[shader("miss")]
void SecondaryMiss(inout TestPayload payload)
{
    payload.Marker = 32u;
    payload.AppId = 0xffffffffu;
}
