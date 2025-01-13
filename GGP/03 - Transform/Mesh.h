#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Vertex.h"


class Mesh
{
public:
	Mesh(const char* name, Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices);
	~Mesh();

	// Getters for mesh data
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer();
	const char* GetName();
	unsigned int GetIndexCount();
	unsigned int GetVertexCount();

	// Basic mesh drawing
	void SetBuffersAndDraw();

private:
	// D3D buffers
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib;

	// Total indices & vertices in this mesh
	unsigned int numIndices;
	unsigned int numVertices;

	// Name (mostly for UI purposes)
	const char* name;

	// Helper for creating buffers (in the event we add more constructor overloads)
	void CreateBuffers(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices);

};

