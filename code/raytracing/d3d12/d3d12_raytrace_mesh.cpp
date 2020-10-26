// d3d12_raytrace_mesh.cpp
//

#include "precompiled.h"
#include "d3d12_local.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include <vector>

UINT8* pVertexDataBegin;

#define Vector2Subtract(a,b,c)  ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1])
static ID_INLINE void CrossProduct(const idVec3 v1, const idVec3 v2, idVec3 &cross) {
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

float VectorNormalize2(const idVec3 v, idVec3 &out) {
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt(length);

	if (length)
	{
#ifndef Q3_VM // bk0101022 - FPE related
		//	  assert( ((Q_fabs(v[0])!=0.0f) || (Q_fabs(v[1])!=0.0f) || (Q_fabs(v[2])!=0.0f)) );
#endif
		ilength = 1 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}
	else {
		out.Zero();
	}

	return length;
}

float VectorNormalize(idVec3 &v) {
	// NOTE: TTimo - Apple G4 altivec source uses double?
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt(length);

	if (length) {
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

std::vector<dxrMesh_t *> dxrMeshList;


ComPtr<ID3D12Resource> m_vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
std::vector<dxrVertex_t> sceneVertexes;

void GL_UpdateBottomLevelAccelStruct(idRenderModel* model) {	
	dxrMesh_t* mesh = (dxrMesh_t * )model->GetDXRFrame(0);
	if (mesh == NULL)
		return;

	if (mesh->buffers.pScratch.Get() == NULL)
		return;

	dxrVertex_t* sceneVertexes = (dxrVertex_t*)pVertexDataBegin;
	dxrVertex_t* modelVertexes = &sceneVertexes[mesh->startSceneVertex];

	// TODO: Use a index buffer here : )
	{
		for (int i = 0; i < mesh->meshSurfaces.size(); i++)
		{
			const modelSurface_t* surface = model->Surface(i);
			for (int d = 0; d < mesh->meshSurfaces[i].numIndexes; d++)
			{
				int indexId = mesh->meshSurfaces[i].startIndex + d;
				int meshIndexId = surface->geometry->indexes[d];

				modelVertexes[indexId].xyz = surface->geometry->verts[meshIndexId].xyz;
			}
		}
	}

	// !!Performance!! This should be a update rather then a full-regen, I can't do that though without causing a D3D12 Device Failure :(
	// I had to re-write these changes because the device changes were so bad it nuked this file.
	mesh->bottomLevelAS.Generate(m_commandList.Get(), mesh->buffers.pScratch.Get(), mesh->buffers.pResult.Get(), false, nullptr);
}

void GL_LoadBottomLevelAccelStruct(dxrMesh_t* mesh, idRenderModel* model) {
	mesh->startSceneVertex = sceneVertexes.size();

	for (int i = 0; i < model->NumSurfaces(); i++)
	{
		const modelSurface_t* fa = model->Surface(i);
		srfTriangles_t* tri = fa->geometry;

		if (tri == NULL) {
			continue;
		}

		dxrSurface_t surf;

		int materialInfo = 1;

		float x, y, w, h;

		if (fa->shader == NULL)
			continue;

		const shaderStage_t* stage = fa->shader->GetAlbedoStage();		

		if (fa->shader->GetEmissiveStage().isEnabled) {
			materialInfo = 2;
		}

		if (fa->shader->GetSort() == SS_DECAL)
			continue;

		if (stage != NULL) {
			idStr fileName = stage->texture.image->imgName.c_str();
			fileName = fileName.StripPath().StripFileExtension();
			GL_FindMegaTile(fileName.c_str(), x, y, w, h);
		}
		else {
			//x = -1;
			//y = -1;
			//w = -1;
			//h = -1;
			continue;
		}
		
		surf.startVertex = mesh->meshVertexes.size();
		surf.numVertexes = 0;
		for (int d = 0; d < tri->numVerts; d++) {
			dxrVertex_t v;

			v.xyz[0] = tri->verts[d].xyz[0];
			v.xyz[1] = tri->verts[d].xyz[1];
			v.xyz[2] = tri->verts[d].xyz[2];
			v.st[0] = tri->verts[d].st[0];
			v.st[1] = tri->verts[d].st[1];
			v.normal[0] = tri->verts[d].normal[0];
			v.normal[1] = tri->verts[d].normal[1];
			v.normal[2] = tri->verts[d].normal[2];
			v.tangent[0] = tri->verts[d].tangents[0][0];
			v.tangent[1] = tri->verts[d].tangents[0][1];
			v.tangent[2] = tri->verts[d].tangents[0][2];
			v.st[2] = materialInfo;
			v.vtinfo[0] = x;
			v.vtinfo[1] = y;
			v.vtinfo[2] = w;
			v.vtinfo[3] = h;

			mesh->meshVertexes.push_back(v);
			surf.numVertexes++;
		}

		surf.numIndexes = 0;
		surf.startIndex = mesh->meshIndexes.size();
		for (int d = 0; d < tri->numIndexes; d++)
		{
			mesh->meshIndexes.push_back(surf.startVertex + tri->indexes[d]);
			surf.numIndexes++;
		}

		mesh->meshSurfaces.push_back(surf);
	}

	// TODO: Use a index buffer here : )
	{
		for (int i = 0; i < mesh->meshSurfaces.size(); i++)
		{
			mesh->meshSurfaces[i].startMegaVertex = mesh->meshTriVertexes.size();

			for (int d = 0; d < mesh->meshSurfaces[i].numIndexes; d++)
			{
				int indexId = mesh->meshSurfaces[i].startIndex + d;
				int idx = mesh->meshIndexes[indexId];

				mesh->meshTriVertexes.push_back(mesh->meshVertexes[idx]);
				sceneVertexes.push_back(mesh->meshVertexes[idx]);
				mesh->numSceneVertexes++;
			}
		}
	}

	// Calculate the normals
	//{
	//	for (int i = 0; i < mesh->numSceneVertexes; i += 3)
	//	{
	//		float* pA = &sceneVertexes[mesh->startSceneVertex + i + 0].xyz[0];
	//		float* pC = &sceneVertexes[mesh->startSceneVertex + i + 1].xyz[0];
	//		float* pB = &sceneVertexes[mesh->startSceneVertex + i + 2].xyz[0];
	//
	//		float* tA = &sceneVertexes[mesh->startSceneVertex + i + 0].st[0];
	//		float* tC = &sceneVertexes[mesh->startSceneVertex + i + 1].st[0];
	//		float* tB = &sceneVertexes[mesh->startSceneVertex + i + 2].st[0];
	//
	//		idVec3 normal;
	//		idVec3 tangent;
	//
	//		{
	//			idVec3 dP0, dP1;
	//			VectorSubtract(pB, pA, dP0);
	//			VectorSubtract(pC, pA, dP1);
	//
	//			idVec3 dt0, dt1;
	//			Vector2Subtract(tB, tA, dt0);
	//			Vector2Subtract(tC, tA, dt1);
	//
	//			float r = 1.f / (dt0[0] * dt1[1] - dt1[0] * dt0[1]);
	//
	//			idVec3 sdir = idVec3(
	//				(dt1[1] * dP0[0] - dt0[1] * dP1[0]) * r,
	//				(dt1[1] * dP0[1] - dt0[1] * dP1[1]) * r,
	//				(dt1[1] * dP0[2] - dt0[1] * dP1[2]) * r );
	//
	//			idVec3 tdir = idVec3(
	//				(dt0[0] * dP1[0] - dt1[0] * dP0[0]) * r,
	//				(dt0[0] * dP1[1] - dt1[0] * dP0[1]) * r,
	//				(dt0[0] * dP1[2] - dt1[0] * dP0[2]) * r );
	//
	//			CrossProduct(dP0, dP1, normal);
	//			VectorNormalize(normal);
	//
	//			idVec3 t;
	//			VectorScale(normal, DotProduct(normal, sdir), t);
	//			VectorSubtract(sdir, t, t);
	//			VectorNormalize2(t, tangent); // Graham-Schmidt : t = normalize(t - n * (n.t))
	//
	//		}
	//
	//		sceneVertexes[mesh->startSceneVertex + i + 0].normal = normal;
	//		sceneVertexes[mesh->startSceneVertex + i + 1].normal = normal;
	//		sceneVertexes[mesh->startSceneVertex + i + 2].normal = normal;
	//
	//		memcpy(sceneVertexes[mesh->startSceneVertex + i + 0].tangent.ToFloatPtr(), tangent.ToFloatPtr(), sizeof(float) * 3);
	//		memcpy(sceneVertexes[mesh->startSceneVertex + i + 1].tangent.ToFloatPtr(), tangent.ToFloatPtr(), sizeof(float) * 3);
	//		memcpy(sceneVertexes[mesh->startSceneVertex + i + 2].tangent.ToFloatPtr(), tangent.ToFloatPtr(), sizeof(float) * 3);
	//	}
	//}
}

void *GL_LoadDXRMesh(idRenderModel *model)  {
	dxrMesh_t* mesh = new dxrMesh_t();
	
	//mesh->meshId = dxrMeshList.size();
	
	GL_LoadBottomLevelAccelStruct(mesh, model);

	dxrMeshList.push_back(mesh);

	return mesh;
}

void GL_FinishVertexBufferAllocation(void) {
//	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Create the vertex buffer.
	{
		const UINT vertexBufferSize = sizeof(dxrVertex_t) * sceneVertexes.size();

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		// Copy the triangle data to the vertex buffer.		
		CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, &sceneVertexes[0], sizeof(dxrVertex_t) * sceneVertexes.size());
		//m_vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(dxrVertex_t);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	for(int i = 0; i < dxrMeshList.size(); i++)
	{
		dxrMesh_t* mesh = dxrMeshList[i];
		mesh->bottomLevelAS.AddVertexBuffer(m_vertexBuffer.Get(), mesh->startSceneVertex * sizeof(dxrVertex_t), mesh->numSceneVertexes, sizeof(dxrVertex_t), NULL, 0);

		// Adding all vertex buffers and not transforming their position.
		//for (const auto& buffer : vVertexBuffers) {
		//	bottomLevelAS.AddVertexBuffer(buffer.first.Get(), 0, buffer.second,
		//		sizeof(Vertex), 0, 0);
		//}

		// The AS build requires some scratch space to store temporary information.
		// The amount of scratch memory is dependent on the scene complexity.
		UINT64 scratchSizeInBytes = 0;
		// The final AS also needs to be stored in addition to the existing vertex
		// buffers. It size is also dependent on the scene complexity.
		UINT64 resultSizeInBytes = 0;

		mesh->bottomLevelAS.ComputeASBufferSizes(m_device.Get(), false, &scratchSizeInBytes,
			&resultSizeInBytes);

		// Once the sizes are obtained, the application is responsible for allocating
		// the necessary buffers. Since the entire generation will be done on the GPU,
		// we can directly allocate those on the default heap	
		mesh->buffers.pScratch = nv_helpers_dx12::CreateBuffer(m_device.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);
		mesh->buffers.pResult = nv_helpers_dx12::CreateBuffer(m_device.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);

		// Build the acceleration structure. Note that this call integrates a barrier
		// on the generated AS, so that it can be used to compute a top-level AS right
		// after this method.

		mesh->bottomLevelAS.Generate(m_commandList.Get(), mesh->buffers.pScratch.Get(), mesh->buffers.pResult.Get(), false, nullptr);
	}

	// Flush the command list and wait for it to finish
	//m_commandList->Close();
	//ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	//m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
	//m_fenceValue++;
	//m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
	//
	//m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
	//WaitForSingleObject(m_fenceEvent, INFINITE);
}