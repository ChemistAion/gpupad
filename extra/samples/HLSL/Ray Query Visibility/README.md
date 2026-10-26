# Ray Query Visibility — functionally accepted composition workaround

**010-B2 FUNCTIONALLY ACCEPTED / ROOT CAUSE UNRESOLVED** — rtj's adoption
instruction, 2026-10-26; final headless regression passes36/36 and all eight
independent references. This is platform-specific functional acceptance;
publication and wider adoption remain with rtj.

Requires Direct3D12 DXR Tier1.1+, DXC and SM6.5. Open rayquery.gpjs and evaluate.
The6x6 board compares pipeline and inline visibility for36 fixed FP32 rays.
Top: pipeline; middle: inline; bottom: green agreement, red disagreement or
failed reference. Dark means blocked, white clear. Results/Ray exposes
PipelineBlocked, InlineBlocked, Match and ExpectedBlocked. The first eight
cases have independent expectations; later ExpectedBlocked values are0xffffffff.

Both paths use the original unit sphere, AABB, identity instance, finite ray
intervals and shifted-origin/direct-root solver, including nonunit directions.
Pipeline flags remain FORCE_OPAQUE | ACCEPT_FIRST_HIT_AND_END_SEARCH |
SKIP_CLOSEST_HIT_SHADER. The miss shader clears the initialized uint1 payload.
Inline queries solve and commit procedural candidates explicitly.

## Candidate qualification

DXR permits procedural hit groups with no closest-hit shader. Nevertheless,
the tested local stack corrupts payloads for that legal composition. Adding an
exported, associated, **empty** RayClosestHit entry restores preservation even
though the trace still skips it. The reviewed candidate5090383b is adopted into
dxr-raygen-query with its history preserved. This is a validated composition
workaround, not a demonstrated GPUpad root-cause fix or a DXR requirement.

The clean combined sample passes36/36 and all eight references on the existing
accepted GPUpad executable. A separate guarded combined control preserves all
payload words. This sample has no diagnostic buffer, witness writes, temporary
barriers or hit-shader payload writes. Captured DXIL for the empty hit entry
contains only ret void apart from debug information.

An independent plain-D3D12 runner replays the same submitted shader bytes and
configuration on the same GPU: IS-only fails and IS+skipped-CHS passes with
byte-identical results/diagnostics to GPUpad. Exact compiler/runtime/driver
attribution remains unresolved. Further attribution is separate research and
does not gate010-B3 compute RayQuery/TLAS binding. B3 is authorized next but is
not implemented by this adoption. Do not infer that closest-hit is mandatory
in DXR or change generic host behavior.

Acceptance environment: NVIDIA RTX5080, driver617.42 (Windows32.0.16.1742),
the accepted GPUpad4.6.0 B1 executable and its unchanged DXC package. DXC reports
1.9 /1.8.2505.32(b106a961d); selected shader profile is lib_6_5. No universal
portability or performance claim is made. Original failing IS-only, passing
composition and guarded controls plus independent replay remain regression
evidence in engine's010-B2 records; preserve them when extending this sample.

Provenance: rtj's engine Stage010-B2 R11/R12 investigation. Sphere arithmetic
and36 uint4 ray records derive from engine Stage007/008 precision controls;
source indices0..31 and36..39 retain their original FP32 bits. This sample
changes no renderer transport, native host or compute-AS binding feature.
