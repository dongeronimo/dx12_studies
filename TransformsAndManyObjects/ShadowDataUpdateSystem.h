#pragma once
#include "pch.h"
#include "per_object_uniform_buffer.h"
#include "cube_map_shadow_map.h"
#include "components.h"
namespace transforms {
    template<typename View>
    void ShadowDataDataUploadSystem(View&& view, transforms::UniformBufferForSRVs<transforms::ShadowMapConstants>* shadowMapUniformBuffer, UINT frameIndex) {
        //TODO SHADOW: for each shadow and each of their 6 faces, calculate the view matrix, set the data, etc
        UINT id = 0;
        view.each(
            [&shadowMapUniformBuffer, frameIndex, &id]
            (   entt::entity e, 
                transforms::components::Transform& t, 
                transforms::components::PointLight& pl, 
                std::shared_ptr<transforms::CubeMapShadowMap> sm)
            {
                sm->UpdateLightPosition(t.GetWorldPosition());
                for (int i = 0; i < 6; i++) 
                {
                    transforms::ShadowMapConstants constants = {};
                    DirectX::XMStoreFloat4x4(&constants.viewMatrix, XMMatrixTranspose(sm->GetViewMatrix(i)));
                    DirectX::XMStoreFloat4x4(&constants.projMatrix, XMMatrixTranspose(sm->GetProjectionMatrix()));
                    constants.lightPosition = t.GetWorldPosition();
                    constants.farPlane = sm->GetFarPlane();
                    shadowMapUniformBuffer->SetValue(frameIndex, id, constants);
                    id++;
                }
            }
        );
    }
}