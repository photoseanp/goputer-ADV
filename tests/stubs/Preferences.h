#pragma once
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <string.h>
struct NVSStub {
    static std::map<std::string,std::vector<uint8_t>> values;
    static bool failWrite,failRead;
    static int writes;
};
class Preferences {
public:
    bool begin(const char*,bool) {return true;}
    size_t getBytesLength(const char* k) {return NVSStub::values[k].size();}
    size_t getBytes(const char* k,void* out,size_t cap) {
        auto& v=NVSStub::values[k];if(NVSStub::failRead||cap<v.size()) return 0;
        memcpy(out,v.data(),v.size());return v.size();
    }
    size_t putBytes(const char* k,const void* data,size_t len) {
        if(NVSStub::failWrite) return 0;
        const auto* p=static_cast<const uint8_t*>(data);NVSStub::values[k]=std::vector<uint8_t>(p,p+len);++NVSStub::writes;return len;
    }
};
