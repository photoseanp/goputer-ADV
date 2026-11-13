#include "Storage.h"
#include <Preferences.h>
#include <string.h>
namespace Storage {
static Preferences prefs;
static bool ready=false;
static uint8_t buffer[Save::MaxBytes],verify[Save::MaxBytes];
static const char* keys[2]={"slot0","slot1"};
static bool read(int slot,uint8_t* out,size_t& len,uint32_t& seq) {
    len=prefs.getBytesLength(keys[slot]);
    return len>=70&&len<=Save::MaxBytes&&prefs.getBytes(keys[slot],out,len)==len&&Save::inspect(out,len,seq);
}
static int latest(uint32_t& seq) {
    uint32_t a=0,b=0;size_t len=0;bool va=read(0,buffer,len,a),vb=read(1,verify,len,b);
    if(!va&&!vb) {seq=0;return -1;}
    if(va&&(!vb||int32_t(a-b)>0)) {seq=a;return 0;}
    seq=b;return 1;
}
bool begin() {ready=prefs.begin("cardputer-go",false);return ready;}
bool available() {uint32_t seq;return ready&&latest(seq)>=0;}
bool save(const Go::Game& g,const Save::Settings& s) {
    if(!ready) return false;
    uint32_t seq;int active=latest(seq),target=active<0?0:1-active;
    size_t len=Save::encode(g,s,seq+1,buffer,sizeof buffer);if(!len) return false;
    if(prefs.putBytes(keys[target],buffer,len)!=len) return false;
    size_t got=0;uint32_t check=0;
    return read(target,verify,got,check)&&got==len&&check==seq+1&&!memcmp(buffer,verify,len);
}
bool load(Go::Game& g,Save::Settings& s) {
    if(!ready) return false;
    uint32_t seq;int newest=latest(seq);if(newest<0) return false;
    for(int t=0;t<2;++t) {
        size_t len;uint32_t sequence;int slot=t?1-newest:newest;
        if(read(slot,buffer,len,sequence)&&Save::decode(buffer,len,g,s)) return true;
    }
    return false;
}
}
