#pragma once
#include "GoEngine.h"
#include <stddef.h>
namespace Save {
constexpr size_t MaxBytes=32+Go::MaxTurns*2+46;
struct Settings {
    bool againstBot=true,finished=false,zoomed=false;
    uint8_t human=Go::Black,level=1,cursorX=4,cursorY=4;
};
uint32_t crc32(const uint8_t* data,size_t size);
size_t encode(const Go::Game&,const Settings&,uint32_t sequence,uint8_t* out,size_t cap);
bool inspect(const uint8_t*,size_t,uint32_t& sequence);
bool decode(const uint8_t*,size_t,Go::Game&,Settings&);
}
