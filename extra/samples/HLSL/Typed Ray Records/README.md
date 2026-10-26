# Typed ray records — Stage010-D1 MVP

Open `records.gpjs` with a Stage010-D1 build. This native Direct3D12/DXC DXR
fixture uses two ray types, four named typed hit groups, two miss records and
one raygeneration record. All shaders remain ordinary editable session items.

Program's optional `rayTracing` object names exports/groups and ordered table
records. Its Direct3D Program property editor displays the object as JSON.
Only valid JSON objects are applied; invalid edits retain the previous value.
Empty configuration preserves the legacy single-group pipeline. Explicit mode
compiles ray-stage items independently, using unique nonempty entry names.
Existing non-ray/legacy compilation behavior is preserved.

Each group has `name`, `type` (Triangles/Procedural) and optional `closestHit`,
`anyHit`, `intersection` imports. Procedural requires IS; triangle forbids IS.
Optional CHS/AHS are genuinely optional; the host inserts no dummy shader.
Raygen/miss records select shader exports; hit records select group exports.
Array order is record index order, including when an export is repeated.
Unknown/conflicting names and mismatched import types are errors.
D1 does not accept localRoots, localRoot associations or record arguments;
per-record local constants belong to D2. No new generic binding framework.

One triangle BLAS is translated to x=-2, ID17, contribution0. One procedural
unit-sphere BLAS is translated to x=2, ID42, contribution2. Each has one geometry.
Both masks are255. The sphere helper is copied unchanged from the accepted
Compute Ray Query sample. Rays start at z=-3/y=0, point+Z, TMin0.01/TMax10;
flags FORCE_OPAQUE|ACCEPT_FIRST_HIT_AND_END_SEARCH, mask255. CHS runs normally.

```text
hitIndex = instanceContribution + 2*geometryIndex + rayType
triangle: records0/1; sphere: records2/3; missIndex = rayType
```

| Case | x | Ray type | Expected export | Marker | App ID |
|---|---:|---:|---|---:|---:|
|0 |-2 |0 |TrianglePrimaryCHS |11 |17 |
|1 |2 |0 |SpherePrimaryCHS |21 |42 |
|2 |5 |0 |PrimaryMiss |31 |0xffffffff |
|3 |-2 |1 |TriangleSecondaryCHS |12 |17 |
|4 |2 |1 |SphereSecondaryCHS |22 |42 |
|5 |5 |1 |SecondaryMiss |32 |0xffffffff |

Results/Ray has six uint4 rows: Marker, Expected, InstanceID, Match.
All Match values should be1. Board: three columns, two tile rows; each tile's
upper band is green on agreement/red on failure, bottom is dark on hit/white
on miss. Expected bands: G/G/G over B/B/W, repeated for the second ray type.

SBT records are32-byte state-object identifiers, no local arguments. Bases are
64-byte aligned: raygen0, miss64 (two records), hit128 (four), allocation256 bytes.
Existing pipeline ownership retains the SBT, state object and global resources.
Set `GPUPAD_DXR_SBT_INFO=1` for a small host log of actual record copies and layout.
This fixture proves the six authored paths; it is not a numerical stress test,
generic renderer, performance comparison or validation of every DXR configuration.
