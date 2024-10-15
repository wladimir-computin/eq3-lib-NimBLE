#include <Arduino.h>
#include <string>
#include <NimBLEDevice.h>
#include <ctime>
#include <sstream>
#include "eQ3.h"
using namespace std;

int _LockStatus = 0;
int _RSSI = 0;

eQ3* cb_instance;

#define SEMAPHORE_WAIT_TIME  (10000 / portTICK_PERIOD_MS)

// -----------------------------------------------------------------------------
// --[notify_func]--------------------------------------------------------------
// -----------------------------------------------------------------------------
void notify_func(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    cb_instance->onNotify(pBLERemoteCharacteristic,pData,length,isNotify);
}

// -----------------------------------------------------------------------------
// --[ctor]---------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Important: Initialize BLEDevice::init(""); yourself
eQ3::eQ3() {
    cb_instance = this;
    //mutex = xSemaphoreCreateMutex();
}

void eQ3::loop(){
    onTick();
}

void eQ3::setup(String ble_address, String user_key, unsigned char user_id, String name){
    this->address = ble_address.c_str();
    std::string user_key_std = user_key.c_str();
    state.user_key = hexstring_to_string(user_key_std);
    state.user_id = user_id;
    
    NimBLEDevice::init(name.c_str());

    bleClient = NimBLEDevice::createClient();
    bleClient->setClientCallbacks((NimBLEClientCallbacks *)this);
    bleClient->setConnectTimeout(CONNECT_TIMEOUT);
    
    lastActivity = millis();
}

// -----------------------------------------------------------------------------
// --[onConnect]----------------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::onConnect(BLEClient *pClient) {
    printDebug("[EQ3] EQ3_CONNECTED");
    state.connectionState = EQ3_CONNECTED;
}

// -----------------------------------------------------------------------------
// --[onDisconnect]-------------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::onDisconnect(BLEClient *pClient) {
    if(state.connectionState == EQ3_DISCONNECTING){
        printDebug("[EQ3] EQ3_DISCONNECTED");
        state.connectionState = EQ3_DISCONNECTED;
    } else {
        connect();
    }
    recvFragments.clear();
    sendQueue = std::queue<eQ3Message::MessageFragment>(); // clear queue
    sendChar = recvChar = nullptr;
}

// -----------------------------------------------------------------------------
// --[onTick]-------------------------------------------------------------------
// -----------------------------------------------------------------------------
bool eQ3::onTick() {
    long now = millis();
    if ((state.connectionState > EQ3_CONNECTING || now - lastActivity > RECONNECT_INTERVALL) && state.connectionState != EQ3_DISCONNECTED){
        lastActivity = now;
        //if (xSemaphoreTake(mutex, 0)) {
            if (state.connectionState == EQ3_CONNECTING) {
                bleClient->connect(NimBLEAddress(address));
            } else if (state.connectionState == EQ3_CONNECTED) {
                NimBLERemoteService *comm;
                comm = bleClient->getService(BLEUUID(BLE_UUID_SERVICE));
                sendChar = comm->getCharacteristic(BLEUUID(BLE_UUID_WRITE)); // write buffer characteristic
                recvChar = comm->getCharacteristic(BLEUUID(BLE_UUID_READ)); // read buffer characteristic
                if(recvChar->canNotify()) {
                    if(recvChar->subscribe(true, &notify_func)) {
                        printDebug("[EQ3] Subscribe succeeded");
                        state.connectionState = EQ3_SUBSCRIBED;
                    } else {
                        /** Disconnect if subscribe failed */
                        printDebug("[EQ3] Subscribe failed");
                        //bleClient->disconnect();
                        return false;
                    }
                }
            } else if (state.connectionState == EQ3_SUBSCRIBED){
                state.connectionState = EQ3_EXCHANGING_NONCES;
                exchangeNonces();
            } else if (state.connectionState >= EQ3_CONNECTED) {
                sendNextFragment();
            } else if (state.connectionState == EQ3_DISCONNECTING){

            }
            // TODO disconnect if no answer for long time?
            /* if (state.connectionState >= EQ3_CONNECTED && difftime(lastActivity, time(NULL)) > EQ3_LOCK_TIMEOUT && sendQueue.empty()) {
                printDebug("# Lock timeout");
                bleClient->disconnect();
            }*/
            //xSemaphoreGive(mutex);
        //}
    }
    return true;
}

// -----------------------------------------------------------------------------
// --[setOnStatusChange]--------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::setOnStatusChange(std::function<void(LockStatus)> cb) {
    onStatusChange = cb;
}

// -----------------------------------------------------------------------------
// --[exchangeNonces]-----------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::exchangeNonces() {
    state.local_session_nonce.clear();
    for (int i = 0; i < 8; i++) {
        state.local_session_nonce.append(1,esp_random());
    }
    auto *msg = new eQ3Message::Connection_Request_Message;
    printDebug("[EQ3] Exchanging Nonces...");
    sendMessage(msg);
}

// -----------------------------------------------------------------------------
// --[connect]------------------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::connect() {
    if(!bleClient->isConnected()){
        state.connectionState = EQ3_CONNECTING;
        printDebug("[EQ3] EQ3_CONNECTING");
    }
}

void eQ3::disconnect() {
    if(bleClient->isConnected()){
        printDebug("[EQ3] EQ3_DISCONNECTING");
        state.connectionState = EQ3_DISCONNECTING;
        bleClient->disconnect();
    } else {
        state.connectionState = EQ3_DISCONNECTED;
        printDebug("[EQ3] EQ3_DISCONNECTED");
    }
}

bool eQ3::isConnected() {
    return state.connectionState >= EQ3_NONCES_EXCHANGED;
}

bool eQ3::isPaired() {
    return state.connectionState == EQ3_PAIRED;
}

// -----------------------------------------------------------------------------
// --[sendMessage]--------------------------------------------------------------
// -----------------------------------------------------------------------------
bool eQ3::sendMessage(eQ3Message::Message *msg) {
    std::string data;
    if (msg->isSecure()) {
        string padded_data;
        padded_data.append(msg->encode(&state));
        int pad_to = generic_ceil(padded_data.length(), 15, 8);
        if (pad_to > padded_data.length())
            padded_data.append(pad_to - padded_data.length(), 0);
        crypt_data(padded_data, msg->id, state.remote_session_nonce, state.local_security_counter, state.user_key);
        data.append(1, msg->id);
        data.append(crypt_data(padded_data, msg->id, state.remote_session_nonce, state.local_security_counter, state.user_key));
        data.append(1, (char) (state.local_security_counter >> 8));
        data.append(1, (char) state.local_security_counter);
        data.append(compute_auth_value(padded_data, msg->id, state.remote_session_nonce, state.local_security_counter, state.user_key));
        state.local_security_counter++;
    } else {
        data.append(1, msg->id);
        data.append(msg->encode(&state));
    }
    // fragment
    int chunks = data.length() / 15;
    if (data.length() % 15 > 0)
        chunks += 1;
    for (int i = 0; i < chunks; i++) {
        eQ3Message::MessageFragment frag;
        frag.data.append(1, (i ? 0 : 0x80) + (chunks - 1 - i)); // fragment status byte
        frag.data.append(data.substr(i * 15, 15));
        if (frag.data.length() < 16)
            frag.data.append(16 - (frag.data.length() % 16), 0);  // padding
        printfDebug("[EQ3] Enqueue Fragment (%d/%d): %s\n", i+1, chunks, string_to_hex(frag.data).c_str());
        sendQueue.push(frag);
    }
    free(msg);
    return true;
}

// -----------------------------------------------------------------------------
// --[onNotify]-----------------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::onNotify(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    //xSemaphoreTake(mutex, SEMAPHORE_WAIT_TIME);
    eQ3Message::MessageFragment frag;
    frag.data = std::string((char *) pData, length);
    recvFragments.push_back(frag);
    printfDebug("[EQ3] Received Fragment: %s\n", string_to_hex(frag.data).c_str());
    if (frag.isLast()) {
        if (!sendQueue.empty())
            sendQueue.pop();
        // concat message
        std::stringstream ss;
        auto msgtype = recvFragments.front().getType();
        for (auto &received_fragment : recvFragments) {
            ss << received_fragment.getData();
        }
        std::string msgdata = ss.str();
        recvFragments.clear();
        if (eQ3Message::Message::isTypeSecure(msgtype)) {
            auto msg_security_counter = static_cast<uint16_t>(msgdata[msgdata.length() - 6]);
            msg_security_counter <<= 8;
            msg_security_counter += msgdata[msgdata.length() - 5];
            //printDebug((int)msg_security_counter);
            if (msg_security_counter <= state.remote_security_counter) {
                printfDebug("[EQ3] Msg. security counter: %d\n", msg_security_counter);
                printfDebug("[EQ3] Remote Security counter: %d\n", state.remote_security_counter);
                printDebug("[EQ3] Security counter mismatch!");
                //xSemaphoreGive(mutex);
                return;
            }
            state.remote_security_counter = msg_security_counter;
            string msg_auth_value = msgdata.substr(msgdata.length() - 4, 4);
            printfDebug("[EQ3] Auth value: %s\n", string_to_hex(msg_auth_value).c_str());
            //std::string decrypted = crypt_data(msgdata.substr(0, msgdata.length() - 6), msgtype,
            std::string decrypted = crypt_data(msgdata.substr(1, msgdata.length() - 7), msgtype, state.local_session_nonce, state.remote_security_counter, state.user_key);
            printfDebug("[EQ3] Encrypted: %s\n", string_to_hex(msgdata.substr(1, msgdata.length() - 7)).c_str());
            std::string computed_auth_value = compute_auth_value(decrypted, msgtype, state.local_session_nonce, state.remote_security_counter, state.user_key);
            if (msg_auth_value != computed_auth_value) {
                printDebug("[EQ3] Auth value mismatch!");
                //xSemaphoreGive(mutex);
                return;
            }
            msgdata = decrypted;
            printfDebug("[EQ3] Decrypted: %s\n",string_to_hex(msgdata).c_str());
            state.connectionState = EQ3_PAIRED;
        }

        switch (msgtype) {
            case 0: {
                // fragment ack, remove first
                printDebug("[EQ3] Fragment ACK");
                if (!sendQueue.empty()){
                    //sendQueue.pop();
                }
                //xSemaphoreGive(mutex);
                return;
            }

            case 0x81: // answer with security
                printDebug("[EQ3] Pairing succeeded!");
                break;

            case 0x01: // answer without security
                // TODO report error
                printDebug("[EQ3] Error!");
                break;

            case 0x05: {
                // status changed notification
                printDebug("[EQ3] Status changed notification");
                auto * message = new eQ3Message::StatusRequestMessage;
                sendMessage(message);
                break;
            }

            case 0x03: {
                // connection info message
                eQ3Message::Connection_Info_Message message;
                message.data = msgdata;
                state.user_id = message.getUserId();
                state.remote_session_nonce = message.getRemoteSessionNonce();
                assert(state.remote_session_nonce.length() == 8);
                state.local_security_counter = 1;
                state.remote_security_counter = 0;
                state.connectionState = EQ3_NONCES_EXCHANGED;

                printDebug("[EQ3] Nonces exchanged!");
                
                updateInfo();
                //xSemaphoreGive(mutex);
                return;
            }

            case 0x83: {
                // status info
                eQ3Message::Status_Info_Message message;
                message.data = msgdata;
                printfDebug("[EQ3] New state: %d\n", message.getLockStatus());
                _LockStatus = message.getLockStatus();  
                raw_data = message.data;
                if(onStatusChange != nullptr){
                    onStatusChange(_LockStatus);
                }
                break;
            }

            default:
            /*case 0x8f: */{ // user info
                printDebug("# User info");
                //xSemaphoreGive(mutex);
                return;
            }
        }

    } else {
        // create ack
        printDebug("[EQ3] Sending ACK");
        eQ3Message::FragmentAckMessage ack(frag.getStatusByte());
        sendQueue.push(ack);
    }
    //xSemaphoreGive(mutex);
}

// -----------------------------------------------------------------------------
// --[pairingRequest]-----------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::pairingRequest(std::string cardkey) {
    printDebug("[EQ3] Attempting Pairing");
    
    auto* message = new eQ3Message::PairingRequestMessage();
    
    assert(state.remote_session_nonce.length() == 8);
    assert(state.user_key.length() == 16);
    
    // Append user ID
    message->data.append(1, state.user_id);
    
    std::string card_key = hexstring_to_string(cardkey);

    // Encrypt the pair key using the provided parameters
    std::string encrypted_pair_key = crypt_data(state.user_key, 0x04, state.remote_session_nonce, state.local_security_counter, card_key);

    // Pad the encrypted pair key to 22 bytes if necessary
    encrypted_pair_key.append(22 - encrypted_pair_key.length(), '\0');
    assert(encrypted_pair_key.length() == 22);

    // Append the encrypted pair key
    message->data.append(encrypted_pair_key);

    // Append the security counter (2 bytes)
    message->data.append(1, static_cast<char>(state.local_security_counter >> 8));
    message->data.append(1, static_cast<char>(state.local_security_counter & 0xFF));

    // Prepare the data for authentication
    std::string extra;
    extra.append(1, state.user_id);
    extra.append(state.user_key);
    extra.append(23 - extra.length(), '\0');  // Ensure length of 23 bytes
    assert(extra.length() == 23);

    // Compute and append the authentication value
    std::string auth_value = compute_auth_value(extra, 0x04, state.remote_session_nonce, state.local_security_counter, card_key);
    message->data.append(auth_value);

    assert(message->data.length() == 29);

    sendMessage(message);
}

// -----------------------------------------------------------------------------
// --[sendNextFragment]---------------------------------------------------------
// -----------------------------------------------------------------------------
void eQ3::sendNextFragment() {
    if (sendQueue.empty())
        return;
    if (sendQueue.front().sent && std::difftime(sendQueue.front().timeSent, std::time(NULL)) < 5)
        return;
    sendQueue.front().sent = true;
    string data = sendQueue.front().data;
    sendQueue.front().timeSent = std::time(NULL);
    printfDebug("[EQ3] Sending Fragment: %s\n", string_to_hex(data).c_str());
    assert(data.length() == 16);
    sendChar->writeValue((uint8_t *) (data.c_str()), 16, true);
}

void eQ3::sendCommand(CommandType command) {
    auto msg = new eQ3Message::CommandMessage(command);
    sendMessage(msg);
}

void eQ3::unlock() {
    sendCommand(EQ3_UNLOCK);
}

void eQ3::lock() {
    sendCommand(EQ3_LOCK);
}

void eQ3::open() {
    sendCommand(EQ3_OPEN);
}

void eQ3::updateInfo() {
    //xSemaphoreTake(mutex, SEMAPHORE_WAIT_TIME);
    _RSSI = bleClient->getRssi ();
    printfDebug("[EQ3] Got RSSI: %d dBm\n", _RSSI);
    auto * message = new eQ3Message::StatusRequestMessage;
    sendMessage(message);
    //xSemaphoreGive(mutex);
}
