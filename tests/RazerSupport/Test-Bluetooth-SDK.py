# SPDX-License-Identifier: GPL-2.0-or-later
"""Explicit grouped-color tests against an isolated Bluetooth-only OpenRGB SDK server."""
import argparse, socket, struct, time
p=argparse.ArgumentParser()
p.add_argument('--port',type=int,default=6745)
p.add_argument('--action',choices=['colors','off','wave'],default='colors')
p.add_argument('--mode',help='Exact displayed mode name; applies only to the selected device')
p.add_argument('--device',choices=['keyboard','mouse','both'],default='both')
p.add_argument('--include-hyperspeed',action='store_true',help='Allow the verified shared USB/HyperSpeed mouse in the isolated test server')
p.add_argument('--list-only',action='store_true')
p.add_argument('--brightness',type=int,default=96)
a=p.parse_args()
if not 0 <= a.brightness <= 255: p.error('brightness must be 0..255')
def exact(s,n):
    data=b''
    while len(data)<n:
        block=s.recv(n-len(data))
        if not block: raise RuntimeError('SDK disconnected')
        data+=block
    return data
class Reader:
    def __init__(self,data): self.data=data;self.pos=0
    def read(self,fmt):
        value=struct.unpack_from('<'+fmt,self.data,self.pos);self.pos+=struct.calcsize('<'+fmt)
        return value[0] if len(value)==1 else value
    def string(self):
        n=self.read('H');text=self.data[self.pos:self.pos+n];self.pos+=n
        return text.rstrip(b'\0').decode('utf-8')
with socket.create_connection(('127.0.0.1',a.port),timeout=5) as s:
    s.settimeout(8)
    def send(dev,kind,data=b''):
        s.sendall(struct.pack('<4sIII',b'ORGB',dev,kind,len(data))+data)
    def query(dev,kind,data=b''):
        send(dev,kind,data)
        while True:
            magic,target,reply,n=struct.unpack('<4sIII',exact(s,16))
            if magic!=b'ORGB' or n>1000000: raise RuntimeError('Invalid SDK reply')
            body=exact(s,n)
            if target==dev and reply==kind: return body
    query(0,40,struct.pack('<I',3))
    count=struct.unpack('<I',query(0,0))[0]
    if not count: raise RuntimeError('No Bluetooth controllers detected')
    devices=[]
    for index in range(count):
        r=Reader(query(index,1,struct.pack('<I',3)))
        r.read('II');name=r.string()
        for _ in range(5): r.string()
        modes=r.read('H');r.read('I');records=[]
        for _ in range(modes):
            start=r.pos;mode_name=r.string();fields=r.pos
            values=r.read('12I');n=r.read('H');r.pos+=n*4
            record=bytearray(r.data[start:r.pos])
            if n and a.mode: struct.pack_into('<I',record,len(record)-n*4,0x0000FF00)
            if values[5] > values[4]: struct.pack_into('<I',record,fields-start+9*4,a.brightness)
            records.append((mode_name,record))
        if name not in ('Razer BlackWidow V3 Mini (Bluetooth)','Razer Basilisk V3 Pro 35K Phantom Green (Bluetooth)') and not (a.include_hyperspeed and name=='Razer Basilisk V3 Pro 35K Phantom Green (USB/HyperSpeed auto)'):
            raise RuntimeError('Use an isolated Bluetooth-only server')
        if a.device=='keyboard' and 'BlackWidow' not in name: continue
        if a.device=='mouse' and 'Basilisk' not in name: continue
        devices.append((index,name,records))
    if not devices: raise RuntimeError('Requested Bluetooth device is absent; wake it and rescan')
    if a.list_only:
        for _,name,records in devices: print(name,':',', '.join(n for n,_ in records))
        raise SystemExit(0)
    expected={}
    for index,name,records in devices:
        keyboard='BlackWidow' in name
        selected=a.mode or ('Off' if a.action=='off' else ('Wave' if a.action=='wave' and keyboard else 'Static'))
        mode_index=next(i for i,(n,_) in enumerate(records) if n==selected)
        if selected=='Static' or '(software)' in selected or (a.mode and not keyboard and '(Bluetooth)' in name):
            colors=[0x000000FF] if keyboard else [0x0000FF00,0x00000000] if a.mode else [0x00FF0000,0x0000FF00]
            payload=struct.pack('<IH',6+4*len(colors),len(colors))+struct.pack('<'+'I'*len(colors),*colors)
            send(index,1050,payload)
        record=records[mode_index][1]
        send(index,1101,struct.pack('<II',8+len(record),mode_index)+record)
        expected[index]=mode_index
        print('SENT',name,selected,'brightness',0 if selected=='Off' else a.brightness,flush=True)
    time.sleep(1)
    for index,mode_index in expected.items():
        r=Reader(query(index,1,struct.pack('<I',3)));r.read('II')
        for _ in range(6): r.string()
        r.read('H');active=r.read('I')
        if active != mode_index: raise RuntimeError('SDK rejected mode change')
    print('PASS SDK accepted requested modes; physical confirmation is separate')
