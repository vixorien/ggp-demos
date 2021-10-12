#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Vertex.h"


class Mesh
{
public:
	Mesh(Vertex* vertArray, int numVerts, unsigned int* indexArray, int numIndices, Microsoft::WRL::ComPtr<ID3D11Device> device);
	Mesh(const char* objFile, Microsoft::WRL::ComPtr<ID3D11Device> device);
	~Mesh();

	// Getters for mesh data
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer() { return vb; }
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() { return ib; }
	int GetIndexCount() { return numIndices; }

	// Basic mesh drawing
	void SetBuffersAndDraw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

private:
	// D3D buffers
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib;

	// Total indices in this mesh
	int numIndices;

	// Helper for creating buffers (in the event we add more constructor overloads)
	void CreateBuffers(Vertex* vertArray, int numVerts, unsigned int* indexArray, int numIndices, Microsoft::WRL::ComPtr<ID3D11Device> device);

};

