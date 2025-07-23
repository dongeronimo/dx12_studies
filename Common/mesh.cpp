#include "pch.h"
#include "mesh.h"
#include "vertex.h"
#include <locale>
#include "concatenate.h"
#include "d3d_utils.h"

using Microsoft::WRL::ComPtr;


common::Mesh::Mesh(MeshData& data, 
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue):
    mNumberOfIndices(static_cast<int>(data.indices.size())),
    name(multi2wide(data.name))
{
    std::vector<common::Vertex> vertexes(data.vertices.size());
    for (auto i = 0; i < data.vertices.size(); i++)
    {
        vertexes[i].pos = data.vertices[i];
        vertexes[i].normal = data.normals[i];
        vertexes[i].uv = data.uv[i];
    }
	int vBufferSize = static_cast<int>(vertexes.size()) * sizeof(common::Vertex);
    int iBufferSize = static_cast<int>(data.indices.size()) * sizeof(uint16_t);

    mVertexBuffer = CreateGPUBufferAndCopyDataToIt(device.Get(), commandQueue.Get(), vertexes.data(), vBufferSize);
    std::wstring vertex_w_name = Concatenate(multi2wide(data.name), "vertexBuffer");
    mVertexBuffer->SetName(vertex_w_name.c_str());
    mIndexBuffer = CreateGPUBufferAndCopyDataToIt(device.Get(), commandQueue.Get(), data.indices.data(), iBufferSize);
    std::wstring index_w_name = Concatenate(multi2wide(data.name), "indexBuffer");
    mIndexBuffer->SetName(index_w_name.c_str());

    mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVertexBufferView.StrideInBytes = sizeof(common::Vertex);
    mVertexBufferView.SizeInBytes = vBufferSize;

    mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIndexBufferView.SizeInBytes = iBufferSize;
    mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
}
