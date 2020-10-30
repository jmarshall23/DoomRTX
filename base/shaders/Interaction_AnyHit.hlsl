#include "Common.hlsl"

StructuredBuffer<STriVertex> BTriVertex : register(t0);
Texture2D<float4> MegaTexture : register(t1);
StructuredBuffer<SInstanceProperties> BInstanceProperties : register(t2);

[shader("anyhit")] void InteractionAnyHit(inout HitInfo payload, Attributes attrib)
{
    float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
	uint vertId = BInstanceProperties[InstanceID()].startVertex + (3 * PrimitiveIndex());
	
	float u = 0, v = 0;
	float nu, nv;
	for(int i = 0; i < 3; i++)
	{
	u += BTriVertex[vertId + i].st.x * barycentrics[i];
	v += BTriVertex[vertId + i].st.y * barycentrics[i];
	}
	
	u = frac(u) / 16384;
	v = frac(v) / 16384;
	
	nu = u;
	nv = v;
	
	u = u * BTriVertex[vertId + 0].vtinfo.z;
	v = v * BTriVertex[vertId + 0].vtinfo.w;
	
	u = u + (BTriVertex[vertId + 0].vtinfo.x / 16384);
	v = v + (BTriVertex[vertId + 0].vtinfo.y / 16384);
	
	nu = nu * BTriVertex[vertId + 0].normalVtInfo.z;
	nv = nv * BTriVertex[vertId + 0].normalVtInfo.w;
	
	nu = nu + (BTriVertex[vertId + 0].normalVtInfo.x / 16384);
	nv = nv + (BTriVertex[vertId + 0].normalVtInfo.y / 16384);
	
	float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

	float4 megaColor = MegaTexture.Load(int3(u * 16384, v * 16384, 0)); //normalize(BTriVertex[vertId + 0].vertex) * 4;
	if(BTriVertex[vertId + 0].st.z == STAT_FORCE_TRANSPARENT) {
		payload.decalColor.xyz += (megaColor.rgb * float3(1, 0 , 0 ));
		IgnoreHit();
		return;
	}
	else if(BTriVertex[vertId + 0].st.z == STAT_FORCE_BLEND_TEST) {
		payload.decalColor.xyz += megaColor.rgb * (1.0 - megaColor.a);
		IgnoreHit();
		return;
	}
		
	AcceptHitAndEndSearch();
}