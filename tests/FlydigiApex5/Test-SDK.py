# SPDX-License-Identifier: GPL-2.0-or-later
"""Explicit lighting test against a Flydigi-only isolated OpenRGB SDK server."""
import argparse,socket,struct,time,colorsys
p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=6745);p.add_argument('--list-only',action='store_true');p.add_argument('--mode',default='Direct');p.add_argument('--brightness',type=int,default=30);p.add_argument('--speed',type=int,default=86);p.add_argument('--seconds',type=float,default=0);p.add_argument('--fps',type=int,default=20);a=p.parse_args()
if not 0<=a.brightness<=100:p.error('brightness must be 0..100')
if not 1<=a.speed<=100:p.error('speed must be 1..100')
def exact(s,n):
 b=b''
 while len(b)<n:
  part=s.recv(n-len(b))
  if not part:raise RuntimeError('SDK disconnected')
  b+=part
 return b
class Reader:
 def __init__(self,data):self.data=data;self.pos=0
 def read(self,fmt):
  v=struct.unpack_from('<'+fmt,self.data,self.pos);self.pos+=struct.calcsize('<'+fmt);return v[0] if len(v)==1 else v
 def string(self):
  n=self.read('H');v=self.data[self.pos:self.pos+n];self.pos+=n;return v.rstrip(b'\0').decode()
with socket.create_connection(('127.0.0.1',a.port),timeout=5) as s:
 s.settimeout(8)
 def send(device,kind,body=b''):s.sendall(struct.pack('<4sIII',b'ORGB',device,kind,len(body))+body)
 def query(device,kind,body=b''):
  send(device,kind,body)
  while True:
   magic,target,reply,n=struct.unpack('<4sIII',exact(s,16));assert magic==b'ORGB' and n<1000000
   data=exact(s,n)
   if reply==10:
    ack_kind,status=struct.unpack('<II',data)
    if status:raise RuntimeError(f'SDK rejected packet {ack_kind} for controller {target}: status {status}')
   if target==device and reply==kind:return data
 query(0,40,struct.pack('<I',6));listing=query(0,0);count=struct.unpack_from('<I',listing)[0]
 controller_id=struct.unpack_from('<I',listing,4)[0] if count else None
 if count!=1:raise RuntimeError(f'Use an isolated Flydigi-only SDK server, got {count} devices')
 r=Reader(query(controller_id,1));r.read('II');name=r.string()
 if name not in ('Flydigi APEX 5 (USB)','Flydigi APEX 5 (2.4 GHz)'):raise RuntimeError('Unexpected target: '+name)
 for _ in range(5):r.string()
 n=r.read('H');r.read('I');records=[]
 for _ in range(n):
  start=r.pos;title=r.string();fields=r.pos;values=r.read('11I');colors=r.read('H');r.pos+=colors*4;record=bytearray(r.data[start:r.pos]);offset=fields-start
  struct.pack_into('<I',record,offset+8*4,a.brightness)
  if values[2]>values[1]:struct.pack_into('<I',record,offset+7*4,a.speed)
  palette=[0x0000FF00] if title!='Gradient' else [0x00FF0000,0x000000FF,0x0000FF00]
  for i in range(colors):struct.pack_into('<I',record,len(record)-colors*4+i*4,palette[i%len(palette)])
  records.append((title,record))
 zones=r.read('H');assert zones==1
 r.string();kind,minimum,maximum,led_count=r.read('4I')
 assert led_count==12,led_count
 print(name,'LEDs',led_count,'modes:',', '.join(n for n,_ in records),flush=True)
 if a.list_only:raise SystemExit(0)
 selected=next(i for i,(title,_) in enumerate(records) if title==a.mode);record=records[selected][1]
 send(controller_id,1101,struct.pack('<II',8+len(record),selected)+record)
 def colors(values):send(controller_id,1050,struct.pack('<IH',6+len(values)*4,len(values))+struct.pack('<'+'I'*len(values),*values))
 if a.mode=='Direct':colors([0x000000FF,0x0000FF00,0x00FF0000]*4)
 start_time=time.monotonic();end=start_time+a.seconds;ticks=0
 while a.seconds and time.monotonic()<end:
  values=[]
  for i in range(12):
   red,green,blue=colorsys.hsv_to_rgb((i/12+(time.monotonic()-start_time)/6)%1,1,1);values.append(round(red*255)|(round(green*255)<<8)|(round(blue*255)<<16))
  colors(values);ticks+=1;time.sleep(1/a.fps)
 time.sleep(.15)
 current=Reader(query(controller_id,1));current.read('II')
 for _ in range(6):current.string()
 current.read('H');active=current.read('I')
 if active!=selected:raise RuntimeError('SDK did not apply selected mode')
 print('SENT',a.mode,'brightness',a.brightness,'stream frames',ticks,'; visual confirmation required',flush=True)
