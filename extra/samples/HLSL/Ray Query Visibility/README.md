# Ray Query Visibility

Requires Direct3D12 DXR Tier1.1+, DXC and SM6.5. This session selects6.5 through
the optional compiler setting; it does not require compute TLAS binding.

Open `rayquery.gpjs`, F5/evaluate, then open OutputImage and fit the image.
The6×6 board contains36 fixed rays, in row-major order. Each tile has three bands:

- Top: pipeline visibility, dark = blocked and white = clear.
- Middle: inline visibility, same colors.
- Bottom: green = agreement; red = mismatch or failed reference classification.

Results/Ray exposes four uint columns: PipelineBlocked, InlineBlocked, Match,
ExpectedBlocked. Match should be1 for every ray. The first eight cases also
have simple expected classifications `{1,0,0,1,1,1,1,1}`; later expected values
are `0xffffffff` (comparison only). A broken all-miss setup therefore cannot
pass merely because both paths miss everything. No timing conclusion is implied.

The first operator comparison found nine pipeline-clear/inline-blocked cases.
The board still exposes both paths; this is not yet an accepted equivalence test.
PipelineDiagnostics/Case now records each pipeline intersection callback:
OriginHitT = object origin + selected hit t; DirectionMin = object direction +
TMin; Sphere = center/radius; State = invoked, solver hit, initial TMax,
ReportHit return value (-1 means it did not return, or was not called).
The fifth row (case4, sphere-center origin) is the first focused diagnostic.
Zero invoked means no intersection callback was recorded. A changed pattern
with diagnostics is evidence to investigate, not proof of a numerical fix.

Both methods trace the same FP32 finite segment, mask0xff and opaque unit sphere.
The pipeline skips closest-hit; inline traversal handles procedural candidates
explicitly and never accepts their AABB as a surface hit. Both call SphereHit,
which retains the shifted-origin/direct-root arithmetic, strict local/original
upper bounds and unnormalized direction. This compares accepted visibility
policy; it does not repair near-zero/tangent limitations of that solver.

The raygen pipeline still uses its state object, procedural hit group and miss
record. The inline query itself invokes no intersection/closest-hit/miss shader
or SBT lookup. One identity instance and one geometry keep primitive indexing
explicit; general mixed/instanced addressing is outside this sample.

Provenance: sphere C arithmetic and36 exact uint4 origin/direction
inputs are reused from rtj's engine RTX Stage007/008 diagnostics. RayInputs
selects source indices0..31 and36..39, retaining original FP32 bits for
clear/blocked, interior, finite clipping,
grazing, distance and nonunit cases. Both paths use identical decoded inputs.
This addition belongs to Stage010-B2; no renderer material/transport or native
compute-binding behavior is changed.
