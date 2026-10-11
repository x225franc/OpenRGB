# SPDX-License-Identifier: GPL-2.0-or-later
"""Explicit physical RGB test. Sends independent configurable colors to the keyboard and mouse.
Stop other OpenRGB instances/effects first. Needs the isolated test SDK on 6744.
"""
import argparse, json, socket, struct, time
parser=argparse.ArgumentParser();parser.add_argument('--inventory',required=True);parser.add_argument('--port',type=int,default=6744);parser.add_argument('--keyboard',choices=['green','red','blue'],default='green');parser.add_argument('--mouse',choices=['green','red','magenta'],default='magenta');args=parser.parse_args()
devices=json.load(open(args.inventory,encoding='utf-8'))['devices']
with socket.create_connection(('127.0.0.1',args.port),timeout=5) as sock:
    for device in devices:
        assert device['name'].startswith('Razer ') and 'USB/HyperSpeed' in device['name']
        index=device['index'];count=len(device['leds']);keyboard='BlackWidow' in device['name']
        color_name=args.keyboard if keyboard else args.mouse
        color={'green':0x0000FF00,'red':0x000000FF,'blue':0x00FF0000,'magenta':0x00FF00FF}[color_name]
        sock.sendall(struct.pack('<4sIII',b'ORGB',index,1100,0))
        payload=struct.pack('<IH',6+count*4,count)+struct.pack('<I',color)*count
        sock.sendall(struct.pack('<4sIII',b'ORGB',index,1050,len(payload))+payload)
        print('SENT',device['name'],color_name,flush=True)
    time.sleep(.3)
