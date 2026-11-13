#define private public
#include "../goputer_ADV/GoEngine.h"
#undef private
#include "../goputer_ADV/Bot.h"
#include "../goputer_ADV/SaveCodec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <random>
#include <chrono>
#include <memory>
using namespace Go;
static Game g,restored;
static int checks=0;
static bool same(const Position& a,const Position& b) {
    return !memcmp(a.board,b.board,MaxPoints)&&a.turn==b.turn&&a.passes==b.passes&&a.last==b.last&&
        !memcmp(a.captured,b.captured,sizeof a.captured);
}
static void invariants(const Game& game) {
    bool seen[MaxPoints]={};int b=0,w=0;
    for(int p=0;p<game.points();++p) {
        auto c=game.position().board[p];assert(c<=2);++checks;
        if(!c||seen[p]) continue;
        Group gr=groupAt(game.position().board,game.size(),p);assert(gr.liberties>0);++checks;
        for(int j=0;j<gr.count;++j) {assert(gr.stones[j]<game.points());seen[gr.stones[j]]=true;}
    }
    auto s=game.score();for(int p=0;p<game.points();++p) {if(s.owner[p]==Black) ++b;if(s.owner[p]==White) ++w;}
    assert(s.black==b&&s.white==w&&s.black+s.white<=game.points());++checks;
}
static void codecRoundTrip() {
    Save::Settings s;s.level=3;s.human=White;s.cursorX=uint8_t(g.size()-1);s.cursorY=0;
    s.finished=g.reviewing();s.zoomed=true;
    uint8_t bytes[Save::MaxBytes];size_t len=Save::encode(g,s,42,bytes,sizeof bytes);assert(len);
    uint32_t sequence;assert(Save::inspect(bytes,len,sequence)&&sequence==42);
    Save::Settings t;assert(Save::decode(bytes,len,restored,t));
    assert(restored.size()==g.size()&&restored.ply()==g.ply()&&same(restored.position(),g.position()));
    assert(t.human==s.human&&t.level==s.level&&t.finished==s.finished&&t.cursorX==s.cursorX);
    for(int i=0;i<g.points();++i) assert(g.dead(i)==restored.dead(i));
    for(int i=0;i<g.ply();++i) assert(g.moves()[i]==restored.moves()[i]);
    for(size_t i=0;i<len;++i) {bytes[i]^=1;assert(!Save::inspect(bytes,len,sequence));bytes[i]^=1;}
    assert(!Save::decode(bytes,len-1,restored,t));
    ++checks;
}
static void explicitRules() {
    g.reset(9);
    assert(g.play(-1)==Error::Outside&&g.play(81)==Error::Outside);
    assert(g.play(1)==Error::Ok);assert(g.play(0)==Error::Ok);assert(g.play(9)==Error::Ok);
    assert(!g.position().board[0]&&g.position().captured[Black]==1);
    assert(g.play(0)==Error::Suicide);assert(g.play(9)==Error::Occupied);
    assert(g.undo());assert(g.position().board[0]==White);assert(g.play(9)==Error::Ok);
    // Reach a ko using legal moves and alternating remote filler moves.
    g.reset(9);
    int seq[]={1,10,9,2,19,12,80,20,11};
    for(int p:seq) assert(g.play(p)==Error::Ok);
    assert(g.position().board[10]==Empty);assert(g.play(10)==Error::Superko);
    codecRoundTrip();Save::Settings settings;uint8_t bytes[Save::MaxBytes];
    size_t len=Save::encode(g,settings,1,bytes,sizeof bytes);assert(Save::decode(bytes,len,restored,settings));
    assert(restored.play(10)==Error::Superko);
    g.pass();g.pass();assert(g.reviewing());assert(g.play(3)==Error::Review);
    g.toggleDead(1);codecRoundTrip();g.resume();assert(!g.reviewing()&&g.ply()==9);
}
static void hoshi() {
    assert(!supportedSize(5));
    for(int n:{9,13,19}) {
        int count=0;for(int y=0;y<n;++y) for(int x=0;x<n;++x) if(starPoint(n,x,y)) ++count;
        assert(count==(n==19?9:5));
    }
    for(int x:{3,9}) for(int y:{3,9}) assert(starPoint(13,x,y));
    assert(starPoint(13,6,6)&&!starPoint(13,1,1)&&!starPoint(13,2,2));
    for(int x:{3,9,15}) for(int y:{3,9,15}) assert(starPoint(19,x,y));
    assert(!starPoint(19,2,2)&&!starPoint(19,4,4)&&!starPoint(19,19,19));
}
static void limitsAndLargeGroups() {
    g.reset(19);
    for(int i=0;i<359;++i) g.current_.board[i]=White;
    g.current_.board[359]=Black;g.current_.turn=Black;g.remember();
    Group giant=groupAt(g.position().board,19,0);assert(giant.count==359&&giant.liberties==1);
    assert(g.play(360)==Error::Ok);assert(g.position().captured[Black]==359);
    assert(g.position().board[359]==Black&&g.position().board[360]==Black);
    assert(g.undo());assert(g.position().board[358]==White&&g.position().board[360]==Empty);
    g.reset(19);g.ply_=MaxTurns-2;g.remember();
    Position next;assert(g.play(0)==Error::HistoryFull);
    assert(g.pass()==Error::Ok&&g.pass()==Error::Ok&&g.reviewing());
    assert(g.ply()==MaxTurns);g.resume();assert(g.ply()==MaxTurns-2);
    g.reset();g.play(0);Save::Settings settings;uint8_t bytes[Save::MaxBytes];
    size_t length=Save::encode(g,settings,17,bytes,sizeof bytes);
    bytes[20]=0xff;bytes[21]=0xff; // Replace a move by a pass; still legal.
    auto fixCRC=[&]() {uint32_t c=Save::crc32(bytes,length-4);for(int i=0;i<4;++i) bytes[length-4+i]=uint8_t(c>>(8*i));};
    fixCRC();assert(Save::decode(bytes,length,restored,settings));
    bytes[6]|=2;fixCRC();assert(!Save::decode(bytes,length,restored,settings)); // finished without review
    bytes[6]&=~2;bytes[22]=1;fixCRC();assert(!Save::decode(bytes,length,restored,settings)); // dead empty point
}
static int runBot(int level,int maxSteps=30000) {
    Go::Bot bot;bot.begin(g,123,level);Position old=g.position();int ply=g.ply(),steps=0;
    std::vector<uint16_t> moves(g.moves(),g.moves()+ply);
    while(!bot.step(g)) {
        assert(same(old,g.position())&&g.ply()==ply);assert(++steps<maxSteps);
        assert(moves.empty()||!memcmp(moves.data(),g.moves(),moves.size()*2));++checks;
    }
    assert(same(old,g.position())&&g.ply()==ply);
    int p=bot.move();Position next;assert(p<0||g.trial(p,next)==Error::Ok);return p;
}
int main() {
    hoshi();explicitRules();limitsAndLargeGroups();std::mt19937 rng(20261113);
    for(int n:{9,13,19}) {
        for(int run=0;run<5;++run) {
            g.reset(n);
            for(int i=0;i<n*n;++i) {
                std::vector<int> legal;Position trial;
                for(int p=0;p<g.points();++p) if(g.trial(p,trial)==Error::Ok) legal.push_back(p);
                if(legal.empty()) break;
                Position old=g.position();int p=legal[rng()%legal.size()];assert(g.play(p)==Error::Ok);
                Position next=g.position();assert(g.undo()&&same(g.position(),old));
                assert(g.play(p)==Error::Ok&&same(g.position(),next));invariants(g);
                if(i%40==0) codecRoundTrip();
            }
            g.pass();g.pass();assert(g.reviewing());codecRoundTrip();g.resume();invariants(g);
        }
        for(int level=0;level<4;++level) {
            g.reset(n);
            for(int i=0;i<8;++i) {int p=int(rng()%g.points());Position next;if(g.trial(p,next)==Error::Ok) g.play(p);}
            auto t=std::chrono::steady_clock::now();int move=runBot(level);
            double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
            printf("BOT n=%d level=%d move=%d host_ms=%.1f\n",n,level,move,ms);
            codecRoundTrip();
        }
        // Capture on first line must beat remote opening moves for both search levels.
        g.reset(n);g.play(1);g.play(0);
        assert(runBot(2)==n);assert(runBot(3)==n);
        for(int level=0;level<4;++level) {
            g.reset(n);
            for(int turn=0;turn<6&&!g.reviewing();++turn) {
                int p=runBot(level);auto e=p<0?g.pass():g.play(p);assert(e==Error::Ok);invariants(g);
            }
            codecRoundTrip();
        }
        printf("PASS board %d\n",n);
    }
    printf("PASS %d checks; sizeof(Game)=%zu; sizeof(Bot)=%zu; save cap=%zu\n",checks,sizeof(Game),sizeof(Bot),Save::MaxBytes);
}
