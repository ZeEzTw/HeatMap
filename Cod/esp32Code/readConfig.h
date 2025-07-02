#ifndef READ_CONFIG_H
#define READ_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

// Variabile externe care vor fi completate de funcție
extern String deviceID;
extern String ssid;
extern String password;
extern String influx_url;
extern String influx_token;
extern String influx_org;
extern String influx_bucket;
extern String mqtt_broker_ip;
extern int mqtt_broker_port;
extern int delaySeconds;

class ConfigManager {
private:
    Preferences prefs;
    const char* namespaceName = "config";

public:
    void begin() {
        prefs.begin(namespaceName, false);
        // Daca nu exista, seteaza valorile default
        if (!prefs.isKey("deviceID")) {
            setDefaults();
        }
        load();
    }

    void end() {
        prefs.end();
    }

    void setDefaults() {
        prefs.putString("deviceID", "sensor_lab_3");
        prefs.putString("ssid", "hotspot");
        prefs.putString("password", "123456789");
        prefs.putString("influx_url", "https://eu-central-1-1.aws.cloud2.influxdata.com");
        prefs.putString("influx_token", "j60HrDCjeKlEyAc4m_GrYpFNjIpX--Jv9UX1v7qYtZxdyXfyuPwh_dqLl_bbJCDPi8hk-gJn_dksyh2eE11eug==");
        prefs.putString("influx_org", "Eli-np");
        prefs.putString("influx_bucket", "temperature%20and%20humidity%20data");
        prefs.putString("mqtt_broker_ip", "192.168.239.190");
        prefs.putInt("mqtt_broker_port", 1883);
        prefs.putInt("delaySeconds", 10);
    }

    void load() {
        deviceID      = prefs.getString("deviceID", "sensor_lab_3");
        ssid          = prefs.getString("ssid", "hotspot");
        password      = prefs.getString("password", "123456789");
        influx_url    = prefs.getString("influx_url", "");
        influx_token  = prefs.getString("influx_token", "");
        influx_org    = prefs.getString("influx_org", "");
        influx_bucket = prefs.getString("influx_bucket", "");
        mqtt_broker_ip = prefs.getString("mqtt_broker_ip", "");
        mqtt_broker_port = prefs.getInt("mqtt_broker_port", 1883);
        delaySeconds  = prefs.getInt("delaySeconds", 10);
    }

    // Funcții pentru modificare la runtime, dacă vrei
    void setDeviceID(const String& val)        { prefs.putString("deviceID", val); deviceID = val; }
    void setSSID(const String& val)            { prefs.putString("ssid", val); ssid = val; }
    void setPassword(const String& val)        { prefs.putString("password", val); password = val; }
    void setInfluxURL(const String& val)       { prefs.putString("influx_url", val); influx_url = val; }
    void setInfluxToken(const String& val)     { prefs.putString("influx_token", val); influx_token = val; }
    void setInfluxOrg(const String& val)       { prefs.putString("influx_org", val); influx_org = val; }
    void setInfluxBucket(const String& val)    { prefs.putString("influx_bucket", val); influx_bucket = val; }
    void setMQTTBrokerIP(const String& val)    { prefs.putString("mqtt_broker_ip", val); mqtt_broker_ip = val; }
    void setMQTTBrokerPort(int val)             { prefs.putInt("mqtt_broker_port", val); mqtt_broker_port = val; }
    void setDelaySeconds(int val)                { prefs.putInt("delaySeconds", val); delaySeconds = val; }
};

// Declarații variabile externe (definite în alt .cpp)
extern String deviceID;
extern String ssid;
extern String password;
extern String influx_url;
extern String influx_token;
extern String influx_org;
extern String influx_bucket;
extern String mqtt_broker_ip;
extern int mqtt_broker_port;
extern int delaySeconds;

#endif
