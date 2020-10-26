/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"

/*
=====================
RB_BakeTextureMatrixIntoTexgen
=====================
*/
void RB_BakeTextureMatrixIntoTexgen( idPlane lightProject[3], const float *textureMatrix ) {
	float	genMatrix[16];
	float	final[16];

	genMatrix[0] = lightProject[0][0];
	genMatrix[4] = lightProject[0][1];
	genMatrix[8] = lightProject[0][2];
	genMatrix[12] = lightProject[0][3];

	genMatrix[1] = lightProject[1][0];
	genMatrix[5] = lightProject[1][1];
	genMatrix[9] = lightProject[1][2];
	genMatrix[13] = lightProject[1][3];

	genMatrix[2] = 0;
	genMatrix[6] = 0;
	genMatrix[10] = 0;
	genMatrix[14] = 0;

	genMatrix[3] = lightProject[2][0];
	genMatrix[7] = lightProject[2][1];
	genMatrix[11] = lightProject[2][2];
	genMatrix[15] = lightProject[2][3];

	myGlMultMatrix( genMatrix, backEnd.lightTextureMatrix, final );

	lightProject[0][0] = final[0];
	lightProject[0][1] = final[4];
	lightProject[0][2] = final[8];
	lightProject[0][3] = final[12];

	lightProject[1][0] = final[1];
	lightProject[1][1] = final[5];
	lightProject[1][2] = final[9];
	lightProject[1][3] = final[13];
}

/*
================
RB_PrepareStageTexturing
================
*/
void RB_PrepareStageTexturing( const shaderStage_t *pStage,  const drawSurf_t *surf, idDrawVert *ac ) {
	
}

/*
================
RB_FinishStageTexturing
================
*/
void RB_FinishStageTexturing( const shaderStage_t *pStage, const drawSurf_t *surf, idDrawVert *ac ) {
	
}

/*
=============================================================================================

FILL DEPTH BUFFER

=============================================================================================
*/


/*
==================
RB_T_FillDepthBuffer
==================
*/
void RB_T_FillDepthBuffer( const drawSurf_t *surf ) {


}

/*
=====================
RB_STD_FillDepthBuffer

If we are rendering a subview with a near clip plane, use a second texture
to force the alpha test to fail when behind that clip plane
=====================
*/
void RB_STD_FillDepthBuffer( drawSurf_t **drawSurfs, int numDrawSurfs ) {

}

/*
=============================================================================================

SHADER PASSES

=============================================================================================
*/

/*
==================
RB_SetProgramEnvironment

Sets variables that can be used by all vertex programs
==================
*/
void RB_SetProgramEnvironment( void ) {

}

/*
==================
RB_SetProgramEnvironmentSpace

Sets variables related to the current space that can be used by all vertex programs
==================
*/
void RB_SetProgramEnvironmentSpace( void ) {

}

/*
==================
RB_STD_T_RenderShaderPasses

This is also called for the generated 2D rendering
==================
*/
void RB_STD_T_RenderShaderPasses( const drawSurf_t *surf ) {

}

/*
=====================
RB_STD_DrawShaderPasses

Draw non-light dependent passes
=====================
*/
int RB_STD_DrawShaderPasses( drawSurf_t **drawSurfs, int numDrawSurfs ) {


	return 0;
}



/*
==============================================================================

BACK END RENDERING OF STENCIL SHADOWS

==============================================================================
*/

/*
=====================
RB_T_Shadow

the shadow volumes face INSIDE
=====================
*/
static void RB_T_Shadow( const drawSurf_t *surf ) {
	
}

/*
=====================
RB_StencilShadowPass

Stencil test should already be enabled, and the stencil buffer should have
been set to 128 on any surfaces that might receive shadows
=====================
*/
void RB_StencilShadowPass( const drawSurf_t *drawSurfs ) {
	
}



/*
=============================================================================================

BLEND LIGHT PROJECTION

=============================================================================================
*/

/*
=====================
RB_T_BlendLight

=====================
*/
static void RB_T_BlendLight( const drawSurf_t *surf ) {

}


/*
=====================
RB_BlendLight

Dual texture together the falloff and projection texture with a blend
mode to the framebuffer, instead of interacting with the surface texture
=====================
*/
static void RB_BlendLight( const drawSurf_t *drawSurfs,  const drawSurf_t *drawSurfs2 ) {
	
}


//========================================================================

static idPlane	fogPlanes[4];

/*
=====================
RB_T_BasicFog

=====================
*/
static void RB_T_BasicFog( const drawSurf_t *surf ) {

}



/*
==================
RB_FogPass
==================
*/
static void RB_FogPass( const drawSurf_t *drawSurfs,  const drawSurf_t *drawSurfs2 ) {
	
}


/*
==================
RB_STD_FogAllLights
==================
*/
void RB_STD_FogAllLights( void ) {

}

//=========================================================================================

/*
==================
RB_STD_LightScale

Perform extra blending passes to multiply the entire buffer by
a floating point value
==================
*/
void RB_STD_LightScale( void ) {
	
}

//=========================================================================================

/*
=============
RB_STD_DrawView

=============
*/
void	RB_STD_DrawView( void ) {
	viewLight_t* vLight;

	if (tr.viewDef == NULL)
		return;

	for (vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next) {
		backEnd.vLight = vLight;

		//idVec3 dist = vLight->lightDef->parms.end - vLight->lightDef->parms.start;
		float lightColor[4];

		// backEnd.lightScale is calculated so that lightColor[] will never exceed
		// tr.backEndRendererMaxLight
		//lightColor[0] = backEnd.lightScale * lightRegs[lightStage->color.registers[0]];
		//lightColor[1] = backEnd.lightScale * lightRegs[lightStage->color.registers[1]];
		//lightColor[2] = backEnd.lightScale * lightRegs[lightStage->color.registers[2]];
		//lightColor[3] = lightRegs[lightStage->color.registers[3]];

		GL_RegisterWorldLight(vLight->lightDef, vLight->lightDef->parms.origin.x, vLight->lightDef->parms.origin.y, vLight->lightDef->parms.origin.z, vLight->lightDef->parms.lightRadius.y * 0.65, 0, vLight->lightDef->parms.shaderParms[SHADERPARM_RED], vLight->lightDef->parms.shaderParms[SHADERPARM_GREEN], vLight->lightDef->parms.shaderParms[SHADERPARM_BLUE]);
	}

	//viewEntity_t* vEntity;
	//int index = 0;
	//for (vEntity = tr.viewDef->viewEntitys; vEntity; vEntity = vEntity->next) {
	//	const renderEntity_t* currententity = &vEntity->entityDef->parms;
	//	if (currententity == NULL) {
	//		continue;
	//	}
	//
	//	idRenderModel* qmodel = currententity->hModel;
	//
	//	if (qmodel->GetNumDXRFrames() <= 0)
	//		continue;
	//
	//	for (int i = 0; i < qmodel->NumSurfaces(); i++)
	//	{
	//		const modelSurface_t* surface = qmodel->Surface(i);
	//		const icdEmissiveStage& emissive = surface->shader->GetEmissiveStage();
	//
	//		idBounds bounds = qmodel->Bounds() + currententity->origin;
	//
	//		if (emissive.isEnabled) {
	//			idAngles angle = currententity->axis.ToAngles();
	//			idVec3 normal;
	//			float yaw = angle.yaw;
	//			float pitch = angle.pitch;
	//			float roll = angle.roll;
	//			normal.x = -cos(yaw)*sin(pitch)*sin(roll) - sin(yaw)*cos(roll);
	//			normal.y = -sin(yaw)*sin(pitch)*sin(roll) + cos(yaw)*cos(roll);
	//			normal.z = cos(pitch) * sin(roll);
	//
	//			GL_RegisterWorldAreaLight(normal, bounds[0], bounds[1], 0, emissive.radius, emissive.color[0], emissive.color[1], emissive.color[2]);
	//		}
	//	}
	//}
}
