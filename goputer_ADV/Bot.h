#pragma once
#include "GoEngine.h"
namespace Go {
// Bounded beam minimax, not a neural engine; all temporary moves are undone per step.
class Bot {
public:
    void begin(const Game& g,uint32_t seed,int level);
    bool step(Game& g);
    void finish();
    int move() const {return result_;}
    int progress() const {return stage_==0?scan_*40/points_:40+rootIndex_*60/(rootCount_?rootCount_:1);}
    int nodes() const {return nodes_;}
private:
    struct Candidate {int p=-1,v=-1000000;};
    Candidate roots_[12],replies_[4];
    int stage_=0,scan_=0,rootCount_=0,replyCount_=0,rootIndex_=0,replyIndex_=0;
    int points_=81,level_=0,startPly_=0,rootColor_=Black,result_=-1,nodes_=0;
    int replyMin_=1000000,followMax_=-1000000,best_=-1000000,rootBestReply_=-1;
    uint32_t seed_=1;
    static void insert(Candidate* list,int& count,int cap,int p,int v);
    bool relevant(const Game& g,int p) const;
    int heuristic(const Game& g,int p,const Position& next);
    int evaluate(const Game& g) const;
    void completeRoot(int value);
};
}
