// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct HitInfo {
  float4 colorAndDistance;
  float4 lightColor;
  float4 worldOrigin;
  float4 worldNormal;
  float4 decalColor;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes {
  float2 bary;
};


struct SecondHitInfo {
   float4 payload_color;
   float4 payload_vert_info; 
};

struct STriVertex {
  float3 vertex;
  float3 st;
  float3 normal;
  float4 vtinfo;
  float4 normalVtInfo;
  float4 tangent;
  float3 imageAveragePixel;
};


struct SInstanceProperties
{
	int startVertex;
	float3 matX;
	float3 matY;
	float3 matZ;
};


struct sceneLightInfo_t {
	float4 origin_radius;
	float4 light_color;
	float4 light_clamp;
	float4 light_color2;
};

#define STAT_FORCE_TRANSPARENT 2
#define STAT_FORCE_BLEND_TEST 3

