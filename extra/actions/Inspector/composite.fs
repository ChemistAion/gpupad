#version 430

in vec2 vTexCoords;
out vec4 fragColor;

uniform sampler2D uInspectorSampler;

// Must match histogram.comp layout exactly
layout(std430) buffer HistogramBuffer {
    uint binsR[128];
    uint binsG[128];
    uint binsB[128];
    uint binsA[128];
    uint uDataMin;
    uint uDataMax;
    uint uAutoMin;
    uint uAutoMax;
    uint totalPixels;
};

uniform int   uMappingMode;     // 0=linear, 1=sigmoid, 2=log
uniform vec2  uMappingRange;    // (min, max)
uniform ivec4 uChannelMask;     // which channels to draw in histogram
uniform int   uHighlightOOR;   // out-of-range checkerboard enable
uniform float uHistogramHeight; // fraction of viewport for histogram strip
uniform vec2  uMouseFragCoord;  // mouse position for crosshair + printf

// ── Sortable-uint → float decode ──────────────────────────────────────────

float sortableUintToFloat(uint u) {
    uint bits = (u & 0x80000000u) != 0u
        ? u & ~0x80000000u
        : ~u;
    return uintBitsToFloat(bits);
}

// ── Mapping functions ─────────────────────────────────────────────────────

float mapLinear(float v, float lo, float hi) {
    return clamp((v - lo) / max(hi - lo, 1e-20), 0.0, 1.0);
}

float mapSigmoid(float v, float lo, float hi) {
    float t = (v - lo) / max(hi - lo, 1e-20);
    return 1.0 / (1.0 + exp(-8.0 * (2.0 * t - 1.0)));
}

float mapLog(float v, float lo, float hi) {
    float t = clamp((v - lo) / max(hi - lo, 1e-20), 0.0, 1.0);
    return log2(1.0 + t * 255.0) / log2(256.0);
}

float applyMapping(float v, float lo, float hi) {
    if (uMappingMode == 1) return mapSigmoid(v, lo, hi);
    if (uMappingMode == 2) return mapLog(v, lo, hi);
    return mapLinear(v, lo, hi);
}

// ── Out-of-range checkerboard ─────────────────────────────────────────────

vec3 checkerboard(vec2 fragCoord, vec3 color, float v, float lo, float hi) {
    if (uHighlightOOR == 0) return color;
    float checker = mod(floor(fragCoord.x / 8.0) + floor(fragCoord.y / 8.0), 2.0);
    if (v < lo) return mix(color, vec3(1.0, 0.0, 1.0), checker * 0.6); // magenta
    if (v > hi) return mix(color, vec3(0.0, 1.0, 1.0), checker * 0.6); // cyan
    return color;
}

// ── Histogram rendering ───────────────────────────────────────────────────

// Find the maximum bin count across all visible channels for normalization
uint histogramMaxBin() {
    uint mx = 1u;
    for (int i = 0; i < 128; i++) {
        if (uChannelMask.r != 0) mx = max(mx, binsR[i]);
        if (uChannelMask.g != 0) mx = max(mx, binsG[i]);
        if (uChannelMask.b != 0) mx = max(mx, binsB[i]);
        if (uChannelMask.a != 0) mx = max(mx, binsA[i]);
    }
    return mx;
}

vec4 renderHistogram(vec2 uv) {
    // uv is normalized within the histogram strip: x=[0,1], y=[0,1] bottom-to-top.
    int bin = clamp(int(uv.x * 128.0), 0, 127);
    uint mx = histogramMaxBin();
    float invMax = 1.0 / float(mx);

    // Bar heights (normalized 0..1)
    float hR = float(binsR[bin]) * invMax;
    float hG = float(binsG[bin]) * invMax;
    float hB = float(binsB[bin]) * invMax;
    float hA = float(binsA[bin]) * invMax;

    float y = uv.y;

    // Semi-transparent background
    vec4 col = vec4(0.0, 0.0, 0.0, 0.75);

    // Draw bars as additive colored layers
    if (uChannelMask.r != 0 && y < hR) col.r += 0.7;
    if (uChannelMask.g != 0 && y < hG) col.g += 0.7;
    if (uChannelMask.b != 0 && y < hB) col.b += 0.7;
    if (uChannelMask.a != 0 && y < hA) col.rgb += vec3(0.3);

    // Mapping range indicators: vertical lines at mapped min/max positions
    float dataMin = sortableUintToFloat(uDataMin);
    float dataMax = sortableUintToFloat(uDataMax);
    float dataRange = dataMax - dataMin;
    if (dataRange > 1e-20) {
        float minPos = (uMappingRange.x - dataMin) / dataRange;
        float maxPos = (uMappingRange.y - dataMin) / dataRange;
        if (abs(uv.x - minPos) < 0.004) col = vec4(1.0, 0.6, 0.0, 1.0); // orange
        if (abs(uv.x - maxPos) < 0.004) col = vec4(1.0, 0.6, 0.0, 1.0);
    }

    // Mapping curve overlay (white line)
    float curveV = dataMin + uv.x * dataRange;
    float curveY = applyMapping(curveV, uMappingRange.x, uMappingRange.y);
    if (abs(y - curveY) < 0.015) col = vec4(1.0, 1.0, 1.0, 1.0);

    return col;
}

// ── Main ──────────────────────────────────────────────────────────────────

void main() {
    vec2 texSize = vec2(textureSize(uInspectorSampler, 0));
    vec2 fragCoord = vTexCoords * texSize;

    // Determine if we're in the histogram strip (bottom portion)
    float histH = max(uHistogramHeight, 0.05);
    bool inHistogram = vTexCoords.y > (1.0 - histH);

    if (inHistogram) {
        // Map to histogram-local UV: x stays, y remapped to [0,1] within strip
        vec2 histUV = vec2(
            vTexCoords.x,
            (vTexCoords.y - (1.0 - histH)) / histH
        );
        vec4 histCol = renderHistogram(histUV);

        // Blend histogram over the mapped view beneath
        vec4 texel = texture(uInspectorSampler, vTexCoords);
        float lo = uMappingRange.x, hi = uMappingRange.y;
        vec3 mapped = vec3(
            applyMapping(texel.r, lo, hi),
            applyMapping(texel.g, lo, hi),
            applyMapping(texel.b, lo, hi)
        );
        fragColor = vec4(mix(mapped, histCol.rgb, histCol.a), 1.0);
    } else {
        vec4 texel = texture(uInspectorSampler, vTexCoords);
        float lo = uMappingRange.x, hi = uMappingRange.y;
        vec3 mapped = vec3(
            applyMapping(texel.r, lo, hi),
            applyMapping(texel.g, lo, hi),
            applyMapping(texel.b, lo, hi)
        );

        // Out-of-range checkerboard on the dominant channel
        float vmax = max(max(texel.r, texel.g), texel.b);
        float vmin = min(min(texel.r, texel.g), texel.b);
        mapped = checkerboard(fragCoord, mapped, vmax, lo, hi);
        mapped = checkerboard(fragCoord, mapped, vmin, lo, hi);

        fragColor = vec4(mapped, 1.0);
    }

    // ── Crosshair at mouse position ──
    vec2 mouseUV = uMouseFragCoord / texSize;
    float pxW = 1.0 / texSize.x;
    float pxH = 1.0 / texSize.y;
    if (abs(vTexCoords.x - mouseUV.x) < pxW ||
        abs(vTexCoords.y - mouseUV.y) < pxH) {
        fragColor.rgb = vec3(1.0) - fragColor.rgb;
    }
}
