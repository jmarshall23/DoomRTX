#include "opengl.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include <stdint.h>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

// ============================================================
// Logging / checks
// ============================================================

static void glRaytracingLog(const char* fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	OutputDebugStringA(buffer);
	OutputDebugStringA("\n");
}

static void glRaytracingFatal(const char* fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	OutputDebugStringA(buffer);
	OutputDebugStringA("\n");
	MessageBoxA(nullptr, buffer, "glRaytracing Fatal", MB_OK | MB_ICONERROR);
	DebugBreak();
}

#define GLR_CHECK(x) \
    do { HRESULT _hr = (x); if (FAILED(_hr)) { glRaytracingFatal("HRESULT 0x%08X failed at %s:%d", (unsigned)_hr, __FILE__, __LINE__); return 0; } } while (0)

#define GLR_CHECKV(x) \
    do { HRESULT _hr = (x); if (FAILED(_hr)) { glRaytracingFatal("HRESULT 0x%08X failed at %s:%d", (unsigned)_hr, __FILE__, __LINE__); return; } } while (0)

// ============================================================
// Helpers
// ============================================================

static UINT64 glRaytracingAlignUp(UINT64 v, UINT64 a)
{
	return (v + (a - 1)) & ~(a - 1);
}

template<typename T>
static T glRaytracingClamp(T v, T lo, T hi)
{
	return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static void glRaytracingSetIdentity4x4(float* m)
{
	if (!m)
		return;

	memset(m, 0, sizeof(float) * 16);
	m[0] = 1.0f;
	m[5] = 1.0f;
	m[10] = 1.0f;
	m[15] = 1.0f;
}

static int glRaytracingInvertMatrix4x4(const float* m, float* out)
{
	if (!m || !out)
		return 0;

	float a[4][8];

	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
			a[r][c] = m[r * 4 + c];

		for (int c = 0; c < 4; ++c)
			a[r][4 + c] = (r == c) ? 1.0f : 0.0f;
	}

	for (int col = 0; col < 4; ++col)
	{
		int pivot = col;
		float best = a[col][col] < 0.0f ? -a[col][col] : a[col][col];

		for (int r = col + 1; r < 4; ++r)
		{
			const float v = a[r][col] < 0.0f ? -a[r][col] : a[r][col];
			if (v > best)
			{
				best = v;
				pivot = r;
			}
		}

		if (best <= 1.0e-8f)
			return 0;

		if (pivot != col)
		{
			for (int c = 0; c < 8; ++c)
			{
				const float tmp = a[col][c];
				a[col][c] = a[pivot][c];
				a[pivot][c] = tmp;
			}
		}

		const float invPivot = 1.0f / a[col][col];
		for (int c = 0; c < 8; ++c)
			a[col][c] *= invPivot;

		for (int r = 0; r < 4; ++r)
		{
			if (r == col)
				continue;

			const float f = a[r][col];
			if (f == 0.0f)
				continue;

			for (int c = 0; c < 8; ++c)
				a[r][c] -= f * a[col][c];
		}
	}

	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
			out[r * 4 + c] = a[r][4 + c];
	}

	return 1;
}


static DXGI_FORMAT glRaytracingGetSrvFormatForDepth(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_D32_FLOAT:         return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_D16_UNORM:         return DXGI_FORMAT_R16_UNORM;
	default:                            return fmt;
	}
}

struct glRaytracingBuffer_t
{
	ComPtr<ID3D12Resource> resource;
	UINT64 size;
	D3D12_GPU_VIRTUAL_ADDRESS gpuVA;

	glRaytracingBuffer_t()
	{
		size = 0;
		gpuVA = 0;
	}
};

static glRaytracingBuffer_t glRaytracingCreateBuffer(
	ID3D12Device* device,
	UINT64 size,
	D3D12_HEAP_TYPE heapType,
	D3D12_RESOURCE_STATES initialState,
	D3D12_RESOURCE_FLAGS flags)
{
	glRaytracingBuffer_t out;

	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = heapType;

	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rd.Flags = flags;

	HRESULT hr = device->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&rd,
		initialState,
		nullptr,
		IID_PPV_ARGS(&out.resource));

	if (FAILED(hr))
	{
		glRaytracingFatal("CreateCommittedResource failed 0x%08X", (unsigned)hr);
		return out;
	}

	out.size = size;
	out.gpuVA = out.resource->GetGPUVirtualAddress();
	return out;
}

struct glRaytracingTexture_t
{
	ComPtr<ID3D12Resource> resource;
	UINT width;
	UINT height;
	DXGI_FORMAT format;
	D3D12_RESOURCE_STATES state;

	glRaytracingTexture_t()
	{
		width = 0;
		height = 0;
		format = DXGI_FORMAT_UNKNOWN;
		state = D3D12_RESOURCE_STATE_COMMON;
	}
};

static glRaytracingTexture_t glRaytracingCreateTexture2D(
	ID3D12Device* device,
	UINT width,
	UINT height,
	DXGI_FORMAT format,
	D3D12_RESOURCE_STATES initialState,
	D3D12_RESOURCE_FLAGS flags)
{
	glRaytracingTexture_t out;

	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = width;
	rd.Height = height;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = format;
	rd.SampleDesc.Count = 1;
	rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rd.Flags = flags;

	HRESULT hr = device->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&rd,
		initialState,
		nullptr,
		IID_PPV_ARGS(&out.resource));

	if (FAILED(hr))
	{
		glRaytracingFatal("CreateCommittedResource texture failed 0x%08X", (unsigned)hr);
		return out;
	}

	out.width = width;
	out.height = height;
	out.format = format;
	out.state = initialState;
	return out;
}

static void glRaytracingMapCopy(ID3D12Resource* res, const void* src, size_t bytes)
{
	void* dst = nullptr;
	HRESULT hr = res->Map(0, nullptr, &dst);
	if (FAILED(hr))
	{
		glRaytracingFatal("Map failed 0x%08X", (unsigned)hr);
		return;
	}

	memcpy(dst, src, bytes);
	res->Unmap(0, nullptr);
}

static void glRaytracingTransition(
	ID3D12GraphicsCommandList* cmd,
	ID3D12Resource* res,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after)
{
	if (!res || before == after)
		return;

	D3D12_RESOURCE_BARRIER b = {};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = res;
	b.Transition.StateBefore = before;
	b.Transition.StateAfter = after;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmd->ResourceBarrier(1, &b);
}

static D3D12_CPU_DESCRIPTOR_HANDLE glRaytracingOffsetCpu(D3D12_CPU_DESCRIPTOR_HANDLE h, UINT stride, UINT idx)
{
	h.ptr += UINT64(stride) * UINT64(idx);
	return h;
}

static D3D12_GPU_DESCRIPTOR_HANDLE glRaytracingOffsetGpu(D3D12_GPU_DESCRIPTOR_HANDLE h, UINT stride, UINT idx)
{
	h.ptr += UINT64(stride) * UINT64(idx);
	return h;
}

// ============================================================
// Shared command context
// ============================================================

struct glRaytracingCmdContext_t
{
	ComPtr<ID3D12Device5> device;
	ComPtr<ID3D12CommandQueue> queue;

	ComPtr<ID3D12CommandAllocator> cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList4> cmdList;
	UINT64 cmdLastFenceValue;

	ComPtr<ID3D12CommandAllocator> blasCmdAlloc;
	ComPtr<ID3D12GraphicsCommandList4> blasCmdList;
	UINT64 blasLastFenceValue;

	ComPtr<ID3D12CommandAllocator> tlasCmdAlloc;
	ComPtr<ID3D12GraphicsCommandList4> tlasCmdList;
	UINT64 tlasLastFenceValue;

	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 nextFenceValue;
	bool initialized;

	glRaytracingCmdContext_t()
	{
		cmdLastFenceValue = 0;
		blasLastFenceValue = 0;
		tlasLastFenceValue = 0;
		fenceEvent = nullptr;
		nextFenceValue = 0;
		initialized = false;
	}
};

static glRaytracingCmdContext_t g_glRaytracingCmd;
static std::mutex g_glRaytracingMutex;

static void glRaytracingWaitFenceValue(UINT64 value)
{
	if (!value || !g_glRaytracingCmd.fence)
		return;

	if (g_glRaytracingCmd.fence->GetCompletedValue() >= value)
		return;

	g_glRaytracingCmd.fence->SetEventOnCompletion(value, g_glRaytracingCmd.fenceEvent);
	WaitForSingleObject(g_glRaytracingCmd.fenceEvent, INFINITE);
}

static UINT64 glRaytracingSignalQueue(void)
{
	if (!g_glRaytracingCmd.queue || !g_glRaytracingCmd.fence)
		return 0;

	const UINT64 value = ++g_glRaytracingCmd.nextFenceValue;
	g_glRaytracingCmd.queue->Signal(g_glRaytracingCmd.fence.Get(), value);
	return value;
}

static void glRaytracingWaitIdle(void)
{
	const UINT64 value = glRaytracingSignalQueue();
	glRaytracingWaitFenceValue(value);
}

static int glRaytracingInitCmdContext(void)
{
	ID3D12Device* baseDevice = QD3D12_GetDevice();
	ID3D12CommandQueue* baseQueue = QD3D12_GetQueue();

	if (g_glRaytracingCmd.initialized)
	{
		if (!baseDevice || !baseQueue)
		{
			glRaytracingFatal("glRaytracingInitCmdContext: missing device or queue");
			return 0;
		}

		ComPtr<ID3D12Device5> currentDevice;
		HRESULT hr = baseDevice->QueryInterface(IID_PPV_ARGS(&currentDevice));
		if (FAILED(hr) || currentDevice.Get() != g_glRaytracingCmd.device.Get() || baseQueue != g_glRaytracingCmd.queue.Get())
		{
			glRaytracingFatal("glRaytracingInitCmdContext: D3D12 device/queue changed. DXR state is device-local; create/use all windows with the same D3D12 device and queue, or fully shut down raytracing before switching devices.");
			return 0;
		}

		return 1;
	}

	if (!baseDevice || !baseQueue)
	{
		glRaytracingFatal("glRaytracingInitCmdContext: missing device or queue");
		return 0;
	}

	GLR_CHECK(baseDevice->QueryInterface(IID_PPV_ARGS(&g_glRaytracingCmd.device)));
	g_glRaytracingCmd.queue = baseQueue;

	GLR_CHECK(g_glRaytracingCmd.device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&g_glRaytracingCmd.cmdAlloc)));

	GLR_CHECK(g_glRaytracingCmd.device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		g_glRaytracingCmd.cmdAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(&g_glRaytracingCmd.cmdList)));

	GLR_CHECK(g_glRaytracingCmd.cmdList->Close());

	GLR_CHECK(g_glRaytracingCmd.device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&g_glRaytracingCmd.blasCmdAlloc)));

	GLR_CHECK(g_glRaytracingCmd.device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		g_glRaytracingCmd.blasCmdAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(&g_glRaytracingCmd.blasCmdList)));

	GLR_CHECK(g_glRaytracingCmd.blasCmdList->Close());

	GLR_CHECK(g_glRaytracingCmd.device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&g_glRaytracingCmd.tlasCmdAlloc)));

	GLR_CHECK(g_glRaytracingCmd.device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		g_glRaytracingCmd.tlasCmdAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(&g_glRaytracingCmd.tlasCmdList)));

	GLR_CHECK(g_glRaytracingCmd.tlasCmdList->Close());

	GLR_CHECK(g_glRaytracingCmd.device->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&g_glRaytracingCmd.fence)));

	g_glRaytracingCmd.fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	if (!g_glRaytracingCmd.fenceEvent)
	{
		glRaytracingFatal("CreateEventA failed");
		return 0;
	}

	g_glRaytracingCmd.initialized = true;
	return 1;
}

static void glRaytracingShutdownCmdContext(void)
{
	if (!g_glRaytracingCmd.initialized)
		return;

	glRaytracingWaitIdle();

	if (g_glRaytracingCmd.fenceEvent)
	{
		CloseHandle(g_glRaytracingCmd.fenceEvent);
		g_glRaytracingCmd.fenceEvent = nullptr;
	}

	g_glRaytracingCmd = glRaytracingCmdContext_t();
}

static int glRaytracingBeginCmd(void)
{
	glRaytracingWaitFenceValue(g_glRaytracingCmd.cmdLastFenceValue);

	GLR_CHECK(g_glRaytracingCmd.cmdAlloc->Reset());
	GLR_CHECK(g_glRaytracingCmd.cmdList->Reset(g_glRaytracingCmd.cmdAlloc.Get(), nullptr));
	return 1;
}

static int glRaytracingEndCmd(void)
{
	GLR_CHECK(g_glRaytracingCmd.cmdList->Close());

	ID3D12CommandList* lists[] = { g_glRaytracingCmd.cmdList.Get() };
	g_glRaytracingCmd.queue->ExecuteCommandLists(1, lists);

	g_glRaytracingCmd.cmdLastFenceValue = glRaytracingSignalQueue();
	glRaytracingWaitFenceValue(g_glRaytracingCmd.cmdLastFenceValue);
	return 1;
}

static int glRaytracingBeginBlasCmd(void)
{
	glRaytracingWaitFenceValue(g_glRaytracingCmd.blasLastFenceValue);

	GLR_CHECK(g_glRaytracingCmd.blasCmdAlloc->Reset());
	GLR_CHECK(g_glRaytracingCmd.blasCmdList->Reset(g_glRaytracingCmd.blasCmdAlloc.Get(), nullptr));
	return 1;
}

static UINT64 glRaytracingEndBlasCmd(void)
{
	GLR_CHECK(g_glRaytracingCmd.blasCmdList->Close());

	ID3D12CommandList* lists[] = { g_glRaytracingCmd.blasCmdList.Get() };
	g_glRaytracingCmd.queue->ExecuteCommandLists(1, lists);

	g_glRaytracingCmd.blasLastFenceValue = glRaytracingSignalQueue();
	return g_glRaytracingCmd.blasLastFenceValue;
}

static int glRaytracingBeginTlasCmd(void)
{
	glRaytracingWaitFenceValue(g_glRaytracingCmd.tlasLastFenceValue);

	GLR_CHECK(g_glRaytracingCmd.tlasCmdAlloc->Reset());
	GLR_CHECK(g_glRaytracingCmd.tlasCmdList->Reset(g_glRaytracingCmd.tlasCmdAlloc.Get(), nullptr));
	return 1;
}

static UINT64 glRaytracingEndTlasCmd(void)
{
	GLR_CHECK(g_glRaytracingCmd.tlasCmdList->Close());

	ID3D12CommandList* lists[] = { g_glRaytracingCmd.tlasCmdList.Get() };
	g_glRaytracingCmd.queue->ExecuteCommandLists(1, lists);

	g_glRaytracingCmd.tlasLastFenceValue = glRaytracingSignalQueue();
	return g_glRaytracingCmd.tlasLastFenceValue;
}


// ============================================================
// Scene builder state
// ============================================================

#ifndef GL_RAYTRACING_MAX_RENDER_WORLDS
#define GL_RAYTRACING_MAX_RENDER_WORLDS 24
#endif

#ifndef GL_RAYTRACING_SCENE_HANDLE_T_DEFINED
typedef uint32_t glRaytracingSceneHandle_t;
#define GL_RAYTRACING_SCENE_HANDLE_T_DEFINED
#endif

// Internal material metadata encoded into D3D12's 24-bit shader-visible
// InstanceID. Lower 16 bits remain caller/user ID; bits 16-23 are material flags.
static const uint32_t GL_RAYTRACING_INSTANCE_USER_ID_MASK = 0x0000FFFFu;
static const uint32_t GL_RAYTRACING_INSTANCE_MATERIAL_SHIFT = 16u;
static const uint32_t GL_RAYTRACING_INSTANCE_MATERIAL_MASK = 0x000000FFu;

struct glRaytracingMeshRecord_t
{
	uint32_t handle;
	int      alive;

	glRaytracingMeshDesc_t descCpu;

	std::vector<glRaytracingVertex_t> verticesCpu;
	std::vector<uint32_t>             indicesCpu;

	glRaytracingBuffer_t vertexBuffer;
	glRaytracingBuffer_t indexBuffer;

	glRaytracingBuffer_t blasScratch;
	glRaytracingBuffer_t blasResult[2];

	UINT64 blasScratchSize;
	UINT64 blasResultSize;

	int blasBuilt;
	int dirty;
	int currentBlasIndex;
	uint32_t materialFlags;

	glRaytracingMeshRecord_t()
	{
		handle = 0;
		alive = 0;
		memset(&descCpu, 0, sizeof(descCpu));
		blasScratchSize = 0;
		blasResultSize = 0;
		blasBuilt = 0;
		dirty = 0;
		currentBlasIndex = 0;
		materialFlags = 0;
	}
};

struct glRaytracingInstanceRecord_t
{
	uint32_t handle;
	int      alive;
	glRaytracingInstanceDesc_t descCpu;
	int dirty;
	int cachedActive;
	D3D12_GPU_VIRTUAL_ADDRESS cachedBlasGpuVA;
	D3D12_RAYTRACING_INSTANCE_DESC cachedDescCpu;

	glRaytracingInstanceRecord_t()
	{
		handle = 0;
		alive = 0;
		memset(&descCpu, 0, sizeof(descCpu));
		dirty = 0;
		cachedActive = 0;
		cachedBlasGpuVA = 0;
		memset(&cachedDescCpu, 0, sizeof(cachedDescCpu));
	}
};

struct glRaytracingSceneUploadBuffer_t
{
	glRaytracingBuffer_t buffer;
	UINT64 capacityBytes;
	D3D12_RAYTRACING_INSTANCE_DESC* mapped;

	glRaytracingSceneUploadBuffer_t()
	{
		capacityBytes = 0;
		mapped = nullptr;
	}
};

// One render world owns exactly one TLAS pair and its own list of geometry
// instances. Mesh/BLAS resources stay shared across all worlds.
struct glRaytracingRenderWorld_t
{
	uint32_t handle;
	int alive;

	std::vector<glRaytracingInstanceRecord_t> instances;
	std::vector<int> activeInstanceIndices;
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> cpuInstanceDescs;
	std::vector<int> instanceHandleToIndex;

	uint32_t nextInstanceHandle;

	glRaytracingSceneUploadBuffer_t instanceDescUpload[2];
	glRaytracingBuffer_t tlasScratch;
	glRaytracingBuffer_t tlasResult[2];

	UINT64 tlasScratchSize;
	UINT64 tlasResultSize;

	UINT activeInstanceCount;
	UINT builtInstanceCount;

	int tlasBuilt;
	int tlasNeedsRebuild;
	int tlasNeedsUpdate;
	int currentTLASIndex;

	glRaytracingRenderWorld_t()
	{
		handle = 0;
		alive = 0;
		nextInstanceHandle = 1;
		tlasScratchSize = 0;
		tlasResultSize = 0;
		activeInstanceCount = 0;
		builtInstanceCount = 0;
		tlasBuilt = 0;
		tlasNeedsRebuild = 1;
		tlasNeedsUpdate = 1;
		currentTLASIndex = 0;
	}
};

struct glRaytracingSceneState_t
{
	std::vector<glRaytracingMeshRecord_t> meshes;
	std::vector<int> meshHandleToIndex;

	uint32_t nextMeshHandle;

	glRaytracingRenderWorld_t worlds[GL_RAYTRACING_MAX_RENDER_WORLDS];

	int initialized;

	glRaytracingSceneState_t()
	{
		nextMeshHandle = 1;
		initialized = 0;
	}
};

static glRaytracingSceneState_t g_glRaytracingScene;

void glRaytracingClear(void);

static void glRaytracingReleaseWorldResources(glRaytracingRenderWorld_t* world)
{
	if (!world)
		return;

	for (int i = 0; i < 2; ++i)
	{
		if (world->instanceDescUpload[i].buffer.resource && world->instanceDescUpload[i].mapped)
			world->instanceDescUpload[i].buffer.resource->Unmap(0, nullptr);

		world->instanceDescUpload[i] = glRaytracingSceneUploadBuffer_t();
		world->tlasResult[i].resource.Reset();
	}

	world->tlasScratch.resource.Reset();
	world->tlasScratchSize = 0;
	world->tlasResultSize = 0;
}

static void glRaytracingResetWorldSlot(glRaytracingRenderWorld_t* world, uint32_t handle, int alive)
{
	if (!world)
		return;

	glRaytracingReleaseWorldResources(world);
	*world = glRaytracingRenderWorld_t();
	world->handle = handle;
	world->alive = alive ? 1 : 0;
	world->nextInstanceHandle = 1;
}

static int glRaytracingWorldHandleToSlot(glRaytracingSceneHandle_t worldHandle)
{
	if (worldHandle == 0 || worldHandle > GL_RAYTRACING_MAX_RENDER_WORLDS)
		return -1;
	return (int)(worldHandle - 1);
}

static glRaytracingRenderWorld_t* glRaytracingFindWorld(glRaytracingSceneHandle_t worldHandle)
{
	const int slot = glRaytracingWorldHandleToSlot(worldHandle);
	if (slot < 0)
		return nullptr;

	glRaytracingRenderWorld_t& world = g_glRaytracingScene.worlds[slot];
	if (!world.alive || world.handle != worldHandle)
		return nullptr;

	return &world;
}

static const glRaytracingRenderWorld_t* glRaytracingFindWorldConst(glRaytracingSceneHandle_t worldHandle)
{
	const int slot = glRaytracingWorldHandleToSlot(worldHandle);
	if (slot < 0)
		return nullptr;

	const glRaytracingRenderWorld_t& world = g_glRaytracingScene.worlds[slot];
	if (!world.alive || world.handle != worldHandle)
		return nullptr;

	return &world;
}

static void glRaytracingClearWorldContents(glRaytracingRenderWorld_t* world)
{
	if (!world)
		return;

	const uint32_t handle = world->handle;
	const int alive = world->alive;

	glRaytracingReleaseWorldResources(world);

	world->instances.clear();
	world->activeInstanceIndices.clear();
	world->cpuInstanceDescs.clear();
	world->instanceHandleToIndex.clear();

	world->handle = handle;
	world->alive = alive;
	world->nextInstanceHandle = 1;
	world->tlasScratchSize = 0;
	world->tlasResultSize = 0;
	world->activeInstanceCount = 0;
	world->builtInstanceCount = 0;
	world->tlasBuilt = 0;
	world->tlasNeedsRebuild = 1;
	world->tlasNeedsUpdate = 1;
	world->currentTLASIndex = 0;
}

static void glRaytracingClearAllSceneStateInternal(void)
{
	for (int i = 0; i < GL_RAYTRACING_MAX_RENDER_WORLDS; ++i)
		glRaytracingReleaseWorldResources(&g_glRaytracingScene.worlds[i]);

	const int wasInitialized = g_glRaytracingScene.initialized;
	g_glRaytracingScene = glRaytracingSceneState_t();
	g_glRaytracingScene.initialized = wasInitialized;
}

static void glRaytracingMarkWorldNeedsRebuild(glRaytracingRenderWorld_t* world)
{
	if (!world || !world->alive)
		return;

	world->tlasNeedsRebuild = 1;
	world->tlasNeedsUpdate = 0;
}

static void glRaytracingMarkWorldNeedsUpdate(glRaytracingRenderWorld_t* world)
{
	if (!world || !world->alive)
		return;

	if (!world->tlasNeedsRebuild)
		world->tlasNeedsUpdate = 1;
}

static void glRaytracingMarkAllWorldsNeedRebuild(void)
{
	for (int i = 0; i < GL_RAYTRACING_MAX_RENDER_WORLDS; ++i)
	{
		if (g_glRaytracingScene.worlds[i].alive)
			glRaytracingMarkWorldNeedsRebuild(&g_glRaytracingScene.worlds[i]);
	}
}

static uint32_t glRaytracingCountAliveInstances(const glRaytracingRenderWorld_t* world)
{
	if (!world)
		return 0;

	uint32_t count = 0;
	for (size_t i = 0; i < world->instances.size(); ++i)
	{
		if (world->instances[i].alive)
			++count;
	}
	return count;
}

static void glRaytracingEnsureMeshHandleTable(uint32_t handle)
{
	if (handle >= g_glRaytracingScene.meshHandleToIndex.size())
		g_glRaytracingScene.meshHandleToIndex.resize((size_t)handle + 1, -1);
}

static void glRaytracingEnsureInstanceHandleTable(glRaytracingRenderWorld_t* world, uint32_t handle)
{
	if (!world)
		return;

	if (handle >= world->instanceHandleToIndex.size())
		world->instanceHandleToIndex.resize((size_t)handle + 1, -1);
}

static glRaytracingBuffer_t* glRaytracingGetMeshCurrentBLAS(glRaytracingMeshRecord_t* mesh)
{
	if (!mesh)
		return nullptr;
	return &mesh->blasResult[mesh->currentBlasIndex & 1];
}

static const glRaytracingBuffer_t* glRaytracingGetMeshCurrentBLASConst(const glRaytracingMeshRecord_t* mesh)
{
	if (!mesh)
		return nullptr;
	return &mesh->blasResult[mesh->currentBlasIndex & 1];
}

static int glRaytracingGetInactiveTLASIndex(const glRaytracingRenderWorld_t* world)
{
	if (!world)
		return 0;
	return world->currentTLASIndex ^ 1;
}

static glRaytracingSceneUploadBuffer_t* glRaytracingGetBuildInstanceUpload(glRaytracingRenderWorld_t* world)
{
	return &world->instanceDescUpload[glRaytracingGetInactiveTLASIndex(world)];
}

static glRaytracingBuffer_t* glRaytracingGetCurrentTLASBuffer(glRaytracingRenderWorld_t* world)
{
	return &world->tlasResult[world->currentTLASIndex & 1];
}

static const glRaytracingBuffer_t* glRaytracingGetCurrentTLASBufferConst(const glRaytracingRenderWorld_t* world)
{
	return &world->tlasResult[world->currentTLASIndex & 1];
}

static glRaytracingBuffer_t* glRaytracingGetBuildTLASBuffer(glRaytracingRenderWorld_t* world)
{
	return &world->tlasResult[glRaytracingGetInactiveTLASIndex(world)];
}

static int glRaytracingEnsureTLASBuffers(
	glRaytracingRenderWorld_t* world,
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* inputs)
{
	if (!world)
		return 0;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
	g_glRaytracingCmd.device->GetRaytracingAccelerationStructurePrebuildInfo(inputs, &prebuild);

	if (prebuild.ResultDataMaxSizeInBytes == 0)
	{
		glRaytracingFatal("TLAS prebuild size is zero");
		return 0;
	}

	const UINT64 requiredScratch = glRaytracingAlignUp(
		prebuild.ScratchDataSizeInBytes,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	const UINT64 requiredResult = glRaytracingAlignUp(
		prebuild.ResultDataMaxSizeInBytes,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	if (!world->tlasScratch.resource ||
		world->tlasScratchSize < requiredScratch)
	{
		world->tlasScratch.resource.Reset();
		world->tlasScratch = glRaytracingCreateBuffer(
			g_glRaytracingCmd.device.Get(),
			requiredScratch,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		if (!world->tlasScratch.resource)
			return 0;

		world->tlasScratchSize = requiredScratch;
	}

	for (int i = 0; i < 2; ++i)
	{
		if (!world->tlasResult[i].resource ||
			world->tlasResultSize < requiredResult)
		{
			world->tlasResult[i].resource.Reset();
			world->tlasResult[i] = glRaytracingCreateBuffer(
				g_glRaytracingCmd.device.Get(),
				requiredResult,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			if (!world->tlasResult[i].resource)
				return 0;
		}
	}

	world->tlasResultSize = requiredResult;
	return 1;
}

static glRaytracingMeshRecord_t* glRaytracingFindMesh(uint32_t handle)
{
	if (handle == 0 || handle >= g_glRaytracingScene.meshHandleToIndex.size())
		return nullptr;

	const int index = g_glRaytracingScene.meshHandleToIndex[handle];
	if (index < 0 || (size_t)index >= g_glRaytracingScene.meshes.size())
		return nullptr;

	glRaytracingMeshRecord_t& mesh = g_glRaytracingScene.meshes[(size_t)index];
	if (!mesh.alive || mesh.handle != handle)
		return nullptr;

	return &mesh;
}

static const glRaytracingMeshRecord_t* glRaytracingFindMeshConst(uint32_t handle)
{
	if (handle == 0 || handle >= g_glRaytracingScene.meshHandleToIndex.size())
		return nullptr;

	const int index = g_glRaytracingScene.meshHandleToIndex[handle];
	if (index < 0 || (size_t)index >= g_glRaytracingScene.meshes.size())
		return nullptr;

	const glRaytracingMeshRecord_t& mesh = g_glRaytracingScene.meshes[(size_t)index];
	if (!mesh.alive || mesh.handle != handle)
		return nullptr;

	return &mesh;
}

static glRaytracingInstanceRecord_t* glRaytracingFindInstance(glRaytracingRenderWorld_t* world, uint32_t handle)
{
	if (!world || handle == 0 || handle >= world->instanceHandleToIndex.size())
		return nullptr;

	const int index = world->instanceHandleToIndex[handle];
	if (index < 0 || (size_t)index >= world->instances.size())
		return nullptr;

	glRaytracingInstanceRecord_t& inst = world->instances[(size_t)index];
	if (!inst.alive || inst.handle != handle)
		return nullptr;

	return &inst;
}

static const glRaytracingInstanceRecord_t* glRaytracingFindInstanceConst(const glRaytracingRenderWorld_t* world, uint32_t handle)
{
	if (!world || handle == 0 || handle >= world->instanceHandleToIndex.size())
		return nullptr;

	const int index = world->instanceHandleToIndex[handle];
	if (index < 0 || (size_t)index >= world->instances.size())
		return nullptr;

	const glRaytracingInstanceRecord_t& inst = world->instances[(size_t)index];
	if (!inst.alive || inst.handle != handle)
		return nullptr;

	return &inst;
}

static int glRaytracingEnsureMeshScratch(glRaytracingMeshRecord_t* mesh, UINT64 requiredScratch)
{
	if (!mesh)
		return 0;

	requiredScratch = glRaytracingAlignUp(requiredScratch, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	if (!mesh->blasScratch.resource || mesh->blasScratchSize < requiredScratch)
	{
		mesh->blasScratch.resource.Reset();
		mesh->blasScratch = glRaytracingCreateBuffer(
			g_glRaytracingCmd.device.Get(),
			requiredScratch,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		if (!mesh->blasScratch.resource)
			return 0;

		mesh->blasScratchSize = requiredScratch;
	}

	return 1;
}

static int glRaytracingEnsureMeshResultBuffers(glRaytracingMeshRecord_t* mesh, UINT64 requiredResult)
{
	if (!mesh)
		return 0;

	requiredResult = glRaytracingAlignUp(requiredResult, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	const int resultCount = mesh->descCpu.allowUpdate ? 2 : 1;
	for (int i = 0; i < resultCount; ++i)
	{
		if (!mesh->blasResult[i].resource || mesh->blasResultSize < requiredResult)
		{
			mesh->blasResult[i].resource.Reset();
			mesh->blasResult[i] = glRaytracingCreateBuffer(
				g_glRaytracingCmd.device.Get(),
				requiredResult,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			if (!mesh->blasResult[i].resource)
				return 0;
		}
	}

	if (!mesh->descCpu.allowUpdate)
		mesh->blasResult[1].resource.Reset();

	mesh->blasResultSize = requiredResult;
	return 1;
}

static inline void glRaytracingBuildInstanceDesc(
	D3D12_RAYTRACING_INSTANCE_DESC* outDesc,
	const glRaytracingInstanceRecord_t& inst,
	uint32_t meshMaterialFlags,
	D3D12_GPU_VIRTUAL_ADDRESS blasGpuVA)
{
	memcpy(outDesc->Transform, inst.descCpu.transform, sizeof(float) * 12);

	// InstanceID is 24-bit in D3D12_RAYTRACING_INSTANCE_DESC. Preserve the
	// caller's lower 16 bits, then pack material flags into bits 16-23 so the
	// any-hit shader can decide whether visibility rays pass through the hit.
	//
	// Accept both sources:
	// - meshMaterialFlags set by glRaytracingSetMeshMaterialFlags()/shim mesh tags
	// - already-encoded high InstanceID bits for direct low-level callers
	const uint32_t userInstanceId = inst.descCpu.instanceID & GL_RAYTRACING_INSTANCE_USER_ID_MASK;
	const uint32_t instanceMaterialFlags =
		(inst.descCpu.instanceID >> GL_RAYTRACING_INSTANCE_MATERIAL_SHIFT) & GL_RAYTRACING_INSTANCE_MATERIAL_MASK;
	const uint32_t combinedMaterialFlags =
		(meshMaterialFlags | instanceMaterialFlags) & GL_RAYTRACING_INSTANCE_MATERIAL_MASK;
	const uint32_t materialBits = combinedMaterialFlags << GL_RAYTRACING_INSTANCE_MATERIAL_SHIFT;
	outDesc->InstanceID = userInstanceId | materialBits;
	outDesc->InstanceMask = (UINT8)(inst.descCpu.mask ? inst.descCpu.mask : 0xFF);
	outDesc->InstanceContributionToHitGroupIndex = 0;
	outDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	outDesc->AccelerationStructure = blasGpuVA;
}

static void glRaytracingInvalidateInstanceCache(glRaytracingInstanceRecord_t* inst)
{
	if (!inst)
		return;

	inst->cachedActive = 0;
	inst->cachedBlasGpuVA = 0;
	memset(&inst->cachedDescCpu, 0, sizeof(inst->cachedDescCpu));
}

static int glRaytracingResolveInstanceDesc(
	glRaytracingInstanceRecord_t* inst,
	D3D12_RAYTRACING_INSTANCE_DESC* outDesc,
	D3D12_GPU_VIRTUAL_ADDRESS* outBlasGpuVA)
{
	if (!inst || !inst->alive)
		return 0;

	const glRaytracingMeshRecord_t* mesh = glRaytracingFindMeshConst(inst->descCpu.meshHandle);
	if (!mesh || !mesh->blasBuilt)
		return 0;

	const glRaytracingBuffer_t* blas = glRaytracingGetMeshCurrentBLASConst(mesh);
	if (!blas || !blas->resource || blas->gpuVA == 0)
		return 0;

	if (outDesc)
		glRaytracingBuildInstanceDesc(outDesc, *inst, mesh->materialFlags, blas->gpuVA);
	if (outBlasGpuVA)
		*outBlasGpuVA = blas->gpuVA;
	return 1;
}

static int glRaytracingRebuildActiveInstanceCache(glRaytracingRenderWorld_t* world)
{
	if (!world)
		return 0;

	world->activeInstanceIndices.clear();
	world->cpuInstanceDescs.clear();
	world->activeInstanceIndices.reserve(world->instances.size());
	world->cpuInstanceDescs.reserve(world->instances.size());

	for (size_t i = 0; i < world->instances.size(); ++i)
	{
		glRaytracingInstanceRecord_t& inst = world->instances[i];
		glRaytracingInvalidateInstanceCache(&inst);

		if (!inst.alive)
			continue;

		D3D12_RAYTRACING_INSTANCE_DESC desc = {};
		D3D12_GPU_VIRTUAL_ADDRESS blasGpuVA = 0;
		if (!glRaytracingResolveInstanceDesc(&inst, &desc, &blasGpuVA))
			continue;

		inst.cachedActive = 1;
		inst.cachedBlasGpuVA = blasGpuVA;
		inst.cachedDescCpu = desc;
		inst.dirty = 0;

		world->activeInstanceIndices.push_back((int)i);
		world->cpuInstanceDescs.push_back(desc);
	}

	world->activeInstanceCount = (UINT)world->cpuInstanceDescs.size();
	return 1;
}

static int glRaytracingRefreshDirtyInstanceCache(glRaytracingRenderWorld_t* world)
{
	if (!world)
		return 0;

	for (size_t listIndex = 0; listIndex < world->activeInstanceIndices.size(); ++listIndex)
	{
		const int instIndex = world->activeInstanceIndices[listIndex];
		if (instIndex < 0 || (size_t)instIndex >= world->instances.size())
			return 0;

		glRaytracingInstanceRecord_t& inst = world->instances[(size_t)instIndex];
		if (!inst.alive)
			return 0;

		D3D12_RAYTRACING_INSTANCE_DESC desc = {};
		D3D12_GPU_VIRTUAL_ADDRESS blasGpuVA = 0;
		if (!glRaytracingResolveInstanceDesc(&inst, &desc, &blasGpuVA))
			return 0;

		if (inst.dirty || !inst.cachedActive || inst.cachedBlasGpuVA != blasGpuVA)
		{
			inst.cachedActive = 1;
			inst.cachedBlasGpuVA = blasGpuVA;
			inst.cachedDescCpu = desc;
			world->cpuInstanceDescs[listIndex] = desc;
		}

		inst.dirty = 0;
	}

	world->activeInstanceCount = (UINT)world->cpuInstanceDescs.size();
	return 1;
}

static int glRaytracingEnsureSceneUploadBuffer(glRaytracingRenderWorld_t* world, UINT64 requiredBytes);

static int glRaytracingUploadCachedInstanceDescs(glRaytracingRenderWorld_t* world)
{
	if (!world)
		return 0;

	const UINT activeCount = (UINT)world->cpuInstanceDescs.size();
	const UINT64 instBytes = glRaytracingAlignUp(
		(UINT64)activeCount * (UINT64)sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
		D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);

	if (!glRaytracingEnsureSceneUploadBuffer(world, instBytes))
		return 0;

	glRaytracingSceneUploadBuffer_t* upload = glRaytracingGetBuildInstanceUpload(world);
	if (!upload->mapped)
		return 0;

	if (activeCount > 0)
		memcpy(upload->mapped, world->cpuInstanceDescs.data(), (size_t)activeCount * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	return 1;
}

static int glRaytracingUploadMeshBuffers(glRaytracingMeshRecord_t* mesh);

static int glRaytracingBuildDirtyMeshesInternal(void)
{
	std::vector<glRaytracingMeshRecord_t*> dirtyMeshes;
	dirtyMeshes.reserve(g_glRaytracingScene.meshes.size());

	for (size_t i = 0; i < g_glRaytracingScene.meshes.size(); ++i)
	{
		glRaytracingMeshRecord_t& mesh = g_glRaytracingScene.meshes[i];
		if (!mesh.alive)
			continue;

		if (!mesh.blasBuilt || mesh.dirty)
			dirtyMeshes.push_back(&mesh);
	}

	if (dirtyMeshes.empty())
		return 1;

	struct glRaytracingMeshBuildInfo_t
	{
		glRaytracingMeshRecord_t* mesh;
		D3D12_RAYTRACING_GEOMETRY_DESC geomDesc;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc;
		ID3D12Resource* barrierResource;
		int newBlasIndex;
	};

	std::vector<glRaytracingMeshBuildInfo_t> builds;
	builds.resize(dirtyMeshes.size());

	for (size_t i = 0; i < dirtyMeshes.size(); ++i)
	{
		glRaytracingMeshRecord_t* mesh = dirtyMeshes[i];
		if (!mesh->vertexBuffer.resource || !mesh->indexBuffer.resource)
		{
			if (!glRaytracingUploadMeshBuffers(mesh))
				return 0;
		}

		glRaytracingMeshBuildInfo_t& info = builds[i];
		memset(&info, 0, sizeof(info));
		info.mesh = mesh;

		info.geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		const bool meshIsGlass = (mesh->materialFlags & GL_RAYTRACING_MATERIAL_FLAG_GLASS) != 0u;
		info.geomDesc.Flags = (mesh->descCpu.opaque && !meshIsGlass)
			? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE
			: D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
		info.geomDesc.Triangles.Transform3x4 = 0;
		info.geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		info.geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		info.geomDesc.Triangles.IndexCount = (UINT)mesh->indicesCpu.size();
		info.geomDesc.Triangles.VertexCount = (UINT)mesh->verticesCpu.size();
		info.geomDesc.Triangles.IndexBuffer = mesh->indexBuffer.gpuVA;
		info.geomDesc.Triangles.VertexBuffer.StartAddress = mesh->vertexBuffer.gpuVA;
		info.geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(glRaytracingVertex_t);

		info.inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		info.inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		info.inputs.NumDescs = 1;
		info.inputs.pGeometryDescs = &info.geomDesc;
		info.inputs.Flags = mesh->descCpu.allowUpdate
			? (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)
			: D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		const bool canUpdateInPlace = (mesh->blasBuilt != 0) && (mesh->descCpu.allowUpdate != 0);
		if (canUpdateInPlace)
			info.inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
		g_glRaytracingCmd.device->GetRaytracingAccelerationStructurePrebuildInfo(&info.inputs, &prebuild);
		if (prebuild.ResultDataMaxSizeInBytes == 0)
		{
			glRaytracingFatal("BLAS prebuild size is zero");
			return 0;
		}

		if (!glRaytracingEnsureMeshScratch(mesh, prebuild.ScratchDataSizeInBytes))
			return 0;
		if (!glRaytracingEnsureMeshResultBuffers(mesh, prebuild.ResultDataMaxSizeInBytes))
			return 0;

		const int oldIndex = mesh->currentBlasIndex & 1;
		info.newBlasIndex = (mesh->descCpu.allowUpdate && mesh->blasBuilt) ? (oldIndex ^ 1) : oldIndex;

		info.buildDesc.Inputs = info.inputs;
		info.buildDesc.ScratchAccelerationStructureData = mesh->blasScratch.gpuVA;
		info.buildDesc.DestAccelerationStructureData = mesh->blasResult[info.newBlasIndex].gpuVA;
		info.buildDesc.SourceAccelerationStructureData = 0;

		if (canUpdateInPlace)
			info.buildDesc.SourceAccelerationStructureData = mesh->blasResult[oldIndex].gpuVA;

		info.barrierResource = mesh->blasResult[info.newBlasIndex].resource.Get();
	}

	if (!glRaytracingBeginBlasCmd())
		return 0;

	for (size_t i = 0; i < builds.size(); ++i)
	{
		g_glRaytracingCmd.blasCmdList->BuildRaytracingAccelerationStructure(&builds[i].buildDesc, 0, nullptr);

		D3D12_RESOURCE_BARRIER uav = {};
		uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uav.UAV.pResource = builds[i].barrierResource;
		g_glRaytracingCmd.blasCmdList->ResourceBarrier(1, &uav);
	}

	const UINT64 blasFenceValue = glRaytracingEndBlasCmd();
	if (!blasFenceValue)
		return 0;

	glRaytracingWaitFenceValue(blasFenceValue);

	for (size_t i = 0; i < builds.size(); ++i)
	{
		glRaytracingMeshRecord_t* mesh = builds[i].mesh;
		mesh->currentBlasIndex = builds[i].newBlasIndex;
		mesh->blasBuilt = 1;
		mesh->dirty = 0;
	}

	glRaytracingMarkAllWorldsNeedRebuild();
	return 1;
}

static int glRaytracingUploadMeshBuffers(glRaytracingMeshRecord_t* mesh)
{
	if (!mesh)
		return 0;

	if (mesh->verticesCpu.empty() || mesh->indicesCpu.empty())
		return 0;

	const UINT64 vbBytes = UINT64(mesh->verticesCpu.size()) * sizeof(glRaytracingVertex_t);
	const UINT64 ibBytes = UINT64(mesh->indicesCpu.size()) * sizeof(uint32_t);

	mesh->vertexBuffer = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		vbBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	mesh->indexBuffer = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		ibBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	if (!mesh->vertexBuffer.resource || !mesh->indexBuffer.resource)
		return 0;

	glRaytracingMapCopy(mesh->vertexBuffer.resource.Get(), mesh->verticesCpu.data(), (size_t)vbBytes);
	glRaytracingMapCopy(mesh->indexBuffer.resource.Get(), mesh->indicesCpu.data(), (size_t)ibBytes);

	return 1;
}

static int glRaytracingBuildMeshInternal(glRaytracingMeshRecord_t* mesh)
{
	if (!mesh)
		return 0;

	const int oldDirty = mesh->dirty;
	mesh->dirty = 1;
	const int ok = glRaytracingBuildDirtyMeshesInternal();
	if (!ok)
		mesh->dirty = oldDirty;
	return ok;
}

static int glRaytracingEnsureSceneUploadBuffer(glRaytracingRenderWorld_t* world, UINT64 requiredBytes)
{
	if (!world)
		return 0;

	glRaytracingSceneUploadBuffer_t* upload = glRaytracingGetBuildInstanceUpload(world);

	if (requiredBytes == 0)
		requiredBytes = D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT;

	requiredBytes = glRaytracingAlignUp(
		requiredBytes,
		D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);

	if (upload->buffer.resource &&
		upload->capacityBytes >= requiredBytes &&
		upload->mapped)
	{
		return 1;
	}

	if (upload->buffer.resource && upload->mapped)
		upload->buffer.resource->Unmap(0, nullptr);

	upload->mapped = nullptr;
	upload->buffer.resource.Reset();
	upload->capacityBytes = 0;

	upload->buffer = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		requiredBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	if (!upload->buffer.resource)
		return 0;

	void* mapped = nullptr;
	D3D12_RANGE readRange = {};
	if (FAILED(upload->buffer.resource->Map(0, &readRange, &mapped)) || !mapped)
	{
		upload->buffer.resource.Reset();
		return 0;
	}

	upload->mapped = (D3D12_RAYTRACING_INSTANCE_DESC*)mapped;
	upload->capacityBytes = requiredBytes;
	return 1;
}

static int glRaytracingBuildSceneInternal(glRaytracingRenderWorld_t* world)
{
	if (!world || !world->alive)
		return 0;

	UINT aliveCount = 0;
	int anyDirty = 0;
	int needsRebuild = world->tlasNeedsRebuild;
	int needsUpdate = world->tlasNeedsUpdate;

	for (size_t i = 0; i < world->instances.size(); ++i)
	{
		const glRaytracingInstanceRecord_t& inst = world->instances[i];
		if (!inst.alive)
			continue;

		++aliveCount;
		if (inst.dirty)
			anyDirty = 1;
	}

	if (aliveCount == 0)
	{
		world->activeInstanceIndices.clear();
		world->cpuInstanceDescs.clear();
		world->activeInstanceCount = 0;
		world->builtInstanceCount = 0;
		world->tlasBuilt = 0;
		world->tlasNeedsRebuild = 0;
		world->tlasNeedsUpdate = 0;
		return 1;
	}

	if (!world->tlasBuilt)
		needsRebuild = 1;

	if ((UINT)world->activeInstanceIndices.size() != world->builtInstanceCount)
		needsRebuild = 1;

	if (needsRebuild)
	{
		if (!glRaytracingRebuildActiveInstanceCache(world))
			return 0;
	}
	else
	{
		if (!needsUpdate && !anyDirty)
		{
			world->activeInstanceCount = (UINT)world->cpuInstanceDescs.size();
			return 1;
		}

		if (!glRaytracingRefreshDirtyInstanceCache(world))
		{
			world->tlasNeedsRebuild = 1;
			if (!glRaytracingRebuildActiveInstanceCache(world))
				return 0;
			needsRebuild = 1;
		}
	}

	const UINT activeCount = (UINT)world->cpuInstanceDescs.size();
	if (activeCount == 0)
	{
		world->activeInstanceCount = 0;
		world->builtInstanceCount = 0;
		world->tlasBuilt = 0;
		world->tlasNeedsRebuild = 0;
		world->tlasNeedsUpdate = 0;
		return 1;
	}

	if (!world->tlasBuilt || activeCount != world->builtInstanceCount)
		needsRebuild = 1;

	if (!glRaytracingUploadCachedInstanceDescs(world))
		return 0;

	glRaytracingSceneUploadBuffer_t* upload = glRaytracingGetBuildInstanceUpload(world);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.NumDescs = activeCount;
	inputs.InstanceDescs = upload->buffer.gpuVA;
	inputs.Flags =
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

	if (!glRaytracingEnsureTLASBuffers(world, &inputs))
		return 0;

	glRaytracingBuffer_t* dstTLAS = glRaytracingGetBuildTLASBuffer(world);
	const glRaytracingBuffer_t* srcTLAS = glRaytracingGetCurrentTLASBufferConst(world);

	glRaytracingWaitFenceValue(g_glRaytracingCmd.blasLastFenceValue);

	if (!glRaytracingBeginTlasCmd())
		return 0;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = world->tlasScratch.gpuVA;
	buildDesc.DestAccelerationStructureData = dstTLAS->gpuVA;
	buildDesc.SourceAccelerationStructureData = 0;

	if (!needsRebuild && world->tlasBuilt)
	{
		buildDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		buildDesc.SourceAccelerationStructureData = srcTLAS->gpuVA;
	}

	g_glRaytracingCmd.tlasCmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER uav = {};
	uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uav.UAV.pResource = dstTLAS->resource.Get();
	g_glRaytracingCmd.tlasCmdList->ResourceBarrier(1, &uav);

	const UINT64 tlasFenceValue = glRaytracingEndTlasCmd();
	if (!tlasFenceValue)
		return 0;

	glRaytracingWaitFenceValue(tlasFenceValue);

	world->currentTLASIndex = glRaytracingGetInactiveTLASIndex(world);
	world->activeInstanceCount = activeCount;
	world->builtInstanceCount = activeCount;
	world->tlasBuilt = 1;
	world->tlasNeedsRebuild = 0;
	world->tlasNeedsUpdate = 0;

	for (size_t i = 0; i < world->instances.size(); ++i)
	{
		if (world->instances[i].alive)
			world->instances[i].dirty = 0;
	}

	return 1;
}

static void glRaytracingInvalidateInstancesForMesh(uint32_t meshHandle, int deleteInstances)
{
	for (int w = 0; w < GL_RAYTRACING_MAX_RENDER_WORLDS; ++w)
	{
		glRaytracingRenderWorld_t& world = g_glRaytracingScene.worlds[w];
		if (!world.alive)
			continue;

		int touched = 0;
		for (size_t i = 0; i < world.instances.size(); ++i)
		{
			glRaytracingInstanceRecord_t& inst = world.instances[i];
			if (inst.alive && inst.descCpu.meshHandle == meshHandle)
			{
				glRaytracingInvalidateInstanceCache(&inst);
				inst.dirty = 1;
				touched = 1;

				if (deleteInstances)
				{
					inst.alive = 0;
					if (inst.handle < world.instanceHandleToIndex.size())
						world.instanceHandleToIndex[inst.handle] = -1;
				}
			}
		}

		if (touched)
			glRaytracingMarkWorldNeedsRebuild(&world);
	}
}

// ============================================================
// Scene public API
// ============================================================

int glRaytracingInit(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (g_glRaytracingScene.initialized)
		return 1;

	if (!glRaytracingInitCmdContext())
		return 0;

	g_glRaytracingScene.initialized = 1;

	glRaytracingLog("glRaytracingInit ok");
	return 1;
}

void glRaytracingShutdown(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized)
		return;

	glRaytracingClearAllSceneStateInternal();
	g_glRaytracingScene = glRaytracingSceneState_t();
	glRaytracingShutdownCmdContext();
}

void glRaytracingClear(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	glRaytracingClearAllSceneStateInternal();
}

glRaytracingSceneHandle_t glRaytracingCreateScene(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized)
		return 0;

	for (uint32_t i = 0; i < GL_RAYTRACING_MAX_RENDER_WORLDS; ++i)
	{
		glRaytracingRenderWorld_t& world = g_glRaytracingScene.worlds[i];
		if (!world.alive)
		{
			const uint32_t handle = i + 1;
			glRaytracingResetWorldSlot(&world, handle, 1);
			return handle;
		}
	}

	return 0;
}

void glRaytracingClearScene(glRaytracingSceneHandle_t worldHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return;

	glRaytracingClearWorldContents(world);
}

void glRaytracingDeleteScene(glRaytracingSceneHandle_t worldHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return;

	glRaytracingReleaseWorldResources(world);
	*world = glRaytracingRenderWorld_t();
}

uint32_t glRaytracingGetSceneCount(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	uint32_t count = 0;
	for (int i = 0; i < GL_RAYTRACING_MAX_RENDER_WORLDS; ++i)
	{
		if (g_glRaytracingScene.worlds[i].alive)
			++count;
	}
	return count;
}

glRaytracingMeshHandle_t glRaytracingCreateMesh(const glRaytracingMeshDesc_t* desc)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized || !desc)
		return 0;

	if (!desc->vertices || !desc->indices || desc->vertexCount == 0 || desc->indexCount == 0)
		return 0;

	glRaytracingMeshRecord_t mesh;
	mesh.handle = g_glRaytracingScene.nextMeshHandle++;
	mesh.alive = 1;
	mesh.descCpu = *desc;
	mesh.verticesCpu.assign(desc->vertices, desc->vertices + desc->vertexCount);
	mesh.indicesCpu.assign(desc->indices, desc->indices + desc->indexCount);
	mesh.descCpu.vertices = nullptr;
	mesh.descCpu.indices = nullptr;
	mesh.dirty = 1;

	g_glRaytracingScene.meshes.push_back(mesh);
	const size_t newIndex = g_glRaytracingScene.meshes.size() - 1;
	glRaytracingEnsureMeshHandleTable(mesh.handle);
	g_glRaytracingScene.meshHandleToIndex[mesh.handle] = (int)newIndex;

	return mesh.handle;
}

int glRaytracingUpdateMesh(glRaytracingMeshHandle_t meshHandle, const glRaytracingMeshDesc_t* desc)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized || !desc)
		return 0;

	glRaytracingMeshRecord_t* mesh = glRaytracingFindMesh(meshHandle);
	if (!mesh)
		return 0;

	if (!desc->vertices || !desc->indices || desc->vertexCount == 0 || desc->indexCount == 0)
		return 0;

	mesh->descCpu = *desc;
	mesh->verticesCpu.assign(desc->vertices, desc->vertices + desc->vertexCount);
	mesh->indicesCpu.assign(desc->indices, desc->indices + desc->indexCount);
	mesh->descCpu.vertices = nullptr;
	mesh->descCpu.indices = nullptr;

	mesh->vertexBuffer.resource.Reset();
	mesh->indexBuffer.resource.Reset();
	mesh->blasScratch.resource.Reset();
	mesh->blasResult[0].resource.Reset();
	mesh->blasResult[1].resource.Reset();
	mesh->blasScratchSize = 0;
	mesh->blasResultSize = 0;
	mesh->blasBuilt = 0;
	mesh->dirty = 1;
	mesh->currentBlasIndex = 0;

	glRaytracingInvalidateInstancesForMesh(meshHandle, 0);
	glRaytracingMarkAllWorldsNeedRebuild();

	return 1;
}

uint32_t glRaytracingGetMeshMaterialFlags(glRaytracingMeshHandle_t meshHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	const glRaytracingMeshRecord_t* mesh = glRaytracingFindMeshConst(meshHandle);
	if (!mesh)
		return 0;

	return mesh->materialFlags;
}

void glRaytracingSetMeshMaterialFlags(glRaytracingMeshHandle_t meshHandle, uint32_t materialFlags)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	glRaytracingMeshRecord_t* mesh = glRaytracingFindMesh(meshHandle);
	if (!mesh)
		return;

	materialFlags &= GL_RAYTRACING_INSTANCE_MATERIAL_MASK;
	if (mesh->materialFlags == materialFlags)
		return;

	mesh->materialFlags = materialFlags;

	// The glass bit changes whether the BLAS geometry is opaque, so force a full
	// BLAS rebuild. The TLAS is also rebuilt so InstanceID carries the material bit.
	mesh->blasBuilt = 0;
	mesh->dirty = 1;
	glRaytracingInvalidateInstancesForMesh(meshHandle, 0);
	glRaytracingMarkAllWorldsNeedRebuild();
}

void glRaytracingSetMeshGlass(glRaytracingMeshHandle_t meshHandle, int isGlass)
{
	uint32_t flags = glRaytracingGetMeshMaterialFlags(meshHandle);
	if (isGlass)
		flags |= GL_RAYTRACING_MATERIAL_FLAG_GLASS;
	else
		flags &= ~GL_RAYTRACING_MATERIAL_FLAG_GLASS;
	glRaytracingSetMeshMaterialFlags(meshHandle, flags);
}

void glRaytracingDeleteMesh(glRaytracingMeshHandle_t meshHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	glRaytracingMeshRecord_t* mesh = glRaytracingFindMesh(meshHandle);
	if (!mesh)
		return;

	glRaytracingInvalidateInstancesForMesh(meshHandle, 1);

	mesh->alive = 0;
	mesh->vertexBuffer.resource.Reset();
	mesh->indexBuffer.resource.Reset();
	mesh->blasScratch.resource.Reset();
	mesh->blasResult[0].resource.Reset();
	mesh->blasResult[1].resource.Reset();
	mesh->blasScratchSize = 0;
	mesh->blasResultSize = 0;
	mesh->blasBuilt = 0;
	mesh->dirty = 0;

	if (meshHandle < g_glRaytracingScene.meshHandleToIndex.size())
		g_glRaytracingScene.meshHandleToIndex[meshHandle] = -1;

	glRaytracingMarkAllWorldsNeedRebuild();
}

glRaytracingInstanceHandle_t glRaytracingCreateInstanceInScene(glRaytracingSceneHandle_t worldHandle, const glRaytracingInstanceDesc_t* desc)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized || !desc)
		return 0;

	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return 0;

	if (!glRaytracingFindMeshConst(desc->meshHandle))
		return 0;

	glRaytracingInstanceRecord_t inst;
	inst.handle = world->nextInstanceHandle++;
	inst.alive = 1;
	inst.descCpu = *desc;
	inst.dirty = 1;

	world->instances.push_back(inst);
	const size_t newIndex = world->instances.size() - 1;
	glRaytracingEnsureInstanceHandleTable(world, inst.handle);
	world->instanceHandleToIndex[inst.handle] = (int)newIndex;
	glRaytracingMarkWorldNeedsRebuild(world);
	return inst.handle;
}

int glRaytracingUpdateInstanceInScene(glRaytracingSceneHandle_t worldHandle, glRaytracingInstanceHandle_t instanceHandle, const glRaytracingInstanceDesc_t* desc)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized || !desc)
		return 0;

	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return 0;

	if (!glRaytracingFindMeshConst(desc->meshHandle))
		return 0;

	glRaytracingInstanceRecord_t* inst = glRaytracingFindInstance(world, instanceHandle);
	if (!inst)
		return 0;

	const uint32_t oldMeshHandle = inst->descCpu.meshHandle;

	inst->descCpu = *desc;
	inst->dirty = 1;

	if (oldMeshHandle != desc->meshHandle)
	{
		glRaytracingInvalidateInstanceCache(inst);
		glRaytracingMarkWorldNeedsRebuild(world);
	}
	else
	{
		glRaytracingMarkWorldNeedsUpdate(world);
	}

	return 1;
}

void glRaytracingDeleteInstanceInScene(glRaytracingSceneHandle_t worldHandle, glRaytracingInstanceHandle_t instanceHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return;

	glRaytracingInstanceRecord_t* inst = glRaytracingFindInstance(world, instanceHandle);
	if (!inst)
		return;

	glRaytracingInvalidateInstanceCache(inst);
	inst->alive = 0;
	if (instanceHandle < world->instanceHandleToIndex.size())
		world->instanceHandleToIndex[instanceHandle] = -1;
	glRaytracingMarkWorldNeedsRebuild(world);
}

int glRaytracingBuildMesh(glRaytracingMeshHandle_t meshHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized)
		return 0;

	glRaytracingMeshRecord_t* mesh = glRaytracingFindMesh(meshHandle);
	if (!mesh)
		return 0;

	return glRaytracingBuildMeshInternal(mesh);
}

int glRaytracingBuildAllMeshes(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized)
		return 0;

	return glRaytracingBuildDirtyMeshesInternal();
}

int glRaytracingBuildSceneForHandle(glRaytracingSceneHandle_t worldHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized)
		return 0;

	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return 0;

	if (!glRaytracingBuildDirtyMeshesInternal())
		return 0;

	return glRaytracingBuildSceneInternal(world);
}

ID3D12Resource* glRaytracingGetTopLevelASForScene(glRaytracingSceneHandle_t worldHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingScene.initialized)
		return nullptr;

	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return nullptr;

	if (!glRaytracingBuildDirtyMeshesInternal())
		return nullptr;

	if (!glRaytracingBuildSceneInternal(world))
		return nullptr;

	if (!world->tlasBuilt)
		return nullptr;

	return glRaytracingGetCurrentTLASBuffer(world)->resource.Get();
}

uint32_t glRaytracingGetMeshCount(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	uint32_t count = 0;
	for (size_t i = 0; i < g_glRaytracingScene.meshes.size(); ++i)
	{
		if (g_glRaytracingScene.meshes[i].alive)
			++count;
	}
	return count;
}

uint32_t glRaytracingGetInstanceCountForScene(glRaytracingSceneHandle_t worldHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	const glRaytracingRenderWorld_t* world = glRaytracingFindWorldConst(worldHandle);
	return glRaytracingCountAliveInstances(world);
}

// ============================================================
// Lighting state
// ============================================================

struct glRaytracingLightingConstants_t
{
	float invViewProj[16];
	float invViewMatrix[16];
	float viewProj[16];
	float cameraPos[4];
	float ambientColor[4];
	float screenSize[4];

	// Keep this CPU layout 16-byte aligned with the HLSL cbuffer.
	float normalReconstructZ;
	uint32_t lightCount;
	uint32_t enableSpecular;
	uint32_t enableHalfLambert;

	float shadowBias;
	uint32_t frameIndex;
	uint32_t samplesPerPixel;
	uint32_t maxBounces;

	uint32_t enableDenoiser;
	uint32_t denoisePassIndex;
	float denoiseStepWidth;
	float denoiseStrength;

	float denoisePhiColor;
	float denoisePhiNormal;
	float denoisePhiPosition;
	float bumpStrength;
};

struct glRaytracingLightingState_t
{
	std::vector<glRaytracingLight_t> cpuLights;
	glRaytracingLightingConstants_t constants;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	UINT descriptorStride;

	glRaytracingBuffer_t constantBuffer;
	glRaytracingBuffer_t lightBuffer;

	ComPtr<ID3D12RootSignature> globalRootSig;
	ComPtr<ID3D12RootSignature> localRootSig;

	ComPtr<ID3D12StateObject> rtStateObject;
	ComPtr<ID3D12StateObjectProperties> rtStateProps;

	glRaytracingBuffer_t raygenTable;
	glRaytracingBuffer_t missTable;
	glRaytracingBuffer_t hitTable;

	ComPtr<ID3D12PipelineState> denoisePSO;
	glRaytracingTexture_t pathTraceTexture;
	glRaytracingTexture_t denoiseTemp[2];
	glRaytracingBuffer_t denoiseConstantBuffer[3];
	UINT denoiseWidth;
	UINT denoiseHeight;
	DXGI_FORMAT denoiseFormat;
	uint32_t frameCounter;
	bool externalDenoiser;
	bool initialized;

	glRaytracingLightingState_t()
	{
		memset(&constants, 0, sizeof(constants));
		descriptorStride = 0;
		denoiseWidth = 0;
		denoiseHeight = 0;
		denoiseFormat = DXGI_FORMAT_UNKNOWN;
		frameCounter = 0;
		externalDenoiser = false;
		initialized = false;
	}
};

static glRaytracingLightingState_t g_glRaytracingLighting;

static const DXGI_FORMAT GL_RAYTRACING_DENOISE_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;

enum glRaytracingLightingDescriptorIndex_t
{
	GLR_DESC_LIGHTS_SRV = 0,
	GLR_DESC_ALBEDO_SRV = 1,
	GLR_DESC_DEPTH_SRV = 2,
	GLR_DESC_NORMAL_SRV = 3,
	GLR_DESC_POSITION_SRV = 4,
	GLR_DESC_TLAS_SRV = 5,
	GLR_DESC_PATHTRACE_SRV = 6,
	GLR_DESC_DENOISE_A_SRV = 7,
	GLR_DESC_DENOISE_B_SRV = 8,
	GLR_DESC_PATHTRACE_UAV = 9,
	GLR_DESC_DENOISE_A_UAV = 10,
	GLR_DESC_DENOISE_B_UAV = 11,
	GLR_DESC_OUTPUT_UAV = 12,
	GLR_DESC_COUNT = 13,
	GLR_DESC_SRV_COUNT = 9,
	GLR_DESC_UAV_COUNT = 4
};

static void glRaytracingLightingResetDenoiseHistory(void)
{
	// Kept under the old name so existing call sites keep compiling. The denoiser
	// below is no longer a temporal history blend; this only restarts stochastic
	// sample indexing after material/camera/light changes.
	g_glRaytracingLighting.frameCounter = 0;
	g_glRaytracingLighting.constants.frameIndex = 0;
}

static const char* g_glRaytracingLightingHlsl = R"(
struct Light
{
    float3 position;
    float  radius;

    float3 color;
    float  intensity;

    float3 normal;
    uint   type;

    float3 axisU;
    float  halfWidth;

    float3 axisV;
    float  halfHeight;

    uint   samples;
    uint   twoSided;
    float  persistant;
    float  pad1;

    // For point lights, this is the axis-aligned XYZ attenuation radius.
    // For spot lights, pointRadius.x stores the near clip plane.
    // The scalar radius above is still kept as a max/fallback range for point lights,
    // as the influence range for rect lights, and as the far clip distance for spot lights.
    float3 pointRadius;
    float  pointRadiusPad; // non-zero disables specular for this light
};

struct ShadowPayload
{
    uint hit;
};

struct BouncePayload
{
    uint  hit;
    float hitT;
    uint  materialFlags;
    uint  pad0;
};

cbuffer LightingCB : register(b0)
{
    float4x4 gInvViewProj;
    float4x4 gInvViewMatrix;
    float4x4 gViewProj;
    float4   gCameraPos;
    float4   gAmbientColor;
    float4   gScreenSize;
    float    gNormalReconstructZ;
    uint     gLightCount;
    uint     gEnableSpecular;
    uint     gEnableHalfLambert;
    float    gShadowBias;
    uint     gFrameIndex;
    uint     gSamplesPerPixel;
    uint     gMaxBounces;
    uint     gEnableDenoiser;
    uint     gDenoisePassIndex;
    float    gDenoiseStepWidth;
    float    gDenoiseStrength;
    float    gDenoisePhiColor;
    float    gDenoisePhiNormal;
    float    gDenoisePhiPosition;
    float    gBumpStrength;
};

StructuredBuffer<Light> gLights : register(t0);
Texture2D<float4>       gAlbedoTex   : register(t1);
Texture2D<float>        gDepthTex    : register(t2);
Texture2D<float4>       gNormalTex   : register(t3);
Texture2D<float4>       gPositionTex : register(t4);
RaytracingAccelerationStructure gSceneBVH : register(t5);
RWTexture2D<float4>     gOutputTex   : register(u0);

static const uint GL_RAYTRACING_LIGHT_TYPE_POINT = 0;
static const uint GL_RAYTRACING_LIGHT_TYPE_RECT  = 1;
static const uint GL_RAYTRACING_LIGHT_TYPE_SPOT  = 2;

static const uint GEOMETRY_FLAG_NONE     = 0;
static const uint GEOMETRY_FLAG_SKELETAL = 1;
static const uint GEOMETRY_FLAG_UNLIT    = 2;
static const uint GEOMETRY_FLAG_GLASS    = 4;

// The shim encodes per-instance material flags into the upper bits of
// D3D12_RAYTRACING_INSTANCE_DESC::InstanceID so any-hit shaders can make
// visibility decisions without binding a separate material table.
static const uint GL_RAYTRACING_INSTANCE_USER_ID_MASK       = 0x0000FFFFu;
static const uint GL_RAYTRACING_INSTANCE_MATERIAL_SHIFT     = 16u;
static const uint GL_RAYTRACING_INSTANCE_MATERIAL_MASK      = 0x000000FFu;
static const uint GL_RAYTRACING_MATERIAL_FLAG_GLASS         = 0x00000001u;

uint DecodeInstanceMaterialFlags()
{
    return (InstanceID() >> GL_RAYTRACING_INSTANCE_MATERIAL_SHIFT) &
        GL_RAYTRACING_INSTANCE_MATERIAL_MASK;
}

bool CurrentRayHitIsGlass()
{
    return (DecodeInstanceMaterialFlags() & GL_RAYTRACING_MATERIAL_FLAG_GLASS) != 0u;
}

uint DecodeGeometryFlag(float geoFlag)
{
    // position.w comes from a render target / buffer path, so do not require exact
    // float equality. Values like 0.999, 1.001, 1.99, 2.02 should decode correctly.
    //
    // Clamp negative garbage to 0, then round to nearest integer flag.
    float f = max(geoFlag, 0.0);
    return (uint)floor(f + 0.5);
}

bool GeometryFlagEquals(float geoFlag, uint expectedFlag)
{
    return DecodeGeometryFlag(geoFlag) == expectedFlag;
}

bool GeometryFlagHas(float geoFlag, uint expectedFlag)
{
    // Supports both current single-value usage and future bitmask usage.
    uint decoded = DecodeGeometryFlag(geoFlag);
    return (decoded & expectedFlag) != 0u;
}

float3 LoadScenePosition(uint2 pixel)
{
    float4 p = gPositionTex.Load(int3(pixel, 0));
    return p.xyz;
}

float4 LoadSceneNormal(uint2 pixel)
{
    float4 nSample = gNormalTex.Load(int3(pixel, 0));
    return nSample;
}

float SafeLengthSq(float3 v)
{
    return max(dot(v, v), 1e-8);
}

float3 SafeNormalizeLocal(float3 v, float3 fallback)
{
    float lenSq = dot(v, v);
    return (lenSq > 1e-8) ? (v * rsqrt(lenSq)) : fallback;
}

float BumpLuminance(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}

float3 EnhanceBumpNormal(uint2 pixel, float3 worldPos, float3 baseAlbedo)
{
    float3 N = SafeNormalizeLocal(LoadSceneNormal(pixel).xyz, float3(0.0, 0.0, 1.0));

    float strength = max(gBumpStrength, 0.0);
    if (strength <= 0.001)
        return N;

    int2 p = int2(pixel);
    int2 maxP = int2((int)gScreenSize.x - 1, (int)gScreenSize.y - 1);

    int2 pxm = clamp(p + int2(-1,  0), int2(0, 0), maxP);
    int2 pxp = clamp(p + int2( 1,  0), int2(0, 0), maxP);
    int2 pym = clamp(p + int2( 0, -1), int2(0, 0), maxP);
    int2 pyp = clamp(p + int2( 0,  1), int2(0, 0), maxP);

    float3 posL = gPositionTex.Load(int3(pxm, 0)).xyz;
    float3 posR = gPositionTex.Load(int3(pxp, 0)).xyz;
    float3 posU = gPositionTex.Load(int3(pym, 0)).xyz;
    float3 posD = gPositionTex.Load(int3(pyp, 0)).xyz;

    float3 T = posR - posL;
    float3 B = posD - posU;

    // Fall back to a stable tangent basis when the position buffer is flat or invalid.
    if (dot(T, T) <= 1e-8 || dot(B, B) <= 1e-8)
    {
        float3 up = (abs(N.z) < 0.999) ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
        T = SafeNormalizeLocal(cross(up, N), float3(1.0, 0.0, 0.0));
        B = cross(N, T);
    }
    else
    {
        T = SafeNormalizeLocal(T - N * dot(N, T), float3(1.0, 0.0, 0.0));
        B = SafeNormalizeLocal(B - N * dot(N, B), float3(0.0, 1.0, 0.0));
    }

    float hL = BumpLuminance(saturate(gAlbedoTex.Load(int3(pxm, 0)).rgb));
    float hR = BumpLuminance(saturate(gAlbedoTex.Load(int3(pxp, 0)).rgb));
    float hU = BumpLuminance(saturate(gAlbedoTex.Load(int3(pym, 0)).rgb));
    float hD = BumpLuminance(saturate(gAlbedoTex.Load(int3(pyp, 0)).rgb));

    // Height-gradient bump from the diffuse texture.  The scale is intentionally
    // aggressive because the current renderer has no dedicated height/normal map
    // slot here, and old idTech/Build textures need the fake relief to read.
    float dhdx = (hR - hL);
    float dhdy = (hD - hU);
    float3 heightNormal = SafeNormalizeLocal(N - (T * dhdx + B * dhdy) * (strength * 4.25), N);

    float3 nL = SafeNormalizeLocal(gNormalTex.Load(int3(pxm, 0)).xyz, N);
    float3 nR = SafeNormalizeLocal(gNormalTex.Load(int3(pxp, 0)).xyz, N);
    float3 nU = SafeNormalizeLocal(gNormalTex.Load(int3(pym, 0)).xyz, N);
    float3 nD = SafeNormalizeLocal(gNormalTex.Load(int3(pyp, 0)).xyz, N);
    float3 avgN = SafeNormalizeLocal((nL + nR + nU + nD) * 0.25, N);

    // Amplify real G-buffer normal-map variation as well.
    float3 detailN = SafeNormalizeLocal(N + (N - avgN) * (strength * 1.75), N);

    float3 outN = SafeNormalizeLocal(lerp(detailN, heightNormal, 0.65), N);

    // Avoid flipping normals so far that shadows/specular explode.
    if (dot(outN, N) < 0.25)
        outN = SafeNormalizeLocal(lerp(N, outN, 0.45), N);

    return outN;
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload)
{
    payload.hit = 0;
}

[shader("anyhit")]
void ShadowAnyHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Glass should participate in the primary/raster image, but visibility rays
    // must continue through it.  Mark glass BLAS geometry non-opaque on the CPU
    // side so this any-hit shader runs, then IgnoreHit() lets the ray keep going
    // to whatever is behind the pane.
    if (CurrentRayHitIsGlass())
    {
        IgnoreHit();
        return;
    }
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Safety fallback for incorrectly-built glass geometry. Correct glass meshes
    // are non-opaque and are ignored by ShadowAnyHit() above.
    if (CurrentRayHitIsGlass())
    {
        payload.hit = 0;
        return;
    }

    payload.hit = 1;
}

[shader("miss")]
void BounceMiss(inout BouncePayload payload)
{
    payload.hit = 0;
    payload.hitT = 0.0;
    payload.materialFlags = 0;
    payload.pad0 = 0;
}

[shader("anyhit")]
void BounceAnyHit(inout BouncePayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Secondary diffuse rays should see through the same glass that shadow rays
    // see through.  This keeps a glass pane from killing all bounced light behind it.
    if (CurrentRayHitIsGlass())
    {
        IgnoreHit();
        return;
    }
}

[shader("closesthit")]
void BounceClosestHit(inout BouncePayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Fallback for glass geometry that was accidentally built opaque.  Correctly
    // tagged glass is ignored in BounceAnyHit() above.
    if (CurrentRayHitIsGlass())
    {
        payload.hit = 0;
        payload.hitT = 0.0;
        payload.materialFlags = DecodeInstanceMaterialFlags();
        payload.pad0 = 0;
        return;
    }

    payload.hit = 1;
    payload.hitT = RayTCurrent();
    payload.materialFlags = DecodeInstanceMaterialFlags();
    payload.pad0 = 0;
}

float TraceShadow(float3 origin, float3 dir, float maxT)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.001;
    ray.TMax = maxT;

    ShadowPayload payload;
    payload.hit = 0;

    TraceRay(
        gSceneBVH,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_FRONT_FACING_TRIANGLES,
        0xFF,
        0,
        0,
        0,
        ray,
        payload);

    return (payload.hit != 0) ? 0.0 : 1.0;
}

bool TraceBounce(float3 origin, float3 dir, float maxT, out float hitT, out uint materialFlags)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.001;
    ray.TMax = maxT;

    BouncePayload payload;
    payload.hit = 0;
    payload.hitT = 0.0;
    payload.materialFlags = 0;
    payload.pad0 = 0;

    TraceRay(
        gSceneBVH,
        RAY_FLAG_NONE,
        0xFF,
        1,
        0,
        1,
        ray,
        payload);

    hitT = payload.hitT;
    materialFlags = payload.materialFlags;
    return payload.hit != 0;
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hammersley2D(uint i, uint N, float rand)
{
    float e1 = frac((float)i / (float)N + rand);

    uint bits = i;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);

    float e2 = (float)bits * 2.3283064365386963e-10;
    return float2(e1, e2);
}

float2 ConcentricSampleDisk(float2 u)
{
    float2 uOffset = 2.0 * u - 1.0;

    if (uOffset.x == 0.0 && uOffset.y == 0.0)
        return float2(0.0, 0.0);

    float r, theta;
    if (abs(uOffset.x) > abs(uOffset.y))
    {
        r = uOffset.x;
        theta = (3.14159265 / 4.0) * (uOffset.y / uOffset.x);
    }
    else
    {
        r = uOffset.y;
        theta = (3.14159265 / 2.0) - (3.14159265 / 4.0) * (uOffset.x / uOffset.y);
    }

    return r * float2(cos(theta), sin(theta));
}

void BuildOrthonormalBasis(float3 n, out float3 t, out float3 b)
{
    float3 up = (abs(n.z) < 0.999) ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    t = normalize(cross(up, n));
    b = cross(n, t);
}

float3 CosineSampleHemisphere(float2 u)
{
    float2 d = ConcentricSampleDisk(u);
    float z = sqrt(saturate(1.0 - dot(d, d)));
    return float3(d.x, d.y, z);
}

float3 GetPointLightRadius(Light Lgt)
{
    float scalarRadius = max(abs(Lgt.radius), 1e-4);
    float3 r = abs(Lgt.pointRadius);

    // Allow older/zero-initialized light records to behave like the old scalar radius.
    if (max(max(r.x, r.y), r.z) <= 1e-4)
    {
        r = float3(scalarRadius, scalarRadius, scalarRadius);
    }

    return max(r, float3(1e-4, 1e-4, 1e-4));
}

float GetPointLightMaxRadius(Light Lgt)
{
    float3 r = GetPointLightRadius(Lgt);
    return max(max(r.x, r.y), r.z);
}

float3 Doom3SafeNormalizeOr(float3 v, float3 fallbackDir)
{
    float lenSq = dot(v, v);
    return (lenSq > 1e-8) ? (v * rsqrt(lenSq)) : fallbackDir;
}

float Doom3QuadraticFalloffImage(float texCoord)
{
    // Math version of Doom 3 BFG's built-in _quadratic lookup table.
    // The source table is 32 texels wide, brightest at the center and clamped
    // to black outside the light volume.
    if (texCoord <= 0.0 || texCoord >= 1.0)
        return 0.0;

    const float QUADRATIC_WIDTH = 32.0;

    // Convert a normalized lookup coordinate to the source generator's texel-space
    // x value, then apply the same centered squared ramp used by R_QuadraticImage().
    float x = texCoord * QUADRATIC_WIDTH - 0.5;
    float d = x - (QUADRATIC_WIDTH * 0.5 - 0.5);
    d = abs(d);
    d -= 0.5;
    d /= (QUADRATIC_WIDTH * 0.5);
    d = 1.0 - d;
    d = saturate(d);
    return d * d;
}

float Doom3QuadraticCentered(float centeredCoord)
{
    // centeredCoord is -1 at one side of the light volume, 0 at the light center,
    // and +1 at the opposite side.
    return Doom3QuadraticFalloffImage(centeredCoord * 0.5 + 0.5);
}

float Doom3ProjectionTexture2D(float2 centeredCoord)
{
    // Doom 3 multiplies a projected light image by a separate falloff image. This
    // renderer does not bind Doom light materials/cookies, so use the same built-in
    // quadratic shape on S/T as a neutral default projection texture approximation.
    if (abs(centeredCoord.x) >= 1.0 || abs(centeredCoord.y) >= 1.0)
        return 0.0;

    return Doom3QuadraticCentered(centeredCoord.x) * Doom3QuadraticCentered(centeredCoord.y);
}

float Doom3ProjectedDepthFalloff(float depth, float nearClip, float farClip)
{
    // For spot/projected lights, the renderer API supplies a conventional near/far
    // range.  Sample the bright-to-far half of Doom's centered falloff image so the
    // light is strongest at the projector and fades out at the far plane.
    float depth01 = saturate((depth - nearClip) / max(farClip - nearClip, 1e-4));
    return Doom3QuadraticFalloffImage(0.5 + depth01 * 0.5);
}

float ComputePointLightAttenuation(float3 worldPos, Light Lgt)
{
    float3 radii = GetPointLightRadius(Lgt);
    float3 offset = worldPos - Lgt.position;

    // Doom 3 point lights are box/projector lights, not inverse-square or
    // ellipsoidal distance lights. S/T sample the projected light image and the
    // third axis samples lightFalloffImage.  Use the light axes when provided so
    // rectangular radii behave like idTech4 light volumes.
    float3 axisU = Doom3SafeNormalizeOr(Lgt.axisU, float3(1.0, 0.0, 0.0));
    float3 axisV = Doom3SafeNormalizeOr(Lgt.axisV, float3(0.0, 1.0, 0.0));
    float3 axisW = Doom3SafeNormalizeOr(Lgt.normal, float3(0.0, 0.0, 1.0));

    float u = dot(offset, axisU) / radii.x;
    float v = dot(offset, axisV) / radii.y;
    float w = dot(offset, axisW) / radii.z;

    if (abs(u) >= 1.0 || abs(v) >= 1.0 || abs(w) >= 1.0)
        return 0.0;

    float projection = Doom3ProjectionTexture2D(float2(u, v));
    float falloff    = Doom3QuadraticCentered(w);

    return projection * falloff;
}
)"
R"(
float ComputeSpotLightAttenuation(float3 worldPos, Light Lgt)
{
    float3 lightToSurface = worldPos - Lgt.position;

    float nearClip = max(Lgt.pointRadius.x, 0.0);
    float farClip  = max(Lgt.radius, nearClip + 1e-4);

    float3 spotDir = Doom3SafeNormalizeOr(Lgt.normal, float3(0.0, 0.0, 1.0));
    float depth = dot(lightToSurface, spotDir);
    if (depth <= nearClip || depth >= farClip)
        return 0.0;

    float3 axisU = Doom3SafeNormalizeOr(Lgt.axisU, float3(1.0, 0.0, 0.0));
    float3 axisV = Doom3SafeNormalizeOr(Lgt.axisV, float3(0.0, 1.0, 0.0));

    float invDepth = 1.0 / max(depth, 1e-4);
    float projU = dot(lightToSurface, axisU) * invDepth;
    float projV = dot(lightToSurface, axisV) * invDepth;

    float halfU = max(abs(Lgt.halfWidth), 1e-4);
    float halfV = max(abs(Lgt.halfHeight), 1e-4);

    float signedU = projU / halfU;
    float signedV = projV / halfV;

    if (abs(signedU) >= 1.0 || abs(signedV) >= 1.0)
        return 0.0;

    float projection = Doom3ProjectionTexture2D(float2(signedU, signedV));
    float falloff    = Doom3ProjectedDepthFalloff(depth, nearClip, farClip);

    return projection * falloff;
}

float TraceSpotShadow(float3 worldPos, float3 N, float3 toLight, float dist)
{
    float3 L = toLight / max(dist, 1e-6);

    float NdotLRaw   = saturate(dot(N, L));
    float normalBias = lerp(gShadowBias * 3.0, gShadowBias * 0.75, NdotLRaw);

    float3 shadowOrigin = worldPos + N * normalBias + L * (gShadowBias * 0.5);
    float  shadowTMax   = max(dist - gShadowBias * 0.5, 0.001);

    return TraceShadow(shadowOrigin, L, shadowTMax);
}

float TraceSoftShadow(float3 worldPos, float3 N, Light Lgt, float3 toLight, float dist)
{
    const uint SHADOW_SAMPLES = 12;

    float3 L = toLight / max(dist, 1e-6);

    float3 tangent, bitangent;
    BuildOrthonormalBasis(L, tangent, bitangent);

    float areaRadius = max(GetPointLightMaxRadius(Lgt) * 0.03, 0.12);

    float shadowAccum = 0.0;
    float rand = Hash12(worldPos.xy + float2(worldPos.z, dist));

    [unroll]
    for (uint s = 0; s < SHADOW_SAMPLES; ++s)
    {
        float2 xi = Hammersley2D(s, SHADOW_SAMPLES, rand);
        float2 d  = ConcentricSampleDisk(xi) * areaRadius;

        float3 sampleLightPos = Lgt.position + tangent * d.x + bitangent * d.y;
        float3 sampleVec      = sampleLightPos - worldPos;
        float  sampleDist     = length(sampleVec);

        if (sampleDist <= 1e-4)
        {
            shadowAccum += 1.0;
            continue;
        }

        float3 sampleDir = sampleVec / sampleDist;

        float NdotLRaw   = saturate(dot(N, sampleDir));
        float normalBias = lerp(gShadowBias * 3.0, gShadowBias * 0.75, NdotLRaw);

        float3 shadowOrigin = worldPos + N * normalBias + sampleDir * (gShadowBias * 0.5);
        float  shadowTMax   = max(sampleDist - gShadowBias * 0.5, 0.001);

        shadowAccum += TraceShadow(shadowOrigin, sampleDir, shadowTMax);
    }

    return shadowAccum / (float)SHADOW_SAMPLES;
}
)"
R"(
float RectLightShadow(float3 worldPos, float3 N, Light Lgt, uint2 pixel)
{
    uint sampleCount = max(Lgt.samples, 1u);
    sampleCount = min(sampleCount, 16u);

    float visibility = 0.0;

    float rand = Hash12((float2)pixel + worldPos.xy + float2(worldPos.z, dot(N.xy, N.xy)));

    float NoL_center = saturate(dot(N, normalize(Lgt.position - worldPos)));
    float normalBias = lerp(gShadowBias * 4.0, gShadowBias * 0.75, NoL_center);
    float3 baseOrigin = worldPos + N * normalBias;

    [loop]
    for (uint s = 0; s < sampleCount; ++s)
    {
        float2 xi = Hammersley2D(s, sampleCount, rand);
        float2 uv = xi * 2.0 - 1.0;

        float3 sampleLightPos =
            Lgt.position +
            Lgt.axisU * (uv.x * Lgt.halfWidth) +
            Lgt.axisV * (uv.y * Lgt.halfHeight);

        float3 toLight = sampleLightPos - baseOrigin;
        float distToLight = length(toLight);

        if (distToLight <= 1e-4)
        {
            visibility += 1.0;
            continue;
        }

        float3 L = toLight / distToLight;

        float NdotL = dot(N, L);
        if (NdotL <= 0.0)
        {
            continue;
        }

        float emitTerm = (Lgt.twoSided != 0)
            ? abs(dot(Lgt.normal, -L))
            : dot(Lgt.normal, -L);

        if (emitTerm <= 0.0)
        {
            continue;
        }

        float3 shadowOrigin = baseOrigin + L * (gShadowBias * 0.5);
        float shadowTMax = max(distToLight - gShadowBias, 0.001);

        visibility += TraceShadow(shadowOrigin, L, shadowTMax);
    }

    return visibility / (float)sampleCount;
}

float ComputeAmbientOcclusion(float3 worldPos, float3 N, uint2 pixel)
{
    const uint AO_SAMPLES = 24;
    const float AO_RADIUS = 32.0;

    float3 tangent, bitangent;
    BuildOrthonormalBasis(N, tangent, bitangent);

    float rand = Hash12((float2)pixel + worldPos.xy + worldPos.zz);

    float visibility = 0.0;

    [unroll]
    for (uint i = 0; i < AO_SAMPLES; ++i)
    {
        float2 xi = Hammersley2D(i, AO_SAMPLES, rand);
        float3 h  = CosineSampleHemisphere(xi);

        float3 aoDir =
            tangent   * h.x +
            bitangent * h.y +
            N         * h.z;

        aoDir = normalize(aoDir);

        float3 aoOrigin = worldPos + N * (gShadowBias * 0.15);

        visibility += TraceShadow(aoOrigin, aoDir, AO_RADIUS);
    }

    visibility /= (float)AO_SAMPLES;
    visibility = saturate(pow(visibility, 1.5));

    return visibility;
}

float3 GetSkyLightDirection10AM()
{
    // Direction from the shaded point TO the sky/sun.
    // 10:00 AM style: angled, not straight vertical.
    //
    // Flip X/Y signs if you want the shadows cast the opposite horizontal way.
    return normalize(float3(-0.55, -0.25, 0.80));
}
)"
R"(
float ComputeSkyVisibility(float3 worldPos, float3 N, uint2 pixel)
{
    const uint  SKY_SAMPLES = 12;
    const float SKY_TMAX    = 1000000.0;

    // Soft angular size of the sky/sun shadow cone.
    // Larger = softer shadows, but more chance of light leaking.
    const float SKY_SOFTNESS = 0.085;

    float3 skyCenterDir = GetSkyLightDirection10AM();

    float NoSky = dot(N, skyCenterDir);

    // Mostly back-facing relative to the sky direction.
    // Return black visibility instead of casting unstable grazing rays.
    if (NoSky <= -0.35)
    {
        return 0.0;
    }

    float3 tangent, bitangent;
    BuildOrthonormalBasis(skyCenterDir, tangent, bitangent);

    float normalBias = lerp(gShadowBias * 4.0, gShadowBias * 1.0, saturate(NoSky));

    float3 baseOrigin =
        worldPos +
        N * normalBias +
        skyCenterDir * (gShadowBias * 2.0);

    float visibility = 0.0;

    // IMPORTANT:
    // No per-pixel random rotation here.
    // The old noise came from random hemisphere sky sampling.
    // This keeps the soft shadow sampling pattern stable per pixel/frame.
    [unroll]
    for (uint i = 0; i < SKY_SAMPLES; ++i)
    {
        float2 xi = Hammersley2D(i, SKY_SAMPLES, 0.0);
        float2 d  = ConcentricSampleDisk(xi) * SKY_SOFTNESS;

        float3 skyDir = normalize(
            skyCenterDir +
            tangent   * d.x +
            bitangent * d.y);

        // Do not shoot rays below the world horizon.
        if (skyDir.z <= 0.02)
        {
            visibility += 0.0;
            continue;
        }

        float sampleFacing = dot(N, skyDir);

        // Avoid very noisy grazing rays on back-facing surfaces.
        if (sampleFacing <= -0.35)
        {
            visibility += 0.0;
            continue;
        }

        float3 skyOrigin =
            worldPos +
            N * normalBias +
            skyDir * (gShadowBias * 2.0);

        visibility += TraceShadow(skyOrigin, skyDir, SKY_TMAX);
    }

    visibility /= (float)SKY_SAMPLES;

    // Slightly smooth the binary ray result so it does not look harsh.
    return saturate(visibility);
}

float ComputeCavity(uint2 pixel, float3 worldPos, float3 N)
{
    static const int2 taps[12] =
    {
        int2(-2,  0), int2( 2,  0),
        int2( 0, -2), int2( 0,  2),
        int2(-2, -2), int2( 2, -2),
        int2(-2,  2), int2( 2,  2),
        int2(-4,  0), int2( 4,  0),
        int2( 0, -4), int2( 0,  4)
    };

    float accum = 0.0;
    float weightSum = 0.0;

    [unroll]
    for (int i = 0; i < 12; ++i)
    {
        int2 sp = int2(pixel) + taps[i];

        if (sp.x < 0 || sp.y < 0 || sp.x >= (int)gScreenSize.x || sp.y >= (int)gScreenSize.y)
            continue;

        float3 samplePos = gPositionTex.Load(int3(sp, 0)).xyz;
        float3 sampleN   = normalize(gNormalTex.Load(int3(sp, 0)).xyz);

        float3 d = samplePos - worldPos;
        float distSq = dot(d, d);

        if (distSq > (24.0 * 24.0))
            continue;

        float nd = dot(N, sampleN);
        if (nd < 0.65)
            continue;

        float curvature = 1.0 - saturate(nd);
        float w = 1.0 / (1.0 + distSq * 0.02);

        accum += curvature * w;
        weightSum += w;
    }

    float cavity = (weightSum > 0.0) ? (accum / weightSum) : 0.0;
    cavity = saturate(cavity * 2.0);

    return 1.0 - cavity * 0.18;
}

float Doom3SpecularLookup(float x)
{
    // Doom 3 used a lookup table for specular falloff.
    // This approximates the classic broad idTech4 highlight.
    x = saturate(x);

    // Broad enough to read like Doom 3 plastic/metal,
    // not razor-sharp PBR.
    return pow(x, 16.0);
}

float3 Doom3PseudoSpecularMask(float3 baseAlbedo)
{
    // Doom 3 normally uses a dedicated specular map.
    // This pass does not have one bound, so DO NOT square diffuse albedo.
    // Squaring albedo makes dark Doom 3 textures lose all specular response.
    //
    // Use luminance only as a weak hint, with a neutral floor.
    float lum = dot(saturate(baseAlbedo), float3(0.299, 0.587, 0.114));

    float specStrength = lerp(0.22, 0.72, saturate(lum * 1.35));

    // Slight warm/colored contribution from the diffuse texture, but mostly neutral
    // like a missing/default specular map.
    float3 neutralSpec = float3(specStrength, specStrength, specStrength);
    float3 tintedSpec  = saturate(baseAlbedo) * 0.35 + neutralSpec * 0.65;

    return max(tintedSpec, float3(0.18, 0.18, 0.18));
}

float3 ComputeSpecular(
    float3 N,
    float3 V,
    float3 L,
    float3 lightColor,
    float lightIntensity,
    float atten,
    float shadow,
    float3 baseAlbedo)
{
    if (gEnableSpecular == 0)
        return 0.0;

    N = normalize(N);
    V = normalize(V);
    L = normalize(L);

    float NdotL = dot(N, L);
    float NdotV = dot(N, V);

    // Doom 3 specular should not show on the wrong side of the surface,
    // but once it passes this gate, do not multiply the final spec by NdotL again.
    // The old code did that and made highlights collapse too aggressively.
    if (NdotL <= 0.0 || NdotV <= 0.0 || atten <= 0.0 || shadow <= 0.0)
        return 0.0;

    // idTech4/Doom 3 interaction shader uses half-angle style specular,
    // not the reflect(-L,N) Phong vector used in the old code here.
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));

    float specTerm = Doom3SpecularLookup(NdotH);

    float3 specMask = Doom3PseudoSpecularMask(baseAlbedo);

    // Doom 3's interaction pass is strongly additive. Keep it punchy,
    // but clamp enough to avoid fireflies with stochastic light sampling.
    const float DOOM3_SPECULAR_SCALE = 4.75;

    float3 specular =
        lightColor *
        lightIntensity *
        atten *
        shadow *
        specMask *
        specTerm *
        DOOM3_SPECULAR_SCALE;

    return clamp(specular, 0.0, 8.0);
}
)"
R"(
float TraceStraightUpToSky(float3 worldPos, float3 N)
{
    const float SKY_TMAX = 1000000.0;
    float3 skyDir = float3(0.0, 0.0, 1.0);
    float NoSky = dot(N, skyDir);
    float normalBias = lerp(gShadowBias * 4.0, gShadowBias * 1.0, saturate(NoSky));

    float3 skyOrigin =
        worldPos +
        N * normalBias +
        skyDir * (gShadowBias * 2.0);

    return TraceShadow(skyOrigin, skyDir, SKY_TMAX);
}

uint PcgHash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint InitRng(uint2 pixel, uint frameIndex, uint sampleIndex)
{
    uint seed = pixel.x * 1973u;
    seed ^= pixel.y * 9277u;
    seed ^= frameIndex * 26699u;
    seed ^= sampleIndex * 374761393u;
    return PcgHash(seed) | 1u;
}

float Rand(inout uint rng)
{
    rng = PcgHash(rng);
    return (float)rng * 2.3283064365386963e-10;
}

float2 Rand2(inout uint rng)
{
    return float2(Rand(rng), Rand(rng));
}

float3 SampleCosineWorld(float3 N, inout uint rng)
{
    float3 tangent, bitangent;
    BuildOrthonormalBasis(N, tangent, bitangent);

    float3 localDir = CosineSampleHemisphere(Rand2(rng));
    return normalize(
        tangent   * localDir.x +
        bitangent * localDir.y +
        N         * localDir.z);
}

float3 SampleConeWorld(float3 centerDir, float coneRadius, inout uint rng)
{
    float3 tangent, bitangent;
    BuildOrthonormalBasis(centerDir, tangent, bitangent);

    float2 d = ConcentricSampleDisk(Rand2(rng)) * coneRadius;
    return normalize(centerDir + tangent * d.x + bitangent * d.y);
}

float3 GetSkyRadiance(float3 dir)
{
    float upness = saturate(dir.z * 0.5 + 0.5);

    float3 warmSky = float3(0.98, 0.55, 0.35);
    float3 coolSky = float3(0.30, 0.40, 0.62);
    float3 sky = lerp(warmSky * 0.22, coolSky * 0.55, upness);

    float sunAmount = pow(saturate(dot(dir, GetSkyLightDirection10AM())), 96.0);
    sky += warmSky * (sunAmount * 2.25);

    return sky;
}

float TraceVisibilityBiased(float3 worldPos, float3 N, float3 dir, float maxT)
{
    float NoD = saturate(dot(N, dir));
    float normalBias = lerp(gShadowBias * 3.0, gShadowBias * 0.75, NoD);
    float3 origin = worldPos + N * normalBias + dir * (gShadowBias * 0.5);
    return TraceShadow(origin, dir, max(maxT - gShadowBias * 0.5, 0.001));
}

float3 SafeNormalizeOr(float3 v, float3 fallback)
{
    float lenSq = dot(v, v);
    if (lenSq <= 1e-8)
        return fallback;
    return v * rsqrt(lenSq);
}

bool ProjectClipToGBufferCandidate(
    float4 clipPos,
    bool flipY,
    float3 rayHitPos,
    inout float bestDistSq,
    inout uint2 bestPixel,
    inout float3 bestPos,
    inout float3 bestNormal,
    inout float3 bestAlbedo,
    inout uint bestGeoFlag)
{
    if (abs(clipPos.w) <= 1e-6)
        return false;

    float3 ndc = clipPos.xyz / clipPos.w;

    if (ndc.x < -1.0 || ndc.x > 1.0 ||
        ndc.y < -1.0 || ndc.y > 1.0 ||
        ndc.z <  0.0 || ndc.z > 1.0)
    {
        return false;
    }

    float2 uv;
    uv.x = ndc.x * 0.5 + 0.5;
    uv.y = flipY ? (0.5 - ndc.y * 0.5) : (ndc.y * 0.5 + 0.5);

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return false;

    int2 ip = int2(uv * gScreenSize.xy);
    ip = clamp(ip, int2(0, 0), int2((int)gScreenSize.x - 1, (int)gScreenSize.y - 1));

    float depth = gDepthTex.Load(int3(ip, 0));
    if (depth <= 0.0 || depth >= 1.0)
        return false;

    float4 posSample = gPositionTex.Load(int3(ip, 0));
    float3 gbufPos = posSample.xyz;
    float3 delta = gbufPos - rayHitPos;
    float distSq = dot(delta, delta);

    if (distSq >= bestDistSq)
        return false;

    float4 normalSample = LoadSceneNormal(uint2(ip));
    float3 gbufNormal = SafeNormalizeOr(normalSample.xyz, float3(0.0, 0.0, 1.0));
    float3 gbufAlbedo = saturate(gAlbedoTex.Load(int3(ip, 0)).rgb);

    bestDistSq = distSq;
    bestPixel = uint2(ip);
    bestPos = gbufPos;
    bestNormal = gbufNormal;
    bestAlbedo = gbufAlbedo;
    bestGeoFlag = DecodeGeometryFlag(posSample.w);
    return true;
}

bool TryFetchGBufferAtRayHit(
    float3 rayHitPos,
    out uint2 hitPixel,
    out float3 hitPos,
    out float3 hitNormal,
    out float3 hitAlbedo,
    out uint hitGeoFlag)
{
    float bestDistSq = 1.0e30;
    uint2 bestPixel = uint2(0, 0);
    float3 bestPos = rayHitPos;
    float3 bestNormal = float3(0.0, 0.0, 1.0);
    float3 bestAlbedo = float3(0.5, 0.5, 0.5);
    uint bestGeoFlag = GEOMETRY_FLAG_NONE;

    // CPU computes gViewProj from the inverse view-projection matrix.  Try both
    // matrix-vector orders and both Y conventions so this remains tolerant of
    // row/column-major engine uploads and render-target origin conventions.
    float4 wpos = float4(rayHitPos, 1.0);
    float4 clipA = mul(wpos, gViewProj);
    float4 clipB = mul(gViewProj, wpos);

    bool found = false;
    found = ProjectClipToGBufferCandidate(clipA, true,  rayHitPos, bestDistSq, bestPixel, bestPos, bestNormal, bestAlbedo, bestGeoFlag) || found;
    found = ProjectClipToGBufferCandidate(clipA, false, rayHitPos, bestDistSq, bestPixel, bestPos, bestNormal, bestAlbedo, bestGeoFlag) || found;
    found = ProjectClipToGBufferCandidate(clipB, true,  rayHitPos, bestDistSq, bestPixel, bestPos, bestNormal, bestAlbedo, bestGeoFlag) || found;
    found = ProjectClipToGBufferCandidate(clipB, false, rayHitPos, bestDistSq, bestPixel, bestPos, bestNormal, bestAlbedo, bestGeoFlag) || found;

    if (!found)
    {
        hitPixel = uint2(0, 0);
        hitPos = rayHitPos;
        hitNormal = bestNormal;
        hitAlbedo = bestAlbedo;
        hitGeoFlag = GEOMETRY_FLAG_NONE;
        return false;
    }

    // The TLAS hit is exact, but the material data comes from the camera G-buffer.
    // Reject projections that land on an unrelated visible surface.
    float viewDist = length(rayHitPos - gCameraPos.xyz);
    float maxPositionError = max(24.0, viewDist * 0.035);
    if (bestDistSq > maxPositionError * maxPositionError)
    {
        hitPixel = bestPixel;
        hitPos = bestPos;
        hitNormal = bestNormal;
        hitAlbedo = bestAlbedo;
        hitGeoFlag = bestGeoFlag;
        return false;
    }

    hitPixel = bestPixel;
    hitPos = bestPos;
    hitNormal = bestNormal;
    hitAlbedo = bestAlbedo;
    hitGeoFlag = bestGeoFlag;
    return true;
}

float3 EstimatePathTracedSky(float3 worldPos, float3 N, inout uint rng)
{
    const float SKY_TMAX = 1000000.0;

    uint bounceSamples = max(gMaxBounces, 1u);
    bounceSamples = min(bounceSamples, 4u);

    float3 accum = 0.0;

    [loop]
    for (uint b = 0; b < bounceSamples; ++b)
    {
        float3 dir = SampleCosineWorld(N, rng);
        float NoD = saturate(dot(N, dir));
        float visibility = TraceVisibilityBiased(worldPos, N, dir, SKY_TMAX);

        // This is a G-buffer path-traced approximation: secondary hits are used as
        // occluders because this pass does not bind per-triangle material data yet.
        accum += GetSkyRadiance(dir) * visibility * NoD;
    }

    accum /= (float)bounceSamples;
    return accum * 0.55;
}

float3 PathTraceDirectPointLight(float3 worldPos, float3 N, float3 V, float3 baseAlbedo, Light Lgt, inout uint rng, out float3 specularOut)
{
    specularOut = 0.0;

    float3 toCenter = Lgt.position - worldPos;
    float centerDist = length(toCenter);
    if (centerDist <= 0.01)
        return 0.0;

    float3 centerDir = toCenter / centerDist;
    float3 tangent, bitangent;
    BuildOrthonormalBasis(centerDir, tangent, bitangent);

    float areaRadius = (Lgt.samples != 0u) ? max(GetPointLightMaxRadius(Lgt) * 0.03, 0.12) : 0.0;
    float2 disk = ConcentricSampleDisk(Rand2(rng)) * areaRadius;

    float3 sampleLightPos = Lgt.position + tangent * disk.x + bitangent * disk.y;
    float3 toLight = sampleLightPos - worldPos;
    float dist = length(toLight);
    if (dist <= 0.01)
        return 0.0;

    float3 L = toLight / dist;
    float atten = ComputePointLightAttenuation(worldPos, Lgt);

    float wrap = 0.28;
    float NdotLWrap = saturate((dot(N, L) + wrap) / (1.0 + wrap));

    float shadow = 1.0;
    if (Lgt.samples != 0u && NdotLWrap > 0.0001 && atten > 0.0)
        shadow = TraceVisibilityBiased(worldPos, N, L, dist);

    if (Lgt.pointRadiusPad <= 0.5)
        specularOut = ComputeSpecular(N, V, L, Lgt.color, Lgt.intensity, atten, shadow, baseAlbedo);

    return Lgt.color * (Lgt.intensity * atten * NdotLWrap * shadow);
}

float3 PathTraceDirectSpotLight(float3 worldPos, float3 N, float3 V, float3 baseAlbedo, Light Lgt, inout uint rng, out float3 specularOut)
{
    specularOut = 0.0;

    float3 toLight = Lgt.position - worldPos;
    float dist = length(toLight);
    if (dist <= 0.01)
        return 0.0;

    float3 L = toLight / dist;
    float atten = ComputeSpotLightAttenuation(worldPos, Lgt);

    float wrap = 0.28;
    float NdotLWrap = saturate((dot(N, L) + wrap) / (1.0 + wrap));

    float shadow = 1.0;
    if (Lgt.samples != 0u && NdotLWrap > 0.0001 && atten > 0.0)
        shadow = TraceVisibilityBiased(worldPos, N, L, dist);

    if (Lgt.pointRadiusPad <= 0.5)
        specularOut = ComputeSpecular(N, V, L, Lgt.color, Lgt.intensity, atten, shadow, baseAlbedo);

    return Lgt.color * (Lgt.intensity * atten * NdotLWrap * shadow);
}

float3 PathTraceDirectRectLight(float3 worldPos, float3 N, float3 V, float3 baseAlbedo, Light Lgt, inout uint rng, out float3 specularOut)
{
    specularOut = 0.0;

    float3 toCenter = Lgt.position - worldPos;
    float centerDist = length(toCenter);
    if (centerDist <= 0.01)
        return 0.0;

    float attenRadius = max(Lgt.radius, 1e-4);
    float atten = saturate((attenRadius - centerDist) / attenRadius);
    atten = atten * atten * atten * atten;

    if (atten <= 0.0)
        return 0.0;

    float2 uv = Rand2(rng) * 2.0 - 1.0;
    float3 sampleLightPos =
        Lgt.position +
        Lgt.axisU * (uv.x * Lgt.halfWidth) +
        Lgt.axisV * (uv.y * Lgt.halfHeight);

    float3 sampleVec = sampleLightPos - worldPos;
    float sampleDist = length(sampleVec);
    if (sampleDist <= 0.01)
        return 0.0;

    float3 L = sampleVec / sampleDist;
    float NdotL = saturate(dot(N, L));
    if (NdotL <= 0.0)
        return 0.0;

    float faceTerm = (Lgt.twoSided != 0)
        ? abs(dot(-L, Lgt.normal))
        : saturate(dot(-L, Lgt.normal));

    if (faceTerm <= 0.0)
        return 0.0;

    float shadow = 1.0;
    if (Lgt.samples != 0u)
        shadow = TraceVisibilityBiased(worldPos, N, L, sampleDist);

    if (Lgt.pointRadiusPad <= 0.5)
    {
        specularOut = ComputeSpecular(
            N,
            V,
            L,
            Lgt.color,
            Lgt.intensity * faceTerm,
            1.0,
            shadow,
            baseAlbedo) * atten;
    }

    return clamp(Lgt.color * (Lgt.intensity * NdotL * faceTerm * atten * shadow), 0.0, 4.0);
}

float3 EstimateFastBounceLight(float3 hitPos, float3 hitN, Light Lgt)
{
    // Secondary-bounce lighting needs to be cheap.  The primary pass already casts
    // detailed visibility rays.  Here we evaluate every active light analytically
    // without extra shadow rays so all lights still contribute to GI, but the
    // indirect path no longer explodes into many TraceRay() calls per pixel.
    if (Lgt.type == GL_RAYTRACING_LIGHT_TYPE_POINT)
    {
        float3 toLight = Lgt.position - hitPos;
        float dist = length(toLight);
        if (dist <= 0.01)
            return 0.0;

        float3 L = toLight / dist;
        float atten = ComputePointLightAttenuation(hitPos, Lgt);
        if (atten <= 0.0)
            return 0.0;

        float wrap = 0.35;
        float nDotL = saturate((dot(hitN, L) + wrap) / (1.0 + wrap));
        return Lgt.color * (Lgt.intensity * atten * nDotL);
    }
    else if (Lgt.type == GL_RAYTRACING_LIGHT_TYPE_SPOT)
    {
        float3 toLight = Lgt.position - hitPos;
        float dist = length(toLight);
        if (dist <= 0.01)
            return 0.0;

        float3 L = toLight / dist;
        float atten = ComputeSpotLightAttenuation(hitPos, Lgt);
        if (atten <= 0.0)
            return 0.0;

        float wrap = 0.35;
        float nDotL = saturate((dot(hitN, L) + wrap) / (1.0 + wrap));
        return Lgt.color * (Lgt.intensity * atten * nDotL);
    }
    else if (Lgt.type == GL_RAYTRACING_LIGHT_TYPE_RECT)
    {
        // Use the rect center for the bounce estimate.  It is stable and avoids
        // spending another random sample plus visibility ray on secondary hits.
        float3 toCenter = Lgt.position - hitPos;
        float centerDist = length(toCenter);
        if (centerDist <= 0.01)
            return 0.0;

        float attenRadius = max(Lgt.radius, 1e-4);
        float atten = saturate((attenRadius - centerDist) / attenRadius);
        atten = atten * atten;
        if (atten <= 0.0)
            return 0.0;

        float3 L = toCenter / centerDist;
        float nDotL = saturate(dot(hitN, L));
        if (nDotL <= 0.0)
            return 0.0;

        float faceTerm = (Lgt.twoSided != 0)
            ? abs(dot(-L, Lgt.normal))
            : saturate(dot(-L, Lgt.normal));

        if (faceTerm <= 0.0)
            return 0.0;

        return clamp(Lgt.color * (Lgt.intensity * nDotL * faceTerm * atten), 0.0, 4.0);
    }

    return 0.0;
}

float3 EstimateDirectLightingForBounceHit(uint2 hitPixel, float3 hitPos, float3 hitN, float3 hitV, float3 hitAlbedo, uint hitGeoFlag, inout uint rng)
{
    bool hitIsSkeletal = (hitGeoFlag & GEOMETRY_FLAG_SKELETAL) != 0u;
    bool hitIsUnlit    = (hitGeoFlag & GEOMETRY_FLAG_UNLIT)    != 0u;

    // Treat unlit G-buffer surfaces as emissive-ish for bounce purposes.  This is
    // useful for light cards / bright UI-like surfaces that deliberately bypass
    // the regular lighting pass.
    if (hitIsUnlit)
        return hitAlbedo;

    // Cheap environment term.  The expensive sky visibility probes stay in the
    // primary lighting path; doing them again for every secondary hit was a major
    // source of the framerate drop.
    float upness = saturate(hitN.z * 0.5 + 0.5);
    float3 lighting = gAmbientColor.rgb * (gAmbientColor.a * 0.025);
    lighting += GetSkyRadiance(hitN) * (0.06 + 0.10 * upness);

    // Still walk the full light list, but do not cast secondary-hit shadow rays.
    // That preserves the "all lights bounce" behavior while making the cost
    // roughly one extra bounce TraceRay() per indirect path instead of one bounce
    // TraceRay() plus many more visibility TraceRay() calls.
    [loop]
    for (uint i = 0; i < gLightCount; ++i)
    {
        lighting += EstimateFastBounceLight(hitPos, hitN, gLights[i]);
    }

    if (hitIsSkeletal)
        lighting *= 1.10;

    // Return outgoing diffuse radiance from the bounce surface.  The caller adds
    // this as incoming indirect light at the primary surface; primary albedo is
    // applied later in RayGen just like direct lighting.
    return max(hitAlbedo * max(lighting, 0.0), 0.0);
}

)"
R"(
float3 EstimateReactiveScreenSpaceFinalGather(uint2 pixel, float3 worldPos, float3 N, float3 V, float3 baseAlbedo, inout uint rng)
{
    // Cheap one-frame final gather / irradiance reuse.  This is intentionally not
    // temporal: light and material changes show up immediately, while the costly
    // DXR budget stays at one stochastic bounce ray.  The gather samples current
    // G-buffer surfaces around the shaded pixel and shades them analytically as
    // bounce emitters, so nearby lit/colorful visible surfaces push GI without
    // adding more TraceRay() calls.
    static const int2 kGatherTaps[6] =
    {
        int2(  7,   3),
        int2( -9,   6),
        int2(  5, -13),
        int2( 17,  -8),
        int2(-22, -15),
        int2( 29,  18)
    };

    uint sampleBudget = 5u;

    // Keep the pass cheap when many lights or high SPP are active.  This path
    // has no DXR rays, but it still evaluates bounce lighting against the light
    // list, so adapt the screen-space sample count instead of raising ray count.
    if (gLightCount > 4u)
        sampleBudget = 4u;
    if (gLightCount > 10u)
        sampleBudget = 3u;
    if (gSamplesPerPixel >= 4u)
        sampleBudget = min(sampleBudget, 3u);
    if (gSamplesPerPixel >= 6u)
        sampleBudget = min(sampleBudget, 2u);

    int2 maxPixel = int2((int)gScreenSize.x - 1, (int)gScreenSize.y - 1);
    float pixelScale = max(1.0, round(min(gScreenSize.x, gScreenSize.y) / 720.0));

    // Doom/idTech-like unit scale: large enough to catch wall/floor color bleed,
    // small enough to avoid room-to-room light leaking from unrelated screen hits.
    const float MAX_GATHER_DISTANCE = 176.0;

    float2 jitter = float2(
        Hash12((float2)pixel + float2(13.7, 91.1)),
        Hash12((float2)pixel + float2(47.3, 19.9))) - 0.5;

    float3 accum = 0.0;
    float weightSum = 0.0;

    [loop]
    for (uint i = 0u; i < 6u; ++i)
    {
        if (i >= sampleBudget)
            break;

        float2 tap = float2(kGatherTaps[i].x, kGatherTaps[i].y) * pixelScale;
        int2 sp = int2(pixel) + int2(
            (int)round(tap.x + jitter.x * pixelScale * 2.0),
            (int)round(tap.y + jitter.y * pixelScale * 2.0));
        sp = clamp(sp, int2(0, 0), maxPixel);

        float sampleDepth = gDepthTex.Load(int3(sp, 0));
        if (sampleDepth <= 0.0 || sampleDepth >= 1.0)
            continue;

        float4 samplePos4 = gPositionTex.Load(int3(sp, 0));
        float3 samplePos = samplePos4.xyz;
        uint sampleGeoFlag = DecodeGeometryFlag(samplePos4.w);
        float3 sampleAlbedo = saturate(gAlbedoTex.Load(int3(sp, 0)).rgb);
        float3 sampleNormal = SafeNormalizeOr(gNormalTex.Load(int3(sp, 0)).xyz, N);

        float3 delta = samplePos - worldPos;
        float distSq = dot(delta, delta);
        if (distSq <= 1e-4)
            continue;

        float dist = sqrt(distSq);
        if (dist >= MAX_GATHER_DISTANCE)
            continue;

        float3 dirToSample = delta / dist;
        float receiverFacing = saturate(dot(N, dirToSample));
        if (receiverFacing <= 0.02)
            continue;

        bool sampleIsUnlit = (sampleGeoFlag & GEOMETRY_FLAG_UNLIT) != 0u;
        float emitterFacing = sampleIsUnlit ? 1.0 : saturate(dot(sampleNormal, -dirToSample));
        if (emitterFacing <= 0.02)
            continue;

        float distanceFade = saturate(1.0 - dist / MAX_GATHER_DISTANCE);
        distanceFade *= distanceFade;
        float distanceWeight = 1.0 / (1.0 + distSq * 0.00018);

        // Favor concave/near-facing exchange and suppress unrelated background
        // samples that happen to be close in screen-space but far in world-space.
        float normalAffinity = saturate(dot(N, sampleNormal) * 0.35 + 0.65);
        float formWeight = receiverFacing * emitterFacing * distanceFade * distanceWeight * normalAffinity;

        if (formWeight <= 1e-5)
            continue;

        float3 sampleView = SafeNormalizeOr(-dirToSample, V);
        float3 outgoingRadiance = EstimateDirectLightingForBounceHit(
            uint2(sp),
            samplePos,
            sampleNormal,
            sampleView,
            sampleAlbedo,
            sampleGeoFlag,
            rng);

        accum += outgoingRadiance * formWeight;
        weightSum += formWeight;
    }

    if (weightSum <= 1e-5)
        return 0.0;

    float3 gatheredRadiance = accum / weightSum;

    // Coverage keeps one bright tap from flooding a pixel while still letting
    // nearby high-confidence samples respond strongly to dynamic lights.
    float coverage = saturate(weightSum * 1.65);

    const float FINAL_GATHER_STRENGTH = 0.38;
    return gatheredRadiance * coverage * FINAL_GATHER_STRENGTH;
}
)"
R"(
float3 TraceOneIndirectBouncePath(uint2 pixel, float3 worldPos, float3 N, float3 V, float3 baseAlbedo, inout uint rng)
{
    const float BOUNCE_TMAX = 1000000.0;

    uint maxIndirectDepth = (gMaxBounces > 1u) ? 1u : 0u;

    // Extra diffuse depths are very expensive because each depth launches another
    // DXR ray.  Keep the common 1-2 SPP mode to one indirect hit.  Higher SPP can
    // opt into one additional diffuse depth, capped at two total secondary hits.
    if (gSamplesPerPixel >= 4u)
        maxIndirectDepth = min(gMaxBounces - 1u, 2u);

    float3 accum = 0.0;
    float3 throughput = 1.0;

    float3 pathPos = worldPos;
    float3 pathNormal = N;
    float3 pathView = V;
    uint2 pathPixel = pixel;

    [loop]
    for (uint depth = 0; depth < maxIndirectDepth; ++depth)
    {
        float3 bounceDir = SampleCosineWorld(pathNormal, rng);
        float NoD = saturate(dot(pathNormal, bounceDir));

        float normalBias = lerp(gShadowBias * 3.0, gShadowBias * 0.75, NoD);
        float3 bounceOrigin =
            pathPos +
            pathNormal * normalBias +
            bounceDir * (gShadowBias * 0.5);

        float hitT = 0.0;
        uint materialFlags = 0;
        bool hit = TraceBounce(bounceOrigin, bounceDir, BOUNCE_TMAX, hitT, materialFlags);

        if (!hit)
        {
            // A miss is ordinary environment lighting for the path. Keep this
            // modest because the main pass already has stable direct sky terms.
            accum += throughput * GetSkyRadiance(bounceDir) * 0.18;
            break;
        }

        float3 rayHitPos = bounceOrigin + bounceDir * hitT;

        uint2 hitPixel = pathPixel;
        float3 hitPos = rayHitPos;
        float3 hitNormal = SafeNormalizeOr(-bounceDir, pathNormal);
        float3 hitAlbedo = float3(0.55, 0.55, 0.55);
        uint hitGeoFlag = GEOMETRY_FLAG_NONE;

        bool hasGBufferMaterial = TryFetchGBufferAtRayHit(
            rayHitPos,
            hitPixel,
            hitPos,
            hitNormal,
            hitAlbedo,
            hitGeoFlag);

        // If the G-buffer normal points away from the incoming bounce ray, flip it
        // so direct lighting at the secondary hit is evaluated on the side that
        // the ray actually reached.
        if (dot(hitNormal, -bounceDir) < 0.0)
            hitNormal = -hitNormal;

        float3 hitV = SafeNormalizeOr(-bounceDir, pathView);
        float3 bouncedRadiance = EstimateDirectLightingForBounceHit(
            hitPixel,
            hitPos,
            hitNormal,
            hitV,
            hitAlbedo,
            hitGeoFlag,
            rng);

        if (!hasGBufferMaterial)
        {
            // IMPORTANT: this is the path that made some lights look like they
            // were not bouncing.  The TLAS can hit an off-screen or camera-hidden
            // surface, and the old code returned only a tiny sky fallback because
            // it could not fetch albedo/normal from the G-buffer.  Still shade the
            // real ray hit against every active light using a neutral diffuse
            // material so point, spot, and rect lights all contribute to bounce.
            float3 skyTint = GetSkyRadiance(SafeNormalizeOr(reflect(bounceDir, pathNormal), float3(0.0, 0.0, 1.0)));
            bouncedRadiance += skyTint * 0.035;
        }

        accum += throughput * bouncedRadiance;

        if ((hitGeoFlag & GEOMETRY_FLAG_UNLIT) != 0u)
            break;

        // Cosine-weighted diffuse sampling cancels the Lambertian cosine/pdf term,
        // so the path throughput is the surface albedo plus a conservative energy
        // scale to keep multiple G-buffer-assisted bounces stable.
        throughput *= saturate(hitAlbedo) * 0.68;

        if (max(max(throughput.x, throughput.y), throughput.z) < 0.02)
            break;

        pathPos = hitPos;
        pathNormal = hitNormal;
        pathView = hitV;
        pathPixel = hitPixel;
    }

    return accum;
}

float3 EstimatePathTracedIndirectBounce(uint2 pixel, float3 worldPos, float3 N, float3 V, float3 baseAlbedo, inout uint rng)
{
    if (gMaxBounces <= 1u)
        return 0.0;

    // Keep the realtime GI ray budget flat: one stochastic DXR bounce path per
    // lighting sample.  Extra GI coverage now comes from the reactive final
    // gather in RayGen, which uses current-frame G-buffer reuse instead of more
    // secondary TraceRay() calls.
    float3 accum = TraceOneIndirectBouncePath(pixel, worldPos, N, V, baseAlbedo, rng);

    const float INDIRECT_STRENGTH = 0.58;
    return accum * INDIRECT_STRENGTH;
}

float3 PathTraceLightingSample(uint2 pixel, float3 worldPos, float3 N, float3 V, float3 baseAlbedo, bool isSkeletal, inout uint rng, out float3 specularAccum)
{
    specularAccum = 0.0;

    float cavity = ComputeCavity(pixel, worldPos, N);
    float microShadow = lerp(0.75, 1.0, cavity);

    // Keep the environment / sun shadow path deterministic.  The previous
    // path-traced version sampled the sky cone and AO with frame-varying random
    // rays; with DLSS RR enabled that raw 1spp signal was noisy enough to lose
    // the old environment shadow shapes.  Reuse the stable multi-ray sky and AO
    // probes from the original lighting pass, then add only a small stochastic
    // sky-bounce term when both extra bounces and an SPP budget are requested.
    float ao = ComputeAmbientOcclusion(worldPos, N, pixel);
    float skyVis = ComputeSkyVisibility(worldPos, N, pixel);
    float ambientSkyVis = TraceStraightUpToSky(worldPos, N);

    float upness = saturate(N.z * 0.5 + 0.5);
    float3 skyColorRGB = float3(0.98, 0.55, 0.35);
    float3 skyColor = skyColorRGB * (0.35 + 0.65 * upness);

    float3 lightingAccum = gAmbientColor.rgb * (gAmbientColor.a * 0.04);
    lightingAccum += skyColor * (0.70 * skyVis);
    lightingAccum += ambientSkyVis * (skyColorRGB * 0.15);

    if (gMaxBounces > 1u)
    {
        lightingAccum += EstimatePathTracedIndirectBounce(pixel, worldPos, N, V, baseAlbedo, rng);
    }

    if (isSkeletal)
        lightingAccum += 0.1;

    [loop]
    for (uint i = 0; i < gLightCount; ++i)
    {
        Light Lgt = gLights[i];
        float3 spec = 0.0;
        float3 diffuse = 0.0;

        if (Lgt.type == GL_RAYTRACING_LIGHT_TYPE_POINT)
        {
            diffuse = PathTraceDirectPointLight(worldPos, N, V, baseAlbedo, Lgt, rng, spec);
        }
        else if (Lgt.type == GL_RAYTRACING_LIGHT_TYPE_SPOT)
        {
            diffuse = PathTraceDirectSpotLight(worldPos, N, V, baseAlbedo, Lgt, rng, spec);
        }
        else if (Lgt.type == GL_RAYTRACING_LIGHT_TYPE_RECT)
        {
            diffuse = PathTraceDirectRectLight(worldPos, N, V, baseAlbedo, Lgt, rng, spec);
        }

        lightingAccum += diffuse;
        specularAccum += spec;
    }

    lightingAccum *= ao;
    specularAccum *= ao;
    lightingAccum *= microShadow;

    if (isSkeletal)
    {
        lightingAccum *= 1.2;
        specularAccum *= 1.15;
    }

    return max(lightingAccum, 0.0);
}

[shader("raygeneration")]
void RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;

    if (pixel.x >= (uint)gScreenSize.x || pixel.y >= (uint)gScreenSize.y)
        return;

    float4 albedoSample = gAlbedoTex.Load(int3(pixel, 0));
    float depthSample   = gDepthTex.Load(int3(pixel, 0));

    if (depthSample <= 0.0 || depthSample >= 1.0)
    {
        gOutputTex[pixel] = albedoSample;
        return;
    }

    float3 baseAlbedo     = albedoSample.rgb;
    float4 positionSample = gPositionTex.Load(int3(pixel, 0));
    float3 worldPos       = positionSample.xyz;
    float4 normalSample   = LoadSceneNormal(pixel);
    float3 N              = EnhanceBumpNormal(pixel, worldPos, baseAlbedo);
    float3 V              = normalize(gCameraPos.xyz - worldPos);

    uint geoFlag = DecodeGeometryFlag(positionSample.w);
    bool isSkeletal = (geoFlag & GEOMETRY_FLAG_SKELETAL) != 0u;
    bool isUnlit    = (geoFlag & GEOMETRY_FLAG_UNLIT)    != 0u;

    if (isUnlit)
    {
        gOutputTex[pixel] = float4(baseAlbedo, albedoSample.a);
        return;
    }

    uint spp = max(gSamplesPerPixel, 1u);
    spp = min(spp, 8u);

    float3 reactiveFinalGather = 0.0;
    if (gMaxBounces > 1u)
    {
        // Evaluate this deterministic screen-space irradiance reuse once per
        // pixel, not once per SPP.  That makes the GI more responsive without
        // multiplying the light-list work inside the stochastic sample loop.
        uint gatherRng = InitRng(pixel, 0u, 1337u);
        reactiveFinalGather = EstimateReactiveScreenSpaceFinalGather(
            pixel,
            worldPos,
            N,
            V,
            baseAlbedo,
            gatherRng);
    }

    float3 colorAccum = 0.0;

    [loop]
    for (uint s = 0; s < spp; ++s)
    {
        uint rng = InitRng(pixel, gFrameIndex, s);

        float3 specularAccum = 0.0;
        float3 lightingAccum = PathTraceLightingSample(
            pixel,
            worldPos,
            N,
            V,
            baseAlbedo,
            isSkeletal,
            rng,
            specularAccum);

        float cavity = ComputeCavity(pixel, worldPos, N);
        float3 albedo = baseAlbedo * cavity;

        colorAccum += (albedo * lightingAccum) + specularAccum;
    }

    float3 finalColor = colorAccum / (float)spp;

    if (gMaxBounces > 1u)
    {
        // reactiveFinalGather is incoming indirect radiance.  Apply the primary
        // diffuse albedo here, matching the regular lighting path.
        finalColor += baseAlbedo * reactiveFinalGather;
    }

    gOutputTex[pixel] = float4(max(finalColor, 0.0), albedoSample.a);
}
)";

static const char* g_glRaytracingDenoiseHlsl = R"(
cbuffer LightingCB : register(b0)
{
    float4x4 gInvViewProj;
    float4x4 gInvViewMatrix;
    float4x4 gViewProj;
    float4   gCameraPos;
    float4   gAmbientColor;
    float4   gScreenSize;
    float    gNormalReconstructZ;
    uint     gLightCount;
    uint     gEnableSpecular;
    uint     gEnableHalfLambert;
    float    gShadowBias;
    uint     gFrameIndex;
    uint     gSamplesPerPixel;
    uint     gMaxBounces;
    uint     gEnableDenoiser;
    uint     gDenoisePassIndex;
    float    gDenoiseStepWidth;
    float    gDenoiseStrength;
    float    gDenoisePhiColor;
    float    gDenoisePhiNormal;
    float    gDenoisePhiPosition;
    float    gBumpStrength;
};

Texture2D<float4> gAlbedoTex      : register(t1);
Texture2D<float>  gDepthTex       : register(t2);
Texture2D<float4> gNormalTex      : register(t3);
Texture2D<float4> gPositionTex    : register(t4);
Texture2D<float4> gPathTraceTex   : register(t6);
Texture2D<float4> gDenoiseATex    : register(t7);
Texture2D<float4> gDenoiseBTex    : register(t8);

RWTexture2D<float4> gRayOutputTex      : register(u0);
RWTexture2D<float4> gDenoiseAOutTex    : register(u1);
RWTexture2D<float4> gDenoiseBOutTex    : register(u2);
RWTexture2D<float4> gDenoisedOutputTex : register(u3);

static const float kKernel[5] = { 0.0625, 0.25, 0.375, 0.25, 0.0625 };

float3 SafeNormal(float3 n)
{
    float lenSq = max(dot(n, n), 1e-8);
    return n * rsqrt(lenSq);
}

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

static const uint GEOMETRY_FLAG_GLASS = 4u;

uint DecodeGeometryFlag(float geoFlag)
{
    return (uint)floor(max(geoFlag, 0.0) + 0.5);
}

float3 SafeAlbedoDivisor(float3 albedo)
{
    // Do not let black/dark textures explode when demodulating noisy lighting.
    return max(abs(albedo), float3(0.06, 0.06, 0.06));
}

float3 DemodulateLighting(float3 radiance, float3 albedo)
{
    return radiance / SafeAlbedoDivisor(albedo);
}

float3 RemodulateLighting(float3 lighting, float3 albedo)
{
    return lighting * SafeAlbedoDivisor(albedo);
}

float4 LoadDenoiseSource(int2 p)
{
    if (gDenoisePassIndex == 0u)
        return gPathTraceTex.Load(int3(p, 0));
    if (gDenoisePassIndex == 1u)
        return gDenoiseATex.Load(int3(p, 0));
    return gDenoiseBTex.Load(int3(p, 0));
}

void StoreDenoiseOutput(uint2 p, float4 v)
{
    if (gDenoisePassIndex == 0u)
        gDenoiseAOutTex[p] = v;
    else if (gDenoisePassIndex == 1u)
        gDenoiseBOutTex[p] = v;
    else
        gDenoisedOutputTex[p] = v;
}

float GeometryAwareWeight(
    float3 centerRadiance,
    float3 sampleRadiance,
    float3 centerAlbedo,
    float3 sampleAlbedo,
    float3 centerNormal,
    float3 sampleNormal,
    float3 centerPos,
    float3 samplePos,
    float centerDepth,
    float sampleDepth,
    uint centerGeoFlag,
    uint sampleGeoFlag,
    float kernelWeight)
{
    if (sampleDepth <= 0.0 || sampleDepth >= 1.0)
        return 0.0;

    // Do not smear lighting across material-class boundaries.  This is
    // particularly important for glass, because the primary G-buffer sample can
    // be glass while the ray visibility must continue through it.
    if (((centerGeoFlag ^ sampleGeoFlag) & GEOMETRY_FLAG_GLASS) != 0u)
        return 0.0;

    float3 centerLighting = DemodulateLighting(centerRadiance, centerAlbedo);
    float3 sampleLighting = DemodulateLighting(sampleRadiance, sampleAlbedo);

    // Use albedo, not noisy lit radiance, as the main color edge guide.  The
    // previous filter used the shadowed/noisy signal itself as the guide, which
    // rejected neighbors across shadow variation and left shadow noise intact.
    float albedoDiff = length(centerAlbedo - sampleAlbedo);
    float albedoWeight = exp(-albedoDiff * max(gDenoisePhiColor, 0.001));

    // A deliberately soft lighting-domain gate keeps hard contact-shadow edges
    // from being over-blurred, but still lets noisy penumbra/visibility samples
    // converge across the same surface.
    float centerLum = Luminance(centerLighting);
    float sampleLum = Luminance(sampleLighting);
    float illumDiff = abs(sampleLum - centerLum);
    float relativeIllumDiff = illumDiff / max(max(abs(centerLum), abs(sampleLum)), 0.05);
    float illuminationWeight = exp(-relativeIllumDiff * max(gDenoisePhiColor * 0.035, 0.10));

    float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), max(gDenoisePhiNormal, 1.0));
    float positionDiff = length(samplePos - centerPos);
    float positionWeight = exp(-positionDiff * max(gDenoisePhiPosition, 0.001));
    float depthDiff = abs(sampleDepth - centerDepth);
    float depthWeight = exp(-depthDiff * 300.0);

    return kernelWeight * albedoWeight * illuminationWeight * normalWeight * positionWeight * depthWeight;
}

[numthreads(8, 8, 1)]
void DenoiseCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;

    if (pixel.x >= (uint)gScreenSize.x || pixel.y >= (uint)gScreenSize.y)
        return;

    float4 albedoSample = gAlbedoTex.Load(int3(pixel, 0));
    float depthSample = gDepthTex.Load(int3(pixel, 0));
    float4 centerSource = LoadDenoiseSource(int2(pixel));

    if (depthSample <= 0.0 || depthSample >= 1.0)
    {
        StoreDenoiseOutput(pixel, albedoSample);
        return;
    }

    if (gEnableDenoiser == 0u)
    {
        StoreDenoiseOutput(pixel, centerSource);
        return;
    }

    float3 centerAlbedo = saturate(albedoSample.rgb);
    float3 centerNormal = SafeNormal(gNormalTex.Load(int3(pixel, 0)).xyz);
    float4 centerPos4 = gPositionTex.Load(int3(pixel, 0));
    float3 centerPos = centerPos4.xyz;
    uint centerGeoFlag = DecodeGeometryFlag(centerPos4.w);

    int stepI = max((int)round(max(gDenoiseStepWidth, 1.0)), 1);
    float3 accumLighting = 0.0;
    float weightSum = 0.0;

    // Three-pass a-trous wavelet filter. The CPU dispatches this with step
    // widths 1, 2, and 4. It is geometry-aware and does not blend prior frames.
    [unroll]
    for (int ky = 0; ky < 5; ++ky)
    {
        [unroll]
        for (int kx = 0; kx < 5; ++kx)
        {
            int2 sp = int2(pixel) + int2(kx - 2, ky - 2) * stepI;

            if (sp.x < 0 || sp.y < 0 || sp.x >= (int)gScreenSize.x || sp.y >= (int)gScreenSize.y)
                continue;

            float sampleDepth = gDepthTex.Load(int3(sp, 0));
            float4 sampleColor4 = LoadDenoiseSource(sp);
            float3 sampleAlbedo = saturate(gAlbedoTex.Load(int3(sp, 0)).rgb);
            float3 sampleNormal = SafeNormal(gNormalTex.Load(int3(sp, 0)).xyz);
            float4 samplePos4 = gPositionTex.Load(int3(sp, 0));
            float3 samplePos = samplePos4.xyz;
            uint sampleGeoFlag = DecodeGeometryFlag(samplePos4.w);
            float kernelWeight = kKernel[kx] * kKernel[ky];

            float w = GeometryAwareWeight(
                centerSource.rgb,
                sampleColor4.rgb,
                centerAlbedo,
                sampleAlbedo,
                centerNormal,
                sampleNormal,
                centerPos,
                samplePos,
                depthSample,
                sampleDepth,
                centerGeoFlag,
                sampleGeoFlag,
                kernelWeight);

            accumLighting += DemodulateLighting(sampleColor4.rgb, sampleAlbedo) * w;
            weightSum += w;
        }
    }

    float3 filteredLighting = (weightSum > 1e-6)
        ? (accumLighting / weightSum)
        : DemodulateLighting(centerSource.rgb, centerAlbedo);

    float3 filtered = RemodulateLighting(filteredLighting, centerAlbedo);

    // Final-pass firefly clamp against the raw neighborhood.
    if (gDenoisePassIndex >= 2u)
    {
        float3 minRaw = gPathTraceTex.Load(int3(pixel, 0)).rgb;
        float3 maxRaw = minRaw;

        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                int2 sp = int2(pixel) + int2(x, y);
                if (sp.x < 0 || sp.y < 0 || sp.x >= (int)gScreenSize.x || sp.y >= (int)gScreenSize.y)
                    continue;
                float3 raw = gPathTraceTex.Load(int3(sp, 0)).rgb;
                minRaw = min(minRaw, raw);
                maxRaw = max(maxRaw, raw);
            }
        }

        filtered = clamp(filtered, minRaw - 0.15, maxRaw + 0.15);
    }

    float3 outColor = lerp(centerSource.rgb, filtered, saturate(gDenoiseStrength));
    StoreDenoiseOutput(pixel, float4(max(outColor, 0.0), centerSource.a));
}
)";

static ComPtr<IDxcBlob> glRaytracingLightingCompileLibrary(const char* src)
{
	ComPtr<IDxcUtils> utils;
	ComPtr<IDxcCompiler3> compiler;
	ComPtr<IDxcIncludeHandler> includeHandler;

	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	if (FAILED(hr))
	{
		glRaytracingFatal("DxcCreateInstance utils failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (FAILED(hr))
	{
		glRaytracingFatal("DxcCreateInstance compiler failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	hr = utils->CreateDefaultIncludeHandler(&includeHandler);
	if (FAILED(hr))
	{
		glRaytracingFatal("CreateDefaultIncludeHandler failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	DxcBuffer source = {};
	source.Ptr = src;
	source.Size = strlen(src);
	source.Encoding = DXC_CP_UTF8;

	const wchar_t* args[] =
	{
		L"-T", L"lib_6_3",
		L"-Zi",
		L"-Qembed_debug",
		L"-O3",
		L"-all_resources_bound"
	};

	ComPtr<IDxcResult> result;
	hr = compiler->Compile(&source, args, _countof(args), includeHandler.Get(), IID_PPV_ARGS(&result));
	if (FAILED(hr))
	{
		glRaytracingFatal("DXC compile failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
	{
		OutputDebugStringA(errors->GetStringPointer());
		OutputDebugStringA("\n");
	}

	HRESULT status = S_OK;
	result->GetStatus(&status);
	if (FAILED(status))
	{
		glRaytracingFatal("DXIL compile status failed 0x%08X", (unsigned)status);
		return nullptr;
	}

	ComPtr<IDxcBlob> dxil;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxil), nullptr);
	return dxil;
}

static ComPtr<IDxcBlob> glRaytracingLightingCompileCompute(const char* src, const wchar_t* entryPoint)
{
	ComPtr<IDxcUtils> utils;
	ComPtr<IDxcCompiler3> compiler;
	ComPtr<IDxcIncludeHandler> includeHandler;

	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	if (FAILED(hr))
	{
		glRaytracingFatal("DxcCreateInstance utils failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (FAILED(hr))
	{
		glRaytracingFatal("DxcCreateInstance compiler failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	hr = utils->CreateDefaultIncludeHandler(&includeHandler);
	if (FAILED(hr))
	{
		glRaytracingFatal("CreateDefaultIncludeHandler failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	DxcBuffer source = {};
	source.Ptr = src;
	source.Size = strlen(src);
	source.Encoding = DXC_CP_UTF8;

	const wchar_t* args[] =
	{
		L"-E", entryPoint,
		L"-T", L"cs_6_0",
		L"-Zi",
		L"-Qembed_debug",
		L"-O3",
		L"-all_resources_bound"
	};

	ComPtr<IDxcResult> result;
	hr = compiler->Compile(&source, args, _countof(args), includeHandler.Get(), IID_PPV_ARGS(&result));
	if (FAILED(hr))
	{
		glRaytracingFatal("DXC compute compile failed 0x%08X", (unsigned)hr);
		return nullptr;
	}

	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
	{
		OutputDebugStringA(errors->GetStringPointer());
		OutputDebugStringA("\n");
	}

	HRESULT status = S_OK;
	result->GetStatus(&status);
	if (FAILED(status))
	{
		glRaytracingFatal("DXIL compute compile status failed 0x%08X", (unsigned)status);
		return nullptr;
	}

	ComPtr<IDxcBlob> dxil;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxil), nullptr);
	return dxil;
}

static int glRaytracingLightingCreateDescriptorHeap(void)
{
	D3D12_DESCRIPTOR_HEAP_DESC hd = {};
	hd.NumDescriptors = GLR_DESC_COUNT;
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	GLR_CHECK(g_glRaytracingCmd.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_glRaytracingLighting.descriptorHeap)));
	g_glRaytracingLighting.descriptorStride =
		g_glRaytracingCmd.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	return 1;
}

static int glRaytracingLightingCreateRootSignatures(void)
{
	{
		D3D12_DESCRIPTOR_RANGE ranges[2] = {};

		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[0].NumDescriptors = GLR_DESC_SRV_COUNT;
		ranges[0].BaseShaderRegister = 0;
		ranges[0].RegisterSpace = 0;
		ranges[0].OffsetInDescriptorsFromTableStart = 0;

		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[1].NumDescriptors = GLR_DESC_UAV_COUNT;
		ranges[1].BaseShaderRegister = 0;
		ranges[1].RegisterSpace = 0;
		ranges[1].OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER params[3] = {};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].DescriptorTable.NumDescriptorRanges = 1;
		params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[2].Descriptor.ShaderRegister = 0;
		params[2].Descriptor.RegisterSpace = 0;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rsd = {};
		rsd.NumParameters = _countof(params);
		rsd.pParameters = params;
		rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		ComPtr<ID3DBlob> sig;
		ComPtr<ID3DBlob> err;
		GLR_CHECK(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
		GLR_CHECK(g_glRaytracingCmd.device->CreateRootSignature(
			0, sig->GetBufferPointer(), sig->GetBufferSize(),
			IID_PPV_ARGS(&g_glRaytracingLighting.globalRootSig)));
	}

	{
		D3D12_ROOT_SIGNATURE_DESC rsd = {};
		rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

		ComPtr<ID3DBlob> sig;
		ComPtr<ID3DBlob> err;
		GLR_CHECK(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
		GLR_CHECK(g_glRaytracingCmd.device->CreateRootSignature(
			0, sig->GetBufferPointer(), sig->GetBufferSize(),
			IID_PPV_ARGS(&g_glRaytracingLighting.localRootSig)));
	}

	return 1;
}

static int glRaytracingLightingCreateBuffers(void)
{
	const UINT64 constantsBytes = glRaytracingAlignUp(sizeof(glRaytracingLightingConstants_t), 256);

	g_glRaytracingLighting.constantBuffer = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		constantsBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	for (int i = 0; i < 3; ++i)
	{
		g_glRaytracingLighting.denoiseConstantBuffer[i] = glRaytracingCreateBuffer(
			g_glRaytracingCmd.device.Get(),
			constantsBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE);
	}

	g_glRaytracingLighting.lightBuffer = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		sizeof(glRaytracingLight_t) * GL_RAYTRACING_MAX_LIGHTS,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	return g_glRaytracingLighting.constantBuffer.resource &&
		g_glRaytracingLighting.lightBuffer.resource &&
		g_glRaytracingLighting.denoiseConstantBuffer[0].resource &&
		g_glRaytracingLighting.denoiseConstantBuffer[1].resource &&
		g_glRaytracingLighting.denoiseConstantBuffer[2].resource;
}

static void glRaytracingLightingUploadConstantsTo(
	const glRaytracingBuffer_t& dst,
	const glRaytracingLightingConstants_t& constants)
{
	if (!dst.resource)
		return;

	glRaytracingMapCopy(dst.resource.Get(), &constants, sizeof(constants));
}

static void glRaytracingLightingUpdateConstants(void)
{
	glRaytracingLightingUploadConstantsTo(
		g_glRaytracingLighting.constantBuffer,
		g_glRaytracingLighting.constants);
}

static void glRaytracingLightingUpdateLights(void)
{
	if (g_glRaytracingLighting.cpuLights.empty())
		return;

	glRaytracingMapCopy(
		g_glRaytracingLighting.lightBuffer.resource.Get(),
		g_glRaytracingLighting.cpuLights.data(),
		g_glRaytracingLighting.cpuLights.size() * sizeof(glRaytracingLight_t));
}

static void glRaytracingLightingCreatePersistentLightSRV(void)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Format = DXGI_FORMAT_UNKNOWN;
	srv.Buffer.FirstElement = 0;
	srv.Buffer.NumElements = GL_RAYTRACING_MAX_LIGHTS;
	srv.Buffer.StructureByteStride = sizeof(glRaytracingLight_t);
	srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE base = g_glRaytracingLighting.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	g_glRaytracingCmd.device->CreateShaderResourceView(
		g_glRaytracingLighting.lightBuffer.resource.Get(),
		&srv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, 0));
}

static int glRaytracingLightingCreateStateObject(void)
{
	ComPtr<IDxcBlob> dxil = glRaytracingLightingCompileLibrary(g_glRaytracingLightingHlsl);
	if (!dxil)
		return 0;

	D3D12_EXPORT_DESC exports[7] = {};
	exports[0].Name = L"RayGen";
	exports[1].Name = L"ShadowMiss";
	exports[2].Name = L"ShadowAnyHit";
	exports[3].Name = L"ShadowClosestHit";
	exports[4].Name = L"BounceMiss";
	exports[5].Name = L"BounceAnyHit";
	exports[6].Name = L"BounceClosestHit";

	D3D12_DXIL_LIBRARY_DESC libDesc = {};
	D3D12_SHADER_BYTECODE libBytecode = {};
	libBytecode.pShaderBytecode = dxil->GetBufferPointer();
	libBytecode.BytecodeLength = dxil->GetBufferSize();
	libDesc.DXILLibrary = libBytecode;
	libDesc.NumExports = _countof(exports);
	libDesc.pExports = exports;

	D3D12_HIT_GROUP_DESC hitGroups[2] = {};
	hitGroups[0].HitGroupExport = L"ShadowHitGroup";
	hitGroups[0].AnyHitShaderImport = L"ShadowAnyHit";
	hitGroups[0].ClosestHitShaderImport = L"ShadowClosestHit";
	hitGroups[0].Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

	hitGroups[1].HitGroupExport = L"BounceHitGroup";
	hitGroups[1].AnyHitShaderImport = L"BounceAnyHit";
	hitGroups[1].ClosestHitShaderImport = L"BounceClosestHit";
	hitGroups[1].Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
	shaderConfig.MaxPayloadSizeInBytes = 16; // BouncePayload: uint + float + uint + uint.
	shaderConfig.MaxAttributeSizeInBytes = 8;

	D3D12_GLOBAL_ROOT_SIGNATURE globalRS = {};
	globalRS.pGlobalRootSignature = g_glRaytracingLighting.globalRootSig.Get();

	D3D12_LOCAL_ROOT_SIGNATURE localRS = {};
	localRS.pLocalRootSignature = g_glRaytracingLighting.localRootSig.Get();

	D3D12_STATE_SUBOBJECT subobjects[8] = {};
	UINT sub = 0;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	subobjects[sub].pDesc = &libDesc;
	++sub;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	subobjects[sub].pDesc = &hitGroups[0];
	++sub;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	subobjects[sub].pDesc = &hitGroups[1];
	++sub;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	subobjects[sub].pDesc = &shaderConfig;
	++sub;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	subobjects[sub].pDesc = &globalRS;
	++sub;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	subobjects[sub].pDesc = &localRS;
	++sub;

	LPCWSTR localExports[] =
	{
		L"RayGen",
		L"ShadowMiss",
		L"ShadowHitGroup",
		L"BounceMiss",
		L"BounceHitGroup"
	};

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assoc = {};
	assoc.pSubobjectToAssociate = &subobjects[5];
	assoc.NumExports = _countof(localExports);
	assoc.pExports = localExports;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	subobjects[sub].pDesc = &assoc;
	++sub;

	D3D12_RAYTRACING_PIPELINE_CONFIG pipeConfig = {};
	pipeConfig.MaxTraceRecursionDepth = 1;

	subobjects[sub].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	subobjects[sub].pDesc = &pipeConfig;
	++sub;

	D3D12_STATE_OBJECT_DESC soDesc = {};
	soDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	soDesc.NumSubobjects = sub;
	soDesc.pSubobjects = subobjects;

	GLR_CHECK(g_glRaytracingCmd.device->CreateStateObject(&soDesc, IID_PPV_ARGS(&g_glRaytracingLighting.rtStateObject)));
	GLR_CHECK(g_glRaytracingLighting.rtStateObject.As(&g_glRaytracingLighting.rtStateProps));
	return 1;
}

static int glRaytracingLightingCreateShaderTables(void)
{
	void* raygenId = g_glRaytracingLighting.rtStateProps->GetShaderIdentifier(L"RayGen");
	void* shadowMissId = g_glRaytracingLighting.rtStateProps->GetShaderIdentifier(L"ShadowMiss");
	void* bounceMissId = g_glRaytracingLighting.rtStateProps->GetShaderIdentifier(L"BounceMiss");
	void* shadowHitId = g_glRaytracingLighting.rtStateProps->GetShaderIdentifier(L"ShadowHitGroup");
	void* bounceHitId = g_glRaytracingLighting.rtStateProps->GetShaderIdentifier(L"BounceHitGroup");

	if (!raygenId || !shadowMissId || !bounceMissId || !shadowHitId || !bounceHitId)
	{
		glRaytracingFatal("Failed to fetch shader identifiers");
		return 0;
	}

	const UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	const UINT recordSize = (UINT)glRaytracingAlignUp(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	const UINT missTableSize = recordSize * 2u;
	const UINT hitTableSize = recordSize * 2u;

	g_glRaytracingLighting.raygenTable = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		recordSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	g_glRaytracingLighting.missTable = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		missTableSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	g_glRaytracingLighting.hitTable = glRaytracingCreateBuffer(
		g_glRaytracingCmd.device.Get(),
		hitTableSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_FLAG_NONE);

	if (!g_glRaytracingLighting.raygenTable.resource ||
		!g_glRaytracingLighting.missTable.resource ||
		!g_glRaytracingLighting.hitTable.resource)
	{
		return 0;
	}

	std::vector<uint8_t> temp;
	temp.resize((size_t)max(recordSize, max(missTableSize, hitTableSize)), 0);

	memset(temp.data(), 0, temp.size());
	memcpy(temp.data(), raygenId, shaderIdSize);
	glRaytracingMapCopy(g_glRaytracingLighting.raygenTable.resource.Get(), temp.data(), recordSize);

	memset(temp.data(), 0, temp.size());
	memcpy(temp.data(), shadowMissId, shaderIdSize);
	memcpy(temp.data() + recordSize, bounceMissId, shaderIdSize);
	glRaytracingMapCopy(g_glRaytracingLighting.missTable.resource.Get(), temp.data(), missTableSize);

	memset(temp.data(), 0, temp.size());
	memcpy(temp.data(), shadowHitId, shaderIdSize);
	memcpy(temp.data() + recordSize, bounceHitId, shaderIdSize);
	glRaytracingMapCopy(g_glRaytracingLighting.hitTable.resource.Get(), temp.data(), hitTableSize);

	return 1;
}

static int glRaytracingLightingCreateDenoisePipeline(void)
{
	ComPtr<IDxcBlob> dxil = glRaytracingLightingCompileCompute(g_glRaytracingDenoiseHlsl, L"DenoiseCS");
	if (!dxil)
		return 0;

	D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = g_glRaytracingLighting.globalRootSig.Get();
	pso.CS.pShaderBytecode = dxil->GetBufferPointer();
	pso.CS.BytecodeLength = dxil->GetBufferSize();

	GLR_CHECK(g_glRaytracingCmd.device->CreateComputePipelineState(
		&pso,
		IID_PPV_ARGS(&g_glRaytracingLighting.denoisePSO)));

	return 1;
}

static int glRaytracingLightingEnsureDenoiseResources(UINT width, UINT height)
{
	if (width == 0 || height == 0)
		return 0;

	if (g_glRaytracingLighting.pathTraceTexture.resource &&
		g_glRaytracingLighting.denoiseTemp[0].resource &&
		g_glRaytracingLighting.denoiseTemp[1].resource &&
		g_glRaytracingLighting.denoiseWidth == width &&
		g_glRaytracingLighting.denoiseHeight == height &&
		g_glRaytracingLighting.denoiseFormat == GL_RAYTRACING_DENOISE_FORMAT)
	{
		return 1;
	}

	g_glRaytracingLighting.pathTraceTexture = glRaytracingTexture_t();
	g_glRaytracingLighting.denoiseTemp[0] = glRaytracingTexture_t();
	g_glRaytracingLighting.denoiseTemp[1] = glRaytracingTexture_t();

	g_glRaytracingLighting.pathTraceTexture = glRaytracingCreateTexture2D(
		g_glRaytracingCmd.device.Get(),
		width,
		height,
		GL_RAYTRACING_DENOISE_FORMAT,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	for (int i = 0; i < 2; ++i)
	{
		g_glRaytracingLighting.denoiseTemp[i] = glRaytracingCreateTexture2D(
			g_glRaytracingCmd.device.Get(),
			width,
			height,
			GL_RAYTRACING_DENOISE_FORMAT,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	if (!g_glRaytracingLighting.pathTraceTexture.resource ||
		!g_glRaytracingLighting.denoiseTemp[0].resource ||
		!g_glRaytracingLighting.denoiseTemp[1].resource)
	{
		return 0;
	}

	g_glRaytracingLighting.denoiseWidth = width;
	g_glRaytracingLighting.denoiseHeight = height;
	g_glRaytracingLighting.denoiseFormat = GL_RAYTRACING_DENOISE_FORMAT;
	glRaytracingLightingResetDenoiseHistory();
	return 1;
}

static void glRaytracingLightingCreatePerPassDescriptors(
	const glRaytracingLightingPassDesc_t* pass,
	ID3D12Resource* topLevelAS,
	ID3D12Resource* rayOutputTexture,
	ID3D12Resource* pathTraceTexture,
	ID3D12Resource* denoiseATexture,
	ID3D12Resource* denoiseBTexture)
{
	D3D12_CPU_DESCRIPTOR_HANDLE base = g_glRaytracingLighting.descriptorHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC albedoSrv = {};
	albedoSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	albedoSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	albedoSrv.Format = pass->albedoFormat;
	albedoSrv.Texture2D.MipLevels = 1;
	g_glRaytracingCmd.device->CreateShaderResourceView(pass->albedoTexture, &albedoSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_ALBEDO_SRV));

	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrv = {};
	depthSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthSrv.Format = glRaytracingGetSrvFormatForDepth(pass->depthFormat);
	depthSrv.Texture2D.MipLevels = 1;
	g_glRaytracingCmd.device->CreateShaderResourceView(pass->depthTexture, &depthSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_DEPTH_SRV));

	D3D12_SHADER_RESOURCE_VIEW_DESC normalSrv = {};
	normalSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	normalSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	normalSrv.Format = pass->normalFormat;
	normalSrv.Texture2D.MipLevels = 1;
	g_glRaytracingCmd.device->CreateShaderResourceView(pass->normalTexture, &normalSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_NORMAL_SRV));

	D3D12_SHADER_RESOURCE_VIEW_DESC positionSrv = {};
	positionSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	positionSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	positionSrv.Format = pass->positionFormat;
	positionSrv.Texture2D.MipLevels = 1;
	g_glRaytracingCmd.device->CreateShaderResourceView(pass->positionTexture, &positionSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_POSITION_SRV));

	D3D12_SHADER_RESOURCE_VIEW_DESC tlasSrv = {};
	tlasSrv.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	tlasSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	tlasSrv.RaytracingAccelerationStructure.Location = topLevelAS->GetGPUVirtualAddress();
	g_glRaytracingCmd.device->CreateShaderResourceView(nullptr, &tlasSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_TLAS_SRV));

	D3D12_SHADER_RESOURCE_VIEW_DESC denoiseSrv = {};
	denoiseSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	denoiseSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	denoiseSrv.Format = GL_RAYTRACING_DENOISE_FORMAT;
	denoiseSrv.Texture2D.MipLevels = 1;

	g_glRaytracingCmd.device->CreateShaderResourceView(pathTraceTexture, &denoiseSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_PATHTRACE_SRV));
	g_glRaytracingCmd.device->CreateShaderResourceView(denoiseATexture, &denoiseSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_DENOISE_A_SRV));
	g_glRaytracingCmd.device->CreateShaderResourceView(denoiseBTexture, &denoiseSrv,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_DENOISE_B_SRV));

	D3D12_UNORDERED_ACCESS_VIEW_DESC rayOutputUav = {};
	rayOutputUav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	rayOutputUav.Format = GL_RAYTRACING_DENOISE_FORMAT;
	g_glRaytracingCmd.device->CreateUnorderedAccessView(rayOutputTexture, nullptr, &rayOutputUav,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_PATHTRACE_UAV));

	D3D12_UNORDERED_ACCESS_VIEW_DESC denoiseUav = {};
	denoiseUav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	denoiseUav.Format = GL_RAYTRACING_DENOISE_FORMAT;
	g_glRaytracingCmd.device->CreateUnorderedAccessView(denoiseATexture, nullptr, &denoiseUav,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_DENOISE_A_UAV));
	g_glRaytracingCmd.device->CreateUnorderedAccessView(denoiseBTexture, nullptr, &denoiseUav,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_DENOISE_B_UAV));

	D3D12_UNORDERED_ACCESS_VIEW_DESC outputUav = {};
	outputUav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	outputUav.Format = pass->outputFormat;
	g_glRaytracingCmd.device->CreateUnorderedAccessView(pass->outputTexture, nullptr, &outputUav,
		glRaytracingOffsetCpu(base, g_glRaytracingLighting.descriptorStride, GLR_DESC_OUTPUT_UAV));
}



// ============================================================
// Lighting public API
// ============================================================

static bool glRaytracingLightingExecuteInternal(
	const glRaytracingLightingPassDesc_t* pass,
	ID3D12Resource* topLevelAS)
{
	if (!g_glRaytracingLighting.initialized || !pass || !topLevelAS)
		return false;

	if (!pass->albedoTexture || !pass->depthTexture || !pass->normalTexture || !pass->positionTexture || !pass->outputTexture)
		return false;

	if (pass->width == 0 || pass->height == 0)
		return false;

	if (!glRaytracingLightingEnsureDenoiseResources(pass->width, pass->height))
		return false;

	const bool useInternalDenoiser =
		(g_glRaytracingLighting.constants.enableDenoiser != 0u) &&
		!g_glRaytracingLighting.externalDenoiser;

	ID3D12Resource* rayOutputTexture = useInternalDenoiser
		? g_glRaytracingLighting.pathTraceTexture.resource.Get()
		: pass->outputTexture;

	g_glRaytracingLighting.constants.screenSize[0] = (float)pass->width;
	g_glRaytracingLighting.constants.screenSize[1] = (float)pass->height;
	g_glRaytracingLighting.constants.screenSize[2] = 1.0f / (float)pass->width;
	g_glRaytracingLighting.constants.screenSize[3] = 1.0f / (float)pass->height;
	g_glRaytracingLighting.constants.frameIndex = g_glRaytracingLighting.frameCounter;
	g_glRaytracingLighting.constants.lightCount =
		(uint32_t)glRaytracingClamp<size_t>(g_glRaytracingLighting.cpuLights.size(), 0, GL_RAYTRACING_MAX_LIGHTS);

	glRaytracingLightingUpdateLights();
	glRaytracingLightingUpdateConstants();

	for (uint32_t passIndex = 0; passIndex < 3u; ++passIndex)
	{
		glRaytracingLightingConstants_t denoiseConstants = g_glRaytracingLighting.constants;
		denoiseConstants.denoisePassIndex = passIndex;
		denoiseConstants.denoiseStepWidth = (float)(1u << passIndex);
		glRaytracingLightingUploadConstantsTo(g_glRaytracingLighting.denoiseConstantBuffer[passIndex], denoiseConstants);
	}

	glRaytracingLightingCreatePerPassDescriptors(
		pass,
		topLevelAS,
		rayOutputTexture,
		g_glRaytracingLighting.pathTraceTexture.resource.Get(),
		g_glRaytracingLighting.denoiseTemp[0].resource.Get(),
		g_glRaytracingLighting.denoiseTemp[1].resource.Get());

	if (!glRaytracingBeginCmd())
		return false;

	if (useInternalDenoiser)
	{
		glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
			g_glRaytracingLighting.pathTraceTexture.resource.Get(),
			g_glRaytracingLighting.pathTraceTexture.state,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		g_glRaytracingLighting.pathTraceTexture.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	else
	{
		glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
			pass->outputTexture,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	ID3D12DescriptorHeap* heaps[] = { g_glRaytracingLighting.descriptorHeap.Get() };
	g_glRaytracingCmd.cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	g_glRaytracingCmd.cmdList->SetComputeRootSignature(g_glRaytracingLighting.globalRootSig.Get());

	D3D12_GPU_DESCRIPTOR_HANDLE gpuBase = g_glRaytracingLighting.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	g_glRaytracingCmd.cmdList->SetComputeRootDescriptorTable(0,
		glRaytracingOffsetGpu(gpuBase, g_glRaytracingLighting.descriptorStride, GLR_DESC_LIGHTS_SRV));
	g_glRaytracingCmd.cmdList->SetComputeRootDescriptorTable(1,
		glRaytracingOffsetGpu(gpuBase, g_glRaytracingLighting.descriptorStride, GLR_DESC_PATHTRACE_UAV));
	g_glRaytracingCmd.cmdList->SetComputeRootConstantBufferView(2, g_glRaytracingLighting.constantBuffer.gpuVA);
	g_glRaytracingCmd.cmdList->SetPipelineState1(g_glRaytracingLighting.rtStateObject.Get());

	const UINT shaderRecordSize = (UINT)glRaytracingAlignUp(
		D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
		D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	D3D12_DISPATCH_RAYS_DESC rays = {};
	rays.RayGenerationShaderRecord.StartAddress = g_glRaytracingLighting.raygenTable.gpuVA;
	rays.RayGenerationShaderRecord.SizeInBytes = shaderRecordSize;
	rays.MissShaderTable.StartAddress = g_glRaytracingLighting.missTable.gpuVA;
	rays.MissShaderTable.SizeInBytes = shaderRecordSize * 2u;
	rays.MissShaderTable.StrideInBytes = shaderRecordSize;
	rays.HitGroupTable.StartAddress = g_glRaytracingLighting.hitTable.gpuVA;
	rays.HitGroupTable.SizeInBytes = shaderRecordSize * 2u;
	rays.HitGroupTable.StrideInBytes = shaderRecordSize;
	rays.Width = pass->width;
	rays.Height = pass->height;
	rays.Depth = 1;
	g_glRaytracingCmd.cmdList->DispatchRays(&rays);

	D3D12_RESOURCE_BARRIER rawUav = {};
	rawUav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	rawUav.UAV.pResource = rayOutputTexture;
	g_glRaytracingCmd.cmdList->ResourceBarrier(1, &rawUav);

	if (useInternalDenoiser)
	{
		glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
			g_glRaytracingLighting.pathTraceTexture.resource.Get(),
			g_glRaytracingLighting.pathTraceTexture.state,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		g_glRaytracingLighting.pathTraceTexture.state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		for (int i = 0; i < 2; ++i)
		{
			glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
				g_glRaytracingLighting.denoiseTemp[i].resource.Get(),
				g_glRaytracingLighting.denoiseTemp[i].state,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			g_glRaytracingLighting.denoiseTemp[i].state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
			pass->outputTexture,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		g_glRaytracingCmd.cmdList->SetComputeRootSignature(g_glRaytracingLighting.globalRootSig.Get());
		g_glRaytracingCmd.cmdList->SetComputeRootDescriptorTable(0,
			glRaytracingOffsetGpu(gpuBase, g_glRaytracingLighting.descriptorStride, GLR_DESC_LIGHTS_SRV));
		g_glRaytracingCmd.cmdList->SetComputeRootDescriptorTable(1,
			glRaytracingOffsetGpu(gpuBase, g_glRaytracingLighting.descriptorStride, GLR_DESC_PATHTRACE_UAV));
		g_glRaytracingCmd.cmdList->SetPipelineState(g_glRaytracingLighting.denoisePSO.Get());

		const UINT groupsX = (pass->width + 7u) / 8u;
		const UINT groupsY = (pass->height + 7u) / 8u;

		// Pass 0: raw path trace -> temp A.
		g_glRaytracingCmd.cmdList->SetComputeRootConstantBufferView(2, g_glRaytracingLighting.denoiseConstantBuffer[0].gpuVA);
		g_glRaytracingCmd.cmdList->Dispatch(groupsX, groupsY, 1);

		D3D12_RESOURCE_BARRIER uavA = {};
		uavA.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavA.UAV.pResource = g_glRaytracingLighting.denoiseTemp[0].resource.Get();
		g_glRaytracingCmd.cmdList->ResourceBarrier(1, &uavA);

		glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
			g_glRaytracingLighting.denoiseTemp[0].resource.Get(),
			g_glRaytracingLighting.denoiseTemp[0].state,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		g_glRaytracingLighting.denoiseTemp[0].state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		// Pass 1: temp A -> temp B.
		g_glRaytracingCmd.cmdList->SetComputeRootConstantBufferView(2, g_glRaytracingLighting.denoiseConstantBuffer[1].gpuVA);
		g_glRaytracingCmd.cmdList->Dispatch(groupsX, groupsY, 1);

		D3D12_RESOURCE_BARRIER uavB = {};
		uavB.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavB.UAV.pResource = g_glRaytracingLighting.denoiseTemp[1].resource.Get();
		g_glRaytracingCmd.cmdList->ResourceBarrier(1, &uavB);

		glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
			g_glRaytracingLighting.denoiseTemp[1].resource.Get(),
			g_glRaytracingLighting.denoiseTemp[1].state,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		g_glRaytracingLighting.denoiseTemp[1].state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		// Pass 2: temp B -> final output.
		g_glRaytracingCmd.cmdList->SetComputeRootConstantBufferView(2, g_glRaytracingLighting.denoiseConstantBuffer[2].gpuVA);
		g_glRaytracingCmd.cmdList->Dispatch(groupsX, groupsY, 1);

		D3D12_RESOURCE_BARRIER outputUav = {};
		outputUav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		outputUav.UAV.pResource = pass->outputTexture;
		g_glRaytracingCmd.cmdList->ResourceBarrier(1, &outputUav);
	}

	glRaytracingTransition(g_glRaytracingCmd.cmdList.Get(),
		pass->outputTexture,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	if (!glRaytracingEndCmd())
		return false;

	++g_glRaytracingLighting.frameCounter;
	return true;
}

static ID3D12Resource* glRaytracingResolveTLASForWorld(glRaytracingSceneHandle_t worldHandle)
{
	glRaytracingRenderWorld_t* world = glRaytracingFindWorld(worldHandle);
	if (!world)
		return nullptr;

	if (!glRaytracingBuildDirtyMeshesInternal())
		return nullptr;

	if (!glRaytracingBuildSceneInternal(world))
		return nullptr;

	if (!world->tlasBuilt)
		return nullptr;

	return glRaytracingGetCurrentTLASBuffer(world)->resource.Get();
}

bool glRaytracingLightingInit(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (g_glRaytracingLighting.initialized)
		return true;

	if (!glRaytracingInitCmdContext())
		return false;

	if (!glRaytracingLightingCreateDescriptorHeap())
		return false;

	if (!glRaytracingLightingCreateRootSignatures())
		return false;

	if (!glRaytracingLightingCreateBuffers())
		return false;

	glRaytracingLightingCreatePersistentLightSRV();

	if (!glRaytracingLightingCreateStateObject())
		return false;

	if (!glRaytracingLightingCreateShaderTables())
		return false;

	if (!glRaytracingLightingCreateDenoisePipeline())
		return false;

	memset(&g_glRaytracingLighting.constants, 0, sizeof(g_glRaytracingLighting.constants));
	glRaytracingSetIdentity4x4(g_glRaytracingLighting.constants.invViewProj);
	glRaytracingSetIdentity4x4(g_glRaytracingLighting.constants.invViewMatrix);
	glRaytracingSetIdentity4x4(g_glRaytracingLighting.constants.viewProj);
	g_glRaytracingLighting.constants.ambientColor[0] = 0.08f;
	g_glRaytracingLighting.constants.ambientColor[1] = 0.08f;
	g_glRaytracingLighting.constants.ambientColor[2] = 0.09f;
	g_glRaytracingLighting.constants.ambientColor[3] = 1.0f;
	g_glRaytracingLighting.constants.enableSpecular = 1;
	g_glRaytracingLighting.constants.enableHalfLambert = 1;
	g_glRaytracingLighting.constants.normalReconstructZ = 1.0f;
	g_glRaytracingLighting.constants.shadowBias = 1.5f;
	g_glRaytracingLighting.constants.frameIndex = 0;
	g_glRaytracingLighting.constants.samplesPerPixel = 2;
	g_glRaytracingLighting.constants.maxBounces = 2;
	g_glRaytracingLighting.constants.enableDenoiser = 1;
	g_glRaytracingLighting.constants.denoisePassIndex = 0;
	g_glRaytracingLighting.constants.denoiseStepWidth = 1.0f;
	g_glRaytracingLighting.constants.denoiseStrength = 1.0f;
	g_glRaytracingLighting.constants.denoisePhiColor = 8.0f;
	g_glRaytracingLighting.constants.denoisePhiNormal = 64.0f;
	g_glRaytracingLighting.constants.denoisePhiPosition = 0.045f;
	g_glRaytracingLighting.constants.bumpStrength = 2.35f;
	glRaytracingLightingResetDenoiseHistory();

	glRaytracingLightingUpdateConstants();

	g_glRaytracingLighting.initialized = true;
	glRaytracingLog("glRaytracingLightingInit ok");
	return true;
}

void glRaytracingLightingShutdown(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingLighting.initialized)
		return;

	g_glRaytracingLighting = glRaytracingLightingState_t();
}

bool glRaytracingLightingIsInitialized(void)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);
	return g_glRaytracingLighting.initialized;
}

void glRaytracingLightingClearLights(bool clearPersistant)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (clearPersistant)
	{
		g_glRaytracingLighting.cpuLights.clear();
	}
	else
	{
		size_t writeIndex = 0;

		for (size_t i = 0; i < g_glRaytracingLighting.cpuLights.size(); ++i)
		{
			if (g_glRaytracingLighting.cpuLights[i].persistant)
			{
				if (writeIndex != i)
				{
					g_glRaytracingLighting.cpuLights[writeIndex] = g_glRaytracingLighting.cpuLights[i];
				}
				++writeIndex;
			}
		}

		g_glRaytracingLighting.cpuLights.resize(writeIndex);
	}

	g_glRaytracingLighting.constants.lightCount =
		(uint32_t)g_glRaytracingLighting.cpuLights.size();

	glRaytracingLightingUpdateConstants();
}

bool glRaytracingLightingAddLight(const glRaytracingLight_t* light)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!g_glRaytracingLighting.initialized || !light)
		return false;

	if (g_glRaytracingLighting.cpuLights.size() >= GL_RAYTRACING_MAX_LIGHTS)
		return false;

	g_glRaytracingLighting.cpuLights.push_back(*light);
	g_glRaytracingLighting.constants.lightCount = (uint32_t)g_glRaytracingLighting.cpuLights.size();

	glRaytracingLightingUpdateLights();
	glRaytracingLightingUpdateConstants();
	return true;
}

void glRaytracingLightingSetAmbient(float r, float g, float b, float intensity)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (g_glRaytracingLighting.constants.ambientColor[0] != r ||
		g_glRaytracingLighting.constants.ambientColor[1] != g ||
		g_glRaytracingLighting.constants.ambientColor[2] != b ||
		g_glRaytracingLighting.constants.ambientColor[3] != intensity)
	{
		glRaytracingLightingResetDenoiseHistory();
	}

	g_glRaytracingLighting.constants.ambientColor[0] = r;
	g_glRaytracingLighting.constants.ambientColor[1] = g;
	g_glRaytracingLighting.constants.ambientColor[2] = b;
	g_glRaytracingLighting.constants.ambientColor[3] = intensity;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetCameraPosition(float x, float y, float z)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (g_glRaytracingLighting.constants.cameraPos[0] != x ||
		g_glRaytracingLighting.constants.cameraPos[1] != y ||
		g_glRaytracingLighting.constants.cameraPos[2] != z)
	{
		glRaytracingLightingResetDenoiseHistory();
	}

	g_glRaytracingLighting.constants.cameraPos[0] = x;
	g_glRaytracingLighting.constants.cameraPos[1] = y;
	g_glRaytracingLighting.constants.cameraPos[2] = z;
	g_glRaytracingLighting.constants.cameraPos[3] = 1.0f;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetInvViewProjMatrix(const float* m16)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!m16)
		return;

	if (memcmp(g_glRaytracingLighting.constants.invViewProj, m16, sizeof(float) * 16) != 0)
		glRaytracingLightingResetDenoiseHistory();

	memcpy(g_glRaytracingLighting.constants.invViewProj, m16, sizeof(float) * 16);

	if (!glRaytracingInvertMatrix4x4(m16, g_glRaytracingLighting.constants.viewProj))
		glRaytracingSetIdentity4x4(g_glRaytracingLighting.constants.viewProj);

	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetInvViewMatrix(const float* m16)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (!m16)
		return;

	if (memcmp(g_glRaytracingLighting.constants.invViewMatrix, m16, sizeof(float) * 16) != 0)
		glRaytracingLightingResetDenoiseHistory();

	memcpy(g_glRaytracingLighting.constants.invViewMatrix, m16, sizeof(float) * 16);
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetNormalReconstructSign(float signValue)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (g_glRaytracingLighting.constants.normalReconstructZ != signValue)
		glRaytracingLightingResetDenoiseHistory();

	g_glRaytracingLighting.constants.normalReconstructZ = signValue;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetBumpStrength(float strength)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	strength = glRaytracingClamp<float>(strength, 0.0f, 8.0f);
	if (g_glRaytracingLighting.constants.bumpStrength != strength)
		glRaytracingLightingResetDenoiseHistory();

	g_glRaytracingLighting.constants.bumpStrength = strength;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingEnableSpecular(int enable)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	uint32_t value = enable ? 1u : 0u;
	if (g_glRaytracingLighting.constants.enableSpecular != value)
		glRaytracingLightingResetDenoiseHistory();

	g_glRaytracingLighting.constants.enableSpecular = value;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingEnableHalfLambert(int enable)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	uint32_t value = enable ? 1u : 0u;
	if (g_glRaytracingLighting.constants.enableHalfLambert != value)
		glRaytracingLightingResetDenoiseHistory();

	g_glRaytracingLighting.constants.enableHalfLambert = value;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetShadowBias(float bias)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	if (g_glRaytracingLighting.constants.shadowBias != bias)
		glRaytracingLightingResetDenoiseHistory();

	g_glRaytracingLighting.constants.shadowBias = bias;
	glRaytracingLightingUpdateConstants();
}


void glRaytracingLightingSetPathTracingOptions(uint32_t samplesPerPixel, uint32_t maxBounces, int enableDenoiser, float denoiseStrength)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	samplesPerPixel = glRaytracingClamp<uint32_t>(samplesPerPixel ? samplesPerPixel : 1u, 1u, 8u);
	maxBounces = glRaytracingClamp<uint32_t>(maxBounces ? maxBounces : 1u, 1u, 4u);
	uint32_t denoiser = enableDenoiser ? 1u : 0u;
	denoiseStrength = glRaytracingClamp<float>(denoiseStrength, 0.0f, 1.0f);

	if (g_glRaytracingLighting.constants.samplesPerPixel != samplesPerPixel ||
		g_glRaytracingLighting.constants.maxBounces != maxBounces ||
		g_glRaytracingLighting.constants.enableDenoiser != denoiser ||
		g_glRaytracingLighting.constants.denoiseStrength != denoiseStrength)
	{
		glRaytracingLightingResetDenoiseHistory();
	}

	g_glRaytracingLighting.constants.samplesPerPixel = samplesPerPixel;
	g_glRaytracingLighting.constants.maxBounces = maxBounces;
	g_glRaytracingLighting.constants.enableDenoiser = denoiser;
	g_glRaytracingLighting.constants.denoiseStrength = denoiseStrength;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetDenoiseTuning(float phiColor, float phiNormal, float phiPosition)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	phiColor = glRaytracingClamp<float>(phiColor, 0.001f, 64.0f);
	phiNormal = glRaytracingClamp<float>(phiNormal, 1.0f, 128.0f);
	phiPosition = glRaytracingClamp<float>(phiPosition, 0.001f, 4.0f);

	if (g_glRaytracingLighting.constants.denoisePhiColor != phiColor ||
		g_glRaytracingLighting.constants.denoisePhiNormal != phiNormal ||
		g_glRaytracingLighting.constants.denoisePhiPosition != phiPosition)
	{
		glRaytracingLightingResetDenoiseHistory();
	}

	g_glRaytracingLighting.constants.denoisePhiColor = phiColor;
	g_glRaytracingLighting.constants.denoisePhiNormal = phiNormal;
	g_glRaytracingLighting.constants.denoisePhiPosition = phiPosition;
	glRaytracingLightingUpdateConstants();
}

void glRaytracingLightingSetExternalDenoiser(int enabled)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	const bool newValue = enabled ? true : false;
	if (g_glRaytracingLighting.externalDenoiser != newValue)
		glRaytracingLightingResetDenoiseHistory();

	g_glRaytracingLighting.externalDenoiser = newValue;
}

void glRaytracingLightingUseExternalDenoiser(int enabled)
{
	glRaytracingLightingSetExternalDenoiser(enabled);
}

bool glRaytracingLightingExecuteForScene(const glRaytracingLightingPassDesc_t* pass, glRaytracingSceneHandle_t worldHandle)
{
	std::lock_guard<std::mutex> lock(g_glRaytracingMutex);

	ID3D12Resource* topLevelAS = glRaytracingResolveTLASForWorld(worldHandle);
	if (!topLevelAS)
		return false;

	return glRaytracingLightingExecuteInternal(pass, topLevelAS);
}

glRaytracingLight_t glRaytracingLightingMakePointLight(
	float px, float py, float pz,
	float radiusX, float radiusY, float radiusZ,
	float r, float g, float b,
	float intensity)
{
	glRaytracingLight_t l = {};
	float ax = (radiusX < 0.0f) ? -radiusX : radiusX;
	float ay = (radiusY < 0.0f) ? -radiusY : radiusY;
	float az = (radiusZ < 0.0f) ? -radiusZ : radiusZ;
	float maxRadius = ax;
	if (ay > maxRadius) maxRadius = ay;
	if (az > maxRadius) maxRadius = az;
	if (maxRadius <= 0.0f) maxRadius = 1e-4f;

	if (ax <= 0.0f) ax = maxRadius;
	if (ay <= 0.0f) ay = maxRadius;
	if (az <= 0.0f) az = maxRadius;

	l.position.x = px;
	l.position.y = py;
	l.position.z = pz;

	// Keep radius populated as a scalar fallback/max range, but point lights now
	// attenuate using pointRadius.x/y/z in the ray generation shader.
	l.radius = maxRadius;
	l.pointRadius.x = ax;
	l.pointRadius.y = ay;
	l.pointRadius.z = az;
	l.pointRadiusPad = 0.0f;

	l.color.x = r;
	l.color.y = g;
	l.color.z = b;
	l.intensity = intensity;

	l.normal.x = 0.0f;
	l.normal.y = 0.0f;
	l.normal.z = 1.0f;
	l.type = GL_RAYTRACING_LIGHT_TYPE_POINT;

	l.axisU.x = 1.0f;
	l.axisU.y = 0.0f;
	l.axisU.z = 0.0f;
	l.halfWidth = 0.0f;

	l.axisV.x = 0.0f;
	l.axisV.y = 1.0f;
	l.axisV.z = 0.0f;
	l.halfHeight = 0.0f;

	l.samples = 1;
	l.twoSided = 0;
	l.persistant = 0.0f;
	l.pad1 = 0.0f;
	return l;
}


glRaytracingLight_t glRaytracingLightingMakeSpotLight(
	float px, float py, float pz,
	float dx, float dy, float dz,
	float ux, float uy, float uz,
	float vx, float vy, float vz,
	float nearPlane,
	float farPlane,
	float tanHalfWidth,
	float tanHalfHeight,
	float r, float g, float b,
	float intensity,
	uint32_t samples)
{
	glRaytracingLight_t l = {};

	glRaytracingNormalize3(dx, dy, dz);

	// Make U perpendicular to D.
	{
		const float du = dx * ux + dy * uy + dz * uz;
		ux -= dx * du;
		uy -= dy * du;
		uz -= dz * du;

		const float uLenSq = ux * ux + uy * uy + uz * uz;
		if (uLenSq <= 1e-20f)
		{
			const float absDz = (dz < 0.0f) ? -dz : dz;
			if (absDz < 0.999f)
			{
				glRaytracingCross3(0.0f, 0.0f, 1.0f, dx, dy, dz, ux, uy, uz);
			}
			else
			{
				glRaytracingCross3(0.0f, 1.0f, 0.0f, dx, dy, dz, ux, uy, uz);
			}
		}
		glRaytracingNormalize3(ux, uy, uz);
	}

	// Rebuild V from D x U so the basis is orthonormal, while preserving the
	// sign of the caller-provided V whenever possible.
	{
		float builtVx, builtVy, builtVz;
		glRaytracingCross3(dx, dy, dz, ux, uy, uz, builtVx, builtVy, builtVz);
		glRaytracingNormalize3(builtVx, builtVy, builtVz);

		const float sign = builtVx * vx + builtVy * vy + builtVz * vz;
		if (sign < 0.0f)
		{
			builtVx = -builtVx;
			builtVy = -builtVy;
			builtVz = -builtVz;
		}

		vx = builtVx;
		vy = builtVy;
		vz = builtVz;
	}

	if (nearPlane < 0.0f)
		nearPlane = 0.0f;
	if (farPlane <= nearPlane)
		farPlane = nearPlane + 1e-3f;

	if (tanHalfWidth < 0.0f)  tanHalfWidth = -tanHalfWidth;
	if (tanHalfHeight < 0.0f) tanHalfHeight = -tanHalfHeight;

	if (tanHalfWidth <= 1e-4f)
		tanHalfWidth = 1e-4f;
	if (tanHalfHeight <= 1e-4f)
		tanHalfHeight = 1e-4f;

	l.position.x = px;
	l.position.y = py;
	l.position.z = pz;

	// For spot lights, radius stores the far clip distance while pointRadius.x
	// stores the near clip distance.
	l.radius = farPlane;
	l.pointRadius.x = nearPlane;
	l.pointRadius.y = 0.0f;
	l.pointRadius.z = 0.0f;
	l.pointRadiusPad = 0.0f;

	l.color.x = r;
	l.color.y = g;
	l.color.z = b;
	l.intensity = intensity;

	l.normal.x = dx;
	l.normal.y = dy;
	l.normal.z = dz;
	l.type = GL_RAYTRACING_LIGHT_TYPE_SPOT;

	l.axisU.x = ux;
	l.axisU.y = uy;
	l.axisU.z = uz;
	l.halfWidth = tanHalfWidth;

	l.axisV.x = vx;
	l.axisV.y = vy;
	l.axisV.z = vz;
	l.halfHeight = tanHalfHeight;

	l.samples = samples ? samples : 1u;
	l.twoSided = 0;
	l.persistant = 0.0f;
	l.pad1 = 0.0f;

	return l;
}

glRaytracingLight_t glRaytracingLightingMakeRectLight(
	float px, float py, float pz,
	float nx, float ny, float nz,
	float ux, float uy, float uz,
	float vx, float vy, float vz,
	float halfWidth, float halfHeight,
	float r, float g, float b,
	float intensity,
	uint32_t samples,
	uint32_t twoSided)
{
	glRaytracingLight_t l = {};

	glRaytracingNormalize3(nx, ny, nz);
	glRaytracingNormalize3(ux, uy, uz);
	glRaytracingNormalize3(vx, vy, vz);

	if ((nx == 0.0f && ny == 0.0f && nz == 0.0f) &&
		!((ux == 0.0f && uy == 0.0f && uz == 0.0f) ||
			(vx == 0.0f && vy == 0.0f && vz == 0.0f)))
	{
		glRaytracingCross3(ux, uy, uz, vx, vy, vz, nx, ny, nz);
		glRaytracingNormalize3(nx, ny, nz);
	}

	l.position.x = px;
	l.position.y = py;
	l.position.z = pz;

	// Reuse radius as influence/falloff range for the rect light.
	l.radius = (halfWidth > halfHeight ? halfWidth : halfHeight) * 6.0f;
	l.pointRadius.x = l.radius;
	l.pointRadius.y = l.radius;
	l.pointRadius.z = l.radius;
	l.pointRadiusPad = 0.0f;

	l.color.x = r;
	l.color.y = g;
	l.color.z = b;
	l.intensity = intensity;

	l.normal.x = nx;
	l.normal.y = ny;
	l.normal.z = nz;
	l.type = GL_RAYTRACING_LIGHT_TYPE_RECT;

	l.axisU.x = ux;
	l.axisU.y = uy;
	l.axisU.z = uz;
	l.halfWidth = halfWidth;

	l.axisV.x = vx;
	l.axisV.y = vy;
	l.axisV.z = vz;
	l.halfHeight = halfHeight;

	l.samples = samples ? samples : 4u;
	l.twoSided = twoSided ? 1u : 0u;
	l.persistant = 0.0f;
	l.pad1 = 0.0f;

	return l;
}

uint32_t glRaytracingLightingGetLightCount(void)
{
	return (uint32_t)g_glRaytracingLighting.cpuLights.size();
}

