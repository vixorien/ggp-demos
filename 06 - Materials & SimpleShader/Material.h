#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "SimpleShader.h"
#include "Camera.h"
#include "Transform.h"

class Material
{
public:
	Material(SimplePixelShader* ps, SimpleVertexShader* vs, DirectX::XMFLOAT3 tint);

	SimplePixelShader* GetPixelShader();
	SimpleVertexShader* GetVertexShader();
	DirectX::XMFLOAT3 GetColorTint();

	void SetPixelShader(SimplePixelShader* ps);
	void SetVertexShader(SimpleVertexShader* ps);
	void SetColorTint(DirectX::XMFLOAT3 tint);

	void PrepareMaterial(Transform* transform, Camera* camera);

private:

	SimplePixelShader* ps;
	SimpleVertexShader* vs;
	DirectX::XMFLOAT3 colorTint;
};

