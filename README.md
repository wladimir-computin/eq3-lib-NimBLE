# eq3-lib-NimBLE
ESP32/Arduino port of the keyble library using NimBLE for parallel WiFi and BLE connectivity.
Standalone library.

Thanks go to <a href="https://github.com/RoP09">RoP09</a>, <a href="https://github.com/tc-maxx">tc-maxx</a>, <a href="https://github.com/henfri">henfri</a>, <a href="https://github.com/MariusSchiffer">MariusSchiffer</a>, <a href="https://github.com/oyooyo">oyooyo</a> and <a href="https://github.com/KlausMu">KlausMu</a> for their brillant work!

## API

### Setup

```cpp
#include <eQ3.h>
``` 

```cpp
eQ3();
void setup(String ble_address, String user_key, unsigned char user_id = 0x01, String name = "");
void loop(); // must call!
void setOnStatusChange(std::function<void(LockStatus)> cb); // optional, untested
```

### Get
```cpp
bool isConnected();
bool isPaired();
void updateInfo();
ConnectionState getConnectionState();
LockState getLockState();
BatteryState getBatteryState();
String getBatteryStateStr();
String getConnectionStateStr();
String getLockStateStr();
int getRSSI();
```

### Set
```cpp
void connect();
void disconnect();
void lock();
void unlock();
void open();
void pairingRequest(std::string cardkey); // call after the ESP32 fully connected to the lock
```
