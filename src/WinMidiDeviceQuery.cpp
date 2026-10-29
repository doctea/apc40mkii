#include "WinMidiDeviceQuery.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <iostream>

#pragma comment(lib, "setupapi.lib")

std::vector<MidiDeviceInfo> enumerateMidiDevices() {
    std::vector<MidiDeviceInfo> result;

    HDEVINFO devInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_AUDIO, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (devInfo == INVALID_HANDLE_VALUE)
        return result;

    SP_DEVICE_INTERFACE_DATA ifaceData;
    ifaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD index = 0;
    while (SetupDiEnumDeviceInterfaces(devInfo, NULL, &GUID_DEVINTERFACE_AUDIO, index, &ifaceData)) {
        index++;

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo, &ifaceData, NULL, 0, &requiredSize, NULL);

        std::vector<char> detailData(requiredSize);
        auto* ifaceDetail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(detailData.data());
        ifaceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        SP_DEVINFO_DATA devData;
        devData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(devInfo, &ifaceData, ifaceDetail, requiredSize, NULL, &devData))
            continue;

        char hardwareId[256];
        DWORD type = 0;
        DWORD size = sizeof(hardwareId);

        if (SetupDiGetDeviceRegistryProperty(
                devInfo, &devData, SPDRP_HARDWAREID,
                &type, (BYTE*)hardwareId, size, NULL)) {

            MidiDeviceInfo info;
            info.hardwareId = hardwareId;

            // WinMM port index = enumeration order
            info.winmmIndex = result.size();

            // Friendly name
            char name[256];
            if (SetupDiGetDeviceRegistryProperty(
                    devInfo, &devData, SPDRP_FRIENDLYNAME,
                    &type, (BYTE*)name, sizeof(name), NULL)) {
                info.name = name;
            } else {
                info.name = "Unknown MIDI Device";
            }

            result.push_back(info);
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return result;
}
