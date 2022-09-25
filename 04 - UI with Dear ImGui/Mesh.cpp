#include "Mesh.h"
#include <DirectXMath.h>
#include <vector>
#include <fstream>

using namespace DirectX;

// --------------------------------------------------------
// Creates a new mesh with the given geometry
// 
// vertArray  - An array of vertices
// numVerts   - The number of verts in the array
// indexArray - An array of indices into the vertex array
// numIndices - The number of indices in the index array
// device     - The D3D device to use for buffer creation
// --------------------------------------------------------
Mesh::Mesh(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices, Microsoft::WRL::ComPtr<ID3D11Device> device)
{
	CreateBuffers(vertArray, numVerts, indexArray, numIndices, device);
}


// --------------------------------------------------------
// Destructor doesn't have much to do since we're using ComPtrs
// --------------------------------------------------------
Mesh::~Mesh() { }


// --------------------------------------------------------
// Getters for private variables
// --------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11Buffer> Mesh::GetVertexBuffer() { return vb; }
Microsoft::WRL::ComPtr<ID3D11Buffer> Mesh::GetIndexBuffer() { return ib; }
unsigned int Mesh::GetIndexCount() { return numIndices; }


// --------------------------------------------------------
// Helper for creating the actually D3D buffers
// 
// vertArray  - An array of vertices
// numVerts   - The number of verts in the array
// indexArray - An array of indices into the vertex array
// numIndices - The number of indices in the index array
// device     - The D3D device to use for buffer creation
// --------------------------------------------------------
void Mesh::CreateBuffers(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices, Microsoft::WRL::ComPtr<ID3D11Device> device)
{
	// Create the vertex buffer
	D3D11_BUFFER_DESC vbd	= {};
	vbd.Usage				= D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth			= sizeof(Vertex) * (UINT)numVerts; // Number of vertices
	vbd.BindFlags			= D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags		= 0;
	vbd.MiscFlags			= 0;
	vbd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA initialVertexData = {};
	initialVertexData.pSysMem = vertArray;
	device->CreateBuffer(&vbd, &initialVertexData, vb.GetAddressOf());

	// Create the index buffer
	D3D11_BUFFER_DESC ibd	= {};
	ibd.Usage				= D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth			= sizeof(unsigned int) * (UINT)numIndices; // Number of indices
	ibd.BindFlags			= D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags		= 0;
	ibd.MiscFlags			= 0;
	ibd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA initialIndexData = {};
	initialIndexData.pSysMem = indexArray;
	device->CreateBuffer(&ibd, &initialIndexData, ib.GetAddressOf());

	// Save the indices
	this->numIndices = (unsigned int)numIndices;
}


// --------------------------------------------------------
// Binds the mesh buffers and issues a draw call.  Note that
// this method assumes you're drawing the entire mesh.
// 
// context - D3D context for issuing rendering calls
// --------------------------------------------------------
void Mesh::SetBuffersAndDraw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Set buffers in the input assembler
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Draw this mesh
	context->DrawIndexed(this->numIndices, 0, 0);
}
