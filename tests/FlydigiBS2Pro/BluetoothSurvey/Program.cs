// SPDX-License-Identifier: GPL-2.0-or-later
// READ ONLY: no GATT writes/subscriptions, fan commands, pairing changes or automatic retries.
using System.Security.Cryptography;
using System.Text;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

static string Normalize(string text)=>new(text.Where(char.IsLetterOrDigit).Select(char.ToUpperInvariant).ToArray());
static string Fingerprint(string id)=>Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(id)))[..16];
static async Task<byte[]?> Read(GattCharacteristic characteristic)
{
    var result=await characteristic.ReadValueAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
    if(result.Status!=GattCommunicationStatus.Success)return null;
    using var reader=DataReader.FromBuffer(result.Value);
    byte[] bytes=new byte[reader.UnconsumedBufferLength];reader.ReadBytes(bytes);return bytes;
}
if(args.Any(a=>a is not "--inspect" && !a.StartsWith("--fingerprint=",StringComparison.Ordinal)))
    throw new ArgumentException("Usage: BluetoothSurvey [--inspect] [--fingerprint=HASH]");
var requested=args.FirstOrDefault(a=>a.StartsWith("--fingerprint=",StringComparison.Ordinal))?.Split('=',2)[1];
var paired=await DeviceInformation.FindAllAsync(BluetoothLEDevice.GetDeviceSelectorFromPairingState(true)).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
var candidates=paired.Where(d=>Normalize(d.Name).Contains("FLYDIGIBS2PRO",StringComparison.Ordinal)).ToList();
Console.WriteLine($"Paired BS2 Pro candidates: {candidates.Count}");
foreach(var info in candidates)Console.WriteLine($"Name={info.Name}; fingerprint={Fingerprint(info.Id)} (cached pairing, not proof of a live device)");
if(!args.Contains("--inspect")){Console.WriteLine("Paired-only mode: no Bluetooth connection or GATT access requested.");return;}
if(requested is not null)candidates=candidates.Where(d=>Fingerprint(d.Id).Equals(requested,StringComparison.OrdinalIgnoreCase)).ToList();
if(candidates.Count!=1)throw new InvalidOperationException("Choose one unambiguous paired BS2 Pro fingerprint before inspection.");
using var device=await BluetoothLEDevice.FromIdAsync(candidates[0].Id).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
if(device is null||device.ConnectionStatus!=BluetoothConnectionStatus.Connected)
{
    Console.WriteLine("Device unavailable/disconnected: stop here; no vendor service access or retry.");Environment.ExitCode=2;return;
}
var infoResult=await device.GetGattServicesForUuidAsync(Guid.Parse("0000180a-0000-1000-8000-00805f9b34fb"),BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
bool verified=false;
foreach(var service in infoResult.Services)
using(service)
{
    var characteristics=await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
    foreach(var characteristic in characteristics.Characteristics)
    {
        string uuid=characteristic.Uuid.ToString();
        if(uuid=="00002a50-0000-1000-8000-00805f9b34fb")
        {
            var bytes=await Read(characteristic);
            if(bytes is not null&&bytes.Length>=7)
            {
                int vendor=bytes[1]|bytes[2]<<8,product=bytes[3]|bytes[4]<<8;
                Console.WriteLine($"PnP: source={bytes[0]}, VID={vendor:X4}, PID={product:X4}, revision={bytes[5]|bytes[6]<<8:X4}");
                verified=(bytes[0] is 1 or 2)&&vendor==0x37D7&&product==0x1002;
            }
        }
        else if(uuid is "00002a24-0000-1000-8000-00805f9b34fb" or "00002a26-0000-1000-8000-00805f9b34fb")
        {
            var bytes=await Read(characteristic);
            if(bytes is not null)Console.WriteLine($"Standard identity {uuid}: {Encoding.UTF8.GetString(bytes.Take(128).ToArray()).TrimEnd('\0')}");
        }
    }
}
if(!verified){Console.WriteLine("PnP identity unverified/different: do not inspect vendor services automatically.");Environment.ExitCode=3;return;}
Console.WriteLine("PnP identity verified against cached BS2 Pro candidate; hardware protocol still unvalidated.");
var all=await device.GetGattServicesAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
Console.WriteLine($"GATT catalog: {all.Status}");
foreach(var service in all.Services)
using(service)
{
    Console.WriteLine($"Service {service.Uuid}");
    var result=await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
    Console.WriteLine($"Characteristics: {result.Status}");
    foreach(var characteristic in result.Characteristics)Console.WriteLine($"  {characteristic.Uuid} properties={characteristic.CharacteristicProperties}");
}
Console.WriteLine("Completed: metadata/standard reads only; no RGB, fan or subscription writes were performed.");
