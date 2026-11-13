#pragma once
#include <stdint.h>
namespace Go {
constexpr int MaxPoints=361,MaxTurns=1024,PackedBytes=91;
constexpr uint16_t PassMove=0xffff;
enum Stone:uint8_t {Empty=0,Black=1,White=2};
enum class Error {Ok,Outside,Occupied,Suicide,Superko,HistoryFull,Review};
struct Position {
    uint8_t board[MaxPoints]={},turn=Black,passes=0;
    uint16_t captured[3]={};
    int16_t last=-1;
};
struct Group {uint16_t stones[MaxPoints]={};int count=0,liberties=0;};
struct Score {int black=0,white=0;uint8_t owner[MaxPoints]={};};
bool supportedSize(int n);
bool starPoint(int n,int x,int y);
int neighbors(int n,int p,int out[4]);
Group groupAt(const uint8_t* b,int n,int p);
const char* errorText(Error e);
class Game {
public:
    void reset(int n=9);
    int size() const {return size_;}
    int points() const {return size_*size_;}
    int ply() const {return ply_;}
    const Position& position() const {return current_;}
    const uint16_t* moves() const {return moves_;}
    bool reviewing() const {return current_.passes>=2;}
    Error trial(int p,Position& next) const;
    Error play(int p);
    Error pass();
    bool undo();
    void resume();
    void toggleDead(int p);
    bool dead(int p) const {return p>=0&&p<points()&&dead_[p];}
    Score score() const;
private:
    struct Snapshot {
        uint8_t board[PackedBytes]={},turn=Black,passes=0;
        uint16_t captured[3]={};int16_t last=-1;
    };
    Snapshot history_[MaxTurns+1]={};
    Position current_;
    uint16_t moves_[MaxTurns]={};
    bool dead_[MaxPoints]={};
    int size_=9,ply_=0;
    void remember();
    void restore();
};
}
