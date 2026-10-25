#ifndef PIPELINE_DIAGNOSTICS_HLSL
#define PIPELINE_DIAGNOSTICS_HLSL

struct PipelineState {
    float4 OriginHitT;
    float4 DirectionMin;
    float4 Sphere;
    // invoked, solver hit, pre-ReportHit TMax, ReportHit result (-1 = not returned)
    float4 State;
};

globallycoherent RWStructuredBuffer<PipelineState> PipelineDiagnostics : register(u2);

#endif
