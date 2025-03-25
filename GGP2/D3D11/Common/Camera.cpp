#include "Camera.h"
#include "Input.h"

using namespace DirectX;


Camera::Camera(
	DirectX::XMFLOAT3 position,
	float fieldOfView,
	float aspectRatio,
	float nearClip,
	float farClip,
	CameraProjectionType projType) :
	fieldOfView(fieldOfView), 
	aspectRatio(aspectRatio),
	nearClip(nearClip),
	farClip(farClip),
	projectionType(projType),
	orthographicWidth(10.0f)
{
	transform = std::make_shared<Transform>();
	transform->SetPosition(position);

	UpdateViewMatrix();
	UpdateProjectionMatrix(aspectRatio);
}

// Nothing to really do
Camera::~Camera()
{ }


// Camera's update, which simply updates the view matrix
void Camera::Update(float dt)
{
	// Update the view every frame - could be optimized
	UpdateViewMatrix();
}

// Creates a new view matrix based on current position and orientation
void Camera::UpdateViewMatrix()
{
	// Get the camera's forward vector and position
	XMFLOAT3 forward = transform->GetForward();
	XMFLOAT3 pos = transform->GetPosition();

	// Make the view matrix and save
	XMMATRIX view = XMMatrixLookToLH(
		XMLoadFloat3(&pos),
		XMLoadFloat3(&forward),
		XMVectorSet(0, 1, 0, 0)); // World up axis
	XMStoreFloat4x4(&viewMatrix, view);
}

// Updates the projection matrix
void Camera::UpdateProjectionMatrix(float aspectRatio)
{
	this->aspectRatio = aspectRatio;

	XMMATRIX P;

	// Which type?
	if (projectionType == CameraProjectionType::Perspective)
	{
		P = XMMatrixPerspectiveFovLH(
			fieldOfView,		// Field of View Angle
			aspectRatio,		// Aspect ratio
			nearClip,			// Near clip plane distance
			farClip);			// Far clip plane distance
	}
	else // CameraProjectionType::ORTHOGRAPHIC
	{
		P = XMMatrixOrthographicLH(
			orthographicWidth,	// Projection width (in world units)
			orthographicWidth / aspectRatio,// Projection height (in world units)
			nearClip,			// Near clip plane distance 
			farClip);			// Far clip plane distance
	}

	XMStoreFloat4x4(&projMatrix, P);
}

DirectX::XMFLOAT4X4 Camera::GetView() { return viewMatrix; }
DirectX::XMFLOAT4X4 Camera::GetProjection() { return projMatrix; }
std::shared_ptr<Transform> Camera::GetTransform() { return transform; }

float Camera::GetAspectRatio() { return aspectRatio; }

float Camera::GetFieldOfView() { return fieldOfView; }
void Camera::SetFieldOfView(float fov) 
{ 
	fieldOfView = fov; 
	UpdateProjectionMatrix(aspectRatio);
}

float Camera::GetNearClip() { return nearClip; }
void Camera::SetNearClip(float distance) 
{ 
	nearClip = distance;
	UpdateProjectionMatrix(aspectRatio);
}

float Camera::GetFarClip() { return farClip; }
void Camera::SetFarClip(float distance) 
{ 
	farClip = distance;
	UpdateProjectionMatrix(aspectRatio);
}

float Camera::GetOrthographicWidth() { return orthographicWidth; }
void Camera::SetOrthographicWidth(float width)
{
	orthographicWidth = width;
	UpdateProjectionMatrix(aspectRatio);
}

CameraProjectionType Camera::GetProjectionType() { return projectionType; }
void Camera::SetProjectionType(CameraProjectionType type) 
{
	projectionType = type;
	UpdateProjectionMatrix(aspectRatio);
} 



// ---------------------------------------------
//  FPS CAMERA
// ---------------------------------------------

FPSCamera::FPSCamera(
	DirectX::XMFLOAT3 position,
	float moveSpeed,
	float mouseLookSpeed,
	float fieldOfView,
	float aspectRatio,
	float nearClip,
	float farClip,
	CameraProjectionType projType) :
	Camera(position, fieldOfView, aspectRatio, nearClip, farClip, projType),
	movementSpeed(moveSpeed),
	mouseLookSpeed(mouseLookSpeed)
{

}

float FPSCamera::GetMovementSpeed() { return movementSpeed; }
void FPSCamera::SetMovementSpeed(float speed) { movementSpeed = speed; }

float FPSCamera::GetMouseLookSpeed() { return mouseLookSpeed; }
void FPSCamera::SetMouseLookSpeed(float speed) { mouseLookSpeed = speed; }

void FPSCamera::Update(float dt)
{
	// Current speed
	float speed = dt * movementSpeed;

	// Speed up or down as necessary
	if (Input::KeyDown(VK_SHIFT)) { speed *= 5; }
	if (Input::KeyDown(VK_CONTROL)) { speed *= 0.1f; }

	// Movement
	if (Input::KeyDown('W')) { transform->MoveRelative(0, 0, speed); }
	if (Input::KeyDown('S')) { transform->MoveRelative(0, 0, -speed); }
	if (Input::KeyDown('A')) { transform->MoveRelative(-speed, 0, 0); }
	if (Input::KeyDown('D')) { transform->MoveRelative(speed, 0, 0); }
	if (Input::KeyDown('X')) { transform->MoveAbsolute(0, -speed, 0); }
	if (Input::KeyDown(' ')) { transform->MoveAbsolute(0, speed, 0); }

	// Handle mouse movement only when button is down
	if (Input::MouseLeftDown())
	{
		// Calculate cursor change
		float xDiff = mouseLookSpeed * Input::GetMouseXDelta();
		float yDiff = mouseLookSpeed * Input::GetMouseYDelta();
		transform->Rotate(yDiff, xDiff, 0);

		// Clamp the X rotation
		XMFLOAT3 rot = transform->GetPitchYawRoll();
		if (rot.x > XM_PIDIV2) rot.x = XM_PIDIV2;
		if (rot.x < -XM_PIDIV2) rot.x = -XM_PIDIV2;
		transform->SetRotation(rot);
	}

	// Use base class's update (handles view matrix)
	Camera::Update(dt);
}
