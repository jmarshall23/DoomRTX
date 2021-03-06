// d3d12_local.h
//

#pragma once

#undef strcmp			
#undef strncmp			
#undef strcmpi			
#undef stricmp			
#undef _stricmp		
#undef strcasecmp		
#undef strnicmp		
#undef _strnicmp		
#undef _memicmp		
#undef snprintf		
#undef _snprintf		
#undef vsnprintf		

#define FLT_EPSILON 1.19209290E-07F

#include <float.h>
#include "d3dx12.h"
#include <wrl/client.h>
#include <dxgi1_4.h>
#include "DXRHelper.h"
#include "tinydx.h"

#include "../../renderer/tr_local.h"

typedef idRenderModel qmodel_t;

using namespace Microsoft::WRL;

#include <exception>

#include "nv_helpers_dx12/BottomLevelASGenerator.h"

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
inline void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}

struct dxrVertex_t {
	idVec3 xyz;
	idVec3 st;
	idVec3 normal;
	idVec4 vtinfo;
	idVec4 normalVtInfo;
	idVec4 tangent;
	idVec3 imageAveragePixel;
};


struct AccelerationStructureBuffers
{
	ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
	ComPtr<ID3D12Resource> pResult;       // Where the AS is
	ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
};


struct dxrSurface_t {
	int startVertex;
	int numVertexes;

	int startMegaVertex;
	int startIndex;
	int numIndexes;

	bool isOpaque;
	AccelerationStructureBuffers buffers;

	nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;
};

struct dxrMesh_t {
	std::vector<dxrVertex_t> meshVertexes;
	std::vector<dxrVertex_t> meshTriVertexes;
	std::vector<int> meshIndexes;
	std::vector<dxrSurface_t> meshSurfaces;

	int startSceneVertex;
	int numSceneVertexes;	
};

void GL_FinishVertexBufferAllocation(void);

const int FrameCount = 3;

extern ComPtr<IDXGISwapChain3> m_swapChain;
extern ComPtr<ID3D12Device5> m_device;
extern ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
extern ComPtr<ID3D12CommandAllocator> m_commandAllocator;
extern ComPtr<ID3D12CommandQueue> m_commandQueue;
extern ComPtr<ID3D12RootSignature> m_rootSignature;
extern ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
extern ComPtr<ID3D12PipelineState> m_pipelineState;
extern ComPtr<ID3D12GraphicsCommandList4> m_commandList;

extern HANDLE m_fenceEvent;
extern ComPtr<ID3D12Fence> m_fence;
extern UINT64 m_fenceValue;

extern ComPtr<ID3D12Resource> m_vertexBuffer;

void GL_CreateTopLevelAccelerationStructs(bool forceUpdate);

extern std::vector<dxrMesh_t*> dxrMeshList;

void GL_LoadMegaXML(const char* path);
void GL_LoadMegaTexture(D3D12_CPU_DESCRIPTOR_HANDLE &srvPtr);
void GL_LoadMegaNormalTexture(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr);
void GL_CreateInstanceInfo(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr);

void GL_InitCompositePass(tr_texture *albedoPass, tr_texture *lightPass, tr_texture* compositeStagingPass, tr_texture *compositePass, tr_texture* uiTexturePass);
void GL_CompositePass(tr_texture* albedoPass, tr_texture* lightPass, tr_texture* compositeStagingPass, tr_texture* compositePas, ID3D12GraphicsCommandList4* cmdList, ID3D12CommandAllocator *commandAllocator);
void GL_InitLightInfoBuffer(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr);
extern tr_renderer *renderer;
extern std::vector<dxrVertex_t> sceneVertexes;

void GL_BuildLightList(float x, float y, float z);
void GL_ClearLights(void);
void GL_WaitForPreviousFrame(void);

void GL_InitClearPass(tr_texture* lightPass);
void GL_ClearLightPass(tr_texture* lightPass, ID3D12GraphicsCommandList4* cmdList, ID3D12CommandAllocator* commandAllocator);

void mult_matrix_vector(float* p, const float* a, const float* b);
void mult_matrix_matrix(float* p, const float* a, const float* b);
void inverse(const float* m, float* inv);
void create_view_matrix(float* matrix, float* vieworg, idMat3 viewaxis);
void create_orthographic_matrix(float matrix[16], float xmin, float xmax, float ymin, float ymax, float znear, float zfar);
void create_projection_matrix(float matrix[16], float znear, float zfar, float fov_x, float fov_y);
void create_entity_matrix(float matrix[16], renderEntity_t* e);

void GL_UpdateBottomLevelAccelStruct(idRenderModel* model);
void GL_UpdateTextureInfo(idRenderModel* model);

void GL_RenderUI(ID3D12GraphicsCommandList4* cmdList, ID3D12CommandAllocator* commandAllocator);
extern tr_render_target* uiRenderTarget;

void GL_UpdateUI(void);

extern int numWorldLights;

static const int STAT_FORCE_TRANSPARENT = 2;
static const int STAT_FORCE_BLEND_TEST = 3;
static const int STAT_MIRROR = 4;
static const int STAT_GLASS = 5;