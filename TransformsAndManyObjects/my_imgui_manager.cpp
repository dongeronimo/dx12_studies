#include "pch.h"
#include "my_imgui_manager.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "shared_descriptor_heap_v2.h"
transforms::SharedDescriptorHeapV2* gSharedHeap = nullptr;
transforms::MyImguiManager::MyImguiManager(HWND hwnd,
	ID3D12Device* device, ID3D12CommandQueue* queue, SharedDescriptorHeapV2* sharedHeap)
	:hwnd(hwnd), device(device), sharedHeap(sharedHeap)
{
	assert(gSharedHeap == nullptr);
	gSharedHeap = sharedHeap;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup platform/backend bindings
	ImGui_ImplWin32_Init(hwnd); // Your Win32 window handle

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = device;
	init_info.CommandQueue = queue;
	init_info.NumFramesInFlight = 2;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	init_info.SrvDescriptorHeap = sharedHeap->GetHeap();
	init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
		{
			assert(gSharedHeap != nullptr);
			auto [cpu, gpu] = gSharedHeap->AllocateDescriptor();
			*out_cpu = cpu;
			*out_gpu = gpu;
		};
	init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			// No-op (we never free)
		};

	ImGui_ImplDX12_Init(&init_info);
}

void transforms::MyImguiManager::BeginFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();   // if using Win32
	ImGui::NewFrame();
	windowStackCount = 0;
}

void transforms::MyImguiManager::PushImguiWindow(const std::string& name, int w, int h)
{
	//TODO imgui: be able to create child windows
	ImVec2 windowSize(w, h); 
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once); // Set size the first time window appears
	ImGui::Begin(name.c_str());
	windowStackCount++;
}

void transforms::MyImguiManager::PushButton(const std::string& name, std::function<void()> onClick)
{
	if (ImGui::Button(name.c_str()))
	{
		onClick();
	}
}

void transforms::MyImguiManager::FloatInput(const std::string& name, float& value, 
	float inc, float fastInc)
{
	ImGui::InputFloat(name.c_str(), &value, inc, fastInc, "%.5f");
}

void transforms::MyImguiManager::PopImguiWindow()
{
	//TODO imgui: be able to create child windows
	ImGui::End();
	windowStackCount--;
}

void transforms::MyImguiManager::EndFrame(ID3D12GraphicsCommandList* commandList)
{
	//TODO imgui: be able to create child windows
	for (auto i = 0; i < windowStackCount; i++) {
		ImGui::End();
	}
	ImGui::Render();  // Finalize ImGui draw data
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}
