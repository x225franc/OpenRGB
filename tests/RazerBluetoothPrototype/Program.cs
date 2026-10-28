using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

var keyboard = args.Contains("--keyboard");
var v4 = args.Contains("--v4");
if(v4 && keyboard) throw new ArgumentException("Choose one model");
args = args.Where(a => a != "--keyboard" && a != "--v4").ToArray();
if (args.Length > 1 || args.Any(a => a != "--color-test" && a != "--restore" && a != "--wave-test" && a != "--breathing-test" && a != "--spectrum-test" && a != "--effects-restore" && a != "--reactive-red-test" && a != "--reactive-test" && a != "--starlight-test"))
    throw new ArgumentException("Usage: prototype [--color-test | --restore]");

var effectProbe = args.Any(a => a is "--breathing-test" or "--spectrum-test" or "--effects-restore" or "--wave-test" or "--reactive-test" or "--reactive-red-test" or "--starlight-test");
var snapshotPath = Path.Combine(AppContext.BaseDirectory, (v4 ? "v4-" : "") + (effectProbe ? (keyboard ? "effect-probe-keyboard-original.json" : "effect-probe-mouse-original.json") : (keyboard ? "original-keyboard-lighting.json" : "original-lighting.json")));
var vendor = Guid.Parse("52401523-f97c-7f90-0e7f-6c6f4e36db1c");
var devices = await DeviceInformation.FindAllAsync(BluetoothLEDevice.GetDeviceSelectorFromPairingState(true)).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
foreach (var info in devices.Where(d => (keyboard ? d.Name.Contains("BlackWidow", StringComparison.OrdinalIgnoreCase) : d.Name.Equals(v4 ? "BSK V4 PRO" : "BSKV3P 35K", StringComparison.OrdinalIgnoreCase))))
{
    Console.WriteLine($"Device: {info.Name}");
    using var device = await BluetoothLEDevice.FromIdAsync(info.Id).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
    if (device is null) { Console.WriteLine("Device unavailable"); continue; }
    Console.WriteLine($"Connection: {device.ConnectionStatus}");
    bool verifiedIdentity = !v4;
    var identityServices = await device.GetGattServicesForUuidAsync(Guid.Parse("0000180a-0000-1000-8000-00805f9b34fb"), BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
    foreach (var identityService in identityServices.Services)
    using (identityService)
    {
        var identityChars = await identityService.GetCharacteristicsForUuidAsync(Guid.Parse("00002a50-0000-1000-8000-00805f9b34fb"), BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
        foreach (var identityChar in identityChars.Characteristics)
        {
            var identity = await identityChar.ReadValueAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
            if (identity.Status == GattCommunicationStatus.Success)
            {
                using var reader = DataReader.FromBuffer(identity.Value);
                var value = new byte[reader.UnconsumedBufferLength]; reader.ReadBytes(value);
                Console.WriteLine($"PnP identity: {Convert.ToHexString(value)}");
                if(value.Length==7 && value[0]==2 && value[1]==0x8E && value[2]==6 && value[3]==0xE4 && value[4]==0) verifiedIdentity=true;
            }
        }
    }

    if(!verifiedIdentity) throw new InvalidOperationException("Unexpected V4 PnP identity; no lighting query/write attempted");
    var result = await device.GetGattServicesForUuidAsync(vendor, BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
    Console.WriteLine($"Vendor discovery: {result.Status}");
    foreach (var service in result.Services)
    {
        using (service)
        {
            var access = await service.RequestAccessAsync().AsTask().WaitAsync(TimeSpan.FromSeconds(10));
            Console.WriteLine($"Access: {access}");
            var chars = await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
            Console.WriteLine($"Characteristics: {chars.Status}");
            if(chars.Status!=GattCommunicationStatus.Success)throw new InvalidOperationException($"GATT characteristics unavailable: {chars.Status}");
            var write = chars.Characteristics.Single(c => c.Uuid == Guid.Parse("52401524-f97c-7f90-0e7f-6c6f4e36db1c"));
            var notify = chars.Characteristics.Single(c => c.Uuid == Guid.Parse("52401525-f97c-7f90-0e7f-6c6f4e36db1c"));
            var frames = new List<byte[]>();
            object gate = new();
            void OnValue(GattCharacteristic sender, GattValueChangedEventArgs e)
            {
                using var reader = DataReader.FromBuffer(e.CharacteristicValue);
                var data = new byte[reader.UnconsumedBufferLength]; reader.ReadBytes(data);
                lock (gate) { if (frames.Count < 64) frames.Add(data); }
            }
            notify.ValueChanged += OnValue;
            try
            {
                var subscribed = await notify.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.Notify).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
                Console.WriteLine($"Notification subscription: {subscribed}");
                if (subscribed != GattCommunicationStatus.Success) continue;
                byte requestId = 0x30;
                async Task<byte[]?> Exchange(byte[] key, byte[]? payload = null)
                {
                    byte id = requestId++;
                    lock(gate) frames.Clear();
                    using var writer = new DataWriter();
                    writer.WriteBytes(new byte[] {id, (byte)(payload?.Length ?? 0), 0, 0}); writer.WriteBytes(key);
                    var sent = await write.WriteValueWithResultAsync(writer.DetachBuffer(), GattWriteOption.WriteWithResponse).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
                    if(sent.Status != GattCommunicationStatus.Success) throw new Exception($"GATT write failed: {sent.Status}");
                    if(payload is not null) {
                        writer.WriteBytes(payload);
                        sent = await write.WriteValueWithResultAsync(writer.DetachBuffer(), GattWriteOption.WriteWithResponse).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
                        if(sent.Status != GattCommunicationStatus.Success) throw new Exception($"Payload failed: {sent.Status}");
                    }
                    var deadline = DateTime.UtcNow.AddSeconds(3);
                    while(DateTime.UtcNow < deadline)
                    {
                        lock(gate) {
                            int headerIndex = frames.FindIndex(f => f.Length >= 8 && f[0] == id && f[7] is 2 or 3 or 5);
                            if(headerIndex >= 0) {
                                var header = frames[headerIndex];
                                if(header[7] != 2) {Console.WriteLine($"{Convert.ToHexString(key)}: device status {header[7]}"); return null;}
                                var bytes = frames.Skip(headerIndex + 1).SelectMany(f => f).ToArray();
                                if(bytes.Length >= header[1]) {
                                    var value = bytes.Take(header[1]).ToArray();
                                    Console.WriteLine($"{Convert.ToHexString(key)}: {Convert.ToHexString(value)}"); return value;
                                }
                            }
                        }
                        await Task.Delay(20);
                    }
                    Console.WriteLine($"{Convert.ToHexString(key)}: timeout");return null;
                }
                var zones = await Exchange(new byte[] {0x10,0x80,0,1});
                if (keyboard) await Exchange(new byte[] {0x10,0x84,0,0});
                var original = new Dictionary<byte, byte[]>();
                if(zones is not null)
                    foreach(byte zone in zones.Distinct().Where(z => z != 0)) {
                        var state = await Exchange(new byte[] {0x10,0x83,0,zone});
                        var brightness = await Exchange(new byte[] {0x10,0x85,1,zone});
                        if ((keyboard ? zone == 5 : v4 ? zone is 1 or 4 or 10 : zone is 1 or 10) && state?.Length == 10 && brightness?.Length == 1)
                            original.Add(zone, state.Concat(brightness).ToArray());
                    }
                if (args.Length != 0)
                {
                    if (original.Count != (keyboard ? 1 : v4 ? 3 : 2))
                        throw new InvalidOperationException("All physical zones must respond before changing lighting.");
                    Dictionary<byte, byte[]> target;
                    if (args[0] is "--restore" or "--effects-restore")
                    {
                        target = System.Text.Json.JsonSerializer.Deserialize<Dictionary<byte, byte[]>>(File.ReadAllText(snapshotPath))
                            ?? throw new InvalidOperationException("Invalid snapshot.");
                        if (target.Count != original.Count || target.Any(p => !original.ContainsKey(p.Key) || p.Value.Length != 11))
                            throw new InvalidOperationException("Invalid snapshot zones.");
                    }
                    else
                    {
                        if (!File.Exists(snapshotPath))
                            File.WriteAllText(snapshotPath, System.Text.Json.JsonSerializer.Serialize(original));
                        target = keyboard ? new() { [5] = args[0] == "--wave-test" ? new byte[] {4,1,40,1,0,255,0,0,0,0,96} : new byte[] {1,0,0,1,255,0,0,0,0,0,96} } : new() {
                            [1] = new byte[] {1,0,0,1,0,0,255,0,0,0,96},
                            [10] = new byte[] {1,0,0,1,0,255,0,0,0,0,96}
                        };
                    }
                    if(v4 && args[0] == "--color-test") target[4]=new byte[]{1,0,0,1,255,0,0,0,0,0,96};
                    if (effectProbe && args[0] != "--effects-restore")
                    {
                        byte[] state = args[0] switch {
                            "--breathing-test" => new byte[] {2,0,0,1,0,255,0,0,0,0,96},
                            "--spectrum-test" => new byte[] {3,0,0,0,0,0,0,0,0,0,96},
                            "--wave-test" => new byte[] {4,1,40,1,0,255,0,0,0,0,96},
                            "--reactive-red-test" => new byte[] {5,0,3,1,255,0,0,0,0,0,160},
                            "--reactive-test" => new byte[] {5,0,3,1,0,255,0,0,0,0,96},
                            "--starlight-test" => new byte[] {7,0,2,1,0,255,0,0,0,0,96},
                            _ => throw new ArgumentException("Unsupported lighting probe")
                        };
                        target = keyboard ? new() { [5] = state } : new() {
                            [1] = state, [10] = new byte[] {1,0,0,1,0,0,0,0,0,0,0}
                        };
                    }
                    if(!keyboard && args.Length>0 && args[0] is "--wave-test" or "--reactive-test" or "--reactive-red-test" or "--starlight-test")
                    {
                        target[10]=target[1];target[1]=new byte[]{1,0,0,1,0,0,0,0,0,0,0};
                    }
                    if(v4 && effectProbe && args[0] != "--effects-restore") target[4]=new byte[]{1,0,0,1,0,0,0,0,0,0,0};
                    if(target.Count != original.Count) throw new InvalidOperationException("Every verified physical group must have an explicit target");
                    foreach (var pair in target)
                    {
                        if (await Exchange(new byte[] {0x10,3,0,pair.Key}, pair.Value[..10]) is null)
                            throw new InvalidOperationException("Color rejected; stopping test.");
                        if (await Exchange(new byte[] {0x10,5,1,pair.Key}, pair.Value[10..]) is null)
                            throw new InvalidOperationException("Brightness rejected; stopping test.");
                        var state = await Exchange(new byte[] {0x10,0x83,0,pair.Key});
                        var brightness = await Exchange(new byte[] {0x10,0x85,1,pair.Key});
                        if (state is null || brightness is null || !state.SequenceEqual(pair.Value[..10]) || !brightness.SequenceEqual(pair.Value[10..]))
                            throw new InvalidOperationException("Readback does not match requested lighting.");
                    }
                    Console.WriteLine("All zones confirmed by device readback. Visual confirmation still required.");
                }
            }
            finally
            {
                try {
                    await notify.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.None).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
                } finally { notify.ValueChanged -= OnValue; }
            }
        }
    }
}
