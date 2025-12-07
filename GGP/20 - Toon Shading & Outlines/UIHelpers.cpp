
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
	ToonOptions& options)
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
			ImGui::Checkbox("Freeze Lights", &options.FreezeLightMovement);
			ImGui::SliderInt("Light Count", &options.LightCount, 1, MAX_LIGHTS);

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

		// === Toon Shading & Outlines ===
		if (ImGui::TreeNode("Toon Shading & Outlines"))
		{
			// Toon
			ImGui::SeparatorText("Toon Shading");
			ImGui::Spacing();
			ImGui::Text("Columns (Left to Right):");
			ImGui::BulletText("Standard shading (Lambert + Phong)");
			ImGui::BulletText("Conditionals in shader");
			ImGui::BulletText("Ramp texture (4 bands) + specular ramp");
			ImGui::BulletText("Ramp texture (3 bands) + specular ramp");
			ImGui::BulletText("Ramp texture (2 bands) + specular ramp");

			ImGui::Spacing();
			ImGui::Checkbox("Show Ramp Textures (above columns)", &options.ShowRampTextures);
			ImGui::Checkbox("Show Specular Ramp (right of top row)", &options.ShowSpecularRamp);

			// Outlines
			ImGui::Spacing(); 
			ImGui::SeparatorText("Outlines");
			ImGui::Spacing();
			ImGui::Combo("Outline Mode", (int*)&options.OutlineMode, "None\0Inside Out Geometry\0Sobel Filter (Post Process)\0Silhouette (Post Process)\0Depth/Normal Comparison (Post Process)");

			ImGui::Indent(10);
			switch (options.OutlineMode)
			{
			case OutlineNone:
				ImGui::Text("No outlines being rendered");
				break;

			case OutlineInsideOut:
				ImGui::TextWrapped("This mode literally draws each object inside out, using a special vertex shader that moves the vertices along their normals.  This works best when the model has no hard edges.");
				ImGui::TextWrapped("As you can see, the sphere and torus work the best here, as they have no hard edges. Outlines on the helmet and crate break down with this technique due to the hard edges.");
				break;

			case OutlineSobelFilter:
				ImGui::TextWrapped("This mode uses a simple post process to compare surrounding pixel colors and, based on the strength of color differences, interpolates towards an outline color.");
				ImGui::TextWrapped("This is easy to implement but clearly gets a bit noisy, as it is completely based on pixel colors.  This works  best on areas of flat color, like the very simple toon shading examples. This technique is the basis of many Photoshop filters.");
				break;

			case OutlineSilhouette:
				ImGui::TextWrapped("This mode outputs a unique ID value to the alpha channel of the main render target.  A post process then changes the current pixel to black when a neighboring pixel has a different ID value.");
				ImGui::TextWrapped("This technique only puts outlines around the silhouette of the object. There are no 'interior' edges being outlined.  This may or may not be the desired effect!");
				break;

			case OutlineDepthNormals:
				ImGui::TextWrapped("This mode uses multiple active render targets to capture not only the colors of the scene, but the normals and depths, too (see below).  A post process then compares neighboring normals & depths.");
				ImGui::TextWrapped("The post process used by this technique works similarly to the Sobel filter, except it compares normals of surrounding pixels as well as the depths of surrounding pixels.");
				ImGui::TextWrapped("A large enough discrepancy in either the normals or the depths of surrounding pixels causes an outline to appear.");

				float width = ImGui::GetWindowWidth() - 30;
				float height = width / Window::AspectRatio();

				ImGui::Spacing();
				ImGui::Text("Scene Depth");
				ImGui::Image(options.SceneDepthsSRV.Get(), ImVec2(width, height));

				ImGui::Spacing();
				ImGui::Text("Scene Normals");
				ImGui::Image(options.SceneNormalsSRV.Get(), ImVec2(width, height));
				break;
				
			}
			ImGui::Indent(-10);

			// Finalize the tree node
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

		ImGui::Text("%d", it.first);
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