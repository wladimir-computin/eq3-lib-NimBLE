#ifndef DOOR_OPENER_EQ3_H
#define DOOR_OPENER_EQ3_H

#include <NimBLEScan.h>
#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <queue>
#include <functional>
#include "eQ3_constants.h"
#include "eQ3_message.h"
#include "eQ3_util.h"

#include "PrintDebug.h"

#define BLE_UUID_SERVICE "58e06900-15d8-11e6-b737-0002a5d5c51b"
#define BLE_UUID_WRITE "3141dd40-15db-11e6-a24b-0002a5d5c51b"
#define BLE_UUID_READ "359d4820-15db-11e6-82bd-0002a5d5c51b"
#define CONNECT_TIMEOUT 1
#define RECONNECT_INTERVALL 10*1000
#define LOCK_TIMEOUT 10
class eQ3;

typedef std::function<void(void*, eQ3*)> KeyBleStatusHandler;

void tickTask(void *params);
void notify_func(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

class eQ3 : public NimBLEClientCallbacks {
    friend void tickTask(void *params);
    bool onTick();
    void onConnect(BLEClient *pClient);
    void onDisconnect(BLEClient *pClient);
    void sendNextFragment();
    void exchangeNonces();
    bool sendMessage(eQ3Message::Message *msg);
    void sendCommand(CommandType command);
    static String formatMacAddress(const char * macCharArray);

    friend NimBLEClient;

    std::queue<eQ3Message::MessageFragment> sendQueue;
    std::vector<eQ3Message::MessageFragment> recvFragments;

    string address;

    NimBLERemoteCharacteristic *sendChar;
    NimBLERemoteCharacteristic *recvChar;
    
    long lastActivity = 0;
    ClientState state;
    LockState lockState;
    BatteryState batteryState;
    bool pairingAllowed;
    int rssi = 0;

    //SemaphoreHandle_t mutex;
    std::function<void(LockState)> onStatusChange = nullptr;
public:
    void onNotify(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);    

public:

    NimBLEClient *bleClient;
    std::string raw_data;

    eQ3();
    void setup(String ble_address, String user_key, unsigned char user_id = 0xFF, String name = "");
    void loop();
    void setOnStatusChange(std::function<void(LockState)> cb);
    
    void lock();
    void unlock();
    void open();
    void pairingRequest(String cardkey);
    void connect();
    void disconnect();
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
    static String genRandomUserKey();
    static String getMACfromSecurityCard(String securityCardStr);
    static String getCardKeyfromSecurityCard(String securityCardStr);
};

#endif //DOOR_OPENER_EQ3_H
