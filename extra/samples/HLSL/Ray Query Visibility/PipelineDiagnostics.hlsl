#ifndef PIPELINE_DIAGNOSTICS_HLSL
#define PIPELINE_DIAGNOSTICS_HLSL

struct PipelineState {
    float4 OriginHitT;
    float4 DirectionMin;
    float4 Sphere;
    // invoked, solver hit, pre-ReportHit TMax, ReportHit result (-1 = not returned)
    float4 State;
    // Miss callback, raw payload before/after inline, final pipeline Boolean.
    uint4 Outcome;
    // Before TraceRay, after TraceRay, original direction.z bits, reserved.
    uint4 Witness;
};

globallycoherent RWStructuredBuffer<PipelineState> PipelineDiagnostics : register(u2);

#endif
