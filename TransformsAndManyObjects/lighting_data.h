#pragma once
#include <DirectXMath.h>
struct alignas(16) LightingData {
	DirectX::XMFLOAT4 position;
	float attenuationConstant;
	float attenuationLinear;
	float attenuationQuadratic;
	float _notUsed; //for alignment
	DirectX::XMFLOAT4 ColorDiffuse;
	DirectX::XMFLOAT4 ColorSpecular;
	DirectX::XMFLOAT4 ColorAmbient;
	//Data for POINT LIGHT shadow map
	DirectX::XMFLOAT4X4 projectionMatrix;
	float shadowFarPlane;
	int shadowMapIndex;
	DirectX::XMFLOAT2 _notUsed2;
};
