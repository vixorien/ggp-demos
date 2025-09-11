#include "Mesh.h"
#include "Graphics.h"

using namespace DirectX;

// --------------------------------------------------------
// Creates a new mesh with the given geometry
// 
// name       - The name of the mesh (mostly for UI purposes)
// vertArray  - An array of vertices
// numVerts   - The number of verts in the array
// indexArray - An array of indices into the vertex array
// numIndices - The number of indices in the index array
// --------------------------------------------------------
Mesh::Mesh(const char* name, Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices) :
	name(name)
{
	CreateBuffers(vertArray, numVerts, indexArray, numIndices);
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
const char* Mesh::GetName() { return name; }
unsigned int Mesh::GetIndexCount() { return numIndices; }
unsigned int Mesh::GetVertexCount() { return numVertices; }

// --------------------------------------------------------
// Helper for creating the actually D3D buffers
// 
// vertArray  - An array of vertices
// numVerts   - The number of verts in the array
// indexArray - An array of indices into the vertex array
// numIndices - The number of indices in the index array
// --------------------------------------------------------
void Mesh::CreateBuffers(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices)
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
	Graphics::Device->CreateBuffer(&vbd, &initialVertexData, vb.GetAddressOf());

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
	Graphics::Device->CreateBuffer(&ibd, &initialIndexData, ib.GetAddressOf());

	// Save the counts
	this->numIndices = (unsigned int)numIndices;
	this->numVertices = (unsigned int)numVerts;
}


// --------------------------------------------------------
// Binds the mesh buffers and issues a draw call.  Note that
// this method assumes you're drawing the entire mesh.
// --------------------------------------------------------
void Mesh::SetBuffersAndDraw()
{
	// Set buffers in the input assembler
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	Graphics::Context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
	Graphics::Context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Draw this mesh
	Graphics::Context->DrawIndexed(this->numIndices, 0, 0);
}
