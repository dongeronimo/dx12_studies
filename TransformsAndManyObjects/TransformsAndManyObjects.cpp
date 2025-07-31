#include "pch.h"

#include "../Common/mesh.h"
#include "../Common/mesh_load.h"
#include "direct3d_context.h"
#include "Pipeline.h"
#include "view_projection.h"
#include "transform.h"
#include "model_matrix.h"
#include <iostream>
#include <filesystem>
#include "entt/entt.hpp"
#include "per_object_uniform_buffer.h"
#include "components.h"
#include "../Common/delta_timer.h"
#include "script_runner_system.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <tuple> 
#include "per_frame_data_for_simple_lighting.h"
#include "per_object_uniform_buffer.h"
#include "lighting_data.h"
#include "per_frame_data_for_unlit_debug.h"
#include "on_esc_handler.h"
#include "camera_input_handler.h"
#include "shared_descriptor_heap_v2.h"
#include "my_imgui_manager.h"
#include "game_window.h"
#include "offscreen_render_target.h"
#include "rtv_dsv_shared_heap.h"
#include "cube_map_shadow_map.h"
#include "ShadowDataUpdateSystem.h"
#include "point_shadow_map_calculation_system.h"
using Microsoft::WRL::ComPtr;

constexpr int W = 1024;
constexpr int H = 768;
typedef std::shared_ptr<transforms::components::BSDFMaterial> BSDFMaterial_t;
/// <summary>
/// Storage for the meshes. Do not repeat IDs.
/// </summary>
std::unordered_map<int, std::shared_ptr<common::Mesh>> gMeshTable;
/// <summary>
/// Load the meshes into gMeshTable. It expects that the context has alredy been created.
/// </summary>
/// <param name="ctx"></param>
void LoadScene(transforms::Context& ctx);
void LoadMeshForOffscreenPresentation(transforms::Context& ctx);
entt::registry gRegistry;

std::unique_ptr<transforms::SharedDescriptorHeapV2> gSharedDescriptors = nullptr;
std::unique_ptr<transforms::UniformBufferForSRVs<transforms::PerObjectData>> gPerObjectUniformBuffer = nullptr;
std::unique_ptr<transforms::UniformBufferForSRVs<PerFrameDataForUnlitDebug>> gPerFrameUnlitDebugUniformBuffer = nullptr;
std::unique_ptr<transforms::UniformBufferForSRVs<PerFrameDataForSimpleLighting>> gPerFrameSimpleLightingUniformBuffer = nullptr;
std::unique_ptr<transforms::UniformBufferForSRVs<transforms::ShadowMapConstants>> gPointShadowUniformBuffer = nullptr;
std::unique_ptr<transforms::UniformBufferForSRVs<LightingData>> gLightingDataUniformBuffer = nullptr;
std::unique_ptr<transforms::MyImguiManager> gImguiManager = nullptr;
std::unique_ptr<transforms::RtvDsvDescriptorHeapManager> gRtvDsvSharedHeap = nullptr;

transforms::Window* gWindow = nullptr;

const std::wstring unlitDebugRootSignature = L"unlit debug";
const std::wstring simpleLightingRootSignature = L"simple lighting";
const std::wstring quadRenderRootSignature = L"QuadRenderRootSignature";
const std::wstring shadowMapRootSignature = L"ShadowMapRootSignature";

void CreateRootSignatures(transforms::RootSignatureService* rootSignatureService, transforms::Context* ctx);
void LoadMeshes(transforms::Context* ctx);

std::shared_ptr<transforms::Pipeline> unlitDebugPipeline;
std::shared_ptr<transforms::Pipeline> BSDFPipeline;
void CreatePipelines(transforms::RootSignatureService* rootSignatureService, transforms::Context* ctx);

///Transition the render target encapsulated by the object of type type_t to render target, sets it as Output Merger
///and clears it. It assumes that the type has the necessary functions since it uses ducktyping.
template<typename type_t>
void SetOffscreenTextureAsCurrentRenderTarget(
	type_t* t, 
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
	uint32_t frameIndex) 
{
	t->TransitionToRenderTarget(commandList.Get(), frameIndex);
	t->SetAsRenderTarget(commandList.Get(), frameIndex);
	t->Clear(commandList.Get(), frameIndex, { 0.0f, 0,0,1 });
}

int main()
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	transforms::Window window(hInstance, L"transforms_t", L"Transforms", W, H);
	window.Show();
	gWindow = &window;
	std::unique_ptr<transforms::Context> ctx = std::make_unique<transforms::Context>(W, H, window.Hwnd());
	LoadMeshes(ctx.get());
	std::unique_ptr<transforms::RootSignatureService> rootSignatureService = std::make_unique<transforms::RootSignatureService>();
	CreateRootSignatures(rootSignatureService.get(), ctx.get());
	CreatePipelines(rootSignatureService.get(), ctx.get());
	//create the shared heaps
	
	gSharedDescriptors = std::make_unique<transforms::SharedDescriptorHeapV2>(ctx->GetDevice().Get(), 1000);
	gRtvDsvSharedHeap = std::make_unique<transforms::RtvDsvDescriptorHeapManager>();
	gRtvDsvSharedHeap->Initialize(ctx->GetDevice().Get(), 1024, 1024);
	gPerObjectUniformBuffer = std::make_unique<transforms::UniformBufferForSRVs<transforms::PerObjectData>>(*ctx, 
		gSharedDescriptors.get(), 0, 10000); 
	gPerFrameUnlitDebugUniformBuffer = std::make_unique<transforms::UniformBufferForSRVs<PerFrameDataForUnlitDebug>>(*ctx, 
		gSharedDescriptors.get(), 1, 1);
	gPerFrameSimpleLightingUniformBuffer = std::make_unique<transforms::UniformBufferForSRVs<PerFrameDataForSimpleLighting>>(*ctx,
		gSharedDescriptors.get(), 2, 1);
	gLightingDataUniformBuffer = std::make_unique<transforms::UniformBufferForSRVs<LightingData>>(*ctx, 
		gSharedDescriptors.get(), 3, 100);
	gPointShadowUniformBuffer = std::make_unique<transforms::UniformBufferForSRVs<transforms::ShadowMapConstants>>(*ctx,
		gSharedDescriptors.get(), 4, MAX_LIGHTS * 6);
	// Fill out the Viewport
	SetViewportAndScissors(unlitDebugPipeline, W, H);
	SetViewportAndScissors(BSDFPipeline, W, H);
	///////////Entity that exists only to hold the on quit event//////////
	transforms::components::CreateEscHandler(gRegistry, ctx.get(), gWindow);
	//TODO lighting: create an entity to get keyboard event to switch between unlit debug and simple lighting;
	/////////////Add a camera to the scene/////////////
	entt::entity mainCamera = gRegistry.create();
	transforms::components::Transform cameraTransform;
	cameraTransform.position = DirectX::XMFLOAT3(-6, 21.f, -12);
	cameraTransform.LookAt(DirectX::XMFLOAT3(-6, 0.f, 0));
	transforms::components::Perspective cameraPerspective;
	cameraPerspective.fovDegrees = 45.0f;
	cameraPerspective.ratio = (float)W/ (float)H;
	cameraPerspective.zNear = 0.1f;
	cameraPerspective.zFar = 500.f;
	gRegistry.emplace<transforms::components::Transform>(mainCamera, cameraTransform);
	gRegistry.emplace<transforms::components::Perspective>(mainCamera, cameraPerspective);
	gRegistry.emplace<transforms::components::tags::MainCamera>(mainCamera, transforms::components::tags::MainCamera{});
	transforms::components::CreateCameraInputHandler(gWindow, gRegistry, mainCamera);
	//add shadow map components to the lights
	int shadowMapId = 0;
	gRegistry.view<transforms::components::PointLight, transforms::components::Transform>()
		.each([&ctx, &shadowMapId](entt::entity e, transforms::components::PointLight& pt, transforms::components::Transform& t) {
		auto shadowMap = std::make_shared<transforms::CubeMapShadowMap>(pt.name, shadowMapId);
		shadowMap->Initialize(ctx->GetDevice().Get(), gRtvDsvSharedHeap.get(),gSharedDescriptors.get(), SHADOW_MAP_SIZE);
		gRegistry.emplace<std::shared_ptr<transforms::CubeMapShadowMap>>(e, shadowMap);
		shadowMapId++;
	});
	shadowMapId = 0;
	//handle window resizing
	window.mOnResize = [ &ctx](int newW, int newH) {
		//TODO RESIZE: Resize is freezing the app
		ctx->Resize(newW, newH);
		unlitDebugPipeline->viewport.Width = (float)newW;
		unlitDebugPipeline->viewport.Height = (float)newH;
		unlitDebugPipeline->scissorRect.right = newW;
		unlitDebugPipeline->scissorRect.bottom = newH;
	};
	common::DeltaTimer deltaTimer;
	std::shared_ptr<transforms::OffscreenRenderTarget> mainRenderPassTarget = std::make_shared<transforms::OffscreenRenderTarget>(W,
		H, ctx.get(), gSharedDescriptors.get(), gRtvDsvSharedHeap.get());
	gImguiManager = std::make_unique<transforms::MyImguiManager>(gWindow->Hwnd(), ctx->GetDevice().Get(),
		ctx->GetCommandQueue().Get(), gSharedDescriptors.get());
	float exposure = 0.002f;

	//set onIdle handle to deal with rendering
	window.mOnIdle = [&ctx, &exposure, &rootSignatureService, &deltaTimer, &mainRenderPassTarget]() {
		const float deltaTime = deltaTimer.GetDelta();
		//A rudimentary animation to test the transform
		transforms::systems::RunScripts(gRegistry, deltaTime);
		//Update matrices
		transforms::components::UpdateAllTransforms(gRegistry);
		//wait until i can interact with this frame again
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = ctx->ResetFrame();

		//Update the per-object uniform buffer
		auto renderables = gRegistry.view<transforms::components::Renderable, transforms::components::Transform, BSDFMaterial_t>();
		renderables.each([&ctx](entt::entity entity,
			const transforms::components::Renderable& renderable,
			const transforms::components::Transform& transform,
			const BSDFMaterial_t mat) {
				//TODO PBR: transfer PBR data
				transforms::PerObjectData pod{};
				pod.modelMatrix = DirectX::XMMatrixTranspose(transform.worldMatrix);
				pod.inverseTransposeModelMat = DirectX::XMMatrixInverse(nullptr, transform.worldMatrix);

				pod.baseColor = DirectX::XMLoadFloat4(&mat->baseColor);
				pod.metallicFactor = mat->metallicFactor;
				pod.roughnessFactor = mat->roughnessFactor;
				pod.opacity = mat->opacity;
				pod.refracti = mat->refracti;
				pod.emissiveColor = DirectX::XMVectorSet(mat->emissiveColor.x, mat->emissiveColor.y, mat->emissiveColor.z, 1);

				gPerObjectUniformBuffer->SetValue(ctx->GetFrameIndex(),
					renderable.uniformBufferId, pod);
			});
		gPerObjectUniformBuffer->CopyToGPU(ctx->GetFrameIndex(), commandList.Get());
		
		auto frameIndex = ctx->GetFrameIndex();
		
		//TODO REFACTOR: Move this to a better place to clean up the main loop.
		auto lights = gRegistry.view<transforms::components::Transform, transforms::components::PointLight, std::shared_ptr<transforms::CubeMapShadowMap>>();
		//TODO REFACTOR: Create the light data upload system to process the light entities and clean up the main loop code.
		uint32_t numLights = 0;
		lights.each([&numLights, &ctx](
			entt::entity e, 
			transforms::components::Transform t, 
			transforms::components::PointLight l, 
			std::shared_ptr<transforms::CubeMapShadowMap> shadowMap) {
			LightingData ld;
			ld.attenuationConstant = l.attenuationConstant;
			ld.attenuationLinear = l.attenuationLinear;
			ld.attenuationQuadratic = l.attenuationQuadratic;
			ld.ColorAmbient = l.ColorAmbient;
			ld.ColorDiffuse = l.ColorDiffuse;
			ld.ColorSpecular = l.ColorSpecular;
			auto _p = t.GetWorldPosition();
			ld.position.x = _p.x;
			ld.position.y = _p.y;
			ld.position.z = _p.z;
			ld.position.w = 0;
			//copy shadow map data.
			DirectX::XMStoreFloat4x4(&ld.projectionMatrix, DirectX::XMMatrixTranspose(shadowMap->GetProjectionMatrix()));
			ld.shadowFarPlane = shadowMap->GetFarPlane();
			ld.shadowMapIndex = numLights;
			
			gLightingDataUniformBuffer->SetValue(ctx->GetFrameIndex(), numLights, ld);
			numLights++;
			});
		gLightingDataUniformBuffer->CopyToGPU(ctx->GetFrameIndex(), commandList.Get());
		//TODO REFACTOR: Move this to a system somewhere to clean up the main loop
		auto mainCameraView = gRegistry.view<transforms::components::Transform,
			transforms::components::Perspective,
			transforms::components::tags::MainCamera>();
		mainCameraView.each([&exposure, numLights, &ctx, &commandList](auto entity, transforms::components::Transform transform, auto perspective) {
			//For now i assume that there's only one camera that matters, the one with the MainCamera tag.
			using namespace DirectX;
			XMMATRIX viewMatrix = XMMatrixInverse(nullptr, transform.worldMatrix);
			XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(perspective.fovDegrees), perspective.ratio, perspective.zNear, perspective.zFar);
			DirectX::XMMATRIX viewProjectionMatrix = XMMatrixTranspose(XMMatrixMultiply(viewMatrix, projectionMatrix));

			PerFrameDataForUnlitDebug pfd{ viewProjectionMatrix };
			gPerFrameUnlitDebugUniformBuffer->SetValue(ctx->GetFrameIndex(), 0, pfd);
			gPerFrameUnlitDebugUniformBuffer->CopyToGPU(ctx->GetFrameIndex(), commandList.Get());

			PerFrameDataForSimpleLighting _sl;
			_sl.cameraPosition = transform.GetWorldPosition();
			_sl.projMatrix = DirectX::XMMatrixTranspose(projectionMatrix);
			_sl.viewMatrix = DirectX::XMMatrixTranspose(viewMatrix);
			_sl.viewProjMatrix = viewProjectionMatrix;
			_sl.numberOfPointLights = numLights;
			_sl.exposure.x = exposure;
			gPerFrameSimpleLightingUniformBuffer->SetValue(ctx->GetFrameIndex(), 0, _sl);
			gPerFrameSimpleLightingUniformBuffer->CopyToGPU(ctx->GetFrameIndex(), commandList.Get());
			});
		
		auto shadowProjectors = gRegistry.view<transforms::components::Transform, transforms::components::PointLight, std::shared_ptr<transforms::CubeMapShadowMap>>(); //list of shadow projectors
		//Fill the shadow data structured buffer and copy to gpu
		transforms::ShadowDataDataUploadSystem(shadowProjectors, gPointShadowUniformBuffer.get(), frameIndex);
		gPointShadowUniformBuffer->CopyToGPU(frameIndex, commandList.Get());
		///POINT LIGHT SHADOW MAP RENDER PASS
		transforms::PointShadowMapCalculationSystem(
			shadowProjectors,
			renderables,
			frameIndex,
			rootSignatureService->Get(shadowMapRootSignature).Get(),
			commandList.Get(),
			ctx->GetShadowMapPipeline(),
			gSharedDescriptors.get(),
			gPerObjectUniformBuffer.get(),
			gPointShadowUniformBuffer.get()
		);
		commandList->BeginEvent(0, L"MainRenderPass", sizeof(L"MainRenderPass"));
		SetOffscreenTextureAsCurrentRenderTarget<transforms::OffscreenRenderTarget>(mainRenderPassTarget.get(),commandList, ctx->GetFrameIndex());
		//Bind the root descriptor
		commandList->SetGraphicsRootSignature(rootSignatureService->Get(simpleLightingRootSignature).Get());
		//Bind the "descriptor sets"
		ID3D12DescriptorHeap* heaps[] = { gSharedDescriptors->GetHeap() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
		// Bind descriptor tables

		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> shadowMapDescriptorStart = gSharedDescriptors->GetShadowMapDescriptorRangeStart();
		// Root parameter 0: per-object data SRV (t0)
		commandList->SetGraphicsRootDescriptorTable(0,
			gPerObjectUniformBuffer->GetGPUHandle(ctx->GetFrameIndex()));
		//lighting: bind the light buffer	
		commandList->SetGraphicsRootDescriptorTable(2,
			gPerFrameSimpleLightingUniformBuffer->GetGPUHandle(ctx->GetFrameIndex()));
		commandList->SetGraphicsRootDescriptorTable(3,
			gLightingDataUniformBuffer->GetGPUHandle(ctx->GetFrameIndex()));
		commandList->SetGraphicsRootDescriptorTable(4, shadowMapDescriptorStart.second);
		// Fill out the Viewport
		SetViewportAndScissors(unlitDebugPipeline, W, H);
		SetViewportAndScissors(BSDFPipeline, W, H);
		BSDFPipeline->Bind(ctx->GetCommandList());
		//Draw, in the same order that we passed the data
		renderables.each([&ctx](entt::entity entity,
			const transforms::components::Renderable& renderable,
			const transforms::components::Transform& transform,
			const BSDFMaterial_t material)
			{
				ctx->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				ctx->GetCommandList()->IASetVertexBuffers(0, 1, &renderable.mVertexBufferView);
				ctx->GetCommandList()->IASetIndexBuffer(&renderable.mIndexBufferView);
				ctx->GetCommandList()->SetGraphicsRoot32BitConstant(1, renderable.uniformBufferId, 0);
				ctx->GetCommandList()->DrawIndexedInstanced(
					renderable.mNumberOfIndices,     // number of indices
					1,              // instance count
					0,              // start index
					0,              // base vertex
					0               // start instance
				);
			});

		commandList->EndEvent();
		commandList->BeginEvent(0, L"PresentationPass", sizeof(L"PresentationPass"));
		///The main render pass is finished. We get the offscreen texture that was used as target and draw it 
		///over a quad.
		//the render target has to be in D3D12_RESOURCE_STATE_RENDER_TARGET state to be be used by the Output Merger to compose the scene
		ctx->TransitionCurrentRenderTarget(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		//then we set the current target for the output merger
		ctx->SetCurrentOutputMergerTarget();;
		// Clear the render target by using the ClearRenderTargetView command
		ctx->ClearRenderTargetView({ 0.4f, 0.2f, 0.4f, 1.0f });
		mainRenderPassTarget->TransitionToPixelShaderResource(commandList.Get(), ctx->GetFrameIndex());
		commandList->SetGraphicsRootSignature(rootSignatureService->Get(quadRenderRootSignature).Get());
		commandList->SetPipelineState(ctx->GetFullscreenQuadPSO().Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootDescriptorTable(0, mainRenderPassTarget->GetSRVHandle(ctx->GetFrameIndex()));
		commandList->DrawInstanced(3, 1, 0, 0);
#pragma region "imgui"
		gImguiManager->BeginFrame();
		gImguiManager->PushImguiWindow("foo", 300, 200);
		gImguiManager->FloatInput("Exposure", exposure, 0.001f, 0.010f);
		//gImguiManager->PushButton("click me", []() {
		//	std::cout << "Button was clicked!" << std::endl;
		//});
		gImguiManager->PopImguiWindow();
		gImguiManager->EndFrame(commandList.Get());
#pragma endregion

		//now that we drew everything, transition the rtv to presentation
		ctx->TransitionCurrentRenderTarget(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->EndEvent();
		//Submit the commands and present
		ctx->Present();
		};
	//fire main loop
	window.MainLoop();
	ctx->WaitForPreviousFrame();

	rootSignatureService.reset();
	ctx.reset();
	for (auto& m : gMeshTable) {
		m.second = nullptr;
	}
	gMeshTable.clear();
	return 0;
}
DirectX::XMFLOAT3 _aiVec3ToDirectXVector(aiVector3D& vec)
{
	DirectX::XMFLOAT3 v(vec.x, vec.y, vec.z);
	return v;
}

DirectX::XMFLOAT4 _aiColor3DToDirectXVector4(aiColor3D& vec)
{
	DirectX::XMFLOAT4 v(vec.r, vec.g, vec.b, 1.0f);
	return v;
}
DirectX::XMFLOAT2 _removeZ(DirectX::XMFLOAT3 vec)
{
	DirectX::XMFLOAT2 v(vec.x, vec.y);
	return v;
}
std::tuple<DirectX::XMFLOAT3, DirectX::XMFLOAT4, DirectX::XMFLOAT3> GetLocalTransform(aiMatrix4x4 transform) {
	aiVector3D position;
	aiQuaternion rotation;
	aiVector3D scale;
	transform.Decompose(scale, rotation, position);
	DirectX::XMFLOAT3 p(position.x, position.y, position.z);
	DirectX::XMFLOAT4 r(rotation.x, rotation.y, rotation.z, rotation.w);
	DirectX::XMFLOAT3 s(scale.x, scale.y, scale.z);
	return std::make_tuple(p, r, s); 
}
entt::entity ProcessNode(aiNode* node, const aiScene* scene, 
	transforms::Context& ctx, 
	std::vector<BSDFMaterial_t> materials,
	entt::entity parent = entt::null) {
	using namespace transforms::components;
	std::string name = node->mName.C_Str();
	std::cout << "Processing node " << name << std::endl;
	entt::entity e = gRegistry.create();
	// Decompose the matrix into components and create the transform component.
	auto prs = GetLocalTransform(node->mTransformation);
	Transform transform;
	transform.position = std::get<0>(prs);
	transform.rotation = std::get<1>(prs);
	transform.scale = std::get<2>(prs);
	transform.updateEulers();
	gRegistry.emplace<Transform>(e, transform);
	// Create the hierarchy component and set the parent received as param as the parent of the hierarchy
	Hierarchy hierarchy;
	hierarchy.parent = parent;
	//Get the meshes
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* currMesh = scene->mMeshes[node->mMeshes[i]];
		common::MeshData md;
		md.normals.resize(currMesh->mNumVertices);
		md.vertices.resize(currMesh->mNumVertices);
		md.uv.resize(currMesh->mNumVertices);
		md.tg.resize(currMesh->mNumVertices);
		md.cotg.resize(currMesh->mNumVertices);
		for (uint32_t i = 0; i < currMesh->mNumVertices; i++) {
			md.vertices[i] = _aiVec3ToDirectXVector(currMesh->mVertices[i]);
			md.normals[i] = _aiVec3ToDirectXVector(currMesh->mNormals[i]);
			md.uv[i] = _removeZ(_aiVec3ToDirectXVector(currMesh->mTextureCoords[0][i]));
			//PBR: get tangent and bitangent
			md.tg[i] = _aiVec3ToDirectXVector(currMesh->mTangents[i]);
			md.cotg[i] = _aiVec3ToDirectXVector(currMesh->mBitangents[i]);
			currMesh->mTangents[i];
		}
		std::vector<uint16_t> indexData;
		for (unsigned int j = 0; j < currMesh->mNumFaces; j++) {
			aiFace face = currMesh->mFaces[j];
			for (unsigned int k = 0; k < face.mNumIndices; k++) {
				indexData.push_back(face.mIndices[k]);
			}
		}
		md.name = std::string(currMesh->mName.C_Str());
		md.indices = indexData;
		std::shared_ptr<common::Mesh> dxMesh = std::make_shared<common::Mesh>(md,ctx.GetDevice(), ctx.GetCommandQueue());
		auto meshIdx = gMeshTable.size();
		gMeshTable.insert({ meshIdx, dxMesh });
		std::cout << " Has mesh, added at index " << meshIdx << " " << currMesh->mName.C_Str() << std::endl;
		//now that i have the mesh, create the renderable
		Renderable renderable;
		renderable.mIndexBufferView = dxMesh->IndexBufferView();
		renderable.mNumberOfIndices = dxMesh->NumberOfIndices();
		renderable.mVertexBufferView = dxMesh->VertexBufferView();
		renderable.uniformBufferId = GetNumberOfRenderables(gRegistry);
		gRegistry.emplace<Renderable>(e, renderable);
		//PBR: Add the material component to the meshes based on the material id
		auto material = materials[currMesh->mMaterialIndex];
		gRegistry.emplace<BSDFMaterial_t>(e, material);
	}

	//lighting: Get the lights
	for (unsigned int i = 0; i < scene->mNumLights; ++i) {
		aiLight* light = scene->mLights[i];
		if (strcmp(light->mName.C_Str(), node->mName.C_Str()) == 0) {
			//I assume one light per node.
			transforms::components::PointLight pl{}; 
			
			std::string str(light->mName.C_Str());
			std::wstring wstr(str.begin(), str.end());
			
			pl.name = wstr;
			pl.attenuationConstant = light->mAttenuationConstant;
			pl.attenuationLinear = light->mAttenuationLinear;
			pl.attenuationQuadratic = light->mAttenuationQuadratic;
			pl.ColorAmbient = _aiColor3DToDirectXVector4(light->mColorAmbient);
			pl.ColorDiffuse = _aiColor3DToDirectXVector4(light->mColorDiffuse);
			pl.ColorSpecular = _aiColor3DToDirectXVector4(light->mColorSpecular);
			gRegistry.emplace<transforms::components::PointLight>(e, pl);
		}
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		entt::entity child = ProcessNode(node->mChildren[i], scene,ctx,materials, e);
		hierarchy.AddChild(child);
	}
	gRegistry.emplace<Hierarchy>(e, hierarchy);
	return e;
}
std::string GetMaterialName(aiMaterial* material) {
	aiString ai_name;
	material->Get(AI_MATKEY_NAME, ai_name);
	std::string str_name = ai_name.C_Str();
	return str_name;
}
DirectX::XMFLOAT4 GetBaseColor(aiMaterial* material) {
	aiColor4D c;
	material->Get(AI_MATKEY_BASE_COLOR, c);
	DirectX::XMFLOAT4 dx_color(c.r, c.g, c.b, c.a);
	return dx_color;
}
float GetMetallicFactor(aiMaterial* material) {
	float f;
	material->Get(AI_MATKEY_METALLIC_FACTOR, f);
	return f;
}
float GetRoughnessFactor(aiMaterial* material) {
	float f;
	material->Get(AI_MATKEY_ROUGHNESS_FACTOR, f);
	return f;
}
DirectX::XMFLOAT3 GetColorEmissive(aiMaterial* m) {
	aiColor3D c;
	m->Get(AI_MATKEY_COLOR_EMISSIVE, c);
	DirectX::XMFLOAT3 dx_color(c.r, c.g, c.b);
	return dx_color;
}
float GetOpacity(aiMaterial* m) {
	float f;
	m->Get(AI_MATKEY_OPACITY, f);
	return f;
}
float GetRefracti(aiMaterial* m) {
	//TODO PBR FIXME: Blender aint exporting the index of refraction
	float f = 2.5f; // Default IOR for most materials (air-glass boundary)
	if (AI_SUCCESS != m->Get(AI_MATKEY_REFRACTI, f)) {
		std::cout << "No IOR (Refracti) in material" << std::endl;
		f = 2.5f; // fallback
	}
	return f;
}
std::string GetBaseColorTexturePath(aiMaterial* m) {
	aiString ai_name;
	// GetTexture needs 3 parameters: texture type, index, and output string
	if (m->GetTexture(aiTextureType_BASE_COLOR, 0, &ai_name) == AI_SUCCESS) {
		return std::string(ai_name.C_Str());
	}
	return ""; 
}
std::string GetMetalnessTexturePath(aiMaterial* m) {
	aiString ai_name;
	// GetTexture needs 3 parameters: texture type, index, and output string
	if (m->GetTexture(aiTextureType_METALNESS, 0, &ai_name) == AI_SUCCESS) {
		return std::string(ai_name.C_Str());
	}
	return "";
}
std::string GetDiffuseRoughnessTexturePath(aiMaterial* m) {
	aiString ai_name;
	// GetTexture needs 3 parameters: texture type, index, and output string
	if (m->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &ai_name) == AI_SUCCESS) {
		return std::string(ai_name.C_Str());
	}
	return "";
	
}
std::string GetNormalsTexturePath(aiMaterial* m) {
	aiString ai_name;
	// GetTexture needs 3 parameters: texture type, index, and output string
	if (m->GetTexture(aiTextureType_NORMALS, 0, &ai_name) == AI_SUCCESS) {
		return std::string(ai_name.C_Str());
	}
	return "";
}
std::string GetEmissiveTexturePath(aiMaterial* m) {
	aiString ai_name;
	// GetTexture needs 3 parameters: texture type, index, and output string
	if (m->GetTexture(aiTextureType_EMISSIVE, 0, &ai_name) == AI_SUCCESS) {
		return std::string(ai_name.C_Str());
	}
	return "";
}
void LoadScene(transforms::Context& ctx) {
	///////path setup
	std::filesystem::path executionPath = std::filesystem::current_path();
	std::cout << "executionPath: " << executionPath << '\n';
	const std::string path = "assets/Map.glb";
	Assimp::Importer importer;
	///////read the file and throws if something bad happens
	const aiScene* scene = importer.ReadFile(path.c_str(),
		aiProcess_Triangulate 
		|aiProcess_JoinIdenticalVertices 
		| aiProcess_CalcTangentSpace //PBR: need this to generate tangents and bitangents
		|aiProcess_MakeLeftHanded 
		|aiProcess_FlipWindingOrder
	);
	std::vector<BSDFMaterial_t> bsdfMaterials;
	const char* err = importer.GetErrorString();
	if (!scene) {
		throw std::runtime_error(importer.GetErrorString());
	}
	//Material: read all materials
	for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
		aiMaterial* material = scene->mMaterials[i];
		std::string name = GetMaterialName(material);
		DirectX::XMFLOAT4 baseColor = GetBaseColor(material);
		float metallicFactor = GetMetallicFactor(material);
		float roughnessFactor = GetRoughnessFactor(material);
		DirectX::XMFLOAT3 emissiveColor = GetColorEmissive(material);
		float opacity = GetOpacity(material);
		float refracti = GetRefracti(material);
		//TODO PBR: Use the textures if they are present
		std::string baseColorTexture = GetBaseColorTexturePath(material); 
		std::string metalnessTexture = GetMetalnessTexturePath(material);
		std::string diffuseRoughnessTexture = GetDiffuseRoughnessTexturePath(material);
		std::string normalsTexture = GetNormalsTexturePath(material);
		std::string emissiveTexture = GetEmissiveTexturePath(material);
		auto m = std::make_shared<transforms::components::BSDFMaterial>();
		m->baseColor = baseColor;
		m->emissiveColor = emissiveColor;
		m->idInFile = i;
		m->metallicFactor = metallicFactor;
		m->name = name;
		m->opacity = opacity;
		m->refracti = refracti;
		m->roughnessFactor = roughnessFactor;
		bsdfMaterials.push_back(m);
		
	}
	//entt::entity child = ProcessNode(node->mChildren[i], scene,ctx,materials, e);
	entt::entity rootEntity = ProcessNode(scene->mRootNode, scene, ctx, bsdfMaterials);
}
void LoadMeshForOffscreenPresentation(transforms::Context& ctx) {
	///////path setup
	std::filesystem::path executionPath = std::filesystem::current_path();
	std::cout << "executionPath: " << executionPath << '\n';
	const std::string path = "assets/cube.glb";
	Assimp::Importer importer;
	///////read the file and throws if something bad happens
	const aiScene* scene = importer.ReadFile(path.c_str(),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices

		| aiProcess_MakeLeftHanded
		| aiProcess_FlipWindingOrder
	);
	const char* err = importer.GetErrorString();
	if (!scene) {
		throw std::runtime_error(importer.GetErrorString());
	}
	aiMesh* currMesh = nullptr;
	std::function<void(aiNode* node)> findMesh = [&scene, &currMesh, &findMesh](aiNode* node) {
		if (node->mNumMeshes == 1) {
			std::string name = node->mName.C_Str();
			assert(name == "Cube");
			currMesh = scene->mMeshes[node->mMeshes[0]];
			return;
		}
		else {
			for (int i = 0; i < node->mNumChildren; i++) {
				findMesh(node->mChildren[i]);
			}
		}
	};
	findMesh(scene->mRootNode);
	assert(currMesh != nullptr);
	common::MeshData md;
	md.normals.resize(currMesh->mNumVertices);
	md.vertices.resize(currMesh->mNumVertices);
	md.uv.resize(currMesh->mNumVertices);
	for (uint32_t i = 0; i < currMesh->mNumVertices; i++) {
		md.vertices[i] = _aiVec3ToDirectXVector(currMesh->mVertices[i]);
		md.normals[i] = _aiVec3ToDirectXVector(currMesh->mNormals[i]);
		md.uv[i] = _removeZ(_aiVec3ToDirectXVector(currMesh->mTextureCoords[0][i]));
	}
	std::vector<uint16_t> indexData;
	for (unsigned int j = 0; j < currMesh->mNumFaces; j++) {
		aiFace face = currMesh->mFaces[j];
		for (unsigned int k = 0; k < face.mNumIndices; k++) {
			indexData.push_back(face.mIndices[k]);
		}
	}
	md.name = std::string(currMesh->mName.C_Str());
	md.indices = indexData;
	std::shared_ptr<common::Mesh> dxMesh = std::make_shared<common::Mesh>(md, ctx.GetDevice(), ctx.GetCommandQueue());
	auto meshIdx = gMeshTable.size();
	gMeshTable.insert({ meshIdx, dxMesh });
	std::cout << " Has mesh, added at index " << meshIdx << " " << currMesh->mName.C_Str() << std::endl;
	//now that i have the mesh, create the renderable
	entt::entity e = gRegistry.create();
	transforms::components::Renderable renderable;
	renderable.mIndexBufferView = dxMesh->IndexBufferView();
	renderable.mNumberOfIndices = dxMesh->NumberOfIndices();
	renderable.mVertexBufferView = dxMesh->VertexBufferView();
	renderable.uniformBufferId = GetNumberOfRenderables(gRegistry);
	gRegistry.emplace<transforms::components::Renderable>(e, renderable);
	//this entity won't have a transform because it'll not be trasformed. It exists to be a surface for drawing a quad
	//over the screen.
	transforms::components::tags::SceneOffscreenRenderResult r{};
	gRegistry.emplace<transforms::components::tags::SceneOffscreenRenderResult>(e, r);

}
void LoadMeshes(transforms::Context* ctx) {
	LoadScene(*ctx);
	LoadMeshForOffscreenPresentation(*ctx); //offscreen: Load a quad from the disk to serve as mesh. it'll have the tag SceneOffscreenRenderResult and a renderable component
}

void CreateRootSignatures(transforms::RootSignatureService* rootSignatureService, transforms::Context* ctx) {
	rootSignatureService->Add(simpleLightingRootSignature, ctx->CreateSimpleLightingRootSignature(simpleLightingRootSignature));
	rootSignatureService->Add(unlitDebugRootSignature, ctx->CreateUnlitDebugRootSignature(unlitDebugRootSignature));
	rootSignatureService->Add(quadRenderRootSignature, ctx->CreateQuadRenderRootSignature());
	rootSignatureService->Add(shadowMapRootSignature, ctx->CreateShadowMapRootSignature());
}

void CreatePipelines(transforms::RootSignatureService* rootSignatureService, transforms::Context* ctx) {
	//TODO SHADOWS: Create a pipeline to the shadow maps
	//TODO SHADOWS: Create the shaders for shadows
	unlitDebugPipeline = std::make_shared<transforms::Pipeline>(
		L"C:\\dev\\directx12\\x64\\Debug\\transforms_vertex_shader.cso",
		L"C:\\dev\\directx12\\x64\\Debug\\transforms_pixel_shader.cso",
		rootSignatureService->Get(unlitDebugRootSignature),
		ctx->GetDevice(),
		L"HelloWorldPipeline"
	);
	BSDFPipeline = std::make_shared<transforms::Pipeline>(
		L"C:\\dev\\directx12\\x64\\Debug\\bsdf_vs.cso",
		L"C:\\dev\\directx12\\x64\\Debug\\bsdf_ps.cso",
		rootSignatureService->Get(simpleLightingRootSignature),
		ctx->GetDevice(),
		L"BSDFPipeline"
	);
	ctx->CreateFullscreenQuadPipeline(rootSignatureService->Get(quadRenderRootSignature).Get());
	ctx->CreateShadowMapPipeline(rootSignatureService->Get(shadowMapRootSignature).Get());
}