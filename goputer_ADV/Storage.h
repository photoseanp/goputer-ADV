#pragma once
#include "SaveCodec.h"
namespace Storage {
bool begin();
bool available();
bool save(const Go::Game&,const Save::Settings&);
bool load(Go::Game&,Save::Settings&);
}
