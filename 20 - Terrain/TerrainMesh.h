#pragma once

#include "Mesh.h"
#include <string>

enum class TerrainBitDepth
{
	BitDepth_8,
	BitDepth_16
};

// Note: Mesh was changed to make all private data protected instead!
class TerrainMesh :
	public Mesh
{
public:
	TerrainMesh(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		const std::wstring heightmap,
		unsigned int heightmapWidth,
		unsigned int heightmapHeight,
		TerrainBitDepth bitDepth = TerrainBitDepth::BitDepth_8,
		float yScale = 256.0f,
		float xzScale = 1.0f);
	~TerrainMesh();

private:

	void Load8bitRaw(const std::wstring heightmap, unsigned int width, unsigned int height, float yScale, float xzScale, Vertex* verts);
	void Load16bitRaw(const std::wstring heightmap, unsigned int width, unsigned int height, float yScale, float xzScale, Vertex* verts);

};

