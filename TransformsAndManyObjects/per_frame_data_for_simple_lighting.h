#pragma once
#include <DirectXMath.h>
struct alignas(16) PerFrameDataForSimpleLighting {
	DirectX::XMMATRIX viewProjMatrix;
	DirectX::XMMATRIX viewMatrix;
	DirectX::XMMATRIX projMatrix;
	DirectX::XMFLOAT3 cameraPosition;
	int numberOfPointLights;
	DirectX::XMFLOAT4 exposure;
};