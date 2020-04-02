#include <Windows.h>
#include <iostream>
#include "include/injector/injector.hpp"
#include "include/IniReader.h"

CIniReader iniReader("");
int32_t Screen_Width = iniReader.ReadInteger("MAIN", "ResolutionX", 0);
int32_t Screen_Height = iniReader.ReadInteger("MAIN", "ResolutionY", 0);
int32_t nWindowedMode = iniReader.ReadInteger("MISC", "WindowedMode", 0);

int32_t Screen_Width43 = 0;
float Screen_fWidth = 0.0f;
float Screen_fHeight = 0.0f;
float Screen_fAspectRatio = 0.0f;
float Screen_fHudScaleX = 0.0f;
float Screen_fHudPosX = 0.0f;

static float mirrorScale = 0.45f;
static float ver3DScale = 0.75f;

static float fRainScaleX = ((0.75f / Screen_fAspectRatio) * (4.0f / 3.0f));
static float fRainDropletsScale =  0.5f;

void __declspec(naked) SetResolution()
{
	__asm
	{
		mov eax, [esp + 0x04];
		mov ecx, Screen_Width;

		mov[eax], ecx;

		mov ecx, [esp + 0x08];
		mov eax, Screen_Height;

		mov[ecx], eax;

		retn 8;
	}
}


auto __stdcall CreateWindowHook(std::uint32_t ex_style, const char*, const char*, std::uint32_t style,
	std::int32_t, std::int32_t, std::int32_t, std::int32_t, HWND, HMENU, HINSTANCE instance, std::int32_t*) -> HWND
{
	if (nWindowedMode >= 1)
	{
		style |= WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU;
	}

	auto rect = ::RECT();

	GetClientRect(GetDesktopWindow(), &rect);

	rect.left = (rect.right / 2) - (Screen_Width / 2);
	rect.top = (rect.bottom / 2) - (Screen_Height / 2);

	return CreateWindowExA(ex_style, "GameFrame", "Need for Speed™ Carbon", style, rect.left, rect.top, Screen_Width, Screen_Height, nullptr, nullptr, instance, nullptr);
}

float LeftGroupX, LeftGroupY, RightGroupX, RightGroupY;

void(__cdecl *FE_Object_SetCenter)(DWORD* FEObject, float _PositionX, float _PositionY) = (void(__cdecl*)(DWORD*, float, float))0x597A70;
void(__cdecl *FE_Object_GetCenter)(DWORD* FEObject, float *PositionX, float *PositionY) = (void(__cdecl*)(DWORD*, float*, float*))0x597900;
void*(__cdecl *FEObject_FindObject)(const char *pkg_name, unsigned int obj_hash) = (void*(__cdecl*)(const char*, unsigned int))0x5A0250;

int __stdcall cFEng_QueuePackageMessage_Hook(unsigned int MessageHash, char const *FEPackageName, DWORD* FEObject)
{
	float Difference;
	Difference = ((Screen_fWidth / Screen_fHeight) * 240.0f) - 320.0f;

	injector::WriteMemory<float*>(0x005D52FB, &Difference, true);
	injector::WriteMemory<float*>(0x005D5358, &Difference, true);

	injector::WriteMemory<float>(0x00750DF5, ((((320.0f + Difference) / 320.0f) - 1.042f) / -2.0f), true);

	DWORD* LeftGroup = (DWORD*)FEObject_FindObject(FEPackageName, 0x1603009E); // "HUD_SingleRace.fng", leftgrouphash

	if (LeftGroup) // Move left group
	{
		FE_Object_GetCenter(LeftGroup, &LeftGroupX, &LeftGroupY);
		FE_Object_SetCenter(LeftGroup, LeftGroupX - Difference, LeftGroupY);
	}

	DWORD* RightGroup = (DWORD*)FEObject_FindObject(FEPackageName, 0x5D0101F1); // "HUD_SingleRace.fng", rightgrouphash

	if (RightGroup) // Move right group
	{
		FE_Object_GetCenter(RightGroup, &RightGroupX, &RightGroupY);
		FE_Object_SetCenter(RightGroup, RightGroupX + Difference, RightGroupY);
	}

	return 1;
}

__declspec(naked) void rain_drops_x_scale_hook()
{
	__asm
	{
		fld[esp + 0x08];

		fmul[fRainScaleX];
		fmul[fRainDropletsScale];

		fadd[esp + 0x10];

		push 0x00722E80;
		retn;
	}
}

__declspec(naked) void rain_drops_y_scale_hook()
{
	__asm
	{
		fmul[fRainDropletsScale];
		fadd[esp + 0x0C];

		mov[esp + 0x34], eax;

		push 0x00722EA4;
		retn;
	}
}

void field_of_view(std::uint32_t eax)
{
	static auto flt1 = 0.0f;
	static auto flt2 = 0.0f;
	static auto flt3 = 0.0f;

	if (eax == 1 || eax == 4)
	{
		flt1 = (1.0f / (Screen_fAspectRatio / (4.0f / 3.0f))) / 1.0511562719f;
		flt2 = 0.6f;
		flt3 = 1.25f;
	}

	else
	{
		flt1 = 1.0f;
		flt2 = 0.5f;
		flt3 = 1.0f;
	}

	injector::WriteMemory<float*>(0x0071B8DC, &flt1, true);
	injector::WriteMemory<float*>(0x0071B8EE, &flt2, true);
	injector::WriteMemory<float*>(0x0071B925, &flt3, true);
}
__declspec(naked) void field_of_view_hook()
{
	__asm
	{
		mov eax, [ebp + 0x08];

		pushad;

		push eax;
		call field_of_view;

		add esp, 4;
		popad;

		cmp eax, 3;
		je mirror_scale;

		fld ver3DScale;

		cmp eax, 1;

		push 0x0071B86A;
		retn;

	mirror_scale:
		fld mirrorScale;

		cmp eax, 1;

		push 0x0071B86A;
		retn;
	}
}

__declspec(naked) void rear_view_mirror_hook()
{
	__asm
	{
		fstp[esi + 0x4C];
		mov[esi + 0x5C], eax;

		fld[esi + 0x00];
		fmul mirrorScale;
		fstp[esi + 0x00];

		fld[esi + 0x18];
		fmul mirrorScale;
		fstp[esi + 0x18];

		fld[esi + 0x30];
		fmul mirrorScale;
		fstp[esi + 0x30];

		fld[esi + 0x48];
		fmul mirrorScale;
		fstp[esi + 0x48];

		pop esi;
		pop ebx;

		retn;
	}
}

void Init()
{

	if (!Screen_Width || !Screen_Height)
	{
		Screen_Width = GetSystemMetrics(SM_CXSCREEN);
		Screen_Height = GetSystemMetrics(SM_CYSCREEN);
	}

	Screen_fWidth = static_cast<float>(Screen_Width);
	Screen_fHeight = static_cast<float>(Screen_Height);
	Screen_fAspectRatio = Screen_fWidth / Screen_fHeight;

	// Resolution
	injector::MakeJMP(0x00712AC0, SetResolution, true);

	// Autosculpt scaling
	injector::WriteMemory<float>(0x009E9B68, 480.0f * Screen_fAspectRatio, true);

	// World map cursor
	injector::MakeNOP(0x00570DCD, 2, true);
	injector::MakeNOP(0x00570DDC, 2, true);

	// Crash fix
	injector::MakeNOP(0x0059606D, 2, true);
	injector::MakeNOP(0x005960A9, 2, true);

	// Fix rain droplets
	injector::MakeJMP(0x00722E78, rain_drops_x_scale_hook, true);
	injector::MakeJMP(0x00722E9C, rain_drops_y_scale_hook, true);

	// Fix HUD
	if (Screen_fAspectRatio != 4.0f / 3.0f || Screen_fAspectRatio != 5.0f / 4.0f)
	{
		Screen_fHudScaleX = (1.0f / Screen_Width * (Screen_Height / 480.0f)) * 2.0f;
		Screen_fHudPosX = 640.0f / (640.0f * Screen_fHudScaleX);
		injector::WriteMemory<float>(0x009E8F8C, Screen_fHudScaleX, true);
		injector::WriteMemory<float>(0x00A604AC, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x009C778C, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x00598DC0, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x005A18BE, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x005D2B46, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x00598FB7, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x00599416, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x005996AE, Screen_fHudPosX, true);
		injector::WriteMemory<float>(0x009D5F3C, (Screen_fHudPosX - 320.0f) + 384.0f, true);
		injector::WriteMemory<unsigned char>(0x005DC508, 0x94, true);
		injector::WriteMemory<unsigned char>(0x005D52B3, 0x94, true);
		injector::WriteMemory<unsigned char>(0x005B6BAE, 0x75, true);
		injector::WriteMemory<unsigned char>(0x005B6B5B, 0x75, true);

		if (Screen_fAspectRatio != 16.0f / 9.0f)
		{
			injector::MakeCALL(0x005D52D8, cFEng_QueuePackageMessage_Hook, true);
			injector::MakeCALL(0x005D5339, cFEng_QueuePackageMessage_Hook, true);
		}

		// Fix FOV
		injector::MakeJMP(0x0071B858, field_of_view_hook, true);
	}

	// Fix rearview mirror
	injector::MakeJMP(0x00750C1D, rear_view_mirror_hook, true);

	// Fix resetting settings
	injector::MakeNOP(0x0071D117, 10, true);


	// Windowed mode

	if (nWindowedMode)
	{
		injector::WriteMemory(0x00AB0AD4, 1, true);

		injector::WriteMemory(0x009C1470, CreateWindowHook, true);

	}
	
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		Init();
	}
	return TRUE;
}