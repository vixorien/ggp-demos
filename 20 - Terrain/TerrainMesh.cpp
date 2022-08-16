#include "TerrainMesh.h"

#include <DirectXMath.h>
#include <vector>
#include <fstream>

using namespace DirectX;


// --------------------------------------------------------
// Creates a terrain mesh by reading the given heightmap,
// which can be either 8-bit RAW or 16-bit RAW, and creating
// a vertex for each height value.
// 
// device - DX device for resource creation
// heightmap - Full path to heightmap
// heightmapWidth - Width in pixels
// heightmapHeight - Height in pixels
// bitDepth - 8-bit or 16-bit height values?
// yScale - How tall should the terrain be?
// xzScale - How wide should the terrain be?
// --------------------------------------------------------
TerrainMesh::TerrainMesh(
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	const std::wstring heightmap,
	unsigned int heightmapWidth,
	unsigned int heightmapHeight,
	TerrainBitDepth bitDepth,
	float yScale,
	float xzScale)
	: Mesh()
{
	unsigned int numVertices = heightmapWidth * heightmapHeight;
	unsigned int numIndices = (heightmapWidth - 1) * (heightmapHeight - 1) * 6;

	Vertex* verts = new Vertex[numVertices];
	if (bitDepth == TerrainBitDepth::BitDepth_8)
		Load8bitRaw(heightmap, heightmapWidth, heightmapHeight, yScale, xzScale, verts);
	else
		Load16bitRaw(heightmap, heightmapWidth, heightmapHeight, yScale, xzScale, verts);

	// Create indices and, while we're at it, calculate the normal
	// of each triangle (as we'll need those for vertex normals)
	unsigned int* indices = new unsigned int[numIndices];
	std::vector<XMFLOAT3> triangleNormals;

	int indexCounter = 0;
	for (unsigned int z = 0; z < heightmapHeight - 1; z++)
	{
		for (unsigned int x = 0; x < heightmapWidth - 1; x++)
		{
			// Calc the vertex index
			int vertIndex = z * heightmapWidth + x;

			// Calculate the indices for these two triangles
			int i0 = vertIndex;
			int i1 = vertIndex + heightmapWidth;
			int i2 = vertIndex + 1 + heightmapWidth;

			int i3 = vertIndex;
			int i4 = vertIndex + 1 + heightmapWidth;
			int i5 = vertIndex + 1;

			// Put these in the index array
			indices[indexCounter++] = i0;
			indices[indexCounter++] = i1;
			indices[indexCounter++] = i2;

			indices[indexCounter++] = i3;
			indices[indexCounter++] = i4;
			indices[indexCounter++] = i5;

			// Get the positions of the three verts of each triangle
			XMVECTOR pos0 = XMLoadFloat3(&verts[i0].Position);
			XMVECTOR pos1 = XMLoadFloat3(&verts[i1].Position);
			XMVECTOR pos2 = XMLoadFloat3(&verts[i2].Position);

			XMVECTOR pos3 = XMLoadFloat3(&verts[i3].Position);
			XMVECTOR pos4 = XMLoadFloat3(&verts[i4].Position);
			XMVECTOR pos5 = XMLoadFloat3(&verts[i5].Position);

			// Calculate the normal of each triangle
			XMFLOAT3 normal0;
			XMFLOAT3 normal1;

			// Cross the edges of the triangle
			XMStoreFloat3(&normal0,
				XMVector3Normalize(XMVector3Cross(pos1 - pos0, pos2 - pos0)));

			XMStoreFloat3(&normal1,
				XMVector3Normalize(XMVector3Cross(pos4 - pos3, pos5 - pos3)));

			// Push the normals into the list
			triangleNormals.push_back(normal0);
			triangleNormals.push_back(normal1);
		}
	}

	// Calculate normals!
	for (unsigned int z = 0; z < heightmapHeight; z++)
	{
		for (unsigned int x = 0; x < heightmapWidth; x++)
		{
			// Get the index of this vertex, and triangle-related indices
			int index = z * heightmapWidth + x;
			int triIndex = index * 2 - (2 * z);
			int triIndexPrevRow = triIndex - (heightmapWidth * 2 - 1);

			// Running total of normals
			int normalCount = 0;
			XMVECTOR normalTotal = XMVectorSet(0, 0, 0, 0);

			// Normals
			//XMVECTOR upLeft = XMLoadFloat3(&triangleNormals[triIndexPrevRow - 1]);
			//XMVECTOR up = XMLoadFloat3(&triangleNormals[triIndexPrevRow]);
			//XMVECTOR upRight = XMLoadFloat3(&triangleNormals[triIndexPrevRow + 1]);
			//XMVECTOR downLeft = XMLoadFloat3(&triangleNormals[triIndex - 1]);
			//XMVECTOR down = XMLoadFloat3(&triangleNormals[triIndex]);
			//XMVECTOR downRight = XMLoadFloat3(&triangleNormals[triIndex + 1]);

			// x-----x-----x
			// |\    |\    |  
			// | \ u | \   |  
			// |  \  |  \  |  ul = up left
			// |   \ |   \ |  u  = up
			// | ul \| ur \|  ur = up right
			// x-----O-----x
			// |\ dl |\ dr |  dl = down left
			// | \   | \   |  d  = down
			// |  \  |  \  |  dr = down right
			// |   \ | d \ |
			// |    \|    \|
			// x-----x-----x

			// If not top row and not first column
			if (z > 0 && x > 0)
			{
				// "Up left" and "up"
				normalTotal += XMLoadFloat3(&triangleNormals[triIndexPrevRow - 1]);
				normalTotal += XMLoadFloat3(&triangleNormals[triIndexPrevRow]);

				normalCount += 2;
			}

			// If not top row and not last column
			if (z > 0 && x < heightmapWidth - 1)
			{
				// "Up right"
				normalTotal += XMLoadFloat3(&triangleNormals[triIndexPrevRow + 1]);

				normalCount++;
			}

			// If not bottom row and not first column
			if (z < heightmapHeight - 1 && x > 0)
			{
				// "Down left"
				normalTotal += XMLoadFloat3(&triangleNormals[triIndex - 1]);

				normalCount++;
			}

			// If not bottom row and not last column
			if (z < heightmapHeight - 1 && x < heightmapWidth - 1)
			{
				// "Down right" and "down"
				normalTotal += XMLoadFloat3(&triangleNormals[triIndex]);
				normalTotal += XMLoadFloat3(&triangleNormals[triIndex + 1]);

				normalCount += 2;
			}

			// Average normal
			normalTotal /= (float)normalCount;
			XMStoreFloat3(&verts[index].Normal, normalTotal);
		}
	}

	// Create the buffers and clean up arrays
	this->CreateBuffers(verts, numVertices, indices, numIndices, device);
	delete[] verts;
	delete[] indices;
}

// --------------------------------------------------------
// Clean up any non-smart-pointer resources, if any
// --------------------------------------------------------
TerrainMesh::~TerrainMesh() { }


// --------------------------------------------------------
// Loads an 8-bit RAW heightmap, where each pixel is a
// single 8-bit height value.
// 
// heightmap - Full path to the heightmap file
// width - heightmap width in pixels
// height - heightmap height in pixels
// yScale - How tall should the terrain be?
// xzScale - How wide should the terrain be?
// verts - Array of verts to fill up
// --------------------------------------------------------
void TerrainMesh::Load8bitRaw(const std::wstring heightmap, unsigned int width, unsigned int height, float yScale, float xzScale, Vertex* verts)
{
	unsigned int numVertices = width * height;
	float halfWidth = width / 2.0f;
	float halfHeight = height / 2.0f;

	// Vector to hold heights
	std::vector<unsigned char> heights(numVertices);

	// Open the file (remember to #include <fstream>)
	std::ifstream file;
	file.open(heightmap, std::ios_base::binary);
	if (!file)
		return;

	// Read raw bytes to vector
	file.read((char*)&heights[0], numVertices); // Same size
	file.close();

	// Create the initial mesh data
	for (unsigned int z = 0; z < height; z++)
	{
		for (unsigned int x = 0; x < width; x++)
		{
			// This vert index
			int index = z * width + x;

			// Set up this vertex
			verts[index] = {};

			// Position on a regular grid, heights from heightmap
			verts[index].Position.x = (x - halfWidth) * xzScale;
			verts[index].Position.y = (heights[index] / 255.0f) * yScale;
			verts[index].Position.z = (z - halfHeight) * xzScale;

			// Assume we're starting flat
			verts[index].Normal.x = 0.0f;
			verts[index].Normal.y = 1.0f;
			verts[index].Normal.z = 0.0f;

			// Simple UV (0-1)
			verts[index].UV.x = x / (float)width;
			verts[index].UV.y = z / (float)height;
		}
	}

}


// --------------------------------------------------------
// Loads a 16-bit RAW heightmap, where each pixel is a single
// 16-bit height value.
// 
// heightmap - Full path to the heightmap file
// width - heightmap width in pixels
// height - heightmap height in pixels
// yScale - How tall should the terrain be?
// xzScale - How wide should the terrain be?
// verts - Array of verts to fill up
// --------------------------------------------------------
void TerrainMesh::Load16bitRaw(const std::wstring heightmap, unsigned int width, unsigned int height, float yScale, float xzScale, Vertex* verts)
{
	unsigned int numVertices = width * height;
	float halfWidth = width / 2.0f;
	float halfHeight = height / 2.0f;

	// Vector to hold heights
	std::vector<unsigned short> heights(numVertices);

	// Open the file (remember to #include <fstream>)
	std::ifstream file;
	file.open(heightmap, std::ios_base::binary);
	if (!file)
		return;

	// Read raw bytes to vector
	file.read((char*)&heights[0], numVertices * 2); // Double the size, since each pixel is 16-bit
	file.close();

	// Create the initial mesh data
	for (unsigned int z = 0; z < height; z++)
	{
		for (unsigned int x = 0; x < width; x++)
		{
			// This vert index
			int index = z * width + x;

			// Set up this vertex
			verts[index] = {};

			// Position on a regular grid, heights from heightmap
			verts[index].Position.x = (x - halfWidth) * xzScale;
			verts[index].Position.y = (heights[index] / 65535.0f) * yScale; // 16-bit, so max value is 65535
			verts[index].Position.z = (z - halfHeight) * xzScale;

			// Assume we're starting flat
			verts[index].Normal.x = 0.0f;
			verts[index].Normal.y = 1.0f;
			verts[index].Normal.z = 0.0f;

			// Simple UV (0-1)
			verts[index].UV.x = x / (float)width;
			verts[index].UV.y = z / (float)height;
		}
	}
}
