// #DXR Extra - Another ray type

// Ray payload for the shadow rays
struct ShadowHitInfo {
  bool isHit;
  float4 vertinfo;
};

struct Attributes {
  float2 uv;
};

[shader("closesthit")] void ShadowClosestHit(inout ShadowHitInfo hit,
                                             Attributes bary) {
  hit.isHit = true;
  hit.vertinfo.xyz = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

[shader("miss")] void ShadowMiss(inout ShadowHitInfo hit
                                  : SV_RayPayload) {
  hit.isHit = false;
}