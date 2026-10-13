using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;
var devices = await DeviceInformation.FindAllAsync(BluetoothLEDevice.GetDeviceSelectorFromPairingState(true)).AsTask().WaitAsync(TimeSpan.FromSeconds(15));
foreach(var info in devices.Where(d => d.Name.Contains("BlackWidow",StringComparison.OrdinalIgnoreCase) || d.Name.Equals("BSKV3P 35K",StringComparison.OrdinalIgnoreCase)))
{
 try {
  using var device = await BluetoothLEDevice.FromIdAsync(info.Id).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
  Console.WriteLine($"Device {info.Name}; connection {device?.ConnectionStatus}");
  if(device is null) continue;
  var services = await device.GetGattServicesForUuidAsync(GattServiceUuids.Battery,BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
  Console.WriteLine($"Battery service: {services.Status}");
  foreach(var service in services.Services) using(service) {
   var chars = await service.GetCharacteristicsForUuidAsync(GattCharacteristicUuids.BatteryLevel,BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
   Console.WriteLine($"Battery characteristic: {chars.Status}");
   foreach(var c in chars.Characteristics) {
    var result = await c.ReadValueAsync(BluetoothCacheMode.Uncached).AsTask().WaitAsync(TimeSpan.FromSeconds(10));
    Console.WriteLine($"Battery read: {result.Status}");
    if(result.Status == GattCommunicationStatus.Success) {
     using var reader = DataReader.FromBuffer(result.Value);
     var bytes = new byte[reader.UnconsumedBufferLength]; reader.ReadBytes(bytes);
     Console.WriteLine($"Battery payload: {Convert.ToHexString(bytes)}; percent {(bytes.Length==1 ? bytes[0] : -1)}");
    }
   }
  }
 } catch(Exception ex) {Console.WriteLine($"ERROR {info.Name}: {ex.Message}");}
}
