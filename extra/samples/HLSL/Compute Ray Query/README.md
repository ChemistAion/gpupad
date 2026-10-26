# Compute Ray Query

Direct3D12, DXC, shader model 6.5; requires DXR Tier 1.1 or later.
Open `compute-query.gpjs` and evaluate. The compute Call explicitly selects
`Scene` through its optional **Acceleration Structure** property.

This is an ordinary compute PSO and Dispatch, without raygeneration, hit/miss
shaders, a DXR state object or shader tables. A procedural AABB is only a
candidate: the caller uses the shared sphere solver and commits an in-range hit.
The native descriptor path builds/orders the BLAS/TLAS before dispatch.

The 36 FP32 rays, sphere solver and authored unit-sphere bounds are identical to
`../Ray Query Visibility/`. Five groups of eight threads process 36 cases; the
last four threads exit. Query flags are FORCE_OPAQUE and ACCEPT_FIRST_HIT only.
The pipeline sample's skipped-empty-CHS workaround is irrelevant to this caller.

`Results/Ray` has four uint32 fields: ComputeBlocked, ReferenceOK,
ExpectedBlocked (0xffffffff outside the first eight), and CommittedStatus.
Each 6x6 image tile shows visibility twice and then a reference-check band.
Green checks the eight independent references; the remaining tiles have no
independent reference. Compare all 36 ComputeBlocked values with both columns
of the unchanged Ray Query Visibility sample for full acceptance.

The field is optional for existing sessions and ordinary compute shaders. AS
selection is exposed for the compute Call family; this sample exercises direct
Dispatch on Direct3D12. Other renderers' inline AS support is outside this example.
