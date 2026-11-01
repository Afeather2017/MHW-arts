#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
extern PresentFn g_originalPresent;
extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_context;

bool InitializeRenderer(IDXGISwapChain* swapChain);
void DrawGauge(IDXGISwapChain* swapChain);
HRESULT __stdcall PresentHook(IDXGISwapChain* swapChain, UINT syncInterval,
                              UINT flags);
