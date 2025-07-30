#pragma once
#include "pch.h"
#include "cube_map_shadow_map.h"
#include "per_object_uniform_buffer.h"
#include "components.h"
namespace transforms {
    namespace _PointShadowCalculationSystem {
        inline void BeginCubeMapEvent(UINT i,
            ID3D12GraphicsCommandList* commandList)
        {
            std::wstring label = L"";
            switch (i) {
            case 0:
                commandList->BeginEvent(0, L"+X", sizeof(L"+X"));
                break;
            case 1:
                commandList->BeginEvent(0, L"-X", sizeof(L"-X"));
                break;
            case 2:
                commandList->BeginEvent(0, L"+Y", sizeof(L"+Y"));
                break;
            case 3:
                commandList->BeginEvent(0, L"-Y", sizeof(L"-Y"));
                break;
            case 4:
                commandList->BeginEvent(0, L"+Z", sizeof(L"+Z"));
                break;
            case 5:
                commandList->BeginEvent(0, L"-Z", sizeof(L"-Z"));
                break;
            }
        }
        inline void Clear(std::shared_ptr<transforms::CubeMapShadowMap> sm,
            ID3D12GraphicsCommandList* commandList, int i) {
            sm->SetAsRenderTarget(commandList, i);
            sm->Clear(commandList, i, { 1.f,1.f,1.f,1.f });
        }

        inline void SetViewport(ID3D12GraphicsCommandList* commandList) {
            D3D12_VIEWPORT viewport = {};
            viewport.Width = static_cast<float>(1024);
            viewport.Height = static_cast<float>(1024);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            commandList->RSSetViewports(1, &viewport);
            D3D12_RECT scissorRect = {};
            scissorRect.left = 0;
            scissorRect.top = 0;
            scissorRect.right = static_cast<float>(1024);
            scissorRect.bottom = static_cast<float>(1024);
            commandList->RSSetScissorRects(1, &scissorRect);
        }
    }
    template<typename RenderablesView>
    void DrawRenderablesForPointLightShadowMapSystem(
        RenderablesView&& renderables, 
        ID3D12GraphicsCommandList* commandList, 
        UINT& shadowDataId) {
        renderables.each([&commandList, &shadowDataId](entt::entity entity, const transforms::components::Renderable& renderable, const transforms::components::Transform& transform, const std::shared_ptr<transforms::components::BSDFMaterial> material)
            {
                commandList->IASetVertexBuffers(0, 1, &renderable.mVertexBufferView);
                commandList->IASetIndexBuffer(&renderable.mIndexBufferView);
                commandList->SetGraphicsRoot32BitConstant(0, renderable.uniformBufferId, 0);
                commandList->SetGraphicsRoot32BitConstant(1, shadowDataId, 0);
                commandList->DrawIndexedInstanced(renderable.mNumberOfIndices, 1, 0, 0, 0);
            });
    }

    template<typename ShadowProjectorsView, typename RenderablesView>
    void PointShadowMapCalculationSystem(
        ShadowProjectorsView&& shadowProjectors,
        RenderablesView&& renderables,
        UINT frameIndex, 
        ID3D12RootSignature* rootSignature,
        ID3D12GraphicsCommandList* commandList,
        ID3D12PipelineState* shadowPipeline,
        transforms::SharedDescriptorHeapV2* gSharedDescriptors,
        transforms::UniformBufferForSRVs<transforms::PerObjectData>* gPerObjectUniformBuffer,
        transforms::UniformBufferForSRVs<transforms::ShadowMapConstants>* gPointShadowUniformBuffer
        ) 
    {
        using namespace DirectX;
        using namespace _PointShadowCalculationSystem;
        UINT shadowDataId = 0;
        commandList->SetGraphicsRootSignature(rootSignature);
        ID3D12DescriptorHeap* heaps[] = { gSharedDescriptors->GetHeap() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetPipelineState(shadowPipeline);

        shadowProjectors.each(
            [&commandList, frameIndex, &rootSignature, &renderables, &shadowDataId, &gPerObjectUniformBuffer,
            &gPointShadowUniformBuffer]
            (entt::entity e, transforms::components::Transform& t, transforms::components::PointLight& pl,
                std::shared_ptr<transforms::CubeMapShadowMap> sm) 
            {
                sm->TransitionToRenderTarget(commandList, 0);
                for (int i = 0; i < 6; i++) 
                {
                    BeginCubeMapEvent(i, commandList);

                    Clear(sm, commandList, i);
                    //bind per-object srv at t0
                    commandList->SetGraphicsRootDescriptorTable(2,
                        gPerObjectUniformBuffer->GetGPUHandle(frameIndex));
                    //bind shadow data srv at t1
                    commandList->SetGraphicsRootDescriptorTable(3,
                        gPointShadowUniformBuffer->GetGPUHandle(frameIndex));
                    SetViewport(commandList);
                    DrawRenderablesForPointLightShadowMapSystem(renderables, commandList, shadowDataId);
                    commandList->EndEvent();
                    shadowDataId++;
                }
                sm->TransitionToPixelShaderResource(commandList, 0);
            });
        
    }
}