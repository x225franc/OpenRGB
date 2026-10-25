"""Observe the official application's ADL calls; never send device commands.

Requires frida. Attach using --pid, or launch the official UI with --launch.
Output is JSONL; Ctrl+C detaches. Device functions are never invoked by this tool.
"""
import argparse
import datetime
import json
import pathlib
import sys
import threading

parser = argparse.ArgumentParser(description=__doc__)
target = parser.add_mutually_exclusive_group(required=True)
target.add_argument('--pid', type=int)
target.add_argument('--launch', type=pathlib.Path, help='Launch the official UI and observe its children')
parser.add_argument('--output', type=pathlib.Path, required=True)
parser.add_argument('--seconds', type=int, default=600)
parser.add_argument('--tools', type=pathlib.Path)
args = parser.parse_args()
if args.tools:
    sys.path.insert(0, str(args.tools.resolve()))
import frida

SOURCE = r'''
const names = ['ADL_Display_WriteAndReadSMUI2C', 'ADL_Display_WriteAndReadI2C',
               'ADL2_Display_WriteAndReadI2C'];
const diagnostics = ['ADL_Main_Control_Create', 'ADL2_Main_Control_Create',
 'ADL_Adapter_NumberOfAdapters_Get', 'ADL_Adapter_AdapterInfo_Get',
 'ADL2_Adapter_Active_Get', 'ADL2_Device_PMLog_Device_Create',
 'ADL2_Adapter_KeepAwake_Start'];
function decode(p) {
    const n = p.add(24).readS32();
    const data = p.add(Process.pointerSize === 8 ? 32 : 28).readPointer();
    return {size:p.readS32(), line:p.add(4).readS32(), address:p.add(8).readS32(),
            offset:p.add(12).readS32(), action:p.add(16).readS32(),
            speed:p.add(20).readS32(), length:n,
            data:n > 0 && n <= 4096 && !data.isNull() ?
                Array.from(new Uint8Array(data.readByteArray(n))) : []};
}
let count = 0;
const installed = new Set();
Process.attachModuleObserver({onAdded(mod) {
if (mod.name.toLowerCase() !== 'atiadlxx.dll') return;
for (const name of diagnostics) {
    const fn = mod.findExportByName(name);
    if (fn === null || installed.has(name)) continue;
    installed.add(name);
    Interceptor.attach(fn, {
        onEnter(args) {
            this.id = ++count;
            this.p = args[0];
            send({event:'diagnostic-enter',id:this.id,api:name,
                  args:[String(args[0]),String(args[1]),String(args[2])]});
        },
        onLeave(ret) {
            const record = {event:'diagnostic-leave',id:this.id,api:name,result:ret.toInt32()};
            if (name === 'ADL_Adapter_NumberOfAdapters_Get' && ret.toInt32() === 0)
                try {record.adapters = this.p.readS32();} catch(e) {}
            send(record);
        }
    });
}
for (const name of names) {
    const fn = mod.findExportByName(name);
    if (fn === null || installed.has(name)) continue;
    installed.add(name);
    Interceptor.attach(fn, {
        onEnter(args) {
            this.id = ++count;
            const is2 = name.startsWith('ADL2_');
            this.p = args[is2 ? 2 : 1];
            try {send({event:'enter',id:this.id,api:name,
                       adapter:args[is2 ? 1 : 0].toInt32(),i2c:decode(this.p)});}
            catch(e) {send({event:'decode-error',id:this.id,error:String(e)});}
        },
        onLeave(ret) {
            try {send({event:'leave',id:this.id,result:ret.toInt32(),i2c:decode(this.p)});}
            catch(e) {send({event:'decode-error',id:this.id,error:String(e)});}
        }
    });
    send({event:'hooked',api:name,address:String(fn)});
}
}});
send({event:'ready',pid:Process.id});
'''

with args.output.open('a', encoding='utf-8', buffering=1) as output:
    def receive(message, data):
        record = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), **message}
        output.write(json.dumps(record) + '\n')
        print(json.dumps(record), flush=True)
    device = frida.get_local_device()
    sessions = []
    scripts = []
    def attach(pid):
        session = device.attach(pid)
        sessions.append(session)
        if args.launch:
            session.enable_child_gating()
        script = session.create_script(SOURCE)
        script.on('message', lambda message, data: receive({'pid': pid, **message}, data))
        scripts.append(script)
        script.load()
    def child_added(child):
        try:
            attach(child.pid)
        except Exception as error:
            receive({'event': 'child-attach-error', 'pid': child.pid, 'error': str(error)}, None)
        finally:
            device.resume(child.pid)
    device.on('child-added', child_added)
    try:
        if args.launch:
            pid = device.spawn([str(args.launch.resolve())], cwd=str(args.launch.resolve().parent))
            try:
                attach(pid)
            finally:
                device.resume(pid)
        else:
            attach(args.pid)
        threading.Event().wait(args.seconds)
    except KeyboardInterrupt:
        pass
    finally:
        for session in reversed(sessions):
            try:
                session.detach()
            except frida.InvalidOperationError:
                pass
