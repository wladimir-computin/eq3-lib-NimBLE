/*
* CryptoIoT - PrintDebug
* 
* Debugging macro.
* If DEBUG is set to 1, printDebug() calls in this project get translated to Serial.println(). Otherwise they will be ignored by the compiler.
* (At least I hope while(false) will be ignored by the compiler, otherwise the compiler is dump)
*/

#pragma once

#include <Arduino.h>

#if DEBUG == 1
	#define printDebug(x) Serial.println(x)
	#define printfDebug(x...) Serial.printf(x)
#else
	#define printDebug(x) while(false)
	#define printfDebug(x...) while(false)
#endif
