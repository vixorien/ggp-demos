#include <fstream>
#include <vector>
#include <stdexcept>
#include <unordered_map>

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
	name(name),
	vbView{},
	ibView{}
{
	CreateBuffers(vertArray, numVerts, indexArray, numIndices);
}

// --------------------------------------------------------
// Creates a new mesh by loading vertices from the given .obj file
// 
// objFile  - Path to the .obj 3D model file to load
// --------------------------------------------------------
Mesh::Mesh(const char* name, const std::wstring& objFile) :
	name(name),
	vbView{},
	ibView{}
{
	// Set indicies to 0 in the event the file reading fails
	numIndices = 0;
	numVertices = 0;

	// File input object
	std::ifstream obj(objFile);

	// Check for successful open
	if (!obj.is_open())
		throw std::invalid_argument("Error opening file: Invalid file path or file is inaccessible");

	// Variables used while reading the file
	std::vector<XMFLOAT3> positions;	// Positions from the file
	std::vector<XMFLOAT3> normals;		// Normals from the file
	std::vector<XMFLOAT2> uvs;			// UVs from the file
	std::vector<Vertex> vertsFromFile;	// Verts from file (including duplicates)
	std::vector<Vertex> finalVertices;	// Final, de-duplicated verts
	std::vector<UINT> finalIndices;		// Indices for final verts
	char chars[100];					// String for line reading

	// Still have data left?
	while (obj.good())
	{
		// Get the line (100 characters should be more than enough)
		obj.getline(chars, 100);

		// Check the type of line
		if (chars[0] == 'v' && chars[1] == 'n')
		{
			// Read the 3 numbers directly into an XMFLOAT3
			XMFLOAT3 norm{};
			sscanf_s(
				chars,
				"vn %f %f %f",
				&norm.x, &norm.y, &norm.z);

			// Add to the list of normals
			normals.push_back(norm);
		}
		else if (chars[0] == 'v' && chars[1] == 't')
		{
			// Read the 2 numbers directly into an XMFLOAT2
			XMFLOAT2 uv{};
			sscanf_s(
				chars,
				"vt %f %f",
				&uv.x, &uv.y);

			// Add to the list of uv's
			uvs.push_back(uv);
		}
		else if (chars[0] == 'v')
		{
			// Read the 3 numbers directly into an XMFLOAT3
			XMFLOAT3 pos{};
			sscanf_s(
				chars,
				"v %f %f %f",
				&pos.x, &pos.y, &pos.z);

			// Add to the positions
			positions.push_back(pos);
		}
		else if (chars[0] == 'f')
		{
			// Read the face indices into an array
			// NOTE: This assumes the given obj file contains
			//  vertex positions, uv coordinates AND normals.
			unsigned int i[12]{};
			int numbersRead = sscanf_s(
				chars,
				"f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
				&i[0], &i[1], &i[2],
				&i[3], &i[4], &i[5],
				&i[6], &i[7], &i[8],
				&i[9], &i[10], &i[11]);

			// If we only got the first number, chances are the OBJ
			// file has no UV coordinates.  This isn't great, but we
			// still want to load the model without crashing, so we
			// need to re-read a different pattern (in which we assume
			// there are no UVs denoted for any of the vertices)
			if (numbersRead == 1)
			{
				// Re-read with a different pattern
				numbersRead = sscanf_s(
					chars,
					"f %d//%d %d//%d %d//%d %d//%d",
					&i[0], &i[2],
					&i[3], &i[5],
					&i[6], &i[8],
					&i[9], &i[11]);

				// The following indices are where the UVs should 
				// have been, so give them a valid value
				i[1] = 1;
				i[4] = 1;
				i[7] = 1;
				i[10] = 1;

				// If we have no UVs, create a single UV coordinate
				// that will be used for all vertices
				if (uvs.size() == 0)
					uvs.push_back(XMFLOAT2(0, 0));
			}

			// - Create the verts by looking up
			//    corresponding data from vectors
			// - OBJ File indices are 1-based, so
			//    they need to be adusted
			Vertex v1{};
			v1.Position = positions[max(i[0] - 1, 0)];
			v1.UV = uvs[max(i[1] - 1, 0)];
			v1.Normal = normals[max(i[2] - 1, 0)];

			Vertex v2{};
			v2.Position = positions[max(i[3] - 1, 0)];
			v2.UV = uvs[max(i[4] - 1, 0)];
			v2.Normal = normals[max(i[5] - 1, 0)];

			Vertex v3{};
			v3.Position = positions[max(i[6] - 1, 0)];
			v3.UV = uvs[max(i[7] - 1, 0)];
			v3.Normal = normals[max(i[8] - 1, 0)];

			// The model is most likely in a right-handed space,
			// especially if it came from Maya.  We probably want 
			// to convert to a left-handed space.  This means we 
			// need to:
			//  - Invert the Z position
			//  - Invert the normal's Z
			//  - Flip the winding order
			// We also need to flip the UV coordinate since Direct3D
			// defines (0,0) as the top left of the texture, and many
			// 3D modeling packages use the bottom left as (0,0)

			// Flip the UV's since they're probably "upside down"
			v1.UV.y = 1.0f - v1.UV.y;
			v2.UV.y = 1.0f - v2.UV.y;
			v3.UV.y = 1.0f - v3.UV.y;

			// Flip Z (LH vs. RH)
			v1.Position.z *= -1.0f;
			v2.Position.z *= -1.0f;
			v3.Position.z *= -1.0f;

			// Flip normal's Z
			v1.Normal.z *= -1.0f;
			v2.Normal.z *= -1.0f;
			v3.Normal.z *= -1.0f;

			// Add the verts to the vector (flipping the winding order)
			vertsFromFile.push_back(v1);
			vertsFromFile.push_back(v3);
			vertsFromFile.push_back(v2);

			// Was there a 4th face?
			// - 12 numbers read means 4 faces WITH uv's
			// - 8 numbers read means 4 faces WITHOUT uv's
			if (numbersRead == 12 || numbersRead == 8)
			{
				// Make the last vertex
				Vertex v4{};
				v4.Position = positions[max(i[9] - 1, 0)];
				v4.UV = uvs[max(i[10] - 1, 0)];
				v4.Normal = normals[max(i[11] - 1, 0)];

				// Flip the UV, Z pos and normal's Z
				v4.UV.y = 1.0f - v4.UV.y;
				v4.Position.z *= -1.0f;
				v4.Normal.z *= -1.0f;

				// Add a whole triangle (flipping the winding order)
				vertsFromFile.push_back(v1);
				vertsFromFile.push_back(v4);
				vertsFromFile.push_back(v3);
			}
		}
	}

	// We'll use hash table (unordered_map) to determine
	// if any of the vertices are duplicates
	std::unordered_map<std::string, unsigned int> vertMap;
	for (auto& v : vertsFromFile)
	{
		// Create a "unique" representation of the vertex (its key)
		// Note: This isn't a super efficient method, but since strings
		//       inherently work with unordered_maps, this saves
		//       us from having to write our own custom hash function
		std::string vStr =
			std::to_string(v.Position.x) +
			std::to_string(v.Position.y) +
			std::to_string(v.Position.z) +
			std::to_string(v.Normal.x) +
			std::to_string(v.Normal.y) +
			std::to_string(v.Normal.z) +
			std::to_string(v.UV.x) +
			std::to_string(v.UV.y);

		// Prepare the index for this vertex and
		// search for the vertex in the hash table
		unsigned int index = -1;
		auto pair = vertMap.find(vStr);
		if (pair == vertMap.end())
		{
			// Vertex not found, so this
			// is the first time we've seen it
			index = (unsigned int)finalVertices.size();
			finalVertices.push_back(v);
			vertMap.insert({ vStr, index });
		}
		else
		{
			// Vert already exists, just
			// grab its index
			index = pair->second;
		}

		// Either way, save the index
		finalIndices.push_back(index);
	}

	// Close the file and create the actual buffers
	obj.close();
	CreateBuffers(&finalVertices[0], finalVertices.size(), &finalIndices[0], finalIndices.size());
}


// --------------------------------------------------------
// Destructor doesn't have much to do since we're using ComPtrs
// --------------------------------------------------------
Mesh::~Mesh() { }


// --------------------------------------------------------
// Getters for private variables
// --------------------------------------------------------
D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView() { return vbView; }
D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() { return ibView; }
const char* Mesh::GetName() { return name; }
size_t Mesh::GetIndexCount() { return numIndices; }
size_t Mesh::GetVertexCount() { return numVertices; }


// --------------------------------------------------------
// Helper for creating the actually D3D buffers
// 
// vertArray  - An array of vertices
// numVerts   - The number of verts in the array
// indexArray - An array of indices into the vertex array
// numIndices - The number of indices in the index array
// device     - The D3D device to use for buffer creation
// --------------------------------------------------------
void Mesh::CreateBuffers(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices)
{
	// Save the counts
	this->numIndices = numIndices;
	this->numVertices = numVerts;

	// Calculate the tangents before copying to buffer
	CalculateTangents(vertArray, numVerts, indexArray, numIndices);

	// Create the two buffers
	vertexBuffer = Graphics::CreateStaticBuffer(sizeof(Vertex), numVerts, vertArray);
	indexBuffer = Graphics::CreateStaticBuffer(sizeof(unsigned int), numIndices, indexArray);

	// Set up the views
	vbView.StrideInBytes = (UINT)sizeof(Vertex);
	vbView.SizeInBytes = (UINT)(sizeof(Vertex) * numVerts);
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();

	ibView.Format = DXGI_FORMAT_R32_UINT;
	ibView.SizeInBytes = (UINT)(sizeof(unsigned int) * numIndices);
	ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
}


// Calculates the tangents of the vertices in a mesh
// Code adapted from: http://www.terathon.com/code/tangent.html
void Mesh::CalculateTangents(Vertex* verts, size_t numVerts, unsigned int* indices, size_t numIndices)
{
	// Reset tangents
	for (int i = 0; i < numVerts; i++)
	{
		verts[i].Tangent = XMFLOAT3(0, 0, 0);
	}

	// Calculate tangents one whole triangle at a time
	for (int i = 0; i < numIndices;)
	{
		// Grab indices and vertices of first triangle
		unsigned int i1 = indices[i++];
		unsigned int i2 = indices[i++];
		unsigned int i3 = indices[i++];
		Vertex* v1 = &verts[i1];
		Vertex* v2 = &verts[i2];
		Vertex* v3 = &verts[i3];

		// Calculate vectors relative to triangle positions
		float x1 = v2->Position.x - v1->Position.x;
		float y1 = v2->Position.y - v1->Position.y;
		float z1 = v2->Position.z - v1->Position.z;

		float x2 = v3->Position.x - v1->Position.x;
		float y2 = v3->Position.y - v1->Position.y;
		float z2 = v3->Position.z - v1->Position.z;

		// Do the same for vectors relative to triangle uv's
		float s1 = v2->UV.x - v1->UV.x;
		float t1 = v2->UV.y - v1->UV.y;

		float s2 = v3->UV.x - v1->UV.x;
		float t2 = v3->UV.y - v1->UV.y;

		// Create vectors for tangent calculation
		float r = 1.0f / (s1 * t2 - s2 * t1);

		float tx = (t2 * x1 - t1 * x2) * r;
		float ty = (t2 * y1 - t1 * y2) * r;
		float tz = (t2 * z1 - t1 * z2) * r;

		// Adjust tangents of each vert of the triangle
		v1->Tangent.x += tx;
		v1->Tangent.y += ty;
		v1->Tangent.z += tz;

		v2->Tangent.x += tx;
		v2->Tangent.y += ty;
		v2->Tangent.z += tz;

		v3->Tangent.x += tx;
		v3->Tangent.y += ty;
		v3->Tangent.z += tz;
	}

	// Ensure all of the tangents are orthogonal to the normals
	for (int i = 0; i < numVerts; i++)
	{
		// Grab the two vectors
		XMVECTOR normal = XMLoadFloat3(&verts[i].Normal);
		XMVECTOR tangent = XMLoadFloat3(&verts[i].Tangent);

		// Use Gram-Schmidt orthogonalize
		tangent = XMVector3Normalize(
			tangent - normal * XMVector3Dot(normal, tangent));

		// Store the tangent
		XMStoreFloat3(&verts[i].Tangent, tangent);
	}
}