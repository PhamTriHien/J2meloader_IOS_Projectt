#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

vertex VertexOut vertexShader(uint vertexID [[vertex_id]]) {
    // Generates a full screen quad from a triangle strip of 4 vertices
    const float2 positions[4] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2(-1.0,  1.0),
        float2( 1.0,  1.0)
    };
    
    const float2 texCoords[4] = {
        float2(0.0, 1.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 0.0)
    };
    
    VertexOut out;
    out.position = float4(positions[vertexID], 0.0, 1.0);
    out.texCoord = texCoords[vertexID];
    return out;
}

struct ShaderUniforms {
    float brightness;
    float contrast;
    float scanlineIntensity;
    float lcdGridStrength;
};

fragment float4 fragmentShader(VertexOut in [[stage_in]],
                               texture2d<float> colorTexture [[texture(0)]],
                               constant ShaderUniforms &uniforms [[buffer(0)]]) {
    constexpr sampler textureSampler(mag_filter::nearest, min_filter::nearest);
    float4 color = colorTexture.sample(textureSampler, in.texCoord);
    color.rgb = (color.rgb - 0.5f) * uniforms.contrast + 0.5f + uniforms.brightness;
    return color;
}

// CRT Scanline Shader
fragment float4 crtFragmentShader(VertexOut in [[stage_in]],
                                  texture2d<float> colorTexture [[texture(0)]],
                                  constant ShaderUniforms &uniforms [[buffer(0)]]) {
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);
    float4 color = colorTexture.sample(textureSampler, in.texCoord);
    
    // Scanline calculation
    float scanline = sin(float(in.position.y) * 3.14159265f) * uniforms.scanlineIntensity;
    color.rgb -= scanline;
    color.rgb = (color.rgb - 0.5f) * uniforms.contrast + 0.5f + uniforms.brightness;
    return color;
}

// LCD Grid Shader (Simulates Nokia LCD subpixel matrix)
fragment float4 lcdGridFragmentShader(VertexOut in [[stage_in]],
                                      texture2d<float> colorTexture [[texture(0)]],
                                      constant ShaderUniforms &uniforms [[buffer(0)]]) {
    constexpr sampler textureSampler(mag_filter::nearest, min_filter::nearest);
    float4 color = colorTexture.sample(textureSampler, in.texCoord);
    
    int px = int(in.position.x);
    int py = int(in.position.y);
    if ((px % 3 == 0) || (py % 3 == 0)) {
        color.rgb *= (1.0f - uniforms.lcdGridStrength);
    }
    color.rgb = (color.rgb - 0.5f) * uniforms.contrast + 0.5f + uniforms.brightness;
    return color;
}