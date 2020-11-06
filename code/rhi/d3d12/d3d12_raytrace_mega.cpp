// d3d12_raytrace_mega.cpp
//

#include "precompiled.h"
#include "d3d12_local.h"

struct MegaTexture_t {
	ComPtr<ID3D12Resource> textureUploadHeap;
	ComPtr<ID3D12Resource> texture2D;
};

MegaTexture_t megaTexture;
MegaTexture_t megaTextureNormal;

extern ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

void GL_LoadMegaTexture(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr) {
	int width = r_megaTextureSize.GetInteger();
	int height = r_megaTextureSize.GetInteger();

	//byte* buffer;
	//LoadTGA("mega/mega.tga", &buffer, &width, &height, NULL);
	//FILE* f = fopen("base/mega/mega.raw", "rb");
	//fread(buffer, 1, width * height * 4, f);
	//fclose(f);

	byte* buffer = tr.diffuseMegaTexture->GetMegaBuffer();

	// Create the texture.
	{
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&megaTexture.texture2D)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(megaTexture.texture2D.Get(), 0, 1);

		// Create the GPU upload buffer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&megaTexture.textureUploadHeap)));

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &buffer[0];
		textureData.RowPitch = width * 4;
		textureData.SlicePitch = textureData.RowPitch * height;

		UpdateSubresources(m_commandList.Get(), megaTexture.texture2D.Get(), megaTexture.textureUploadHeap.Get(), 0, 0, 1, &textureData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(megaTexture.texture2D.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(megaTexture.texture2D.Get(), &srvDesc, srvPtr);
	}
}


void GL_LoadMegaNormalTexture(D3D12_CPU_DESCRIPTOR_HANDLE& srvPtr) {
	int width = r_megaTextureSize.GetInteger();
	int height = r_megaTextureSize.GetInteger();

	//byte* buffer;
	//LoadTGA("mega/mega_local.tga", &buffer, &width, &height, NULL);
	//FILE* f = fopen("base/mega/mega.raw", "rb");
	//fread(buffer, 1, width * height * 4, f);
	//fclose(f);

	byte* buffer = tr.normalMegaTexture->GetMegaBuffer();

	// Create the texture.
	{
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&megaTextureNormal.texture2D)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(megaTextureNormal.texture2D.Get(), 0, 1);

		// Create the GPU upload buffer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&megaTextureNormal.textureUploadHeap)));

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &buffer[0];
		textureData.RowPitch = width * 4;
		textureData.SlicePitch = textureData.RowPitch * height;

		UpdateSubresources(m_commandList.Get(), megaTextureNormal.texture2D.Get(), megaTextureNormal.textureUploadHeap.Get(), 0, 0, 1, &textureData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(megaTextureNormal.texture2D.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(megaTextureNormal.texture2D.Get(), &srvDesc, srvPtr);
	}
}
