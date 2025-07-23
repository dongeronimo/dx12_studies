#include "mesh_loader_v2.h"
#include <tiny_gltf.h>
#include "../Common/concatenate.h"
#include "../Common/d3d_utils.h"
namespace skinning::io
{
    void PrefabLoader::CreateEntitiesForBoneNodes(std::unordered_set<int>& armatureNodesIds,
        entt::registry& registry, std::unordered_map<int, entt::entity>& boneEntities)
    {
        for (const auto& nodeIds : armatureNodesIds)
        {
            entt::entity boneEntity = registry.create();
            BoneId boneId{ nodeIds };
            registry.emplace<BoneId>(boneEntity, boneId);
            //links the id to the entity
            boneEntities.insert({ nodeIds, boneEntity });
        }
    }
    void PrefabLoader::ExtractMeshWithWeights(const tinygltf::Model& model,
        const tinygltf::Mesh& mesh,
        std::vector<Vertex>& vertices,
        std::vector<int>& indices) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.attributes.empty()) continue;

            const tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& indicesBufferView = model.bufferViews[indicesAccessor.bufferView];
            const tinygltf::Buffer& indicesBuffer = model.buffers[indicesBufferView.buffer];
            const unsigned char* indicesData = &indicesBuffer.data[
                indicesBufferView.byteOffset + indicesAccessor.byteOffset];
            indices.resize(indicesAccessor.count);
            if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const uint16_t* shortIndices = reinterpret_cast<const uint16_t*>(indicesData);
                for (size_t i = 0; i < indicesAccessor.count; i++) {
                    indices[i] = static_cast<uint32_t>(shortIndices[i]);
                }
            }
            else if (indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                memcpy(indices.data(), indicesData, indicesAccessor.count * sizeof(uint32_t));
            }

            // Buffers for attributes
            const auto& positionAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const auto& positionBufferView = model.bufferViews[positionAccessor.bufferView];
            const auto& positionBuffer = model.buffers[positionBufferView.buffer];
            const float* positionData = reinterpret_cast<const float*>(&positionBuffer.data[positionBufferView.byteOffset + positionAccessor.byteOffset]);

            const float* normalData = nullptr;
            if (primitive.attributes.count("NORMAL")) {
                const auto& normalAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const auto& normalBufferView = model.bufferViews[normalAccessor.bufferView];
                const auto& normalBuffer = model.buffers[normalBufferView.buffer];
                normalData = reinterpret_cast<const float*>(&normalBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset]);
            }

            const float* texCoordData = nullptr;
            if (primitive.attributes.count("TEXCOORD_0")) {
                const auto& texCoordAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto& texCoordBufferView = model.bufferViews[texCoordAccessor.bufferView];
                const auto& texCoordBuffer = model.buffers[texCoordBufferView.buffer];
                texCoordData = reinterpret_cast<const float*>(&texCoordBuffer.data[texCoordBufferView.byteOffset + texCoordAccessor.byteOffset]);
            }

            const uint8_t* jointData = nullptr;
            if (primitive.attributes.count("JOINTS_0")) {
                const auto& jointAccessor = model.accessors[primitive.attributes.at("JOINTS_0")];
                const auto& jointBufferView = model.bufferViews[jointAccessor.bufferView];
                const auto& jointBuffer = model.buffers[jointBufferView.buffer];
                jointData = &jointBuffer.data[jointBufferView.byteOffset + jointAccessor.byteOffset];
            }

            const float* weightData = nullptr;
            if (primitive.attributes.count("WEIGHTS_0")) {
                const auto& weightAccessor = model.accessors[primitive.attributes.at("WEIGHTS_0")];
                const auto& weightBufferView = model.bufferViews[weightAccessor.bufferView];
                const auto& weightBuffer = model.buffers[weightBufferView.buffer];
                weightData = reinterpret_cast<const float*>(&weightBuffer.data[weightBufferView.byteOffset + weightAccessor.byteOffset]);
            }

            // Iterate through vertices
            for (size_t i = 0; i < positionAccessor.count; ++i) {
                Vertex vertex;
                vertex.position = DirectX::XMFLOAT3(
                    positionData[i * 3],
                    positionData[i * 3 + 1],
                    positionData[i * 3 + 2]);

                if (normalData) {
                    vertex.normal = DirectX::XMFLOAT3(
                        normalData[i * 3],
                        normalData[i * 3 + 1],
                        normalData[i * 3 + 2]);
                }

                if (texCoordData) {
                    vertex.texCoord = DirectX::XMFLOAT2(
                        texCoordData[i * 2],
                        texCoordData[i * 2 + 1]);
                }

                if (jointData) {
                    vertex.jointIDs.assign(&jointData[i * 4], &jointData[i * 4 + 4]);
                }

                if (weightData) {
                    vertex.weights.assign(&weightData[i * 4], &weightData[i * 4 + 4]);
                }

                vertices.push_back(vertex);
            }
        }
    }
    /// <summary>
    /// The files live in assets folder.
    /// </summary>
    /// <param name="file"></param>
    /// <returns></returns>
    std::string AssembleFilePath(std::string file)
    {
        std::stringstream ss;
        ss << "assets/" << file;
        return ss.str();
    }



    /// <summary>
    /// If there are warnings they'll be printed to stdout
    /// </summary>
    /// <param name="warn"></param>
    void PrintWarnings(std::string warn)
    {
        if (!warn.empty())
            printf("Warning: %s\n", warn.c_str());
    }
    /// <summary>
    /// If there are errors, they'll be printed to stdout
    /// </summary>
    /// <param name="err"></param>
    void PrintErrs(std::string err)
    {
        if (!err.empty())
            printf("Error: %s\n", err.c_str());
    }
    /// <summary>
    /// Extract the set of armature nodes
    /// </summary>
    /// <param name="armatureNodesIds"></param>
    /// <param name="model"></param>
    void PrefabLoader::ExtractArmatureNodeIds(std::unordered_set<int>& armatureNodesIds,
        tinygltf::Model& model)
    {
        for (const auto& skin : model.skins)
        {
            for (int joint : skin.joints)
            {
                armatureNodesIds.insert(joint);
            }
            if (skin.skeleton >= 0)
            {
                armatureNodesIds.insert(skin.skeleton);
            }
        }
    }

    void PrefabLoader::ExtractTPoseMatrix(std::unordered_map<int, DirectX::XMMATRIX>& boneId_offsetMatrix_table,
        tinygltf::Model& model)
    {
        for (const auto& skin : model.skins)
        {
            const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

            for (size_t i = 0; i < accessor.count; ++i) {
                auto currJointId = skin.joints[i];
                DirectX::XMFLOAT4X4 mat;
                memcpy(mat.m, &data[i * 16], sizeof(float) * 16);
                DirectX::XMMATRIX xmmMat = DirectX::XMLoadFloat4x4(&mat);
                boneId_offsetMatrix_table.insert({ currJointId, xmmMat });
            }
        }
    }
    /// <summary>
    /// Is the node with the given id part of an armature?
    /// </summary>
    /// <param name="armatureNodesIds"></param>
    /// <param name="id"></param>
    /// <returns></returns>
    bool NodeIsPartOfArmature(const std::unordered_set<int>& armatureNodesIds,
        const int id)
    {
        return armatureNodesIds.count(static_cast<int>(id)) > 0;
    }

    bool IsRootNode(const tinygltf::Node& node)
    {
        return (node.mesh < 0 && node.skin < 0 && node.children.empty() &&
            node.translation.empty() && node.rotation.empty() && node.scale.empty());
    }

    bool IsMeshNode(const tinygltf::Node& node)
    {
        return node.mesh >= 0;
    }
    void PrefabLoader::GetNodeIds(std::unordered_map<int, const tinygltf::Node&>& nodes, tinygltf::Model& model)
    {
        for (int i = 0; i < model.nodes.size(); ++i)
        {
            const tinygltf::Node& n = model.nodes[i];
            nodes.insert({ i, n });
        }
    }

    void PrefabLoader::ExtractBoneHierarchy(std::unordered_map<int, std::unordered_set<int>>& hierarchy, std::unordered_set<int>& armatureNodeIds, std::unordered_map<int, const tinygltf::Node&>& nodes)
    {
        for (const auto& kv : nodes)
        {
            if (!NodeIsPartOfArmature(armatureNodeIds, kv.first))
                continue;
            if (!kv.second.children.empty())
            {
                std::unordered_set<int> children;
                for (auto& id : kv.second.children)
                {
                    children.insert(id);
                }
                hierarchy.insert({ kv.first, children });
            }
        }
    }

    void ExtractLocalTransform(const tinygltf::Node& node,
        DirectX::XMVECTOR& position,
        DirectX::XMVECTOR& scale,
        DirectX::XMVECTOR& rotation)
    {
        // Default values
        position = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        scale = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
        rotation = DirectX::XMQuaternionIdentity();

        if (!node.matrix.empty()) {
            // Decompose the transform matrix
            DirectX::XMFLOAT4X4 mat;
            memcpy(mat.m, node.matrix.data(), sizeof(float) * 16);
            DirectX::XMMATRIX transformMatrix = DirectX::XMLoadFloat4x4(&mat);

            DirectX::XMVECTOR det;
            DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(&det, transformMatrix);

            DirectX::XMMatrixDecompose(&scale, &rotation, &position, transformMatrix);
        }
        else {
            // Translation
            if (!node.translation.empty()) {
                position = DirectX::XMVectorSet(
                    static_cast<float>(node.translation[0]),
                    static_cast<float>(node.translation[1]),
                    static_cast<float>(node.translation[2]),
                    1.0f);
            }

            // Scale
            if (!node.scale.empty()) {
                scale = DirectX::XMVectorSet(
                    static_cast<float>(node.scale[0]),
                    static_cast<float>(node.scale[1]),
                    static_cast<float>(node.scale[2]),
                    0.0f);
            }

            // Rotation (Quaternion)
            if (!node.rotation.empty()) {
                rotation = DirectX::XMVectorSet(
                    static_cast<float>(node.rotation[0]),
                    static_cast<float>(node.rotation[1]),
                    static_cast<float>(node.rotation[2]),
                    static_cast<float>(node.rotation[3]));
            }
        }
    }

    void PrefabLoader::LoadModel(tinygltf::TinyGLTF& loader, tinygltf::Model& model, const std::string& file)
    {
        std::string err, warn;
        bool success = loader.LoadBinaryFromFile(&model,
            &err, &warn, file.c_str());
        PrintWarnings(warn);
        PrintErrs(err);
        assert(success);
    }

    void PrefabLoader::CreateHierarchy(std::unordered_map<int, entt::entity>& boneEntities,
        std::unordered_map<int, std::unordered_set<int>>& hierarchy,
        entt::registry& registry)
    {
        for (const auto& entities : boneEntities)
        {
            const int& id = entities.first;
            const entt::entity& entity = entities.second;
            std::unordered_set<int> children = hierarchy[id];
            //create the relationships
            std::vector<entt::entity> childrenEntities;
            for (const auto& childId : children)
            {
                const entt::entity& childEntity = boneEntities[childId];
                childrenEntities.push_back(childEntity);
                //since we got the child here, we create the parent component and attach to the entity.
                skinning::Parent parentComponent{ entity };
                registry.emplace<skinning::Parent>(childEntity, parentComponent);
            }
            //attach the children component to the entity
            skinning::Children childrenComponent{ childrenEntities };
            registry.emplace<skinning::Children>(entity, childrenComponent);
        }
    }
    void PrefabLoader::CreateTransformAndTPoseComponents(std::unordered_map<int, entt::entity>& boneEntities,
        tinygltf::Model& model,
        std::unordered_map<int, DirectX::XMMATRIX>& tPoseMatrices,
        entt::registry& registry)
    {
        for (const auto& entities : boneEntities)
        {
            const int& id = entities.first;
            const entt::entity& entity = entities.second;
            tinygltf::Node& node = model.nodes[id];
            //TODO: get the offset matrix and create it's component
            skinning::TPoseMatrix tposeMatrix{
                tPoseMatrices[id]
            };
            registry.emplace<skinning::TPoseMatrix>(entity, tposeMatrix);
            //TODO: get local position, scale and rotation
            DirectX::XMVECTOR position, scale, rotation;
            ExtractLocalTransform(node, position, scale, rotation);
            //TODO: create component and add to the entity
            skinning::Transform boneTransform{ position, scale, rotation };
            registry.emplace<skinning::Transform>(entity, boneTransform);
        }
    }

    skinning::Mesh PrefabLoader::CreateMeshComponent(std::vector<skinning::Vertex>& vertexes,
        std::vector<int>& indices,
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
        std::string name) {
        using namespace Microsoft::WRL;
        int vBufferSize = vertexes.size() * sizeof(common::Vertex);
        CD3DX12_HEAP_PROPERTIES vertexHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC vertexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
        Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
        D3D12_VERTEX_BUFFER_VIEW mVertexBufferView{};
        device->CreateCommittedResource(
            &vertexHeapProperties, // a default heap
            D3D12_HEAP_FLAG_NONE, // no flags
            &vertexResourceDesc, // resource description for a buffer
            D3D12_RESOURCE_STATE_COMMON,//the initial state of vertex buffer is D3D12_RESOURCE_STATE_COMMON. Before we use them i'll have to transition it to the correct state
            nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
            IID_PPV_ARGS(&mVertexBuffer));
        std::wstring vertex_w_name = Concatenate(multi2wide(name), "vertexBuffer");
        mVertexBuffer->SetName(vertex_w_name.c_str());
        // create upload heap
        // upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
        // We will upload the vertex buffer using this heap to the default heap
        ComPtr<ID3D12Resource> vertexBufferUploadHeap;
        CD3DX12_HEAP_PROPERTIES vertexUploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        device->CreateCommittedResource(
            &vertexUploadHeapProperties, // upload heap
            D3D12_HEAP_FLAG_NONE, // no flags
            &vertexResourceDesc, // resource description for a buffer
            D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
            nullptr,
            IID_PPV_ARGS(&vertexBufferUploadHeap));
        vertexBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");
        // store vertex buffer in upload heap
        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = reinterpret_cast<BYTE*>(vertexes.data()); // pointer to our vertex array
        vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
        vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data
        ///////now the index buffer
        int iBufferSize = indices.size() * sizeof(int);
        //create the index buffer
        CD3DX12_HEAP_PROPERTIES indexHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC indexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(iBufferSize);
        Microsoft::WRL::ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
        device->CreateCommittedResource(
            &indexHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &indexResourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&mIndexBuffer)
        );
        std::wstring index_w_name = Concatenate(multi2wide(name), "indexBuffer");
        D3D12_INDEX_BUFFER_VIEW mIndexBufferView{};
        mIndexBuffer->SetName(index_w_name.c_str());
        //create the staging buffer for the indexes
        ComPtr<ID3D12Resource> indexBufferUploadHeap;
        CD3DX12_HEAP_PROPERTIES indexUploadHeapProperties = CD3DX12_HEAP_PROPERTIES(
            D3D12_HEAP_TYPE_UPLOAD);
        device->CreateCommittedResource(
            &indexUploadHeapProperties, // upload heap
            D3D12_HEAP_FLAG_NONE, // no flags
            &indexResourceDesc, // resource description for a buffer
            D3D12_RESOURCE_STATE_GENERIC_READ, //D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
            nullptr,
            IID_PPV_ARGS(&indexBufferUploadHeap));
        indexBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");
        // store index buffer in upload heap
        D3D12_SUBRESOURCE_DATA indexData = {};
        indexData.pData = reinterpret_cast<BYTE*>(indices.data()); // pointer to our index array
        indexData.RowPitch = iBufferSize; // size of all our index buffer
        indexData.SlicePitch = iBufferSize; // also the size of our index buffer
        //now we run the commands
        common::RunCommands(
            device.Get(),
            commandQueue.Get(),
            [&vertexBufferUploadHeap, &vertexData, &indexBufferUploadHeap, &indexData, mVertexBuffer,
            mIndexBuffer](
                ComPtr<ID3D12GraphicsCommandList> lst)
            {
                //vertexBuffer will go from common to copy destination
                CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    mVertexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                lst->ResourceBarrier(1, &vertexBufferResourceBarrier);
                //staging buffer will go from common to read origin
                //CD3DX12_RESOURCE_BARRIER stagingBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                //    vertexBufferUploadHeap.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);
                /*lst->ResourceBarrier(1, &stagingBufferResourceBarrier);*/
                //copy the vertex data from RAM to the vertex buffer, thru vBufferUploadHeap
                UpdateSubresources(lst.Get(), mVertexBuffer.Get(), vertexBufferUploadHeap.Get(), 0, 0, 1, &vertexData);
                //now that the data is in _vertexBuffer i transition _vertexBuffer to D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, so that
                //it can be used as vertex buffer
                CD3DX12_RESOURCE_BARRIER secondVertexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mVertexBuffer.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
                lst->ResourceBarrier(1, &secondVertexBufferResourceBarrier);
                //index buffer will go from common to copy destination
                CD3DX12_RESOURCE_BARRIER indexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    mIndexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                lst->ResourceBarrier(1, &indexBufferResourceBarrier);
                //index staging buffer will go from common to read origin
                //CD3DX12_RESOURCE_BARRIER indexStagingBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                //    indexBufferUploadHeap.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);
                //lst->ResourceBarrier(1, &indexStagingBufferResourceBarrier);
                //copy index data to index buffer thru index staging buffer
                UpdateSubresources(lst.Get(), mIndexBuffer.Get(), indexBufferUploadHeap.Get(), 0, 0, 1, &indexData);
                //index buffer go to index buffer state
                CD3DX12_RESOURCE_BARRIER secondIndexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(mIndexBuffer.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
                lst->ResourceBarrier(1, &secondIndexBufferResourceBarrier);
            });
        mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
        mVertexBufferView.StrideInBytes = sizeof(common::Vertex);
        mVertexBufferView.SizeInBytes = vBufferSize;

        mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
        mIndexBufferView.SizeInBytes = iBufferSize;
        mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

        skinning::Mesh result
        {
            name,
            mVertexBuffer,
            mVertexBufferView,
            mIndexBuffer,
            mIndexBufferView,
            indices.size()
        };
        return result;
    }
    /// <summary>
    /// Returns a list of all entities of the given prefab
    /// </summary>
    /// <param name="registry"></param>
    /// <param name="name"></param>
    /// <returns></returns>
    std::vector<entt::entity> findEntitiesByPrefabName(entt::registry& registry, 
        std::string& name)
    {
        std::vector<entt::entity> matching_entities;
        auto view = registry.view<const Prefab>();
        for (const auto entity : view) {
            //const auto& prefab = view.get<const Prefab>(entity);
            //if (prefab.name == name) {
            //    matching_entities.push_back(entity);
            //}
        }
        return matching_entities;
    }

    entt::entity FindRootNode(std::vector<entt::entity>& prefabEntities,
        entt::registry& registry)
    {
        for (auto& entity : prefabEntities)
        {

        }
        return entt::null;
    }


    
}