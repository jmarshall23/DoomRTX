// d3d12_raytrace_cleanup.cpp
//

#include "precompiled.h"
#include "d3d12_local.h"
#include "nv_helpers_dx12/RaytracingPipelineGenerator.h"
#include "nv_helpers_dx12/RootSignatureGenerator.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"

extern ComPtr<ID3D12Resource> m_vertexBuffer;
extern bool raytracingDataInit;

/*
============================
RE_ShutdownRaytracingMap
============================
*/
void RE_ShutdownRaytracingMap(void) {
	//r_finishDXRInit = 1;
	GL_WaitForPreviousFrame();

	raytracingDataInit = false;

	// Remove all the models.
//	Mod_FreeAll();

	// Clear all the lights.
	GL_ClearLights();

	// Clear the scene vertexes.
	sceneVertexes.clear();

	// Let's delete all of our acceleration structures.
	for(int i = 0; i < dxrMeshList.size(); i++)
	{
		for (int d = 0; d < dxrMeshList[i]->meshSurfaces.size(); d++)
		{
			if (dxrMeshList[i]->meshSurfaces[d].buffers.pInstanceDesc.Get() != NULL) {
				dxrMeshList[i]->meshSurfaces[d].buffers.pInstanceDesc->Release();
			}

			if (dxrMeshList[i]->meshSurfaces[d].buffers.pResult.Get() != NULL) {
				dxrMeshList[i]->meshSurfaces[d].buffers.pResult->Release();
			}

			if (dxrMeshList[i]->meshSurfaces[d].buffers.pScratch.Get() != NULL) {
				dxrMeshList[i]->meshSurfaces[d].buffers.pScratch->Release();
			}
		}
	}

	dxrMeshList.clear();

	if (m_vertexBuffer.Get() != NULL) {
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}
}