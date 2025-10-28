#include <iostream>
#include <comdef.h>
#include <Wbemidl.h>
#include <windows.h>
#include <vector>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

class WMITemperatureReader {
private:
    IWbemLocator* pLoc;
    IWbemServices* pSvc;

public:
    WMITemperatureReader() : pLoc(nullptr), pSvc(nullptr) {}
    
    ~WMITemperatureReader() {
        cleanup();
    }

    bool initialize() {
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres)) return false;

        hres = CoInitializeSecurity(
            NULL, -1, NULL, NULL,
            RPC_C_AUTHN_LEVEL_NONE,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL, EOAC_NONE, NULL
        );

        hres = CoCreateInstance(
            CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pLoc
        );
        if (FAILED(hres)) return false;

        hres = pLoc->ConnectServer(
            _bstr_t(L"ROOT\\WMI"),
            NULL, NULL, 0, NULL, 0, 0, &pSvc
        );
        if (FAILED(hres)) return false;

        hres = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
            NULL, RPC_C_AUTHN_LEVEL_CALL,
            RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE
        );

        return true;
    }

    void readTemperatures() {
        IEnumWbemClassObject* pEnumerator = NULL;
        HRESULT hres = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL, &pEnumerator
        );

        if (FAILED(hres)) return;

        IWbemClassObject* pclsObj = NULL;
        ULONG uReturn = 0;

        while (pEnumerator) {
            hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) break;

            VARIANT vtProp;
            hres = pclsObj->Get(L"CurrentTemperature", 0, &vtProp, 0, 0);
            
            if (SUCCEEDED(hres) && vtProp.vt == VT_I4) {
                // ACPI温度通常以十分之一开尔文为单位
                double tempKelvin = vtProp.intVal / 10.0;
                double tempCelsius = tempKelvin - 273.15;
                
                std::wcout << L"温度: " << tempCelsius << L" °C" << std::endl;
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }

        if (pEnumerator) pEnumerator->Release();
    }

private:
    void cleanup() {
        if (pSvc) pSvc->Release();
        if (pLoc) pLoc->Release();
        CoUninitialize();
    }
};

class ACPITemperatureReader {
public:
    struct ACPI_THERMAL_ZONE {
        std::string name;
        double temperature;
    };

    std::vector<ACPI_THERMAL_ZONE> readThermalZones() {
        std::vector<ACPI_THERMAL_ZONE> zones;
        
        HKEY hKey;
        LONG result = RegOpenKeyExA(
            HKEY_LOCAL_MACHINE,
            "HARDWARE\\ACPI\\ThermalZone",
            0,
            KEY_READ,
            &hKey
        );

        if (result != ERROR_SUCCESS) {
            return zones;
        }

        DWORD subkeyCount;
        RegQueryInfoKey(hKey, NULL, NULL, NULL, &subkeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

        for (DWORD i = 0; i < subkeyCount; i++) {
            char subkeyName[256];
            DWORD nameSize = sizeof(subkeyName);
            
            if (RegEnumKeyExA(hKey, i, subkeyName, &nameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                ACPI_THERMAL_ZONE zone = readThermalZoneInfo(hKey, subkeyName);
                if (zone.temperature > 0) {
                    zones.push_back(zone);
                }
            }
        }

        RegCloseKey(hKey);
        return zones;
    }

private:
    ACPI_THERMAL_ZONE readThermalZoneInfo(HKEY parentKey, const char* zoneName) {
        ACPI_THERMAL_ZONE zone = {zoneName, 0.0};
        
        HKEY zoneKey;
        std::string fullPath = std::string(zoneName) + "\\Thermal";
        
        if (RegOpenKeyExA(parentKey, fullPath.c_str(), 0, KEY_READ, &zoneKey) == ERROR_SUCCESS) {
            DWORD tempData;
            DWORD dataSize = sizeof(tempData);
            DWORD type;
            
            // 尝试读取温度数据
            if (RegQueryValueExA(zoneKey, "Temperature", NULL, &type, 
                               (LPBYTE)&tempData, &dataSize) == ERROR_SUCCESS) {
                // 转换为摄氏度
                zone.temperature = (tempData - 2732) / 10.0;
            }
            
            RegCloseKey(zoneKey);
        }
        
        return zone;
    }
};

class ACPIDriverInterface {
private:
    HANDLE hDriver;

public:
    ACPIDriverInterface() : hDriver(INVALID_HANDLE_VALUE) {}
    
    bool initialize() {
        hDriver = CreateFileA(
            "\\\\.\\AcpiDev",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        return hDriver != INVALID_HANDLE_VALUE;
    }
    
    double readThermalZoneTemperature(int zoneIndex) {
        if (hDriver == INVALID_HANDLE_VALUE) return -1.0;
        
        DWORD bytesReturned;
        double temperature = -1.0;
        
        // 发送IOCTL获取温度信息
        DeviceIoControl(
            hDriver,
            CTL_CODE(FILE_DEVICE_ACPI, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS),
            &zoneIndex, sizeof(zoneIndex),
            &temperature, sizeof(temperature),
            &bytesReturned, NULL
        );
        
        return temperature;
    }
    
    ~ACPIDriverInterface() {
        if (hDriver != INVALID_HANDLE_VALUE) {
            CloseHandle(hDriver);
        }
    }
};

int main() {
    std::cout << "=== ACPI温度传感器读取器 ===" << std::endl;
    
    // 方法1: 使用WMI
    std::cout << "\n方法1 - WMI读取:" << std::endl;
    WMITemperatureReader wmiReader;
    if (wmiReader.initialize()) {
        wmiReader.readTemperatures();
    }
    
    // 方法2: 读取ACPI表
    std::cout << "\n方法2 - ACPI表读取:" << std::endl;
    ACPITemperatureReader acpiReader;
    auto zones = acpiReader.readThermalZones();
    
    for (const auto& zone : zones) {
        std::cout << "热区 " << zone.name << ": " << zone.temperature << " °C" << std::endl;
    }
    
    // 方法3: 驱动接口
    std::cout << "\n方法3 - 驱动接口:" << std::endl;
    ACPIDriverInterface driver;
    if (driver.initialize()) {
        for (int i = 0; i < 10; i++) {
            double temp = driver.readThermalZoneTemperature(i);
            if (temp > 0) {
                std::cout << "热区 " << i << ": " << temp << " °C" << std::endl;
            }
        }
    }
    
    return 0;
}