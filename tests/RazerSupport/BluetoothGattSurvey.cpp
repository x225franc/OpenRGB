#include <windows.h>
#include <initguid.h>
#include <setupapi.h>
#include <bthledef.h>
#include <bluetoothleapis.h>
#include <iostream>
#include <vector>
#include <string>
static void uuid(const BTH_LE_UUID& u) { if(u.IsShortUuid) std::wcout << std::hex << u.Value.ShortUuid << std::dec; else { WCHAR s[40];StringFromGUID2(u.Value.LongUuid,s,40);std::wcout << s; } }
int wmain() {
 HDEVINFO list=SetupDiGetClassDevsW(&GUID_BLUETOOTHLE_DEVICE_INTERFACE,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(list==INVALID_HANDLE_VALUE)return 1;
 for(DWORD i=0;;i++) {
  SP_DEVICE_INTERFACE_DATA item={sizeof(item)};if(!SetupDiEnumDeviceInterfaces(list,NULL,&GUID_BLUETOOTHLE_DEVICE_INTERFACE,i,&item))break;
  DWORD size=0;SetupDiGetDeviceInterfaceDetailW(list,&item,NULL,0,&size,NULL);
  std::vector<BYTE> data(size);auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(data.data());detail->cbSize=sizeof(*detail);
  SP_DEVINFO_DATA device={sizeof(device)};if(!SetupDiGetDeviceInterfaceDetailW(list,&item,detail,size,NULL,&device))continue;
  WCHAR name[512]={};SetupDiGetDeviceRegistryPropertyW(list,&device,SPDRP_FRIENDLYNAME,NULL,reinterpret_cast<BYTE*>(name),sizeof(name),NULL);
  if(!name[0])SetupDiGetDeviceRegistryPropertyW(list,&device,SPDRP_DEVICEDESC,NULL,reinterpret_cast<BYTE*>(name),sizeof(name),NULL);
  std::wstring text=name;if(text.find(L"BlackWidow")==std::wstring::npos && text.find(L"Basilisk")==std::wstring::npos && text.find(L"Razer")==std::wstring::npos && text.find(L"BSK")==std::wstring::npos)continue;
  std::wcout << L"Device: " << name << L"\n";
  HANDLE h=CreateFileW(detail->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
  if(h==INVALID_HANDLE_VALUE) {std::wcout<<L"Open error: "<<GetLastError()<<L"\n";continue;}
  USHORT n=0;HRESULT hr=BluetoothGATTGetServices(h,0,NULL,&n,BLUETOOTH_GATT_FLAG_NONE);std::wcout<<L"Service count: "<<n<<L" HRESULT: "<<std::hex<<hr<<std::dec<<L"\n";
  if(n && n<256) {
   std::vector<BTH_LE_GATT_SERVICE> services(n);hr=BluetoothGATTGetServices(h,n,services.data(),&n,BLUETOOTH_GATT_FLAG_NONE);
   if(SUCCEEDED(hr))for(auto& service:services) {
    std::wcout<<L" Service ";uuid(service.ServiceUuid);std::wcout<<L" handle "<<service.AttributeHandle<<L"\n";
    USHORT count=0;BluetoothGATTGetCharacteristics(h,&service,0,NULL,&count,BLUETOOTH_GATT_FLAG_NONE);
    if(count && count<256){std::vector<BTH_LE_GATT_CHARACTERISTIC> chars(count);hr=BluetoothGATTGetCharacteristics(h,&service,count,chars.data(),&count,BLUETOOTH_GATT_FLAG_NONE);
     if(SUCCEEDED(hr))for(auto& c:chars){std::wcout<<L"  Characteristic ";uuid(c.CharacteristicUuid);std::wcout<<L" read="<<(int)c.IsReadable<<L" write="<<(int)c.IsWritable<<L" writeNoResponse="<<(int)c.IsWritableWithoutResponse<<L" notify="<<(int)c.IsNotifiable<<L"\n";
      if(c.CharacteristicUuid.IsShortUuid && c.CharacteristicUuid.Value.ShortUuid == 0x2a19 && c.IsReadable) {
       USHORT bytes=0;HRESULT read=BluetoothGATTGetCharacteristicValue(h,&c,0,NULL,&bytes,BLUETOOTH_GATT_FLAG_FORCE_READ_FROM_DEVICE);
       if(bytes && bytes<128) {std::vector<BYTE> buffer(bytes);auto value=reinterpret_cast<PBTH_LE_GATT_CHARACTERISTIC_VALUE>(buffer.data());
        read=BluetoothGATTGetCharacteristicValue(h,&c,bytes,value,&bytes,BLUETOOTH_GATT_FLAG_FORCE_READ_FROM_DEVICE);
        if(SUCCEEDED(read) && value->DataSize==1)std::wcout<<L"   Battery (fresh read): "<<(int)value->Data[0]<<L"%\n";
        else std::wcout<<L"   Battery read HRESULT: "<<std::hex<<read<<std::dec<<L"\n";
       } else std::wcout<<L"   Battery sizing HRESULT: "<<std::hex<<read<<std::dec<<L"\n";
      }
     }
    }
   }
  }
  CloseHandle(h);
 }
 SetupDiDestroyDeviceInfoList(list);return 0;
}
