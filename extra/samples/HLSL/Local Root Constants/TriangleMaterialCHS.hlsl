#include "Payload.hlsl"

// One DWORD from the selected SBT record; no global CBV supplies this value.
cbuffer Material : register(b1, space1)
{
    uint MaterialValue;
};

[shader("closesthit")]
void TriangleMaterialCHS(inout TestPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    payload.Marker = MaterialValue;
    payload.AppId = InstanceID();
}
