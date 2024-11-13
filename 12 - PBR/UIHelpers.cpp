
#include <DirectXMath.h>

#include "UIHelpers.h"
#include "Window.h"
#include "Input.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

using namespace DirectX;

// --------------------------------------------------------
// Prepares a new frame for the UI, feeding it fresh
// input and time information for this new frame.
// --------------------------------------------------------
void UINewFrame(float deltaTime)
{
	// Feed fresh input data to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)Window::Width();
	io.DisplaySize.y = (float)Window::Height();

	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	Input::SetKeyboardCapture(io.WantCaptureKeyboard);
	Input::SetMouseCapture(io.WantCaptureMouse);
}



// --------------------------------------------------------
// Builds the UI for this frame
// --------------------------------------------------------
void BuildUI(
	std::shared_ptr<Camera> camera,
	std::vector<std::shared_ptr<Mesh>>& meshes,
	std::vector<std::shared_ptr<GameEntity>>& entities,
	std::vector<std::shared_ptr<Material>>& materials,
	std::vector<Light>& lights,
	DemoLightingOptions& lightOptions)
{
	// A static variable to track whether or not the demo window should be shown.  
	//  - Static in this context means that the variable is created once 
	//    at program start up (like a global variable), but is only
	//    accessible within this scope.
	static bool showDemoWindow = false;

	// Should we show the built-in demo window?
	if (showDemoWindow)
	{
		ImGui::ShowDemoWindow();
	}

	// Actually build our custom UI, starting with a window
	ImGui::Begin("Inspector");
	{
		// Set a specific amount of space for widget labels
		ImGui::PushItemWidth(-160); // Negative value sets label width

		// === Overall details ===
		if (ImGui::TreeNode("App Details"))
		{
			ImGui::Spacing();
			ImGui::Text("Frame rate: %f fps", ImGui::GetIO().Framerate);
			ImGui::Text("Window Client Size: %dx%d", Window::Width(), Window::Height());

			// Should we show the demo window?
			if (ImGui::Button(showDemoWindow ? "Hide ImGui Demo Window" : "Show ImGui Demo Window"))
				showDemoWindow = !showDemoWindow;

			ImGui::Spacing();

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Controls ===
		if (ImGui::TreeNode("Controls"))
		{
			ImGui::Spacing();
			ImGui::Text("(WASD, X, Space)");    ImGui::SameLine(175); ImGui::Text("Move camera");
			ImGui::Text("(Left Click & Drag)"); ImGui::SameLine(175); ImGui::Text("Rotate camera");
			ImGui::Text("(Left Shift)");        ImGui::SameLine(175); ImGui::Text("Hold to speed up camera");
			ImGui::Text("(Left Ctrl)");         ImGui::SameLine(175); ImGui::Text("Hold to slow down camera");

			ImGui::Spacing();
			ImGui::Text("(Arrow Up/Down)");		ImGui::SameLine(175); ImGui::Text("Adjust light count");
			ImGui::Text("(Tab)");				ImGui::SameLine(175); ImGui::Text("Randomize lights");
			ImGui::Text("(F)");					ImGui::SameLine(175); ImGui::Text("Freeze/unfreeze lights");
			ImGui::Text("(L)");					ImGui::SameLine(175); ImGui::Text("Show/hide point lights");

			ImGui::Spacing();
			ImGui::Text("(G)");				ImGui::SameLine(175); ImGui::Text("Gamma correction");
			ImGui::Text("(P)");				ImGui::SameLine(175); ImGui::Text("PBR");
			ImGui::Text("(T)");				ImGui::SameLine(175); ImGui::Text("Albedo texture");
			ImGui::Text("(N)");				ImGui::SameLine(175); ImGui::Text("Normal map");
			ImGui::Text("(R)");				ImGui::SameLine(175); ImGui::Text("Roughness map");
			ImGui::Text("(M)");				ImGui::SameLine(175); ImGui::Text("Metalness map");
			ImGui::Text("(O)");				ImGui::SameLine(175); ImGui::Text("All material options on/off");

			ImGui::Spacing();
			ImGui::Text("(1, 2, 3)");			ImGui::SameLine(175); ImGui::Text("Change scene");

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Camera details ===
		if (ImGui::TreeNode("Camera"))
		{
			// Show UI for current camera
			UICamera(camera);

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Meshes ===
		if (ImGui::TreeNode("Meshes"))
		{
			// Loop and show the details for each mesh
			for (int i = 0; i < meshes.size(); i++)
			{
				// Note the use of PushID() here (and PopID() below), 
				// so that each tree node and its widgets have
				// unique internal IDs in the ImGui system
				ImGui::PushID(meshes[i].get());

				if (ImGui::TreeNode("Mesh Node", "Mesh: %s", meshes[i]->GetName()))
				{
					UIMesh(meshes[i]);
					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Entities ===
		if (ImGui::TreeNode("Scene Entities"))
		{
			for (int i = 0; i < entities.size(); i++)
			{
				ImGui::PushID(entities[i].get());
				if (ImGui::TreeNode("Entity Node", "Entity %d", i))
				{
					UIEntity(entities[i]);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Global Material Controls ===
		if (ImGui::TreeNode("Global Material Controls"))
		{
			if (ImGui::Button("Toggle All"))
			{
				// Are they all already on?
				bool allOn =
					lightOptions.GammaCorrection &&
					lightOptions.UseAlbedoTexture &&
					lightOptions.UseMetalMap &&
					lightOptions.UseNormalMap &&
					lightOptions.UseRoughnessMap &&
					lightOptions.UsePBR;

				if (allOn)
				{
					lightOptions.GammaCorrection = false;
					lightOptions.UseAlbedoTexture = false;
					lightOptions.UseMetalMap = false;
					lightOptions.UseNormalMap = false;
					lightOptions.UseRoughnessMap = false;
					lightOptions.UsePBR = false;
				}
				else
				{
					lightOptions.GammaCorrection = true;
					lightOptions.UseAlbedoTexture = true;
					lightOptions.UseMetalMap = true;
					lightOptions.UseNormalMap = true;
					lightOptions.UseRoughnessMap = true;
					lightOptions.UsePBR = true;
				}
			}
			ImGui::Checkbox("Gamma Correction", &lightOptions.GammaCorrection);
			ImGui::Checkbox("Use PBR Materials", &lightOptions.UsePBR);
			ImGui::Checkbox("Albedo Texture", &lightOptions.UseAlbedoTexture);
			ImGui::Checkbox("Normal Map", &lightOptions.UseNormalMap);
			ImGui::Checkbox("Roughness Map", &lightOptions.UseRoughnessMap);
			ImGui::Checkbox("Metalness Map", &lightOptions.UseMetalMap);
			ImGui::Separator();
			ImGui::Checkbox("Use Burley Diffuse", &lightOptions.UseBurleyDiffuse);

			ImGui::TreePop();
			ImGui::Spacing();
		}

		// === Materials ===
		if (ImGui::TreeNode("Materials"))
		{
			// Loop and show the details for each entity
			for (int i = 0; i < materials.size(); i++)
			{
				ImGui::PushID(materials[i].get());
				if (ImGui::TreeNode("Material Node", "Material: %s", materials[i]->GetName()))
				{
					UIMaterial(materials[i]);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Lights ===
		if (ImGui::TreeNode("Lights"))
		{
			// Light details
			ImGui::Spacing();	
			ImGui::ColorEdit3("Ambient Color", &lightOptions.AmbientColor.x);
			ImGui::Checkbox("Show Point Lights", &lightOptions.DrawLights);
			ImGui::Checkbox("Freeze Lights", &lightOptions.FreezeLightMovement);
			ImGui::SliderInt("Light Count", &lightOptions.LightCount, 1, MAX_LIGHTS);

			// Loop and show the details for each entity
			for (int i = 0; i < lights.size(); i++)
			{
				// Name of this light based on type
				std::string lightName = "Light %d";
				switch (lights[i].Type)
				{
				case LIGHT_TYPE_DIRECTIONAL: lightName += " (Directional)"; break;
				case LIGHT_TYPE_POINT: lightName += " (Point)"; break;
				case LIGHT_TYPE_SPOT: lightName += " (Spot)"; break;
				}

				// New node for each light
				ImGui::PushID(i);
				if (ImGui::TreeNode("Light Node", lightName.c_str(), i))
				{
					// Build UI for one entity at a time
					UILight(lights[i]);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Sky box ===
		if (ImGui::TreeNode("Sky Box"))
		{
			ImGui::Checkbox("Show Skybox", &lightOptions.ShowSkybox);
			ImGui::TreePop();
		}
	}

	ImGui::End();
}



// --------------------------------------------------------
// UI for a single mesh
// --------------------------------------------------------
void UIMesh(std::shared_ptr<Mesh> mesh)
{
	ImGui::Spacing();
	ImGui::Text("Triangles: %d", mesh->GetIndexCount() / 3);
	ImGui::Text("Vertices:  %d", mesh->GetVertexCount());
	ImGui::Text("Indices:   %d", mesh->GetIndexCount());
	ImGui::Spacing();
}

// --------------------------------------------------------
// Builds the UI for a single entity
// --------------------------------------------------------
void UIEntity(std::shared_ptr<GameEntity> entity)
{
	// Details
	ImGui::Spacing();
	ImGui::Text("Mesh: %s", entity->GetMesh()->GetName());
	ImGui::Text("Material: %s", entity->GetMaterial()->GetName());
	ImGui::Spacing();

	// Transform details
	std::shared_ptr<Transform> trans = entity->GetTransform();
	XMFLOAT3 pos = trans->GetPosition();
	XMFLOAT3 rot = trans->GetPitchYawRoll();
	XMFLOAT3 sca = trans->GetScale();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f)) trans->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f)) trans->SetRotation(rot);
	if (ImGui::DragFloat3("Scale", &sca.x, 0.01f)) trans->SetScale(sca);

	ImGui::Spacing();
}

// --------------------------------------------------------
// Builds the UI for a single camera
// --------------------------------------------------------
void UICamera(std::shared_ptr<Camera> cam)
{
	ImGui::Spacing();

	// Transform details
	XMFLOAT3 pos = cam->GetTransform()->GetPosition();
	XMFLOAT3 rot = cam->GetTransform()->GetPitchYawRoll();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
		cam->GetTransform()->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f))
		cam->GetTransform()->SetRotation(rot);
	ImGui::Spacing();

	// Clip planes
	float nearClip = cam->GetNearClip();
	float farClip = cam->GetFarClip();
	if (ImGui::DragFloat("Near Clip Distance", &nearClip, 0.01f, 0.001f, 1.0f))
		cam->SetNearClip(nearClip);
	if (ImGui::DragFloat("Far Clip Distance", &farClip, 1.0f, 10.0f, 1000.0f))
		cam->SetFarClip(farClip);

	// Projection type
	CameraProjectionType projType = cam->GetProjectionType();
	int typeIndex = (int)projType;
	if (ImGui::Combo("Projection Type", &typeIndex, "Perspective\0Orthographic"))
	{
		projType = (CameraProjectionType)typeIndex;
		cam->SetProjectionType(projType);
	}

	// Projection details
	if (projType == CameraProjectionType::Perspective)
	{
		// Convert field of view to degrees for UI
		float fov = cam->GetFieldOfView() * 180.0f / XM_PI;
		if (ImGui::SliderFloat("Field of View (Degrees)", &fov, 0.01f, 180.0f))
			cam->SetFieldOfView(fov * XM_PI / 180.0f); // Back to radians
	}
	else if (projType == CameraProjectionType::Orthographic)
	{
		float wid = cam->GetOrthographicWidth();
		if (ImGui::SliderFloat("Orthographic Width", &wid, 1.0f, 10.0f))
			cam->SetOrthographicWidth(wid);
	}

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single material
// --------------------------------------------------------
void UIMaterial(std::shared_ptr<Material> material)
{
	ImGui::Spacing();

	// Color tint editing
	XMFLOAT3 tint = material->GetColorTint();
	if (ImGui::ColorEdit3("Color Tint", &tint.x))
		material->SetColorTint(tint);

	// Textures
	for (auto& it : material->GetTextureSRVMap())
	{
		// If the texture is not a standard 2D texture, we can't actually display it here
		D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
		it.second->GetDesc(&desc);
		if (desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D)
			continue;  // Skip things like cube maps

		ImGui::Text(it.first.c_str());
		ImGui::Image(it.second.Get(), ImVec2(256, 256));
	}

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single light
// --------------------------------------------------------
void UILight(Light& light)
{
	// Light type
	if (ImGui::RadioButton("Directional", light.Type == LIGHT_TYPE_DIRECTIONAL))
	{
		light.Type = LIGHT_TYPE_DIRECTIONAL;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Point", light.Type == LIGHT_TYPE_POINT))
	{
		light.Type = LIGHT_TYPE_POINT;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Spot", light.Type == LIGHT_TYPE_SPOT))
	{
		light.Type = LIGHT_TYPE_SPOT;
	}

	// Direction
	if (light.Type == LIGHT_TYPE_DIRECTIONAL || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Direction", &light.Direction.x, 0.1f);

		// Normalize the direction
		XMVECTOR dirNorm = XMVector3Normalize(XMLoadFloat3(&light.Direction));
		XMStoreFloat3(&light.Direction, dirNorm);
	}

	// Position & Range
	if (light.Type == LIGHT_TYPE_POINT || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Position", &light.Position.x, 0.1f);
		ImGui::SliderFloat("Range", &light.Range, 0.1f, 100.0f);
	}

	// Spot falloff
	if (light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::SliderFloat("Spot Falloff", &light.SpotFalloff, 0.1f, 128.0f);
	}

	// Color details
	ImGui::ColorEdit3("Color", &light.Color.x);
	ImGui::SliderFloat("Intensity", &light.Intensity, 0.0f, 10.0f);
}