#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>

#include "Vertex.h"


class Mesh
{
public:
	Mesh(const char* name, Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices);
	Mesh(const char* name, const std::wstring& objFile);
	~Mesh();

	// Getters for mesh data
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
	Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexBuffer();
	const char* GetName();
	size_t GetIndexCount();
	size_t GetVertexCount();

private:
	// D3D buffers
	D3D12_VERTEX_BUFFER_VIEW vbView;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;

	D3D12_INDEX_BUFFER_VIEW ibView;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;

	// Total indices & vertices in this mesh
	size_t numIndices;
	size_t numVertices;

	// Name (mostly for UI purposes)
	const char* name;

	// Helpers
	void CalculateTangents(Vertex* verts, size_t numVerts, unsigned int* indices, size_t numIndices);
	void CreateBuffers(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices);

};


