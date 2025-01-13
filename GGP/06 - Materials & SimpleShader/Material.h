#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>

#include "SimpleShader.h"
#include "Camera.h"
#include "Transform.h"

class Material
{
public:
	Material(const char* name, std::shared_ptr<SimplePixelShader> ps, std::shared_ptr<SimpleVertexShader> vs, DirectX::XMFLOAT3 tint);

	std::shared_ptr<SimplePixelShader> GetPixelShader();
	std::shared_ptr<SimpleVertexShader> GetVertexShader();
	DirectX::XMFLOAT3 GetColorTint();
	const char* GetName();

	void SetPixelShader(std::shared_ptr<SimplePixelShader> ps);
	void SetVertexShader(std::shared_ptr<SimpleVertexShader> ps);
	void SetColorTint(DirectX::XMFLOAT3 tint);

	void PrepareMaterial(std::shared_ptr<Transform> transform, std::shared_ptr<Camera> camera);

private:

	// Name (mostly for UI purposes)
	const char* name;

	std::shared_ptr<SimplePixelShader> ps;
	std::shared_ptr<SimpleVertexShader> vs;
	DirectX::XMFLOAT3 colorTint;
};

