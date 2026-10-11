# SPDX-License-Identifier: GPL-2.0-or-later
"""Read-only controller and matrix validation through the unchanged SDK (protocol 0)."""
import argparse, hashlib, json, socket, struct, time
parser=argparse.ArgumentParser();parser.add_argument('--port',type=int,default=6744);parser.add_argument('--output');args=parser.parse_args()
def exact(sock,size):
    data=b''
    while len(data)<size:
        part=sock.recv(size-len(data))
        if not part: raise RuntimeError('SDK disconnected')
        data+=part
    return data
class Reader:
    def __init__(self,data): self.data=data;self.pos=0
    def read(self,fmt):
        result=struct.unpack_from('<'+fmt,self.data,self.pos);self.pos+=struct.calcsize('<'+fmt)
        return result[0] if len(result)==1 else result
    def string(self):
        size=self.read('H');data=self.data[self.pos:self.pos+size];self.pos+=size
        return data.rstrip(b'\0').decode('utf-8')
    def skip(self,size): self.pos+=size
with socket.create_connection(('127.0.0.1',args.port),timeout=5) as sock:
    sock.settimeout(5)
    def query(dev,pkt,payload=b''):
        sock.sendall(struct.pack('<4sIII',b'ORGB',dev,pkt,len(payload))+payload)
        while True:
            magic,target,kind,size=struct.unpack('<4sIII',exact(sock,16))
            assert magic==b'ORGB'
            data=exact(sock,size)
            if kind==pkt and target==dev: return data
    count=struct.unpack('<I',query(0,0))[0];devices=[]
    for i in range(count):
        r=Reader(query(i,1,struct.pack('<I',0)))
        size=r.read('I');device_type=r.read('I');name=r.string();description=r.string();version=r.string()
        serial=r.string();location=r.string();modes=r.read('H');active_mode=r.read('I')
        for _ in range(modes):
            r.string();r.skip(9*4);r.skip(r.read('H')*4)
        zones=[]
        for _ in range(r.read('H')):
            zone={'name':r.string()};zone_type,minimum,maximum,leds=r.read('IIII')
            zone['leds']=leds;matrix_size=r.read('H')
            if matrix_size:
                height,width=r.read('II');matrix=[r.read('I') for _ in range(height*width)]
                zone['matrix']={'height':height,'width':width,'map':matrix}
            zones.append(zone)
        leds=[]
        for _ in range(r.read('H')): leds.append({'name':r.string(),'value':r.read('I')})
        devices.append({'index':i,'name':name,'description':description,'version':version,
                        'serial_hash':hashlib.sha256(serial.encode()).hexdigest()[:16],
                        'location':location,'zones':zones,'leds':leds,'active_mode':active_mode})
    result={'count':count,'devices':devices}
    if args.output:
        with open(args.output,'w',encoding='utf-8') as file:json.dump(result,file,indent=2,ensure_ascii=False)
    for device in devices:
        print(device['index'],device['name'],'LEDs:',len(device['leds']),'zones:',[z['name'] for z in device['zones']])
        if 'BlackWidow' in device['name'] or 'Blackwidow' in device['name']:
            for name,address in (('Key: Down Arrow',78),('Key: Right Arrow',79)):
                assert device['leds'][address]['name']==name
                assert device['zones'][0]['matrix']['map'][address]==address
                print('PASS SDK',name,'retains physical address',address)
    assert all('Lenovo' not in device['name'] for device in devices)
    print('PASS Lenovo Lighting is not exposed')
