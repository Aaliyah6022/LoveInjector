#include "ccpch.h"
#include "gui.h"

#include <d3d9.h>
#pragma comment(lib,"d3d9.lib")

#pragma warning ( disable : 4244 )

#include "../imgui/imgui-1.89.1/imgui.h"
#include "../imgui/imgui-1.89.1/imgui_impl_dx9.h"
#include "../imgui/imgui-1.89.1/imgui_impl_win32.h"
#include "../imgui/imgui-1.89.1/imgui_internal.h"
#include <string>
#include <vector>
#include <csignal>
#include <thread>

std::thread discordThread([]
	{
		DiscordState state{};

		discord::Core* core{};
		auto result = discord::Core::Create(926847881816707082, DiscordCreateFlags_Default, &core);
		state.core.reset(core);
		if (!state.core) {
			std::cout << "Failed to instantiate discord core! (err " << static_cast<int>(result)
				<< ")\n";
			std::exit(-1);
		}

		core->UserManager().OnCurrentUserUpdate.Connect([&state]()
			{
				state.core->UserManager().GetCurrentUser(&state.currentUser);

				std::cout << "Current user updated: " << state.currentUser.GetUsername() << "#"
					<< state.currentUser.GetDiscriminator() << "\n";

				state.core->UserManager().GetUser(130050050968518656, [](discord::Result result, discord::User const& user)
					{
						if (result == discord::Result::Ok)
						{
							//std::cout << "Get " << user.GetUsername() << "\n";
						}
						else
						{
							std::cout << "Failed to get User!\n";
						}
					});

				discord::ImageHandle handle{};
				handle.SetId(state.currentUser.GetId());
				handle.SetType(discord::ImageType::User);
				handle.SetSize(256);
			});

		//state.core->ActivityManager().RegisterCommand("run/command/foo/bar/baz/here.exe");

		discord::Activity activity{};
		activity.SetDetails("Target Injected!");
		activity.SetState("Manual Mapping");
		activity.GetAssets().SetSmallImage("small");
		activity.GetAssets().SetSmallText("EasyAntiCheat");
		activity.GetAssets().SetLargeImage("aa");
		activity.GetAssets().SetLargeText("Unknown Game...");
		activity.GetParty().SetPrivacy(discord::ActivityPartyPrivacy::Public);
		activity.SetType(discord::ActivityType::Playing);
		state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result)
			{
				std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed") << " updating activity!\n";
			});


		std::signal(SIGINT, [](int) { interrupted = true; });

		do {
			state.core->RunCallbacks();

			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		} while (!interrupted);

	});


static const char* selectedMethod = NULL;
const char* methods[] = {"LoadLibraryA", "ManualMap", "Method 3", "Method 4", "Method 5", "Random Method"};
static char buf[256];
ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBackground;

const char* themes[]{ "Dark Purple", "Dark Blue", "Dark"};
static int curTheme;
static bool debug_cmd = false;


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);


long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE:
	{
		if (gui::device != NULL && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND:
	{
		if ((wideParameter & 0xfff0) == SC_KEYMENU)
			return 0;
	}break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN:
	{
		gui::position = MAKEPOINTS(longParameter);
	}return 0;

	case WM_MOUSEMOVE:
	{
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{};

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}
	}return 0;
	}
	return DefWindowProcW(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(
	const char* windowName,
	const char* className) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEXA);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WNDPROC(WindowProcess);
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandle(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = className;
	windowClass.hIconSm = 0;

	RegisterClassExA(&windowClass);

	window = CreateWindowA(
		className,
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return false;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestoryDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		device->Release();
		device = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;
	io.Fonts->AddFontFromFileTTF("C:\\windows\\Fonts\\Georgia.ttf", 18.0f);

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	FreeConsole();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}



void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0,0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGuiStyle& style = ImGui::GetStyle();

	if (curTheme == 0)
	{
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.39f, 0.00f, 0.63f, 0.11f);
		style.Colors[ImGuiCol_ChildBg] = ImColor(24, 24, 24);
		style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255);
		style.Colors[ImGuiCol_CheckMark] = ImColor(255, 255, 255);

		style.Colors[ImGuiCol_Header] = ImColor(30, 30, 30);
		style.Colors[ImGuiCol_HeaderActive] = ImColor(28, 28, 28);
		style.Colors[ImGuiCol_HeaderHovered] = ImColor(28, 28, 28);
	}
	else if (curTheme == 1)
	{
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_ChildBg] = ImColor(24, 24, 24);
		style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255);
		style.Colors[ImGuiCol_CheckMark] = ImColor(255, 255, 255);

		style.Colors[ImGuiCol_Header] = ImColor(30, 30, 30);
		style.Colors[ImGuiCol_HeaderActive] = ImColor(28, 28, 28);
		style.Colors[ImGuiCol_HeaderHovered] = ImColor(28, 28, 28);
	}
	else if (curTheme == 2)
	{
		style.Colors[ImGuiCol_WindowBg] = ImColor(16, 16, 16);
		style.Colors[ImGuiCol_ChildBg] = ImColor(24, 24, 24);
		style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255);
		style.Colors[ImGuiCol_CheckMark] = ImColor(255, 255, 255);

		style.Colors[ImGuiCol_Header] = ImColor(30, 30, 30);
		style.Colors[ImGuiCol_HeaderActive] = ImColor(28, 28, 28);
		style.Colors[ImGuiCol_HeaderHovered] = ImColor(28, 28, 28);
	}

	ImGui::Begin(
		"LoveInjector Beta v1.3",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
		//ImGuiWindowFlags_NoTitleBar
	);

	

	ImGui::PushStyleColor(ImGuiCol_Border, ImColor(0, 0, 0, 255).Value);

	ImGui::BeginChild("##LeftSide", ImVec2(ImGui::GetWindowWidth() / 4, ImGui::GetWindowHeight() - 40), false, window_flags);
	
	ImGui::Spacing();
	ImGui::Text(" Injection Method:");
	
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();

	ImGui::Spacing();
	ImGui::Text(" Dll path:");

	ImGui::Text(" Target Process:");

	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Text(" Just messing arround:");

	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("##LeftButtonSide", ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - 40), false, window_flags);

	// Select Method

	ImGui::PushItemWidth(ImGui::GetWindowWidth());
	if (ImGui::BeginCombo("##MethodsCombo", selectedMethod, ImGuiComboFlags_NoArrowButton))
	{
		for (int i = 0; i < IM_ARRAYSIZE(methods); i++)
		{
			bool method_selected = (selectedMethod == methods[i]);
			if (ImGui::Selectable(methods[i], method_selected))
				selectedMethod = methods[i];
			if (method_selected)
			{
				ImGui::SetItemDefaultFocus();
				printf("%d\n", method_selected);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	// Select DLL

	ImGui::PushItemWidth(ImGui::GetWindowWidth());
	static char szFile[260] = "";
	static char szFilter[260] = "DLL Files\0*.DLL\0All\0*.*\0";
	ShowFileOpenDialog(szFile, sizeof(szFile), szFilter);
	ImGui::Text(szFile);
	ImGui::PopItemWidth();

	// Select Process

	std::vector<std::pair<std::string, DWORD>> process_list = GetProcessInfo();

	const char** process_info_array = new const char* [process_list.size()];
	for (size_t i = 0; i < process_list.size(); i++)
	{
		process_info_array[i] = process_list[i].first.c_str();
	}

	static int selected_process = -1;

	if (ImGui::Combo(" ", &selected_process, process_info_array, (int)process_list.size()))
	{
		// A process was selected, do something with it...

	}

	DWORD process_id = process_list[selected_process].second;

	if (process_id > 100000)
	{
		ImGui::Text("Waiting to select a process..");
	}
	else
	{
		std::stringstream ss;
		ss << "Process ID: " << process_id;
		ImGui::Text(ss.str().c_str());
	}

	// inject button

	if (ImGui::Button("Inject"))
	{
		printf("Inject clicked!\n");
		
		if (process_id < 100000)
		{
			std::stringstream ss;
			ss << "Process ID: " << process_id;
			printf("%s\n", ss.str().c_str());

			if (szFile != "")
			{			
				printf("Dll Path: ");
				printf("%s\n", szFile);
				
				if (strcmp(selectedMethod, "LoadLibraryA") == 0)
				{
					// "C:\\Users\\petre\\OneDrive\\Desktop\\WinInternalsDll.dll"
					if (LoadLibraryAMethod(process_id, szFile))
					{
						ImGui::OpenPopup("Injection Successful");
					}
					else
					{
						ImGui::OpenPopup("Injection Failed");
					}
				}
				else if (strcmp(selectedMethod, "ManualMap") == 0)
				{
					if (ManualMapMethod(process_id, szFile))
					{
						ImGui::OpenPopup("Injection Successful");
					}
					else
					{
						ImGui::OpenPopup("Injection Failed");
					}
				}
				else if (strcmp(selectedMethod, "Method 3") == 0)
				{
					// Call Method3
				}
				else if (strcmp(selectedMethod, "Method 4") == 0)
				{
					// Call Method4
				}
				else if (strcmp(selectedMethod, "Method 5") == 0)
				{
					// Call Method5
				}
				else if (strcmp(selectedMethod, "Random Method") == 0)
				{
					// Call RandomMethod
				}
			}
			else
			{
				printf("Dll Path: ");
				printf("%s \n", szFile);
				ImGui::OpenPopup("Injection Failed - Invalid Path");
			}
		}
		else
		{
			if (process_id > 100000)
				printf("Invalid PID\n");
			
			ImGui::OpenPopup("Injection Failed - Invalid PID");
		}

		
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Press for inject!");
	

	if (ImGui::BeginPopupModal("Injection Successful", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The DLL was successfully injected into the target process.");
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Injection Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The injection failed. Make sure the process ID and DLL path are correct.");
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Injection Failed - Invalid PID", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The injection failed. Make sure the process ID is selected!");
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopupModal("Injection Failed - Invalid Path", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The injection failed. Make sure the DLL path is correct!");
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Debug
	if (ImGui::Checkbox(" Debug console", &debug_cmd))
	{
		AllocConsole();
		freopen_s((FILE**)stdout, "conout$", "w", stdout);
		HWND hwnd = GetConsoleWindow();
		_SMALL_RECT Rect;
		Rect.Top = 0;
		Rect.Left = 0;
		Rect.Bottom = 10;
		Rect.Right = 33;
		SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &Rect);
	}
	
	static bool richPresenceEnabled = false;


	ImGui::Text("RichPresence is unstable!");
	if (ImGui::Checkbox("Enable Discord Rich Presence", &richPresenceEnabled)) 
	{
		discordThread.join();
	}



	ImGui::PushItemWidth(ImGui::GetWindowWidth());
	ImGui::Combo("Menu Theme", &curTheme, themes, ARRAYSIZE(themes));
	ImGui::PopItemWidth();


	ImGui::EndChild();
	ImGui::PopStyleColor();

	// End render
	ImGui::End();

}
