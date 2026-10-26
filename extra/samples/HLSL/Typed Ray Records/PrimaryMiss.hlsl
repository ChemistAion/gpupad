#include "Payload.hlsl"

[shader("miss")]
void PrimaryMiss(inout TestPayload payload)
{
    payload.Marker = 31u;
    payload.AppId = 0xffffffffu;
}
