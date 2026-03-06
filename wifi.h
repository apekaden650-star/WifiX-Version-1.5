#ifndef wifi_h
#define wifi_h

#include "Arduino.h"
#include <ESP8266WebServer.h>

namespace wifi {
    void begin();
    void update();
    void startAP();
    void stopAP();
    // Tambahkan baris ini supaya file .ino lu kenal fungsinya
    ESP8266WebServer* getWebServer(); 
}

#endif
