# Shared BLAS instances (Stage010-C)

Direct3D12/DXC SM6.5 compute RayQuery fixture. Open `instances.gpjs` with a
Stage010-C build, reset evaluation and inspect `Results / Ray` and `OutputImage`.
The compute Call explicitly selects acceleration structure `Scene`.

`LeftOwner` owns one unindexed opaque triangle. `RightShared` uses
`sourceInstanceId: 61`, referencing the same actual BLAS, with its own transform
and descriptor fields. Row-major3x4 transforms place the triangle at x=-2 and+2.

| Instance | customIndex (DXR InstanceID) | mask | hitGroupOffset |
|---|---:|---:|---:|
| LeftOwner |17 |1 |3 |
| RightShared |42 |2 |7 |

Optional Direct3D Instance properties:

- `sourceInstanceId`: sibling geometry owner in the same AS. Zero/omitted means
  own child geometry. References may point forward; chains, self-reference,
  missing targets and combining a reference with own geometry are rejected.
- `customIndex`: integer expression,0..0xffffff; empty uses the legacy ordinal.
  This application ID differs from the query's TLAS InstanceIndex.
- `mask`: integer expression,0..255; default255, zero excludes the instance.
- `hitGroupOffset`: integer expression,0..0xffffff; default0. This is an SBT
  record contribution, not a byte offset. Inline queries ignore it; multi-record
  selection is outside C's acceptance and belongs to D1.
- `transform`: existing instance-to-world affine rows; empty is identity.

These new fields are exposed in Direct3D's Instance property editor and survive
session serialization. Existing sessions retain their defaults. A geometry
owner is also a TLAS instance; mask0 can exclude that owner from traversal.
This step shares BLASes within one AS; it does not introduce a global BLAS cache.
Changing these expressions does not imply a live AS update/refit implementation.

All rays have origin z=-2/y=0, direction+Z, TMin0.01/TMax10. Results are eight
uint4 rows: Hit, InstanceID, InstanceIndex, Match. Miss IDs/indices are0xffffffff.

| Case | Origin x | Inclusion mask | Hit | ID | Index |
|---|---:|---:|---:|---:|---:|
|0 |-2 |1 |1 |17 |0 |
|1 |-2 |2 |0 |0xffffffff |0xffffffff |
|2 |2 |2 |1 |42 |1 |
|3 |2 |1 |0 |0xffffffff |0xffffffff |
|4 |-2 |3 |1 |17 |0 |
|5 |2 |3 |1 |42 |1 |
|6 |-2 |0 |0 |0xffffffff |0xffffffff |
|7 |5 |3 |0 |0xffffffff |0xffffffff |

Each tile has a green/red agreement band above a dark-hit/white-miss band.
Read cases0..3 across the top tile row, then4..7 across the bottom.
Expected visibility rows: dark/white/dark/white, then dark/dark/white/white;
all agreement bands green.

Set `GPUPAD_DXR_INSTANCE_INFO=1` before launch for a small optional host info
message showing each actual descriptor's BLAS GPU address, owner, ID, mask and
contribution, plus BLAS-build/TLAS-instance counts. Expected: one BLAS build,
two descriptors, identical nonzero addresses, owner61 for both. This is the
direct ownership observation; matching shader hits alone cannot prove sharing.
The log does not affect shader source or dispatch and is off by default.
