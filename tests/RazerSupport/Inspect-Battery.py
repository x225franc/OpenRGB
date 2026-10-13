# SPDX-License-Identifier: GPL-2.0-or-later
"""Read-only HID inventory and Razer battery/charge GET queries (07:80, 07:84). No RGB/pairing commands.
Run: python tests/RazerSupport/Inspect-Razer.py [--query]
Reads the same shipped HIDAPI ABI used by OpenRGB 1.0.
"""
import argparse, ctypes as c, hashlib, json, pathlib, time
parser=argparse.ArgumentParser();parser.add_argument('--query',action='store_true');parser.add_argument('--output',type=pathlib.Path);args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parents[2]
class Info(c.Structure): pass
Info._fields_=[('path',c.c_char_p),('vendor_id',c.c_ushort),('product_id',c.c_ushort),('serial_number',c.c_wchar_p),('release_number',c.c_ushort),('manufacturer_string',c.c_wchar_p),('product_string',c.c_wchar_p),('usage_page',c.c_ushort),('usage',c.c_ushort),('interface_number',c.c_int),('next',c.POINTER(Info)),('bus_type',c.c_int)]
lib=c.CDLL(str(root/'dependencies/hidapi-hotplug-win/x64/hidapi-hotplug.dll'))
lib.hid_enumerate.argtypes=[c.c_ushort,c.c_ushort];lib.hid_enumerate.restype=c.POINTER(Info)
lib.hid_free_enumeration.argtypes=[c.POINTER(Info)]
lib.hid_open_path.argtypes=[c.c_char_p];lib.hid_open_path.restype=c.c_void_p
lib.hid_close.argtypes=[c.c_void_p]
lib.hid_send_feature_report.argtypes=[c.c_void_p,c.c_void_p,c.c_size_t]
lib.hid_get_feature_report.argtypes=[c.c_void_p,c.c_void_p,c.c_size_t]
lib.hid_error.argtypes=[c.c_void_p];lib.hid_error.restype=c.c_wchar_p
kernel=c.WinDLL('kernel32',use_last_error=True)
kernel.CreateMutexW.argtypes=[c.c_void_p,c.c_bool,c.c_wchar_p];kernel.CreateMutexW.restype=c.c_void_p
kernel.WaitForSingleObject.argtypes=[c.c_void_p,c.c_uint];kernel.ReleaseMutex.argtypes=[c.c_void_p];kernel.CloseHandle.argtypes=[c.c_void_p]
mutex=kernel.CreateMutexW(None,False,'Global\\RazerLinkReadWriteGuardMutex')
def query(path,tx,command,size):
    dev=lib.hid_open_path(path)
    if not dev: return {'error':lib.hid_error(None)}
    owned=False
    try:
        if not mutex or kernel.WaitForSingleObject(mutex,2000) not in (0,0x80): return {'error':'Razer mutex unavailable'}
        owned=True
        request=bytearray(91);request[2]=tx;request[6]=size;request[7]=7;request[8]=command
        for value in request[3:89]:request[89]^=value
        buffer=(c.c_ubyte*91).from_buffer_copy(request)
        sent=lib.hid_send_feature_report(dev,buffer,91)
        if sent!=91:return {'sent':sent,'error':lib.hid_error(dev)}
        time.sleep(.030) # Existing upstream GET serial/firmware turnaround.
        response=(c.c_ubyte*91)();received=lib.hid_get_feature_report(dev,response,91)
        data=bytes(response)
        result={'sent':sent,'received':received,'status':data[1],'transaction':data[2],'class':data[7],'command':data[8],'data_size':data[6],
                'matches':received==91 and data[1]==2 and data[2]==tx and data[7]==7 and data[8]==command}
        checksum=0
        for byte in data[3:89]: checksum ^= byte
        result['crc_valid'] = checksum == data[89]
        result['arguments']=list(data[9:13])
        if result['matches'] and result['crc_valid'] and command==0x80:result['battery_percent']=round(data[10]*100/255)
        return result
    finally:
        if owned:kernel.ReleaseMutex(mutex)
        lib.hid_close(dev)
head=lib.hid_enumerate(0,0);node=head;rows=[]
try:
    while node:
        info=node.contents
        # Razer and every LampArray, including Lenovo devices with non-Lenovo USB VID.
        if info.vendor_id in (0x1532,0x068E) or info.usage_page==0x59:
            row={'vid':f'{info.vendor_id:04X}','pid':f'{info.product_id:04X}','interface':info.interface_number,
                 'usage_page':f'{info.usage_page:04X}','usage':f'{info.usage:04X}','bus_type':info.bus_type,
                 'product':info.product_string,'path':info.path.decode(errors='replace')}
            if args.query and info.product_id in (0x0258,0x0271,0x00D6,0x00D7) and ((info.interface_number==3 and info.usage_page==0x0C and info.usage==1) or (info.interface_number==0 and info.usage_page==1 and info.usage==2) or False):
                tx=0x9F if info.product_id==0x0271 or (info.product_id==0x00D7 and info.interface_number in (2,3)) else 0x1F
                row['queries']={}
                for command,size in [(0x80,2),(0x84,2)]:
                    row['queries'][f'{command:02X}']=query(info.path,tx,command,size)
                if info.product_id==0x00D7 and info.interface_number==0:
                    row['keyboard_route_probe']={f'{command:02X}':query(info.path,0x9F,command,size) for command,size in [(0x80,2),(0x84,2)]}
                if info.product_id==0x0271:
                    row['mouse_route_probe']={f'{command:02X}':query(info.path,0x1F,command,size) for command,size in [(0x80,2),(0x84,2)]}
            rows.append(row)
        node=info.next
finally:
    lib.hid_free_enumeration(head)
    if mutex:kernel.CloseHandle(mutex)
    lib.hid_exit()
output=json.dumps(rows,ensure_ascii=False,indent=2)
if args.output:args.output.write_text(output,encoding='utf-8')
print(output)
