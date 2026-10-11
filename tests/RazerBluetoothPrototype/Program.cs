using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

if (args.Length > 1 || args.Any(a => a != "--color-test" && a != "--restore"))
    throw new ArgumentException("Usage: prototype [--color-test | --restore]");
var snapshotPath = Path.Combine(AppContext.BaseDirectory, "original-lighting.json");
var vendor = Guid.Parse("52401523-f97c-7f90-0e7f-6c6f4e36db1c");
var devices = await DeviceInformation.FindAllAsync(BluetoothLEDevice.GetDeviceSelectorFromPairingState(true)).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
foreach (var info in devices.Where(d => d.Name.Equals("BSKV3P 35K", StringComparison.OrdinalIgnoreCase)))
{
    Console.WriteLine($"Device: {info.Name}");
    using var device = await BluetoothLEDevice.FromIdAsync(info.Id).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
    if (device is null) { Console.WriteLine("Device unavailable"); continue; }
    Console.WriteLine($"Connection: {device.ConnectionStatus}");
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
                var original = new Dictionary<byte, byte[]>();
                if(zones is not null)
                    foreach(byte zone in zones.Distinct().Where(z => z != 0)) {
                        var state = await Exchange(new byte[] {0x10,0x83,0,zone});
                        var brightness = await Exchange(new byte[] {0x10,0x85,1,zone});
                        if (zone is 1 or 10 && state?.Length == 10 && brightness?.Length == 1)
                            original.Add(zone, state.Concat(brightness).ToArray());
                    }
                if (args.Length != 0)
                {
                    if (original.Count != 2)
                        throw new InvalidOperationException("Both physical zones must respond before changing lighting.");
                    Dictionary<byte, byte[]> target;
                    if (args[0] == "--restore")
                    {
                        target = System.Text.Json.JsonSerializer.Deserialize<Dictionary<byte, byte[]>>(File.ReadAllText(snapshotPath))
                            ?? throw new InvalidOperationException("Invalid snapshot.");
                        if (target.Count != 2 || target.Any(p => p.Key is not (1 or 10) || p.Value.Length != 11))
                            throw new InvalidOperationException("Invalid snapshot zones.");
                    }
                    else
                    {
                        if (!File.Exists(snapshotPath))
                            File.WriteAllText(snapshotPath, System.Text.Json.JsonSerializer.Serialize(original));
                        target = new() {
                            [1] = new byte[] {1,0,0,1,0,0,255,0,0,0,96},
                            [10] = new byte[] {1,0,0,1,0,255,0,0,0,0,96}
                        };
                    }
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
                    Console.WriteLine("Both zones confirmed by device readback. Visual confirmation still required.");
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
