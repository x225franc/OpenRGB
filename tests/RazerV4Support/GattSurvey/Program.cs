using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

// Metadata and standard identity/battery reads only. Never writes a GATT value.
var paired = await DeviceInformation.FindAllAsync(BluetoothLEDevice.GetDeviceSelectorFromPairingState(true)).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
foreach(var info in paired) Console.WriteLine($"Paired: {info.Name}");
foreach(var info in paired.Where(d=>d.Name.Contains("V4",StringComparison.OrdinalIgnoreCase) && (d.Name.Contains("BSK",StringComparison.OrdinalIgnoreCase)||d.Name.Contains("Basilisk",StringComparison.OrdinalIgnoreCase))))
{
    Console.WriteLine($"Candidate: {info.Name}");
    using var device=await BluetoothLEDevice.FromIdAsync(info.Id).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
    if(device is null){Console.WriteLine("Unavailable");continue;}
    Console.WriteLine($"Connection: {device.ConnectionStatus}");
    var result=await device.GetGattServicesAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(20));
    Console.WriteLine($"Services: {result.Status}");
    foreach(var service in result.Services)
    using(service)
    {
        Console.WriteLine($"Service: {service.Uuid}");
        var chars=await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
        Console.WriteLine($"Characteristics: {chars.Status}");
        foreach(var characteristic in chars.Characteristics)
        {
            Console.WriteLine($"  Characteristic: {characteristic.Uuid} properties={characteristic.CharacteristicProperties}");
            var id=characteristic.Uuid.ToString();
            if(id is "00002a50-0000-1000-8000-00805f9b34fb" or "00002a19-0000-1000-8000-00805f9b34fb" or "00002a24-0000-1000-8000-00805f9b34fb" or "00002a26-0000-1000-8000-00805f9b34fb")
            {
                var read=await characteristic.ReadValueAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
                Console.WriteLine($"  Read: {read.Status}");
                if(read.Status==GattCommunicationStatus.Success)
                {
                    using var reader=DataReader.FromBuffer(read.Value);var bytes=new byte[reader.UnconsumedBufferLength];reader.ReadBytes(bytes);
                    Console.WriteLine($"  Value: {Convert.ToHexString(bytes)}");
                }
            }
        }
    }
}
