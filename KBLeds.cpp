
#include "pch.h"
#include <iostream>

#include <Windows.h>
#include <cassert>
#include <chrono>
#include <thread>
#include <SetupAPI.h>
#include <stdio.h>
#include <hidsdi.h>
#include <string>


#define KEYBOARD_CAPS_LOCK_ON     4
#define KEYBOARD_NUM_LOCK_ON      2
#define KEYBOARD_SCROLL_LOCK_ON   1
#define IOCTL_KEYBOARD_SET_INDICATORS        CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0002, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KEYBOARD_QUERY_INDICATORS      CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0010, METHOD_BUFFERED, FILE_ANY_ACCESS)   

static HANDLE kbd = NULL;
using namespace std;

void bad(std::string e)
{
	std::cout << e << std::endl;
}

HANDLE test()
{
	int i;
	GUID hidGuid;
	HDEVINFO deviceInfoList;
	const size_t DEVICE_DETAILS_SIZE = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + MAX_PATH;
	SP_DEVICE_INTERFACE_DETAIL_DATA *deviceDetails = (SP_DEVICE_INTERFACE_DETAIL_DATA*) alloca(DEVICE_DETAILS_SIZE);
	deviceDetails->cbSize = sizeof(*deviceDetails);

	HidD_GetHidGuid(&hidGuid);
	deviceInfoList = SetupDiGetClassDevs(&hidGuid, NULL, NULL,
		DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
	if (deviceInfoList == INVALID_HANDLE_VALUE) {
		bad("SetupDiGetClassDevs");
		return 0;
	}

	for (i = 0; ; ++i) {
		SP_DEVICE_INTERFACE_DATA deviceInfo;
		DWORD size = DEVICE_DETAILS_SIZE;
		HIDD_ATTRIBUTES deviceAttributes;
		HANDLE hDev = INVALID_HANDLE_VALUE;

		fprintf(stderr, "Trying device %d\n", i);
		deviceInfo.cbSize = sizeof(deviceInfo);
		if (!SetupDiEnumDeviceInterfaces(deviceInfoList, 0, &hidGuid, i,
			&deviceInfo)) {
			if (GetLastError() == ERROR_NO_MORE_ITEMS) {
				break;
			}
			else {
				bad("SetupDiEnumDeviceInterfaces");
				continue;
			}
		}

		if (!SetupDiGetDeviceInterfaceDetail(deviceInfoList, &deviceInfo,
			deviceDetails, size, &size, NULL)) {
			bad("SetupDiGetDeviceInterfaceDetail");
			continue;
		}

		std::wcout << "Opening device " << deviceDetails->DevicePath << std::endl;
		//fprintf(stderr, "Opening device %s\n", deviceDetails->DevicePath);
		hDev = CreateFile(deviceDetails->DevicePath, 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, 0, NULL);
		if (hDev == INVALID_HANDLE_VALUE) {
			bad("CreateFile");
			continue;
		}
		return hDev;

		deviceAttributes.Size = sizeof(deviceAttributes);
		if (HidD_GetAttributes(hDev, &deviceAttributes)) {
			fprintf(stderr, "VID = %04x PID = %04x\n", (unsigned)deviceAttributes.VendorID, (unsigned)deviceAttributes.ProductID);
		}
		else {
			bad("HidD_GetAttributes");
		}
		CloseHandle(hDev);
	}

	SetupDiDestroyDeviceInfoList(deviceInfoList);
}

DWORD OpenKeyboardDevice()
{
	if (!DefineDosDevice(DDD_RAW_TARGET_PATH, L"KBD000001", L"\\Device\\KeyboardClass0"))
	{
		assert(false);
	}

	kbd = CreateFile(L"\\\\?\\hid#vid_24f0&pid_0140&mi_00#8&293e8582&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	assert(kbd);
	auto e = GetLastError();
	return e;
}

void CloseKeyboardDevice()
{
	DefineDosDevice(DDD_REMOVE_DEFINITION, L"Kbd000000", NULL);
	CloseHandle(kbd);
}

int toggle_led(bool toggle, int led)
{
	uint32_t input = 0, output = 0;

	DWORD len;
	if (!DeviceIoControl(kbd, IOCTL_KEYBOARD_QUERY_INDICATORS,
		&input, sizeof(input),
		&output, sizeof(output),
		&len, NULL))
		return GetLastError();

	input = output;
	input &= ~(led << 16);
	if (toggle)
		input |= led << 16;

	if (!DeviceIoControl(kbd, IOCTL_KEYBOARD_SET_INDICATORS,
		&input, sizeof(input),
		NULL, 0,
		&len, NULL))
		return GetLastError();
	return 0;
}

int set_leds(int led)
{
	uint32_t input = 0;
	DWORD len;
	input |= led << 16;
	if (!DeviceIoControl(kbd, IOCTL_KEYBOARD_SET_INDICATORS,
		&input, sizeof(input),
		NULL, 0,
		&len, NULL))
		return GetLastError();

	return 0;
}

void set_leds_sequence(unsigned char * cmdSeq, int len)
{
	int i;
	for (i = 0; i < len; ++i)
	{
		set_leds(cmdSeq[i]);
	}
}

int main()
{
	DWORD cc = MAX_PATH * 100;
	DWORD cc2;
	
	/*
//	for (;;)
//	{
		//auto Names = (PWCHAR) new BYTE[cc * sizeof(TCHAR)];

		cc2 = QueryDosDeviceW(L"KBD000000", Names, cc);
//	}
	
	if (!cc2) 
	{		
		std::cout << "error" << GetLastError() << std::endl;
		return 0;
	}

    for (auto NameIdx = Names;
		NameIdx[0] != L'\0';
		NameIdx += wcslen(NameIdx) + 1)
	{
		//if (!lstrcmpW(NameIdx, TEXT("KBD000001"))) {
			wprintf(L"  %s\n", NameIdx);
		//}		
	}		
	*/
	kbd = test();
	//return 0;

	//OpenKeyboardDevice();
	auto f = true;
	auto r = toggle_led(f, KEYBOARD_CAPS_LOCK_ON);
	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(2));
		r = toggle_led(f, KEYBOARD_CAPS_LOCK_ON);
		f = !f;//
		std::cout << r << std::endl;		
	}
	CloseKeyboardDevice();

	if (kbd) {
		CloseHandle(kbd);
	}
}

