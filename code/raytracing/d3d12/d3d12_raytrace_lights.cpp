// d3d12_raytrace_lights.cpp
//

#include "precompiled.h"
#include "d3d12_local.h"

#define MAX_DRAW_LIGHTS 64
#define MAX_WORLD_LIGHTS 512

struct glLight_t {
	idVec4 origin_radius;
	idVec4 light_color;
	idVec4 light_color2;
	idVec3 absmin;
	idVec3 absmax;
	idVec4 light_clamp;

	//entity_t* ent;
	int leafnums[16];
	int lightStyle;

	int num_leafs;
	int distance;

	bool isAreaLight;
};

struct sceneLightInfo_t {
	idVec4 origin_radius;
	idVec4 light_color;
	idVec4 light_clamp;
	idVec4 light_color2;
};

glLight_t worldLights[MAX_WORLD_LIGHTS];
glLight_t worldLightsSorted[MAX_WORLD_LIGHTS];
int numWorldLights = 0;

sceneLightInfo_t *sceneLights = NULL;
tr_buffer* sceneLightInfoBuffer;

int numStaticWorldLights = 0;

/*
===============
GL_ClearLights
===============
*/
void GL_SetNumMapLights() {
	numStaticWorldLights = numWorldLights;
}

/*
===============
GL_ClearLights
===============
*/
void GL_ClearLights(void) {
	memset(&worldLights[0], 0, sizeof(worldLights));
	numWorldLights = 0;
	numStaticWorldLights = 0;

	if (sceneLightInfoBuffer != NULL)
	{
		tr_destroy_buffer(renderer, sceneLightInfoBuffer);
		sceneLightInfoBuffer = NULL;
	}
}

void GL_RegisterWorldLight(idRenderLight* ent, float x, float y, float z, idVec3 radius, int lightStyle, float r, float g, float b) {
	glLight_t light = { };
	light.origin_radius[0] = x;
	light.origin_radius[1] = y;
	light.origin_radius[2] = z;
	light.origin_radius[3] = 100;

	light.light_color[0] = r;
	light.light_color[1] = g;
	light.light_color[2] = b;

	light.light_color2[0] = radius.x;
	light.light_color2[1] = radius.y;
	light.light_color2[2] = radius.z;

	light.absmin[0] = x;
	light.absmin[1] = y;
	light.absmin[2] = z;

	light.absmax[0] = x;
	light.absmax[1] = y;
	light.absmax[2] = z;

	light.lightStyle = lightStyle;

	//light.ent = ent;
	light.num_leafs = 0;
	//GL_FindTouchedLeafs(&light, loadmodel->nodes);

	worldLights[numWorldLights++] = light;
}

void GL_RegisterWorldAreaLight(idVec3 normal, idVec3 mins, idVec3 maxs, int lightStyle, float radius, float r, float g, float b) {
	glLight_t light = { };
	idVec3 origin;
	idVec3 light_clamp;
	
	origin = maxs + mins;
	light_clamp = maxs - mins;

	//VectorAdd(maxs, mins, origin);
	//VectorSubtract(maxs, mins, light_clamp);

	light.light_clamp[0] = light_clamp[0];
	light.light_clamp[1] = light_clamp[1];
	light.light_clamp[2] = light_clamp[2];

	light.origin_radius[0] = origin[0] * 0.5f;
	light.origin_radius[1] = origin[1] * 0.5f;
	light.origin_radius[2] = origin[2] * 0.5f;
	light.origin_radius[3] = radius; // area light

	light.light_color[0] = normal[0];
	light.light_color[1] = normal[1];
	light.light_color[2] = normal[2];

	light.light_color2[0] = r;
	light.light_color2[1] = g;
	light.light_color2[2] = b;

	light.absmin[0] = mins[0];
	light.absmin[1] = mins[1];
	light.absmin[2] = mins[2];

	light.absmax[0] = maxs[0];
	light.absmax[1] = maxs[1];
	light.absmax[2] = maxs[2];

	light.lightStyle = lightStyle;

	light.isAreaLight = true;

	// Quake sub divides geometry(lovely) so to hack around that don't add any area lights that are near already registered area lights!
	for (int i = 0; i < numWorldLights; i++) {
		float dist = idMath::Distance(light.origin_radius.ToVec3(), worldLights[i].origin_radius.ToVec3());
		if (dist < 50 && worldLights[i].isAreaLight)
			return;
	}

	light.num_leafs = -1; // arealight
	worldLights[numWorldLights++] = light;
}

void GL_InitLightInfoBuffer(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr) {
	tr_create_uniform_buffer(renderer, sizeof(sceneLightInfo_t) * MAX_DRAW_LIGHTS, true, &sceneLightInfoBuffer);
	sceneLights = (sceneLightInfo_t *)sceneLightInfoBuffer->cpu_mapped_address;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	
	uint32_t bufferSize = ROUND_UP(sizeof(sceneLightInfo_t), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = MAX_DRAW_LIGHTS;
	srvDesc.Buffer.StructureByteStride = sizeof(sceneLightInfo_t);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	// Write the per-instance properties buffer view in the heap
	m_device->CreateShaderResourceView(sceneLightInfoBuffer->dx_resource, &srvDesc, srvPtr);
}

int lightSort(const void* a, const void* b) {
	glLight_t light1 = *((glLight_t*)a);
	glLight_t light2 = *((glLight_t*)b);

	return light1.distance - light2.distance;
}

void GL_BuildLightList(float x, float y, float z) {
	int numVisLights = 0;

	if (sceneLights == NULL)
		return;

	memset(sceneLights, 0, sizeof(sceneLightInfo_t) * MAX_DRAW_LIGHTS);

	memcpy(worldLightsSorted, worldLights, sizeof(worldLights));
	
	for (int i = 0; i < numWorldLights; i++) {
		glLight_t* ent = &worldLightsSorted[i];
		idVec3 viewpos = idVec3( x, y, z );
	
		ent->distance = idMath::Distance(ent->origin_radius.ToVec3(), viewpos);
	}
	
	qsort(worldLightsSorted, numWorldLights, sizeof(glLight_t), lightSort);

	for(int i = 0; i < numWorldLights; i++) {
		if(numVisLights >= MAX_DRAW_LIGHTS) {
			//common->Printf("MAX_DRAW_LIGHTS!\n");
			break;
		}

		glLight_t * ent = &worldLightsSorted[i];

		//if (!ent->isAreaLight)
		//{
		//	if (!ri.PF_inPVS(r_newrefdef.vieworg, ent->origin_radius))
		//		continue;			
		//}

		sceneLights[numVisLights].origin_radius[0] = ent->origin_radius[0];
		sceneLights[numVisLights].origin_radius[1] = ent->origin_radius[1];
		sceneLights[numVisLights].origin_radius[2] = ent->origin_radius[2];

		if (!ent->isAreaLight)
		{
			sceneLights[numVisLights].origin_radius[3] = ent->origin_radius[3];			
		}
		else
		{			
			sceneLights[numVisLights].origin_radius[3] = -ent->origin_radius[3];
		}

		//if (ent->lightStyle) {
		//	sceneLights[numVisLights].light_color[0] = ent->light_color[0] * r_newrefdef.lightstyles[ent->lightStyle].rgb[0];
		//	sceneLights[numVisLights].light_color[1] = ent->light_color[1] * r_newrefdef.lightstyles[ent->lightStyle].rgb[1];
		//	sceneLights[numVisLights].light_color[2] = ent->light_color[2] * r_newrefdef.lightstyles[ent->lightStyle].rgb[2];
		//}
		//else {			
			sceneLights[numVisLights].light_color[0] = ent->light_color[0];
			sceneLights[numVisLights].light_color[1] = ent->light_color[1];
			sceneLights[numVisLights].light_color[2] = ent->light_color[2];
		//}

		sceneLights[numVisLights].light_clamp[0] = ent->light_clamp[0];
		sceneLights[numVisLights].light_clamp[1] = ent->light_clamp[1];
		sceneLights[numVisLights].light_clamp[2] = ent->light_clamp[2];

		sceneLights[numVisLights].light_color2[0] = ent->light_color2[0];
		sceneLights[numVisLights].light_color2[1] = ent->light_color2[1];
		sceneLights[numVisLights].light_color2[2] = ent->light_color2[2];

		numVisLights++;
	}

	numWorldLights = numStaticWorldLights;
}