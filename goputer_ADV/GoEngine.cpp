#include "GoEngine.h"
#include <string.h>
namespace Go {
bool supportedSize(int n) {return n==9||n==13||n==19;}
bool starPoint(int n,int x,int y) {
    if(!supportedSize(n)||x<0||y<0||x>=n||y>=n) return false;
    // Zero-based indices: 3 is the FOURTH line from the physical board edge.
    // 9x9 retains third-line corner hoshi and tengen.
    if(n==9||n==13) {
        int lo=n==13?3:2,hi=n-1-lo,mid=n/2;
        return ((x==lo||x==hi)&&(y==lo||y==hi))||(x==mid&&y==mid);
    }
    return (x==3||x==9||x==15)&&(y==3||y==9||y==15);
}
static void pack(const uint8_t* b,int count,uint8_t* out) {
    memset(out,0,PackedBytes);
    for(int i=0;i<count;++i) out[i/4]|=uint8_t(b[i]<<((i%4)*2));
}
int neighbors(int n,int p,int out[4]) {
    if(!supportedSize(n)||p<0||p>=n*n) return 0;
    int k=0;
    if(p%n) out[k++]=p-1;
    if(p%n<n-1) out[k++]=p+1;
    if(p>=n) out[k++]=p-n;
    if(p<n*(n-1)) out[k++]=p+n;
    return k;
}
Group groupAt(const uint8_t* b,int n,int p) {
    Group g;if(!supportedSize(n)||p<0||p>=n*n||!b[p]) return g;
    bool seen[MaxPoints]={},lib[MaxPoints]={};
    seen[p]=true;g.stones[g.count++]=uint16_t(p);
    for(int h=0;h<g.count;++h) {
        int adj[4],nn=neighbors(n,g.stones[h],adj);
        for(int k=0;k<nn;++k) {
            int q=adj[k];
            if(!b[q]&&!lib[q]) {lib[q]=true;++g.liberties;}
            else if(b[q]==b[p]&&!seen[q]) {seen[q]=true;g.stones[g.count++]=uint16_t(q);}
        }
    }
    return g;
}
const char* errorText(Error e) {
    switch(e) {
        case Error::Ok:return "OK";case Error::Outside:return "Outside board";
        case Error::Occupied:return "Occupied";case Error::Suicide:return "Suicide";
        case Error::Superko:return "Ko / repetition";
        case Error::HistoryFull:return "Limit: pass/undo";case Error::Review:return "Review score";
    }
    return "Error";
}
void Game::remember() {
    auto& s=history_[ply_];pack(current_.board,points(),s.board);
    s.turn=current_.turn;s.passes=current_.passes;s.last=current_.last;
    memcpy(s.captured,current_.captured,sizeof s.captured);
}
void Game::restore() {
    const auto& s=history_[ply_];current_=Position{};
    for(int i=0;i<points();++i) current_.board[i]=(s.board[i/4]>>((i%4)*2))&3;
    current_.turn=s.turn;current_.passes=s.passes;current_.last=s.last;
    memcpy(current_.captured,s.captured,sizeof s.captured);memset(dead_,0,sizeof dead_);
}
void Game::reset(int n) {
    size_=supportedSize(n)?n:9;ply_=0;current_=Position{};
    memset(dead_,0,sizeof dead_);remember();
}
Error Game::trial(int p,Position& next) const {
    if(reviewing()) return Error::Review;
    if(ply_>=MaxTurns-2) return Error::HistoryFull;
    if(p<0||p>=points()) return Error::Outside;
    if(current_.board[p]) return Error::Occupied;
    next=current_;next.board[p]=current_.turn;
    int adj[4],nn=neighbors(size_,p,adj);
    for(int k=0;k<nn;++k) {
        int q=adj[k];if(next.board[q]!=3-current_.turn) continue;
        Group g=groupAt(next.board,size_,q);
        if(!g.liberties) {
            for(int j=0;j<g.count;++j) next.board[g.stones[j]]=Empty;
            next.captured[current_.turn]+=g.count;
        }
    }
    if(!groupAt(next.board,size_,p).liberties) return Error::Suicide;
    uint8_t packed[PackedBytes];pack(next.board,points(),packed);
    for(int i=0;i<=ply_;++i)
        if(!memcmp(history_[i].board,packed,PackedBytes)) return Error::Superko;
    next.turn=3-current_.turn;next.passes=0;next.last=int16_t(p);return Error::Ok;
}
Error Game::play(int p) {
    Position next;Error e=trial(p,next);
    if(e==Error::Ok) {
        moves_[ply_]=uint16_t(p);current_=next;++ply_;remember();memset(dead_,0,sizeof dead_);
    }
    return e;
}
Error Game::pass() {
    if(reviewing()) return Error::Review;
    if(ply_>=MaxTurns) return Error::HistoryFull;
    moves_[ply_]=PassMove;current_.turn=3-current_.turn;++current_.passes;current_.last=-1;
    ++ply_;remember();memset(dead_,0,sizeof dead_);return Error::Ok;
}
bool Game::undo() {if(!ply_) return false;--ply_;restore();return true;}
void Game::resume() {if(reviewing()) {undo();undo();}}
void Game::toggleDead(int p) {
    if(!reviewing()) return;
    Group g=groupAt(current_.board,size_,p);if(!g.count) return;
    bool flag=!dead_[p];for(int j=0;j<g.count;++j) dead_[g.stones[j]]=flag;
}
Score Game::score() const {
    Score s;uint8_t b[MaxPoints]={};bool seen[MaxPoints]={};
    for(int i=0;i<points();++i) {
        b[i]=dead_[i]?uint8_t(Empty):current_.board[i];s.owner[i]=b[i];
        if(b[i]==Black) ++s.black;
        if(b[i]==White) ++s.white;
    }
    for(int p=0;p<points();++p) {
        if(b[p]||seen[p]) continue;
        uint16_t queue[MaxPoints];int count=1,border=0;queue[0]=uint16_t(p);seen[p]=true;
        for(int h=0;h<count;++h) {
            int adj[4],nn=neighbors(size_,queue[h],adj);
            for(int k=0;k<nn;++k) {
                int q=adj[k];if(b[q]) border|=b[q];
                else if(!seen[q]) {seen[q]=true;queue[count++]=uint16_t(q);}
            }
        }
        if(border==Black) s.black+=count;
        if(border==White) s.white+=count;
        if(border==Black||border==White)
            for(int j=0;j<count;++j) s.owner[queue[j]]=uint8_t(border);
    }
    return s;
}
}
