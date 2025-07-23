#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "d3dx12.h"
#include <functional>
/// <summary>
/// I need to know how many renderables are there, that's how i establish the unique id for the renderable
/// component.
/// </summary>
/// <returns></returns>
size_t GetNumberOfRenderables(const entt::registry& gRegistry);
namespace transforms
{
    namespace components {

        namespace tags {
            struct MainCamera {};
            struct SceneOffscreenRenderResult{};
        }
        /// <summary>
        /// The light's intensity at a distance 'd' from the source is inversely proportional to 
        /// 1 / (attenuationConstant + attenuationLinear * d + attenuationQuadratic * d*d).
        /// Mind that some of the attenuations may be zero.
        /// Also the colors are not clamped nor normalized to [0,1] because they can be used to do PBR and
        /// values beyond 1 matter. If your shader needs normalized color you have to take that into account.
        /// </summary>
        struct PointLight {
            // The light's intensity at a distance 'd' from the source is inversely proportional to 
            // 
            float attenuationConstant;
            float attenuationLinear;
            float attenuationQuadratic;
            float _notUsed; //for alignment
            DirectX::XMFLOAT4 ColorDiffuse;
            DirectX::XMFLOAT4 ColorSpecular;
            DirectX::XMFLOAT4 ColorAmbient;
        };
        struct Perspective {
            float fovDegrees;
            float ratio;
            float zNear;
            float zFar;
        };


        struct Script {
            std::function<void(float deltaTime, entt::entity self, entt::registry& registry)> execute;
        };
        /// <summary>
        /// A transform. Entities that have a position on screen need this.
        /// </summary>
        struct Transform {
            DirectX::XMMATRIX parentMatrix = DirectX::XMMatrixIdentity();
            DirectX::XMFLOAT3 position = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
            DirectX::XMFLOAT4 rotation = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f); // Quaternion (x,y,z,w)
            DirectX::XMFLOAT3 scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
            DirectX::XMFLOAT3 rotationAsEulers = DirectX::XMFLOAT3(0.f, 0, 0);
            DirectX::XMMATRIX worldMatrix, localMatrix;
            DirectX::XMFLOAT3 GetWorldPosition()
            {
                // Load SIMD vectors from the input XMFLOAT types for efficient calculation
                DirectX::XMVECTOR scaleVector = XMLoadFloat3(&scale);
                DirectX::XMVECTOR rotationQuaternion = XMLoadFloat4(&rotation);
                DirectX::XMVECTOR positionVector = XMLoadFloat3(&position);

                // 1. Create the local transformation matrix from Scale, Rotation, and Translation (SRT)
                DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(scaleVector);
                DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(rotationQuaternion);
                DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslationFromVector(positionVector);

                // Combine the components to form the local matrix. The order is important: Scale -> Rotate -> Translate
                DirectX::XMMATRIX localMatrix = scaleMatrix * rotationMatrix * translationMatrix;

                // 2. Calculate the final world matrix by multiplying the local matrix by the parent's world matrix
                DirectX::XMMATRIX worldMatrix = GetWorldMatrix();

                // 3. Extract the world position from the final world matrix
                // The position is stored in the fourth row of the matrix (m[3])
                DirectX::XMFLOAT3 worldPosition;
                DirectX::XMStoreFloat3(&worldPosition, worldMatrix.r[3]);

                return worldPosition;
            }
            /// <summary>
            /// Updates the quaternion with the euler rotation
            /// </summary>
            void updateQuaternion()
            {
                using namespace DirectX;
                XMFLOAT3 x(1, 0, 0);
                XMFLOAT3 y(0, 1, 0);
                XMFLOAT3 z(0, 0, 1);
                XMVECTOR _x = XMLoadFloat3(&x);
                XMVECTOR _y = XMLoadFloat3(&y);
                XMVECTOR _z = XMLoadFloat3(&z);

                XMVECTOR qPitch = XMQuaternionRotationAxis(_x, XMConvertToRadians(rotationAsEulers.x));
                XMVECTOR qYaw = XMQuaternionRotationAxis(_y, XMConvertToRadians(rotationAsEulers.y));
                XMVECTOR qRoll = XMQuaternionRotationAxis(_z, XMConvertToRadians(rotationAsEulers.z));
                // Combine in your chosen order (e.g., Unity-style: Z * X * Y)
                XMVECTOR q = XMQuaternionMultiply(qPitch, qRoll); // pitch * roll
                q = XMQuaternionMultiply(qYaw, q);                // yaw * (pitch * roll)
                q = XMQuaternionNormalize(q);

                XMStoreFloat4(&rotation, q);
            }
            
            /// <summary>
            /// Updates the euler angles from the quaternion rotation
            /// </summary>
            void updateEulers()
            {
                using namespace DirectX;
                XMVECTOR q = XMLoadFloat4(&rotation);
                // Convert quaternion to rotation matrix
                XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(q);
                // Extract pitch (x), yaw (y), roll (z) from matrix
                float pitch, yaw, roll;
                // Extract basis vectors
                XMFLOAT3 forward;
                XMStoreFloat3(&forward, rotationMatrix.r[2]); // Z basis
                XMFLOAT3 up;
                XMStoreFloat3(&up, rotationMatrix.r[1]); // Y basis
                // Yaw (around Y axis)
                yaw = atan2f(forward.x, forward.z);
                // Pitch (around X axis)
                pitch = asinf(-forward.y);
                // Roll (around Z axis)
                roll = atan2f(up.x, up.y);
                rotationAsEulers = XMFLOAT3(
                    XMConvertToDegrees(pitch),
                    XMConvertToDegrees(yaw),
                    XMConvertToDegrees(roll));
            }

            /// <summary>
            /// Generate model matrix from position, rotation (quaternion), and scale
            /// </summary>
            /// <returns></returns>
            DirectX::XMMATRIX GetLocalMatrix() {
                using namespace DirectX;
                XMFLOAT3 x(1, 0, 0);
                XMFLOAT3 y(0, 1, 0);
                XMFLOAT3 z(0, 0, 1);
                XMVECTOR _x = XMLoadFloat3(&x);
                XMVECTOR _y = XMLoadFloat3(&y);
                XMVECTOR _z = XMLoadFloat3(&z);

                XMVECTOR qPitch = XMQuaternionRotationAxis(_x, XMConvertToRadians(rotationAsEulers.x));
                XMVECTOR qYaw = XMQuaternionRotationAxis(_y, XMConvertToRadians(rotationAsEulers.y));
                XMVECTOR qRoll = XMQuaternionRotationAxis(_z, XMConvertToRadians(rotationAsEulers.z));
                // Combine in your chosen order (e.g., Unity-style: Z * X * Y)
                XMVECTOR q = XMQuaternionMultiply(qPitch, qRoll); // pitch * roll
                q = XMQuaternionMultiply(qYaw, q);                // yaw * (pitch * roll)
                q = XMQuaternionNormalize(q);

                XMStoreFloat4(&rotation, q);

                DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&position);
                DirectX::XMVECTOR rotationVector = DirectX::XMLoadFloat4(&rotation);
                DirectX::XMVECTOR scaleVector = DirectX::XMLoadFloat3(&scale);

                // Create transformation matrices
                // Note the order: Scale -> Rotate -> Translate
                DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(scaleVector);
                DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(rotationVector);
                DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslationFromVector(positionVector);

                localMatrix = scaleMatrix * rotationMatrix * translationMatrix;
                // Combine matrices in the correct order
                return localMatrix;
            }
            /// <summary>
            /// The world matrix.
            /// </summary>
            /// <returns></returns>
            DirectX::XMMATRIX GetWorldMatrix() {
                worldMatrix = GetLocalMatrix() * parentMatrix;
                return worldMatrix;
            }

            /// <summary>
            /// Look at the target position. The eye is the current position
            /// </summary>
            /// <param name="targetPosition"></param>
            /// <param name="upDirection"></param>
            void LookAt(const DirectX::XMFLOAT3& targetPosition, const DirectX::XMFLOAT3& upDirection = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)) {
                // Get our current position and the target as vectors
                DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&position);
                DirectX::XMVECTOR targetVector = DirectX::XMLoadFloat3(&targetPosition);
                DirectX::XMVECTOR upVector = DirectX::XMLoadFloat3(&upDirection);

                // If position and target are too close, don't change rotation
                DirectX::XMVECTOR directionVector = DirectX::XMVectorSubtract(targetVector, positionVector);
                float distanceSquared;
                DirectX::XMStoreFloat(&distanceSquared, DirectX::XMVector3LengthSq(directionVector));

                if (distanceSquared < 0.0001f) {
                    return; // Too close to calculate meaningful direction
                }

                // Calculate the look-at matrix
                // This gives us an orientation where the Z-axis points toward the target
                DirectX::XMMATRIX lookAtMatrix = DirectX::XMMatrixLookAtLH(positionVector, targetVector, upVector);

                // The look-at matrix is a view matrix (camera space), so we need to invert it
                // to get a world-space orientation
                DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixInverse(nullptr, lookAtMatrix);

                // Extract just the rotation component (upper-left 3x3) and create a quaternion
                DirectX::XMVECTOR newRotation = DirectX::XMQuaternionRotationMatrix(worldMatrix);

                // Store the quaternion back to our transform
                DirectX::XMStoreFloat4(&rotation, newRotation);
                updateEulers();
            }
        };

        /// <summary>
        /// All entities that can be rendered must have this component
        /// </summary>
        struct Renderable {
            /// <summary>
            /// The per-object uniforms live in a huge buffer, shared by many root signatures.
            /// This id is the position in the buffer.
            /// </summary>
            uint32_t uniformBufferId;
            D3D12_VERTEX_BUFFER_VIEW mVertexBufferView{};
            D3D12_INDEX_BUFFER_VIEW mIndexBufferView{};
            int mNumberOfIndices;
        };

        /// <summary>
        /// Hierarchical relationship between entities
        /// </summary>
        struct Hierarchy {
            /// <summary>
            /// Parent entity. If null that means that the entity is the root of it's hierarchy.
            /// </summary>
            entt::entity parent = entt::null;
            /// <summary>
            /// Children entities. If empty that means that the entity is leaf.
            /// </summary>
            std::vector<entt::entity> children;

            /// <summary>
            /// Add a child. It does not check for previous parents, nor set this as the parent
            /// </summary>
            /// <param name="child"></param>
            void AddChild(entt::entity child) {
                children.push_back(child);
            }
            /// <summary>
            /// Remove a child. It does not clear it's parent.
            /// </summary>
            /// <param name="child"></param>
            void RemoveChild(entt::entity child) {
                auto it = std::find(children.begin(), children.end(), child);
                if (it != children.end()) {
                    children.erase(it);
                }
            }
        };

        struct PhongMaterial {
            DirectX::XMFLOAT4 diffuseColor;
            DirectX::XMFLOAT4 specularColor;
            DirectX::XMFLOAT4 ambientColor;
            DirectX::XMFLOAT4 emissiveColor;
            float shininess;
        };
        /// <summary>
        /// Based on blender's Principled BDSF PRB material. It lacks some properties but it
        /// has all the basic ones.
        /// </summary>
        struct BSDFMaterial {
            std::string name;
            uint32_t idInFile;
            DirectX::XMFLOAT4 baseColor;
            float metallicFactor;
            float roughnessFactor;
            DirectX::XMFLOAT3 emissiveColor;
            float opacity;
            float refracti;
        };
        
        void UpdateTransformRecursive(
            entt::registry& registry,
            entt::entity entity,
            const DirectX::XMMATRIX& parentMatrix
        );

        void UpdateAllTransforms(entt::registry& registry);
    }
}