#include "UIHelpers.h"
#include "Window.h"
#include "Input.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"


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
void BuildUI(std::vector<std::shared_ptr<Mesh>>& meshes)
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
