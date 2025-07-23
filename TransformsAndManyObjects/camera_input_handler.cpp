#include "pch.h"
#include "camera_input_handler.h"
#include <DirectXMath.h>
#include "components.h"
#include "game_window.h"
DirectX::XMFLOAT3 camTarget = DirectX::XMFLOAT3(-6, 0.f, 0);
constexpr float speed = 10.0f;
DirectX::XMFLOAT3 right(1, 0, 0);
DirectX::XMFLOAT3 forward(0, 0, 1);
DirectX::XMFLOAT3 up(0, 1, 0);
void transforms::components::CreateCameraInputHandler(transforms::Window* gWindow, entt::registry& gRegistry,
	entt::entity mainCamera)
{
	transforms::components::Script cameraScript;
	cameraScript.execute = [gWindow](float deltaTime, entt::entity self, entt::registry& registry) {
		using namespace DirectX;
		//TODO INPUT: Create something like the axis system from unity input and use it instead.
		transforms::components::Transform& t = registry.get<transforms::components::Transform>(self);

		XMVECTOR _pos = XMLoadFloat3(&t.position);
		XMMATRIX cameraMatrix = XMMatrixRotationQuaternion(XMLoadFloat4(&t.rotation));
		XMVECTOR _ct = XMLoadFloat3(&camTarget);
		// Extract local axes
		XMVECTOR _r = XMLoadFloat3(&right);
		XMVECTOR _f = XMLoadFloat3(&forward);
		XMVECTOR _u = XMLoadFloat3(&up);

		if (gWindow->GetLastKey() == transforms::KeyCodes::A) {
			_r = XMVectorScale(_r, speed * deltaTime);
			_pos = XMVectorAdd(_r, _pos);
			_ct = XMVectorAdd(_r, _ct);
		}
		if (gWindow->GetLastKey() == transforms::KeyCodes::D) {
			_r = XMVectorScale(_r, -speed * deltaTime);
			_pos = XMVectorAdd(_r, _pos);
			_ct = XMVectorAdd(_r, _ct);

		}
		if (gWindow->GetLastKey() == transforms::KeyCodes::W) {
			_f = XMVectorScale(_f, speed * deltaTime);
			_pos = XMVectorAdd(_f, _pos);
			_ct = XMVectorAdd(_f, _ct);
		}
		if (gWindow->GetLastKey() == transforms::KeyCodes::S) {
			_f = XMVectorScale(_f, -speed * deltaTime);
			_pos = XMVectorAdd(_f, _pos);
			_ct = XMVectorAdd(_f, _ct);
		}

		XMStoreFloat3(&t.position, _pos);
		XMStoreFloat3(&camTarget, _ct);
		t.LookAt(camTarget);
		};
	gRegistry.emplace<transforms::components::Script>(mainCamera, cameraScript);
}
