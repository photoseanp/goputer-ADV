#include "../goputer_ADV/App.cpp"
#include "stubs/Preferences.h"
#include <assert.h>
std::map<std::string,std::vector<uint8_t>> NVSStub::values;
bool NVSStub::failWrite=false,NVSStub::failRead=false;
int NVSStub::writes=0;
static void drawCheck() {draw();assert(surface().bad==0);}
static void botDone() {int safety=0;while(botTurn()) {appLoop();assert(++safety<50000);}}
int main() {
    appSetup();assert(M5Cardputer.Display.rotation==0);assert(!hasSave);drawCheck();
    for(int s=0;s<3;++s) {
        sizeIndex=s;human=Go::Black;againstBot=true;botLevel=1;startGame();drawCheck();
        assert(game.size()!=5&&Storage::available());
        // Hoshi draw positions checked in overview and in EVERY zoom viewport.
        for(int z=0;z<2;++z) for(int y=0;y<game.size();++y) for(int x=0;x<game.size();++x) {
            zoomed=bool(z);cursorX=x;cursorY=y;drawCheck();int n=game.size(),v=zoomed&&n>9?9:n;
            int vx=constrain(x-v/2,0,n-v),vy=constrain(y-v/2,0,n-v),step=110/(v-1),count=0;
            if(v==n) vx=vy=0;
            for(int yy=vy;yy<vy+v;++yy) for(int xx=vx;xx<vx+v;++xx) if(Go::starPoint(n,xx,yy)) {
                ++count;bool found=false;
                for(auto c:surface().circles) if(c.x==18+(xx-vx)*step&&c.y==19+(yy-vy)*step&&c.color==0) found=true;
                assert(found);
            }
            assert(int(surface().circles.size())==count);
        }
        cursorX=cursorY=0;handle(Enter);assert(game.ply()==1);botDone();assert(game.ply()==2);
        Save::Settings saved;assert(Storage::load(game,saved));assert(saved.cursorX==0&&saved.human==Go::Black);
        handle(Undo);assert(game.ply()==0);handle(Pass);botDone();assert(game.position().turn==human||game.reviewing());
        cancelBot();game.pass();if(!game.reviewing()) game.pass();assert(game.reviewing());
        drawCheck();handle(Enter);assert(finished);drawCheck();handle(Resume);assert(!finished&&!game.reviewing());
        handle(SaveNow);int last=game.ply();loadGame();assert(game.ply()==last&&!inMenu);
        handle(NewGame);drawCheck();handle(Yes);assert(inMenu);menuRow=4;handle(Enter);assert(dialog==4);drawCheck();handle(Undo);assert(!dialog);
        menuRow=5;handle(Enter);assert(!inMenu);drawCheck();
        human=Go::White;startGame();botDone();assert(game.ply()==1&&game.position().turn==Go::White);
    }
    // Real Storage.cpp with a simulated power reboot and corrupted latest slot.
    sizeIndex=0;againstBot=false;startGame();game.play(0);persist();int one=game.ply();
    game.play(1);persist();auto original=NVSStub::values;
    uint32_t a,b;Save::inspect(NVSStub::values["slot0"].data(),NVSStub::values["slot0"].size(),a);
    Save::inspect(NVSStub::values["slot1"].data(),NVSStub::values["slot1"].size(),b);
    std::string newest=int32_t(a-b)>0?"slot0":"slot1";
    NVSStub::values[newest][4]^=1;game.reset();loadGame();assert(game.ply()==one);
    NVSStub::values=original;loadGame();assert(game.ply()==2);
    NVSStub::failWrite=true;game.play(2);assert(!persist());assert(strstr(notice,"ERROR"));
    NVSStub::failWrite=false;loadGame();assert(game.ply()==2);
    // Truncation behaves like an interrupted write: previous completed snapshot survives.
    NVSStub::values=original;NVSStub::values[newest].resize(23);loadGame();assert(game.ply()==one);
    NVSStub::values=original;loadGame();assert(game.ply()==2);
    // A failed user-requested load must leave the active board untouched.
    game.play(2);int activePly=game.ply();NVSStub::failRead=true;
    loadGame();assert(game.ply()==activePly&&activeGame);assert(!Storage::available());NVSStub::failRead=false;
    // Every mutation commits, but cursor-only movement does not wear flash.
    int writes=NVSStub::writes;handle(Right);assert(NVSStub::writes==writes);
    handle(Undo);assert(NVSStub::writes==writes+1);
    showHelp=true;drawCheck();showHelp=false;
    for(int d=1;d<=4;++d) {dialog=d;drawCheck();}dialog=0;
    inMenu=true;for(int row=0;row<6;++row) {menuRow=row;drawCheck();}
    handle(Three);assert(sizeIndex==2);menuRow=4;handle(Enter);handle(Yes);assert(game.size()==19);
    int before=cursorX;M5Cardputer.Keyboard.pressed={'d'};readKeys(0);assert(cursorX==(before+1)%19);
    readKeys(100);assert(cursorX==(before+1)%19);readKeys(330);assert(cursorX==(before+2)%19);
    puts("PASS UI: all board/zoom/cursor geometry, global hoshi positions, rotation 0, menus, undo, autosave, restore, dual-slot corruption fallback, write errors");
}
