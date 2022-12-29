#pragma once

#include "../imgui/imgui-1.89.1/imgui.h"
#include "../imgui/imgui-1.89.1/imgui_impl_dx9.h"
#include "../imgui/imgui-1.89.1/imgui_impl_win32.h"

#include <d3d9.h>
#pragma comment(lib,"d3d9.lib")

#include "../xorstr/xorstr.hpp"
#include "../utils.h"

namespace gui
{
	constexpr int WIDTH = 800;
	constexpr int HEIGHT = 500;

	inline bool isRunning = true;

	inline HWND window = nullptr;
	inline WNDCLASSEXA windowClass = {};

	inline POINTS position = {};

	inline PDIRECT3D9 d3d = nullptr;
	inline LPDIRECT3DDEVICE9 device = nullptr;
	inline D3DPRESENT_PARAMETERS presentParameters = {};

	void CreateHWindow(
		const char* windowName,
		const char* className) noexcept;
	void DestroyHWindow() noexcept;

	bool CreateDevice() noexcept;
	void ResetDevice() noexcept;
	void DestoryDevice() noexcept;

	void CreateImGui() noexcept;
	void DestroyImGui() noexcept;

	void BeginRender() noexcept;
	void EndRender() noexcept;
	void Render() noexcept;

}
