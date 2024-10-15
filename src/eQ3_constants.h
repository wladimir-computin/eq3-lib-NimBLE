//
// Created by marius on 12/17/18.
//

#ifndef DOOR_OPENER_CONST_H
#define DOOR_OPENER_CONST_H

#include <string>

typedef enum {
    EQ3_DISCONNECTED = 0,
    EQ3_DISCONNECTING = 1,
    EQ3_SCANNING = 2,
    EQ3_FOUND = 3,
    EQ3_CONNECTING = 4,
    EQ3_CONNECTED = 5,
    EQ3_SUBSCRIBED = 6,
    EQ3_EXCHANGING_NONCES = 7,
    EQ3_NONCES_EXCHANGED = 8,
    EQ3_PAIRED = 9
} ConnectionState;

typedef struct {
    uint8_t user_id;
    std::string user_key;
    std::string local_session_nonce;
    std::string remote_session_nonce;
    uint16_t local_security_counter = 1;
    uint16_t remote_security_counter = 0;
    ConnectionState connectionState = EQ3_DISCONNECTED;
} ClientState;

typedef enum {
    EQ3_LOCK = 0,
    EQ3_UNLOCK = 1,
    EQ3_OPEN = 2
} CommandType;

typedef enum {
    EQ3_STATUSPENDING = -1,
    EQ3_UNKNOWN = 0,
    EQ3_MOVING = 1,
    EQ3_UNLOCKED = 2,
    EQ3_LOCKED = 3,
    EQ3_OPENED = 4,
    EQ3_TIMEOUT = 9
} LockStatus;


static std::string ConnectionStateToString(ConnectionState in){
  switch(in){
    case EQ3_DISCONNECTED:
      return "DISCONNECTED";
    case EQ3_DISCONNECTING:
      return "DISCONNECTING";
    case EQ3_SCANNING:
      return "SCANNING";
    case EQ3_FOUND:
      return "FOUND";
    case EQ3_CONNECTING:
      return "CONNECTING";
    case EQ3_CONNECTED:
      return "CONNECTED";
    case EQ3_SUBSCRIBED:
      return "SUBSCRIBED";
    case EQ3_EXCHANGING_NONCES:
      return "EXCHANGING_NONCES";
    case EQ3_NONCES_EXCHANGED:
      return "NONCES_EXCHANGED";
    case EQ3_PAIRED:
      return "PAIRED";
    default:
      return "DISCONNECTED";
  }
}

static std::string lockstatusToString(LockStatus in){
  switch(in){
    case EQ3_STATUSPENDING:
      return "STATUSPENDING";
    case EQ3_UNKNOWN:
      return "UNKNOWN";
    case EQ3_MOVING:
      return "MOVING";
    case EQ3_UNLOCKED:
      return "UNLOCKED";
    case EQ3_LOCKED:
      return "LOCKED";
    case EQ3_OPENED:
      return "OPENED";
    case EQ3_TIMEOUT:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

#endif //DOOR_OPENER_CONST_H
