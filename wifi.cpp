/* This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#include "wifi.h"

extern "C" {
    #include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <EEPROM.h>
#include "Accesspoints.h"
#include "language.h"
#include "debug.h"
#include "settings.h"
#include "repeater.h"
#include "CLI.h"
#include "Attack.h"
#include "Scan.h"
#include "DisplayUI.h"

String MAC_;
String nama;
char eviltwinpath[32];
char pishingpath[32];
File webportal;
String tes_password;
String data_pishing;
String LOG;
String json_data;
bool hidden_target = false;
bool rogueap_continues = false;

extern bool progmemToSpiffs(const char* adr, int len, String path);

#include "webfiles.h"

extern Scan   scan;
extern CLI    cli;
extern Attack attack;

typedef enum wifi_mode_t {
    off = 0,
    ap  = 1,
    st  = 2
} wifi_mode_t;

typedef struct ap_settings_t {
    char    path[33];
    char    ssid[33];
    char    password[65];
    uint8_t channel;
    bool    hidden;
    bool    captive_portal;
} ap_settings_t;

String readFile(fs::FS &fs, const char * path){
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()) return String();
  String fileContent = file.readStringUntil('\n');
  file.close();
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  File file = fs.open(path, "w");
  if(!file) return;
  file.print(message);
  file.close();
}

namespace wifi {
    // ===== PRIVATE ===== //
    wifi_mode_t   mode;
    ap_settings_t ap_settings;
    ESP8266WebServer server(80);
    DNSServer dns;
    IPAddress ip WEB_IP_ADDR;
    IPAddress    netmask(255, 255, 255, 0);

    // --- FUNGSI AKSES SERVER (UNTUK FIX LINKER ERROR) ---
    ESP8266WebServer* getWebServer() {
        return &server;
    }

    void setPath(String path) {
        if (path.charAt(0) != '/') path = '/' + path;
        if (path.length() > 32) debuglnF("ERROR: Path long");
        else strncpy(ap_settings.path, path.c_str(), 32);
    }

    void setSSID(String ssid) {
        if (ssid.length() > 32) debuglnF("ERROR: SSID long");
        else strncpy(ap_settings.ssid, ssid.c_str(), 32);
    }

    void setPassword(String password) {
        if (password.length() > 64) debuglnF("ERROR: Pass long");
        else strncpy(ap_settings.password, password.c_str(), 64);
    }

    void setChannel(uint8_t ch) {
        ap_settings.channel = ch;
    }

    void setHidden(bool hidden) {
        ap_settings.hidden = hidden;
    }

    void setCaptivePortal(bool captivePortal) {
        ap_settings.captive_portal = captivePortal;
    }

    void handleFileList() {
        if (!server.hasArg("dir")) {
            server.send(500, "text/plain", "BAD ARGS");
            return;
        }
        String path = server.arg("dir");
        Dir dir = LittleFS.openDir(path);
        String output = "{";
        bool first = true;
        while (dir.next()) {
            if (first) first = false; else output += ',';
            output += "[\"" + dir.fileName() + "\"]";
        }
        output += "}";
        server.send(200, "application/json", output);
    }

    String getContentType(String filename) {
        if (filename.endsWith(".html")) return "text/html";
        if (filename.endsWith(".css")) return "text/css";
        if (filename.endsWith(".js")) return "application/javascript";
        return "text/plain";
    }
  
    bool handleFileRead(String path) {
        if (path.charAt(0) != '/') path = '/' + path;
        if (path.endsWith("/")) path += "index.html";
        String contentType = getContentType(path);
        if (!LittleFS.exists(path)) return false;
        File file = LittleFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
        return true;
    }

    void sendProgmem(const char* ptr, size_t size, const char* type) {
        server.sendHeader("Content-Encoding", "gzip");
        server.send_P(200, type, ptr, size);
    }

    void begin() {
        setPath("/web");
        setSSID(settings::getAccessPointSettings().ssid);
        setPassword(settings::getAccessPointSettings().password);
        setChannel(settings::getWifiSettings().channel);
        setHidden(settings::getAccessPointSettings().hidden);
        setCaptivePortal(settings::getWebSettings().captive_portal);
        mode = wifi_mode_t::off;
        WiFi.mode(WIFI_OFF);
        wifi_set_macaddr(STATION_IF, (uint8_t*)settings::getWifiSettings().mac_st);
        wifi_set_macaddr(SOFTAP_IF, (uint8_t*)settings::getWifiSettings().mac_ap);
    }

    void startAP() {
        WiFi.softAPConfig(ip, ip, netmask);
        WiFi.softAP(ap_settings.ssid, ap_settings.password, ap_settings.channel, ap_settings.hidden);
        dns.start(53, "*", ip);
        
        // --- ROUTE MASS KILL ---
        server.on("/masskill", []() {
            attack.stop();
            scan.stop();
            accesspoints.removeAll();
            cli.runCommand("scan aps -t 15s");
            cli.runCommand("attack -da");
            server.send(200, "text/plain", "OK");
        });

        // Handler standar lainnya...
        server.on("/", []() {
            if(!handleFileRead("/index.html")) sendProgmem(indexhtml, sizeof(indexhtml), "text/html");
        });

        server.begin();
        mode = wifi_mode_t::ap;
    }

    void stopAP() {
        WiFi.disconnect(true);
        mode = wifi_mode_t::st;
    }

    void update() {
        if (mode != wifi_mode_t::off) {
            server.handleClient();
            dns.processNextRequest();
        }
    }
}
