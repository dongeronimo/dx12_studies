#pragma once
#include "pch.h"
#include <tiny_gltf.h>
namespace skinning::gameobjects
{
    struct Capoeirista
    {
    };

    struct CapoeiristaInstance
    {
        int id;
    };
}
namespace skinning
{
    /// <summary>
    /// describes the vertex layout.
    /// </summary>
    struct Vertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 texCoord;
        std::vector<uint8_t> jointIDs; // Bone IDs
        std::vector<float> weights;    // Weights
    };
    //TODO: Deprecated
    class SkinnedMeshPrefab
    {
    public:
        const std::map<std::string, std::vector<skinning::Vertex>> meshes;
        const std::unordered_set<int> armatureNodesIds;
        const std::unordered_map<int, std::unordered_set<int>> hierarchy;
        SkinnedMeshPrefab(std::map<std::string, std::vector<skinning::Vertex>> _meshes,
            std::unordered_set<int> _armatureNodesIds,
            std::unordered_map<int, std::unordered_set<int>> _hierarchy)
            :meshes(_meshes), armatureNodesIds(_armatureNodesIds), hierarchy(_hierarchy) {}

    };
    /// <summary>
    /// A mesh component. Holds the vertex and index buffers
    /// </summary>
    struct Mesh
    {
        std::string name;
        Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
        Microsoft::WRL::ComPtr<ID3D12Resource> mIndexBuffer;
        D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
        const int mNumberOfIndices;
    };
    /// <summary>
    /// Marks the entity as a prefab. Prefabs are identified by name,
    /// the name must be unique (but at the moment there's no mechanism
    /// to validate uniqueness).
    /// TODO: create assertion to validade if the name is alredy in use
    /// </summary>
    struct Prefab
    {
    };
    /// <summary>
    /// Describes a bone joint. Maybe can be used as a generic transform object?
    /// </summary>
    struct Transform
    {
        DirectX::XMVECTOR position;
        DirectX::XMVECTOR scale;
        DirectX::XMVECTOR rotation;
    };
    /// <summary>
    /// I need the T-Pose matrix because the final world position of a vertex in a 
    /// skin takes that into account
    /// </summary>
    struct TPoseMatrix
    {
        DirectX::XMMATRIX offsetMatrix;
    };
    /// <summary>
    /// Holds the parent.
    /// </summary>
    struct Parent
    {
        entt::entity parent = entt::null;
    };
    /// <summary>
    /// Holds a list of children
    /// </summary>
    struct Children
    {
        std::vector<entt::entity> children;
    };
    /// <summary>
    /// The id of the bone, used by the vertex buffer (jointID)
    /// </summary>
    struct BoneId
    {
        int nodeID;
    };

    struct GameObject
    {
        int id;
        std::string name;
    };
    
}
namespace skinning::io
{
    /// <summary>
    /// A prefab of a type is a list of entities that has the Prefab component
    /// and a type_t that is it's type.
    /// </summary>
    class PrefabLoader
    {

        static void GetNodeIds(
            std::unordered_map<int, const tinygltf::Node&>& nodes,
            tinygltf::Model& model);
        /// <summary>
        /// Load the model using a tinyGLTF loader and model.
        /// </summary>
        /// <param name="loader"></param>
        /// <param name="model"></param>
        /// <param name="file"></param>
        static void LoadModel(tinygltf::TinyGLTF& loader,
            tinygltf::Model& model, const std::string& file);

        static void ExtractTPoseMatrix(
            std::unordered_map<int, DirectX::XMMATRIX>& boneId_offsetMatrix_table,
            tinygltf::Model& model);
        static void ExtractMeshWithWeights(const tinygltf::Model& model,
            const tinygltf::Mesh& mesh,
            std::vector<Vertex>& vertices,
            std::vector<int>& indices);
        static void ExtractArmatureNodeIds(std::unordered_set<int>& armatureNodesIds,
            tinygltf::Model& model);
        static void CreateEntitiesForBoneNodes(
            std::unordered_set<int>& armatureNodesIds,
            entt::registry& registry,
            std::unordered_map<int, entt::entity>& boneEntities);
        /// <summary>
        /// Create the parent and children components in the entities.
        /// </summary>
        /// <param name="boneEntities"></param>
        /// <param name="hierarchy"></param>
        /// <param name="registry"></param>
        static void CreateHierarchy(std::unordered_map<int, entt::entity>& boneEntities,
            std::unordered_map<int, std::unordered_set<int>>& hierarchy,
            entt::registry& registry);
        /// <summary>
        /// It creates the mesh component. To create the mesh component i have to create the vertex and
        /// index buffers and push data to them (that's why i need the device and queue). In the end
        /// the mesh component is ready. Since i'm using ComPtr that means that you (probably) won't need
        /// to delete the objects manually, when the components are destroyed during the registry destruction
        /// the COM counter will drop and eventually go to zero, destroying the object.
        /// </summary>
        /// <param name="vertexes"></param>
        /// <param name="indices"></param>
        /// <param name="device"></param>
        /// <param name="commandQueue"></param>
        /// <param name="name"></param>
        /// <returns></returns>
        static skinning::Mesh CreateMeshComponent(std::vector<skinning::Vertex>& vertexes,
            std::vector<int>& indices,
            Microsoft::WRL::ComPtr<ID3D12Device> device,
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
            std::string name);
        static void CreateTransformAndTPoseComponents(std::unordered_map<int, entt::entity>& boneEntities,
            tinygltf::Model& model,
            std::unordered_map<int, DirectX::XMMATRIX>& tPoseMatrices,
            entt::registry& registry);
        static void ExtractBoneHierarchy(
            std::unordered_map<int, std::unordered_set<int>>& hierarchy, 
            std::unordered_set<int>& armatureNodeIds, 
            std::unordered_map<int, 
            const tinygltf::Node&>& nodes);
    public:
        /// <summary>
        /// Loads the skin from the file and creates entities from it.
        /// It'll create the bone hierarchy entities and the meshes entities. 
        /// All entities will have the Prefab component. Prefab.Name must be unique
        /// or you'll screw up the queries.
        /// The parent node entities of the hierarchy will have no Parent component. 
        /// The leaf will node have no Children component. All the others will have both.
        /// All bone joints will have BoneJoint and TPose components.
        /// Meshes will have only mesh and prefab components.
        /// </summary>
        /// <param name="file"></param>
        /// <param name="registry"></param>
        /// <param name="assetId"></param>
        /// <param name="device"></param>
        /// <param name="commandQueue"></param>
        template<typename type_t>
        static void LoadSkinnedMeshPrefab(const std::string& file,
            entt::registry& registry,
            type_t properties,
            Microsoft::WRL::ComPtr<ID3D12Device> device,
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue)
        {
            const Prefab prefabComponent;
            //type to improve the semantics of the code. WTF is an int? It can be anything. Typedefing to 
            //NODE_ID makes it more readable.
            typedef int NODE_ID;
            tinygltf::TinyGLTF loader;
            tinygltf::Model model;
            LoadModel(loader, model, file);
            //we need the node IDs, but in tinygltf the node object has no id, it's id is its position
            //in the list of nodes. The node id will be used everywhere else.
            std::unordered_map<NODE_ID, const tinygltf::Node&> nodes;
            GetNodeIds(nodes, model);
            //get the list of armature nodes. Not all nodes are armature nodes, there are mesh nodes and there may
            //be other things like camera and lights if exported from blender. So i get the node ids that belong to the
            //armature.
            std::unordered_set<NODE_ID> armatureNodesIds;
            ExtractArmatureNodeIds(armatureNodesIds, model);
            //get the list of offset matrices (aka t-pose matrix), the key is the armature node id, i'll need this to 
            //build the bone components.
            std::unordered_map<NODE_ID, DirectX::XMMATRIX> tPoseMatrices;
            ExtractTPoseMatrix(tPoseMatrices, model);
            //Each entry is the node as the key an it's children as value
            std::unordered_map<NODE_ID, std::unordered_set<NODE_ID>> hierarchy;
            ExtractBoneHierarchy(hierarchy, armatureNodesIds, nodes);
            //create the bone entities, add to them the BoneId component and the Prefab component. The
            //bone id is the node id of that bone, and the prefab is a tag that mark the entity as part 
            //of a prefab
            std::unordered_map<NODE_ID, entt::entity> boneEntities;
            CreateEntitiesForBoneNodes(armatureNodesIds, registry, boneEntities);
            for (auto& kv : boneEntities)
            {
                registry.emplace<Prefab>(kv.second, prefabComponent);
                registry.emplace<type_t>(kv.second, properties);
            }
            //Will navigate the bone components and create the components that describe the hierarchy (
            //parent and children). 
            CreateHierarchy(boneEntities, hierarchy, registry);
            //create the transform components (boneTransform and TPoseMatrix). Remember that the transforms
            //are local
            CreateTransformAndTPoseComponents(boneEntities, model, tPoseMatrices, registry);
            //by now i have the entities with the hierarchy, the transforms, the t-pose matrices, with all entities
            //tagged as belonging to the prefab. Now it's time to create the mesh components. To create the mesh
            //components i'll load the vertex and index data and create the vertex buffers.
            //first step is to load the data into a better data structure.
            std::map<std::string, std::vector<skinning::Vertex>> meshes;
            std::map<std::string, std::vector<int>> indices;
            for (auto& currMesh : model.meshes)
            {
                auto name = currMesh.name;
                std::vector < skinning::Vertex> verts;
                std::vector<int> idx;
                ExtractMeshWithWeights(model, currMesh, verts, idx);
                meshes.insert({ name, verts });
                indices.insert({ name, idx });
            }
            //now i push it into the vertex buffer...
            for (auto& currMesh : meshes)
            {
                skinning::Mesh meshComponent = CreateMeshComponent(currMesh.second, indices[currMesh.first], device, commandQueue, currMesh.first);
                //now that i have the mesh component i can create the mesh entity, with the mesh component and the prefab component
                //remember, meshes do not have transforms, it's the thing that uses meshes, like game objects, that have transforms.
                entt::entity meshEntity = registry.create();
                registry.emplace<skinning::Mesh>(meshEntity, meshComponent);
                registry.emplace<skinning::Prefab>(meshEntity, prefabComponent);
                registry.emplace<type_t>(meshEntity, properties);  
            }
        }
    };

    static int GetNextGameObjectId() {
        static int id = 0;
        id = id + 1;
        return id;
    }
    /// <summary>
    /// Appends the necessary prefixes to the file name.
    /// </summary>
    /// <param name="file"></param>
    /// <returns></returns>
    std::string AssembleFilePath(std::string file);


    // Forward declaration
    entt::entity clone_hierarchy(entt::registry& registry, entt::entity source);

    // Helper function to clone a single entity with its transform
    entt::entity clone_entity_with_transform(entt::registry& registry, entt::entity source);

    // Main recursive function to clone hierarchy
    entt::entity clone_hierarchy(entt::registry& registry, entt::entity source);

    // Usage function to clone a root node and its entire hierarchy
    entt::entity clone_root_hierarchy(entt::registry& registry, entt::entity root);

    /// <summary>
    /// To instantiate a prefab we find all entities that have a prefab component with id = prefabName,
    /// Then we duplicate the all these entities, with their components, with the exception of the Prefab
    /// component. Finally, we add the GameObject component with name = gameObjectName.
    /// If we have the Parent and Children components the hierarchy will be recreated for the cloned entities
    /// based on the relationships
    /// </summary>
    /// <param name="prefabName"></param>
    /// <param name="gameObjectName"></param>
    template<typename prefab_t, typename instance_t>
    void InstantiatePrefab(instance_t instanceProperties,
        entt::registry& registry) {
        //1) duplicate the meshes. To duplicate a mesh we just copy the directx vertex
        //buffer properties.
        auto prefabMeshesView = registry.view<Prefab, prefab_t, Mesh>();
        for (const auto prefabEntity : prefabMeshesView) {
            auto meshComponent = prefabMeshesView.get<Mesh>(prefabEntity);
            entt::entity instanceEntity = registry.create();
            registry.emplace<Mesh>(instanceEntity, meshComponent);
            registry.emplace<instance_t>(instanceEntity, instanceProperties);
        }
        //2) clone the hierarchy
        auto roots = registry.view<Transform>(entt::exclude<Parent>);
        entt::entity single_root = *roots.begin();//i assume only one root
        entt::entity instanceRoot = clone_root_hierarchy(registry, single_root);
    }
}

