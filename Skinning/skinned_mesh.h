#pragma once
#include "pch.h"
#include "entt/entt.hpp"
#include "entities.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace skinning::io
{
	struct alignas(16) SkinnedVertexData {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		struct {
			uint32_t boneID;
			float weight;
		} boneWeights[4];
	};
	struct MeshData
	{
		std::string name;
		std::vector<SkinnedVertexData> vertices;
		std::vector<uint16_t> indices;
	};
	/// <summary>
	/// Loads scene from file to memory
	/// </summary>
	/// <param name="filename"></param>
	/// <returns></returns>
	const aiScene* LoadScene(const std::string& filename);
	/// <summary>
	/// Not all nodes in a scene are bone nodes, and the root node certainly isn't. so i
	/// have to find the bone root node 
	/// </summary>
	/// <param name="rootNode"></param>
	/// <param name="scene"></param>
	/// <returns></returns>
	aiNode* FindArmatureRoot(aiNode* rootNode, const aiScene* scene);
	
	void LoadBoneHierarchy(aiNode* node,
		const aiScene* scene,
		std::vector<entt::entity>& boneEntitiesList,
		entt::entity parentEntity,
		entt::registry& registry);
	///// <summary>
	///// Recursively loads bones, beginning from the root bone. The bones are stored both
	///// at the entity registry and at a temporary struct to help on the next steps
	///// </summary>
	//void LoadBone(aiNode* node, entt::registry& registry, entt::entity parentEntity,
	//	const aiScene* scene, std::unordered_map<std::string, entt::entity>& boneMap,
	//	int id = 0);

	std::vector<std::shared_ptr<MeshData>> LoadMeshes(const aiScene* scene, std::vector<entt::entity>& boneEntitiesList, entt::registry& registry);

}

