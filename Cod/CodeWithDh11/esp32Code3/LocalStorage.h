#ifndef LOCAL_STORAGE_H
#define LOCAL_STORAGE_H
#include <Arduino.h>
#include <vector>
#include <Preferences.h>

struct LocalData {
    String deviceID;
    float temperature;
    float humidity;
    time_t timestamp;
};

class LocalStorage {
public:
    LocalStorage() { prefs.begin("localdata", false); }
    ~LocalStorage() { prefs.end(); }

    void saveData(const LocalData& data) {
        String key = String(prefs.getUInt("count", 0));
        String value = data.deviceID + "," + String(data.temperature, 2) + "," + String(data.humidity, 2) + "," + String((unsigned long)data.timestamp);
        prefs.putString(key.c_str(), value);
        prefs.putUInt("count", prefs.getUInt("count", 0) + 1);
    }

    std::vector<LocalData> getAllData() {
        std::vector<LocalData> result;
        uint32_t count = prefs.getUInt("count", 0);
        for (uint32_t i = 0; i < count; i++) {
            String value = prefs.getString(String(i).c_str(), "");
            if (value.length() > 0) {
                LocalData data;
                int idx1 = value.indexOf(",");
                int idx2 = value.indexOf(",", idx1 + 1);
                int idx3 = value.indexOf(",", idx2 + 1);
                data.deviceID = value.substring(0, idx1);
                data.temperature = value.substring(idx1 + 1, idx2).toFloat();
                data.humidity = value.substring(idx2 + 1, idx3).toFloat();
                data.timestamp = (time_t)value.substring(idx3 + 1).toInt();
                result.push_back(data);
            }
        }
        return result;
    }

    void clearData() {
        uint32_t count = prefs.getUInt("count", 0);
        for (uint32_t i = 0; i < count; i++) {
            prefs.remove(String(i).c_str());
        }
        prefs.putUInt("count", 0);
    }
private:
    Preferences prefs;
};

#endif // LOCAL_STORAGE_H
