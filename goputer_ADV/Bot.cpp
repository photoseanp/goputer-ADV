#include "Bot.h"
#include <stdlib.h>
#include <algorithm>
namespace Go {
void Bot::insert(Candidate* a,int& count,int cap,int p,int v) {
    int index=count;if(index>=cap) index=cap-1;
    if(count==cap&&v<=a[index].v) return;
    while(index>0&&v>a[index-1].v) {if(index<cap) a[index]=a[index-1];--index;}
    a[index].p=p;a[index].v=v;if(count<cap) ++count;
}
void Bot::begin(const Game& g,uint32_t seed,int level) {
    stage_=0;scan_=rootCount_=replyCount_=rootIndex_=replyIndex_=nodes_=0;
    points_=g.points();level_=std::max(0,std::min(level,3));startPly_=g.ply();
    rootColor_=g.position().turn;result_=-1;best_=-1000000;seed_=seed?seed:1;
}
bool Bot::relevant(const Game& g,int p) const {
    if(g.position().board[p]) return false;
    int n=g.size(),x=p%n,y=p/n;
    if(starPoint(n,x,y)||g.ply()<2) return true;
    // Beam search deliberately prunes quiet far-away moves; final legality is exact.
    for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) {
        int xx=x+dx,yy=y+dy;
        if(abs(dx)+abs(dy)>2||xx<0||yy<0||xx>=n||yy>=n) continue;
        if(g.position().board[yy*n+xx]) return true;
    }
    return false;
}
int Bot::heuristic(const Game& g,int p,const Position& next) {
    const Position& old=g.position();int n=g.size(),adj[4],nn=neighbors(n,p,adj);
    int own=0,enemy=0,saved=0,attack=0,joined=0;bool seen[MaxPoints]={};
    for(int k=0;k<nn;++k) {
        int q=adj[k];if(old.board[q]==old.turn) ++own;else if(old.board[q]) ++enemy;
        if(!old.board[q]||seen[q]) continue;
        Group a=groupAt(old.board,n,q);
        for(int j=0;j<a.count;++j) seen[a.stones[j]]=true;
        Group b=groupAt(next.board,n,q);
        if(old.board[q]==old.turn) {
            ++joined;if(a.liberties==1&&b.liberties>1) saved+=a.count;
        } else if(b.count&&b.liberties==1) attack+=a.count;
    }
    int taken=next.captured[old.turn]-old.captured[old.turn];Group placed=groupAt(next.board,n,p);
    if(own==nn&&!taken) {
        int bad=0,diags=0,x=p%n,y=p/n;
        for(int dy=-1;dy<=1;dy+=2) for(int dx=-1;dx<=1;dx+=2) {
            int xx=x+dx,yy=y+dy;if(xx<0||yy<0||xx>=n||yy>=n) continue;
            ++diags;if(old.board[yy*n+xx]!=old.turn) ++bad;
        }
        if(bad<=(diags==4?1:0)) return -1000000;
    }
    seed_^=seed_<<13;seed_^=seed_>>17;seed_^=seed_<<5;
    int value=taken*180+saved*(level_?110:35)+attack*(level_?32:12);
    value+=std::min(placed.liberties,8)*3+int(seed_%(level_?7:40));
    if(placed.liberties==1) value-=180+placed.count*12;
    if(!enemy&&own>=2) value-=45;
    if(joined>1) value+=12;
    int x=p%n,y=p/n,ex=std::min(x,n-1-x),ey=std::min(y,n-1-y);
    value+=(ex>0&&ey>0)?10:-10;
    int nearest=2*n;
    for(int q=0;q<g.points();++q) if(old.board[q]) nearest=std::min(nearest,abs(x-q%n)+abs(y-q/n));
    if(g.ply()<g.points()/3) {
        if(ex>=2&&ey>=2) value+=12;
        if(nearest>=3&&nearest<2*n) value+=18;
        if(nearest==1&&!enemy) value-=12;
        if(starPoint(n,x,y)) value+=5;
    }
    // Strong modes do not blindly agree to a losing early pass.
    if(level_<2&&old.passes&&g.ply()>n*2&&!taken&&!saved&&!attack) return -1000000;
    return value;
}
int Bot::evaluate(const Game& g) const {
    const auto& pos=g.position();const auto* b=pos.board;int n=g.size(),value=0;
    bool seen[MaxPoints]={};
    // Stones, atari risk, low-liberty groups and local influence.
    for(int p=0;p<g.points();++p) {
        if(b[p]) {
            int sign=b[p]==rootColor_?1:-1;value+=sign*40;
            if(!seen[p]) {
                Group group=groupAt(b,n,p);
                for(int j=0;j<group.count;++j) seen[group.stones[j]]=true;
                int risk=group.liberties==1?group.count*34+14:group.liberties==2?group.count*9:0;
                value-=sign*risk;
            }
        } else {
            int own=0,enemy=0,x=p%n,y=p/n;
            for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) {
                int distance=abs(dx)+abs(dy),xx=x+dx,yy=y+dy;
                if(!distance||distance>2||xx<0||yy<0||xx>=n||yy>=n) continue;
                int c=b[yy*n+xx],w=distance==1?4:1;
                if(c==rootColor_) own+=w;else if(c) enemy+=w;
            }
            value+=std::max(-8,std::min(8,own-enemy));
        }
    }
    value+=rootColor_==White?26:-26; // 6.5 komi scaled to influence units, not a final scorer.
    return value;
}
void Bot::completeRoot(int value) {
    // A small opening prior breaks static-evaluation ties without dominating tactics.
    value+=std::max(-10,std::min(10,roots_[rootIndex_].v/8));
    if(value>best_) {best_=value;result_=roots_[rootIndex_].p;}
    ++rootIndex_;scan_=replyCount_=replyIndex_=0;replyMin_=1000000;stage_=1;
}
void Bot::finish() {stage_=4;}
bool Bot::step(Game& g) {
    if(g.ply()!=startPly_||g.reviewing()) {result_=-1;return true;}
    if(stage_==4) return true;
    if(stage_==0) {
        int p=scan_++;Position next;++nodes_;
        if((level_<2||relevant(g,p))&&g.trial(p,next)==Error::Ok) {
            int v=heuristic(g,p,next);
            if(v>-40) {insert(roots_,rootCount_,level_==3?12:8,p,v);result_=roots_[0].p;}
        }
        if(scan_<points_) return false;
        if(!rootCount_) {stage_=4;return true;}
        result_=roots_[0].p;
        if(level_<2) {stage_=4;return true;}
        // Pass only when agreeing with an opponent pass and area count is favorable.
        if(g.position().passes) {
            Score s=g.score();int d=s.black*2-s.white*2-13;
            bool ahead=rootColor_==Black?d>0:d<0;
            if(ahead&&roots_[0].v<100) {result_=-1;stage_=4;return true;}
        }
        scan_=0;stage_=1;replyMin_=1000000;return false;
    }
    if(rootIndex_>=rootCount_) {stage_=4;return true;}
    int root=roots_[rootIndex_].p;
    if(g.play(root)!=Error::Ok) {++rootIndex_;scan_=replyCount_=0;return false;}
    if(stage_==1) {
        int p=scan_++;Position next;++nodes_;
        if(relevant(g,p)&&g.trial(p,next)==Error::Ok) {
            int h=heuristic(g,p,next);
            if(level_==3) {if(h>-1000000) insert(replies_,replyCount_,4,p,h);}
            else if(g.play(p)==Error::Ok) {
                int v=evaluate(g);if(v<replyMin_) replyMin_=v;g.undo();
            }
        }
        if(scan_>=points_) {
            if(level_==2||!replyCount_) {
                int v=replyMin_==1000000?evaluate(g):replyMin_;g.undo();completeRoot(v);return false;
            }
            stage_=2;scan_=0;replyIndex_=0;replyMin_=1000000;followMax_=-1000000;
        }
        g.undo();return false;
    }
    int reply=replies_[replyIndex_].p;
    if(g.play(reply)!=Error::Ok) {g.undo();++replyIndex_;return false;}
    int p=scan_++;Position next;++nodes_;
    if(relevant(g,p)&&g.trial(p,next)==Error::Ok&&heuristic(g,p,next)>-1000000&&g.play(p)==Error::Ok) {
        followMax_=std::max(followMax_,evaluate(g));g.undo();
    }
    if(scan_>=points_) {
        if(followMax_==-1000000) followMax_=evaluate(g);
        replyMin_=std::min(replyMin_,followMax_);++replyIndex_;scan_=0;followMax_=-1000000;
        if(replyIndex_>=replyCount_) {g.undo();g.undo();completeRoot(replyMin_);return false;}
    }
    g.undo();g.undo();return false;
}
}
