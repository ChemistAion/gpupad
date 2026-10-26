# B2simple - raygeneration inline RayQuery

Minimal Direct3D12 / DXC SM 6.5 capability sample. One opaque unindexed triangle,
one identity TLAS instance, four fixed rays and independently specified answers.
Open `b2simple.gpjs` with an SM 6.5-enabled Stage010 GPUpad build, reset evaluation
and inspect `Results / Ray` or `OutputImage`.

Triangle vertices: `(-1,-1,0), (1,-1,0), (0,1,0)`. All rays use `TMin=0.01`,
mask `0xff`, `FORCE_OPAQUE | ACCEPT_FIRST_HIT_AND_END_SEARCH`, no face culling.

| Case / tile (left to right) | Origin | Direction | TMax | Expected Hit | HitT |
|---|---|---|---:|---:|---:|
| 0 - interior hit | (0,0,-2) | (0,0,1) | 10 | 1 | 2 |
| 1 - outside triangle | (2,0,-2) | (0,0,1) | 10 | 0 | -1 |
| 2 - interval ends early | (0,0,-2) | (0,0,1) | 1 | 0 | -1 |
| 3 - points away | (0,0,-2) | (0,0,-1) | 10 | 0 | -1 |

Results are four 16-byte rows: `uint Hit, Expected, Match; float HitT`.
`HitT=-1` denotes a miss. All four `Match` fields should be 1. Each tile's upper
two thirds are green for a match, red for failure; its bottom third is dark for
a hit and white for a miss. Expected bottom bands: dark / white / white / white.
Tile borders and the band separator are black.

`Raygeneration Inline Query` explicitly selects `Scene` as its acceleration
structure. Its 4x1x1 DispatchRays launches `RayGeneration` at `lib_6_5`.
`TraceRayInline`, `Proceed` and `CommittedStatus` drive the actual result;
opaque native triangles need no custom intersection or candidate acceptance.

GPUpad currently requires RayGeneration and a valid Miss export when creating
the raygeneration RTPSO. `RayMiss` is empty and unused; its formal payload is
never transmitted. There is no hit group, conventional TraceRay call, pipeline
comparison, procedural solver or skipped-closest-hit workaround in this sample.
Inline traversal invokes no pipeline hit/miss shader. The raygeneration caller
still launches through the host RTPSO and shader table.

The separate `Ray Query Visibility` sample retains the accepted procedural
pipeline/inline investigation and its platform-specific composition workaround.
This four-case proof makes no root-cause, portability or performance claim.
