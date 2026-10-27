# Local root constants

Direct3D12/DXC lib_6_5 capability sample. Open `constants.gpjs` and reset evaluation.
One triangle/instance (InstanceID17), two hit records naming the same
`TriangleMaterial` group and `TriangleMaterialCHS` export. The records carry
different local DWORD values17/42 at b1/space1; no session CBV binds Material.
Ray contributions0/1 select these records. A third ray misses.

Expected OutputImage (three columns):

```text
G G G
B B W
```

Results/Ray contains (actual, expected, InstanceID, Match):

```text
17 17 17         1
42 42 17         1
0  0  4294967295 1
```

The Program's DXR Records JSON defines the local schema and ordered arguments.
Constants are unsigned32-bit bit values in schema parameter order. Each hit
record uses32 identifier bytes plus4 constant bytes, padded to stride64.
Raygen/miss remain identifier-only; all table starts are64-byte aligned.
`GPUPAD_DXR_SBT_INFO=1` enables the existing small host layout log.

This initial interface supports hit-group root constants only. Root descriptors,
descriptor tables and raygen/miss local arguments are outside this sample.
The sibling Typed Ray Records sample retains its identifier-only D1 contract.
