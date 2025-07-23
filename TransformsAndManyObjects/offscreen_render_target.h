#pragma once
#include "pch.h"
namespace transforms {
    class Context;
    class SharedDescriptorHeapV2;
    class OffscreenRenderTarget
    {
    public:
        OffscreenRenderTarget(int w, int h, Context* ctx,
            SharedDescriptorHeapV2* descriptorHeap);
        void SetAsRenderTarget(ID3D12GraphicsCommandList* commandList, int frameIndex);
        void Clear(ID3D12GraphicsCommandList* commandList, int frameIndex, std::array<float,4> clearColor);
        void TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList, int frameIndex);
        void TransitionToPixelShaderResource(ID3D12GraphicsCommandList* commandList, int frameIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandle(int frameIndex) {
            return srvGPUHandle[frameIndex];
        };
    private:
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srvCPUHandle;
        std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> srvGPUHandle;

        std::vector<ID3D12Resource*> renderTargetTexture;
        std::vector<ID3D12Resource*> depthTexture;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandle;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsvHandle;

    };
}

