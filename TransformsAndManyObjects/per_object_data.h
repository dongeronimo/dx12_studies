#pragma once
#include <DirectXMath.h>

namespace transforms {
    struct alignas(16) PerObjectData {
        DirectX::XMMATRIX modelMatrix;
        DirectX::XMMATRIX inverseTransposeModelMat;
        //TODO PBR: Change it to be the fields that BSDF Needs (DONE)
        DirectX::XMVECTOR baseColor;
        float metallicFactor;
        float roughnessFactor;
        float opacity;
        float refracti;
        DirectX::XMVECTOR emissiveColor;
    };
}