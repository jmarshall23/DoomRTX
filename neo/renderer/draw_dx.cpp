#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include <math.h>

static idVec3 RB_DXRMakeVec3(float x, float y, float z)
{
	idVec3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

static idVec3 RB_DXRSubVec3(const idVec3& a, const idVec3& b)
{
	return RB_DXRMakeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static idVec3 RB_DXRScaleVec3(const idVec3& v, float s)
{
	return RB_DXRMakeVec3(v.x * s, v.y * s, v.z * s);
}

static float RB_DXRDotVec3(const idVec3& a, const idVec3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static idVec3 RB_DXRCrossVec3(const idVec3& a, const idVec3& b)
{
	return RB_DXRMakeVec3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

static float RB_DXRLengthVec3(const idVec3& v)
{
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static idVec3 RB_DXRNormalizeVec3Safe(const idVec3& v, const idVec3& fallback)
{
	const float len = RB_DXRLengthVec3(v);
	if (len > 1e-20f)
	{
		const float invLen = 1.0f / len;
		return RB_DXRMakeVec3(v.x * invLen, v.y * invLen, v.z * invLen);
	}
	return fallback;
}

static idVec3 RB_DXRTransformLightVector(const idMat3& axis, const idVec3& v)
{
	return RB_DXRMakeVec3(
		axis[0].x * v.x + axis[1].x * v.y + axis[2].x * v.z,
		axis[0].y * v.x + axis[1].y * v.y + axis[2].y * v.z,
		axis[0].z * v.x + axis[1].z * v.y + axis[2].z * v.z);
}

static glRaytracingLight_t RB_DXRMakeSpotLightFromRenderLight(
	const renderLight_t& srcLight,
	const idVec3& worldOrigin,
	float r, float g, float b,
	float intensity)
{
	const idVec3 targetW = RB_DXRTransformLightVector(srcLight.axis, srcLight.target);
	const idVec3 rightW = RB_DXRTransformLightVector(srcLight.axis, srcLight.right);
	const idVec3 upW = RB_DXRTransformLightVector(srcLight.axis, srcLight.up);
	const idVec3 startW = RB_DXRTransformLightVector(srcLight.axis, srcLight.start);
	const idVec3 endW = RB_DXRTransformLightVector(srcLight.axis, srcLight.end);

	idVec3 fallbackDir = RB_DXRCrossVec3(rightW, upW);
	fallbackDir = RB_DXRNormalizeVec3Safe(fallbackDir, RB_DXRMakeVec3(0.0f, 0.0f, -1.0f));

	const idVec3 dir = RB_DXRNormalizeVec3Safe(targetW, fallbackDir);

	float centerDepth = RB_DXRDotVec3(targetW, dir);
	if (centerDepth <= 1e-4f)
	{
		centerDepth = RB_DXRLengthVec3(targetW);
	}
	if (centerDepth <= 1e-4f)
	{
		centerDepth = RB_DXRDotVec3(endW, dir);
	}
	if (centerDepth <= 1e-4f)
	{
		centerDepth = 64.0f;
	}

	const idVec3 rightPerp = RB_DXRSubVec3(
		rightW,
		RB_DXRScaleVec3(dir, RB_DXRDotVec3(rightW, dir)));

	const idVec3 upPerp = RB_DXRSubVec3(
		upW,
		RB_DXRScaleVec3(dir, RB_DXRDotVec3(upW, dir)));

	idVec3 axisU = RB_DXRCrossVec3(RB_DXRMakeVec3(0.0f, 0.0f, 1.0f), dir);
	axisU = RB_DXRNormalizeVec3Safe(axisU, RB_DXRMakeVec3(1.0f, 0.0f, 0.0f));
	axisU = RB_DXRNormalizeVec3Safe(rightPerp, axisU);

	idVec3 axisV = RB_DXRCrossVec3(dir, axisU);
	axisV = RB_DXRNormalizeVec3Safe(axisV, RB_DXRMakeVec3(0.0f, 1.0f, 0.0f));
	if (RB_DXRDotVec3(axisV, upPerp) < 0.0f)
	{
		axisV = RB_DXRScaleVec3(axisV, -1.0f);
	}

	float tanHalfWidth = RB_DXRLengthVec3(rightPerp) / centerDepth;
	float tanHalfHeight = RB_DXRLengthVec3(upPerp) / centerDepth;

	float nearPlane = RB_DXRDotVec3(startW, dir);
	if (nearPlane < 0.0f)
	{
		nearPlane = 0.0f;
	}

	float farPlane = RB_DXRDotVec3(endW, dir);
	if (farPlane < centerDepth)
	{
		farPlane = centerDepth;
	}
	if (farPlane <= nearPlane)
	{
		farPlane = nearPlane + 64.0f;
	}

	glRaytracingLight_t light = glRaytracingLightingMakeSpotLight(
		worldOrigin.x, worldOrigin.y, worldOrigin.z,
		dir.x, dir.y, dir.z,
		axisU.x, axisU.y, axisU.z,
		axisV.x, axisV.y, axisV.z,
		nearPlane,
		farPlane,
		tanHalfWidth,
		tanHalfHeight,
		r, g, b,
		intensity,
		1u);

	light.samples = srcLight.noShadows ? 0u : 1u;
	light.pointRadiusPad = srcLight.noSpecular ? 1.0f : 0.0f;

	return light;
}

/*
====================
RB_DXDrawInteractions
====================
*/
void RB_DXDrawInteractions(void)
{
	viewLight_t* vLight;
	bool hasLight = false;

	for (vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next)
	{
		backEnd.vLight = vLight;

		// do fogging later
		if (vLight->lightShader->IsFogLight())
		{
			continue;
		}
		if (vLight->lightShader->IsBlendLight())
		{
			continue;
		}

		if (!vLight->localInteractions && !vLight->globalInteractions
			&& !vLight->translucentInteractions)
		{
			continue;
		}

		const renderLight_t& srcLight = vLight->lightDef->parms;
		const float r = srcLight.shaderParms[SHADERPARM_RED];
		const float g = srcLight.shaderParms[SHADERPARM_GREEN];
		const float b = srcLight.shaderParms[SHADERPARM_BLUE];
		const float intensity = 1.0f;

		glRaytracingLight_t light = {};
		bool supported = true;

		if (srcLight.pointLight)
		{
			light = glRaytracingLightingMakePointLight(
				vLight->globalLightOrigin.x,
				vLight->globalLightOrigin.y,
				vLight->globalLightOrigin.z,
				srcLight.lightRadius[0] * 1.4f,
				srcLight.lightRadius[1] * 1.4f,
				srcLight.lightRadius[2] * 1.4f,
				r, g, b,
				intensity);

			light.samples = srcLight.noShadows ? 0u : 1u;
			light.pointRadiusPad = srcLight.noSpecular ? 1.0f : 0.0f;
		}
		else if (!srcLight.parallel)
		{
			light = RB_DXRMakeSpotLightFromRenderLight(
				srcLight,
				vLight->globalLightOrigin,
				r, g, b,
				intensity);
		}
		else
		{
			// Parallel projected lights are not spot lights.
			// Handle them in a separate directional/orthographic path.
			supported = false;
			common->Warning("Parallel lights are not needed with raytracing! Just place a skybox.");
		}

		if (supported && glRaytracingLightingAddLight(&light))
		{
			hasLight = true;
		}
	}

	if (!hasLight)
	{
		return;
	}

	glFinish();
	glLightScene(backEnd.viewDef->renderWorld->dxrWorldId);
	glRaytracingLightingClearLights(false);
}