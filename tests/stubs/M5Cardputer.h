#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <set>
#include <vector>
#include <string>
#define SET_LOOP_TASK_STACK_SIZE(x)
template<class T> T constrain(T x,T a,T b) {return std::max(a,std::min(x,b));}
static uint32_t fakeMicros=0;
inline uint32_t millis() {return fakeMicros/1000;}
inline uint32_t micros() {return fakeMicros+=100;}
inline void delay(int ms) {fakeMicros+=uint32_t(ms)*1000;}
namespace lgfx {
class LGFXBase {
public:
    struct Circle {int x,y,r;uint16_t color;};
    struct Label {int x,y;std::string text;};
    std::vector<Circle> circles;std::vector<Label> labels;
    int bad=0,cx=0,cy=0,rotation=-1;
    void bounds(int x,int y,int w,int h) {if(x<0||y<0||w<0||h<0||x+w>240||y+h>135) ++bad;}
    void setCursor(int x,int y) {cx=x;cy=y;}
    void print(const char* t) {labels.push_back({cx,cy,t});bounds(cx,cy,int(std::string(t).size())*6,8);}
    void fillCircle(int x,int y,int r,uint16_t color) {circles.push_back({x,y,r,color});bounds(x-r,y-r,2*r+1,2*r+1);}
    void drawCircle(int x,int y,int r,uint16_t) {bounds(x-r,y-r,2*r+1,2*r+1);}
    void fillRect(int x,int y,int w,int h,uint16_t) {bounds(x,y,w,h);}
    void drawRect(int x,int y,int w,int h,uint16_t) {bounds(x,y,w,h);}
    void drawLine(int x,int y,int xx,int yy,uint16_t) {bounds(std::min(x,xx),std::min(y,yy),abs(xx-x)+1,abs(yy-y)+1);}
    void fillScreen(uint16_t) {bad=0;circles.clear();labels.clear();}
    void setTextColor(uint16_t) {}
    void setTextFont(int) {}
    void setTextSize(int) {}
    void setTextWrap(bool) {}
    void setRotation(int n) {rotation=n;}
    void setBrightness(int) {}
};
}
struct FakeKeyboard {
    struct State {bool enter=false,del=false;};State state;std::set<char> pressed;
    State keysState() {return state;}
    bool isKeyPressed(char c) {return pressed.count(c)!=0;}
};
struct FakeM5 {
    lgfx::LGFXBase Display;FakeKeyboard Keyboard;
    int config() {return 0;}
    void begin(int,bool) {}
    void update() {}
};
static FakeM5 M5Cardputer,M5;
class M5Canvas:public lgfx::LGFXBase {
public:
    explicit M5Canvas(lgfx::LGFXBase*) {}
    void setColorDepth(int) {}
    void* createSprite(int,int) {return this;}
    void pushSprite(int,int) {}
};
struct FakeESP {int restarts=0;void restart() {++restarts;}};
static FakeESP ESP;
