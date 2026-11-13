#include "SaveCodec.h"
#include <string.h>
namespace Save {
static uint16_t get16(const uint8_t* p) {return uint16_t(p[0])|(uint16_t(p[1])<<8);}
static uint32_t get32(const uint8_t* p) {return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
static void put16(uint8_t* p,uint16_t n) {p[0]=n&255;p[1]=n>>8;}
static void put32(uint8_t* p,uint32_t n) {for(int k=0;k<4;++k) p[k]=(n>>(k*8))&255;}
uint32_t crc32(const uint8_t* p,size_t size) {
    uint32_t c=0xffffffff;
    while(size--) {c^=*p++;for(int b=0;b<8;++b) c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}
    return ~c;
}
size_t encode(const Go::Game& g,const Settings& s,uint32_t seq,uint8_t* out,size_t cap) {
    size_t size=20+size_t(g.ply())*2+46+4;if(size>cap) return 0;
    memset(out,0,size);memcpy(out,"CPG2",4);out[4]=1;out[5]=uint8_t(g.size());
    out[6]=(s.againstBot?1:0)|(s.finished?2:0)|(s.zoomed?4:0);out[7]=s.human;
    out[8]=s.level;out[9]=s.cursorX;out[10]=s.cursorY;out[11]=13; // 6.5 komi
    put16(out+12,uint16_t(g.ply()));put32(out+14,seq); // 18,19 reserved
    for(int i=0;i<g.ply();++i) put16(out+20+i*2,g.moves()[i]);
    uint8_t* dead=out+20+g.ply()*2;
    for(int i=0;i<g.points();++i) if(g.dead(i)) dead[i/8]|=uint8_t(1u<<(i%8));
    put32(out+size-4,crc32(out,size-4));return size;
}
bool inspect(const uint8_t* p,size_t size,uint32_t& seq) {
    if(size<70||size>MaxBytes||memcmp(p,"CPG2",4)||p[4]!=1||!Go::supportedSize(p[5])) return false;
    if((p[6]&~7)||p[7]<1||p[7]>2||p[8]>3||p[9]>=p[5]||p[10]>=p[5]||p[11]!=13||p[18]||p[19]) return false;
    int count=get16(p+12);if(count>Go::MaxTurns||size!=70+size_t(count)*2) return false;
    if(get32(p+size-4)!=crc32(p,size-4)) return false;
    int points=p[5]*p[5];
    for(int i=0;i<count;++i) {uint16_t m=get16(p+20+i*2);if(m!=Go::PassMove&&m>=points) return false;}
    const uint8_t* dead=p+20+count*2;
    for(int i=points;i<368;++i) if(dead[i/8]&(1u<<(i%8))) return false;
    seq=get32(p+14);return true;
}
bool decode(const uint8_t* p,size_t size,Go::Game& g,Settings& settings) {
    uint32_t seq;if(!inspect(p,size,seq)) return false;
    g.reset(p[5]);int count=get16(p+12);
    for(int i=0;i<count;++i) {
        uint16_t m=get16(p+20+i*2);
        Go::Error e=m==Go::PassMove?g.pass():g.play(m);
        if(e!=Go::Error::Ok) {g.reset();return false;}
    }
    const uint8_t* dead=p+20+count*2;
    for(int i=0;i<g.points();++i) if(dead[i/8]&(1u<<(i%8))) {
        if(!g.reviewing()||!g.position().board[i]) {g.reset();return false;}
        if(!g.dead(i)) g.toggleDead(i);
    }
    // Reject partial-group flags and inconsistent finished status.
    for(int i=0;i<g.points();++i) if(g.dead(i)!=bool(dead[i/8]&(1u<<(i%8)))) {g.reset();return false;}
    if((p[6]&2)&&!g.reviewing()) {g.reset();return false;}
    Settings s;s.againstBot=p[6]&1;s.finished=p[6]&2;s.zoomed=p[6]&4;
    s.human=p[7];s.level=p[8];s.cursorX=p[9];s.cursorY=p[10];settings=s;return true;
}
}
