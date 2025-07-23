#pragma once
#include "pch.h"
#include <string>
namespace transforms {
    class SharedDescriptorHeapV2;
    class MyImguiManager
    {
    public:
        MyImguiManager(HWND hwnd,
            ID3D12Device* device, ID3D12CommandQueue* queue,
            SharedDescriptorHeapV2* sharedHeap);
        void BeginFrame();
        void PushImguiWindow(const std::string& name, int w, int h);
        void PushButton(const std::string& name, std::function<void()> onClick);
        void FloatInput(const std::string& name, float& value, float inc=1.0f, float fastInc=10.0f);
        void PopImguiWindow();
        void EndFrame(ID3D12GraphicsCommandList* commandList);
       
    private:
        HWND hwnd;
        ID3D12Device* device;
        SharedDescriptorHeapV2* sharedHeap;
        uint32_t windowStackCount = 0;
    };
}

