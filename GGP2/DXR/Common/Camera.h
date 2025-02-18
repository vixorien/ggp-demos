#pragma once
#include <DirectXMath.h>

#include "Transform.h"
#include <memory>

enum class CameraProjectionType
{
	Perspective,
	Orthographic
};

class Camera
{
public:
	Camera(
		DirectX::XMFLOAT3 position,
		float fieldOfView, 
		float aspectRatio, 
		float nearClip = 0.01f, 
		float farClip = 100.0f, 
		CameraProjectionType projType = CameraProjectionType::Perspective);

	~Camera();

	// Updating methods
	void Update(float dt);
	void UpdateViewMatrix();
	void UpdateProjectionMatrix(float aspectRatio);

	// Getters
	DirectX::XMFLOAT4X4 GetView();
	DirectX::XMFLOAT4X4 GetProjection();
	std::shared_ptr<Transform> GetTransform();
	float GetAspectRatio();

	float GetFieldOfView();
	void SetFieldOfView(float fov);
	
	float GetNearClip();
	void SetNearClip(float distance);

	float GetFarClip();
	void SetFarClip(float distance);

	float GetOrthographicWidth();
	void SetOrthographicWidth(float width);

	CameraProjectionType GetProjectionType();
	void SetProjectionType(CameraProjectionType type);

protected:
	// Camera matrices
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projMatrix;

	std::shared_ptr<Transform> transform;

	float fieldOfView;
	float aspectRatio;
	float nearClip;
	float farClip;
	float orthographicWidth;

	CameraProjectionType projectionType;
};


class FPSCamera : public Camera
{
public:

	FPSCamera(
		DirectX::XMFLOAT3 position,
		float moveSpeed,
		float mouseLookSpeed,
		float fieldOfView,
		float aspectRatio,
		float nearClip = 0.01f,
		float farClip = 100.0f,
		CameraProjectionType projType = CameraProjectionType::Perspective);

	float GetMovementSpeed();
	void SetMovementSpeed(float speed);

	float GetMouseLookSpeed();
	void SetMouseLookSpeed(float speed);

	void Update(float dt);

private:
	float movementSpeed;
	float mouseLookSpeed;
};