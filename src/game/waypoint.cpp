#include "game.h"

extern selinfo sel;

namespace ai
{
    using namespace game;

    vector<waypoint> waypoints; //this is the box that all waypoints go into

    //bad kinds of materials for bots to path into: clipping, instadeath
    bool clipped(const vec &o)
    {
        int material = lookupmaterial(o),
            clipmat = material&MatFlag_Clip;
        return clipmat == Mat_Clip || material&Mat_Death;
    }

    //weights waypoints by distance from ai
    //additionally weights deathmat points at 10x normal and liquids at 2x
    //returns -2 if not in bounds
    int getweight(const vec &o)
    {
        vec pos = o;
        pos.z += ai::jumpmin;
        if(!insideworld(vec(pos.x, pos.y, min(pos.z, getworldsize() - 1e-3f))))
        {
            return -2;
        }
        float dist = raycube(pos, vec(0, 0, -1), 0, Ray_ClipMat);
        int posmat = lookupmaterial(pos),
            weight = 1;
        if(IS_LIQUID(posmat&MatFlag_Volume))
        {
            weight *= 5;
        }
        if(dist >= 0)
        {
            weight = static_cast<int>(dist/ai::jumpmin);
            pos.z -= std::clamp(dist-8.0f, 0.0f, pos.z);
            int trgmat = lookupmaterial(pos);
            if(trgmat&Mat_Death)
            {
                weight *= 10;
            }
            else if(IS_LIQUID(trgmat&MatFlag_Volume))
            {
                weight *= 2;
            }
        }
        return weight;
    }

    //wpcache enum is local to this file
    enum
    {
        WPCache_Dynamic,
        WPCache_NumWPCaches
    };

    struct wpcache
    {
        struct node
        {
            float split[2];
            uint child[2];

            int axis() const
            {
                return child[0]>>30;
            } //right shift 30?

            int childindex(int which) const
            {
                return child[which]&0x3FFFFFFF;
            }//bitwise AND with magic 0x3FFFFFFF?

            bool isleaf(int which) const
            {
                return (child[1]&(1<<(30+which)))!=0;
            }//child elem 1 AND'd by 1 bitshifted right by 30+which places is not equal to 0?
        };

        vector<node> nodes;
        int firstwp, lastwp;
        vec bbmin, bbmax;

        wpcache()
        {
            clear();
        }

        void clear()
        {
            nodes.setsize(0);
            firstwp = lastwp = -1;
            bbmin = vec(1e16f, 1e16f, 1e16f);
            bbmax = vec(-1e16f, -1e16f, -1e16f);
        }

        // note: waypoints is a vector<waypoint>
        void build(int first = 0, int last = -1)
        {
            if(last < 0) //if list has been cleared
            {
                last = waypoints.length();
            }
            vector<int> indices;
            //go and populate the vector<int> indices with this loop
            for(int i = first; i < last; i++) //if cleared, 0 to waypoints.length
            {
                waypoint &w = waypoints[i];
                indices.add(i);
                if(firstwp < 0)
                {
                    firstwp = i;
                }
                float radius = waypointradius;
                bbmin.min(vec(w.o).sub(radius));
                bbmax.max(vec(w.o).add(radius));
            }
            if(first < last)
            {
                lastwp = max(lastwp, last-1);
            }
            if(indices.length()) //if this above loop yielded something useful
            {
                nodes.reserve(indices.length());//create some space
                build(indices.getbuf(), indices.length(), bbmin, bbmax);//build() calls an unrelated build() because that's what makes sense in this timeline
            }
        }
        //takes from above build() call
        void build(int *indices, int numindices, const vec &vmin, const vec &vmax)
        {
            int axis = 2;
            for(int k = 0; k < 2; ++k)
            {
                if(vmax[k] - vmin[k] > vmax[axis] - vmin[axis])
                {
                    axis = k;
                }
            }
            //declare some bounding vec objects
            vec leftmin(1e16f, 1e16f, 1e16f),
                leftmax(-1e16f, -1e16f, -1e16f),
                rightmin(1e16f, 1e16f, 1e16f),
                rightmax(-1e16f, -1e16f, -1e16f);

            float split = 0.5f*(vmax[axis] + vmin[axis]),
                  splitleft = -1e16f,
                  splitright = 1e16f;
            int left, right;
            for(left = 0, right = numindices; left < right;)
            {
                waypoint &w = waypoints[indices[left]];
                float radius = waypointradius;
                if(max(split - (w.o[axis]-radius), 0.0f) > max((w.o[axis]+radius) - split, 0.0f))
                {
                    ++left;
                    splitleft = max(splitleft, w.o[axis]+radius);
                    leftmin.min(vec(w.o).sub(radius)); //floors & ceils vectors with bounding vecs
                    leftmax.max(vec(w.o).add(radius));
                }
                else
                {
                    --right;
                    swap(indices[left], indices[right]);
                    splitright = min(splitright, w.o[axis]-radius);
                    rightmin.min(vec(w.o).sub(radius)); //floors & ceils again
                    rightmax.max(vec(w.o).add(radius));
                }
            }

            if(!left || right==numindices) //if we didn't manage to move centerwards with the ++left/--right above
            {
                leftmin = rightmin = vec(1e16f, 1e16f, 1e16f);
                leftmax = rightmax = vec(-1e16f, -1e16f, -1e16f);
                left = right = numindices/2;
                splitleft = -1e16f;
                splitright = 1e16f;
                for(int i = 0; i < numindices; ++i)
                {
                    waypoint &w = waypoints[indices[i]];
                    float radius = waypointradius;
                    if(i < left)
                    {
                        splitleft = max(splitleft, w.o[axis]+radius);
                        leftmin.min(vec(w.o).sub(radius)); //more floors & ceils
                        leftmax.max(vec(w.o).add(radius));
                    }
                    else
                    {
                        splitright = min(splitright, w.o[axis]-radius);
                        rightmin.min(vec(w.o).sub(radius)); //more floors & ceils
                        rightmax.max(vec(w.o).add(radius));
                    }
                }
            }

            int offset = nodes.length();
            node &curnode = nodes.add();
            curnode.split[0] = splitleft;
            curnode.split[1] = splitright;

            if(left<=1)
            {
                curnode.child[0] = (axis<<30) | (left>0 ? indices[0] : 0x3FFFFFFF);
            }
            else
            {
                curnode.child[0] = (axis<<30) | (nodes.length()-offset);
                if(left)
                {
                    build(indices, left, leftmin, leftmax);
                }
            }

            if(numindices-right<=1)
            {
                curnode.child[1] = (1<<31) | (left<=1 ? 1<<30 : 0) | (numindices-right>0 ? indices[right] : 0x3FFFFFFF);
            }
            else
            {
                curnode.child[1] = (left<=1 ? 1<<30 : 0) | (nodes.length()-offset);
                if(numindices-right)
                {
                    build(&indices[right], numindices-right, rightmin, rightmax);
                }
            }
        }

    } wpcaches[WPCache_NumWPCaches];

    static int invalidatedwpcaches = 0, clearedwpcaches = (1<<WPCache_NumWPCaches)-1, numinvalidatewpcaches = 0, lastwpcache = 0;

    static inline void invalidatewpcache(int wp)
    {
        if(++numinvalidatewpcaches >= 1000)
        {
            numinvalidatewpcaches = 0;
            invalidatedwpcaches = (1<<WPCache_NumWPCaches)-1;
        }
        else
        {
            for(int i = 0; i < WPCache_Dynamic; ++i)
            {
                if(wp >= wpcaches[i].firstwp && wp <= wpcaches[i].lastwp)
                {
                    invalidatedwpcaches |= 1<<i;
                    return;
                }
            }
            invalidatedwpcaches |= 1<<WPCache_Dynamic;
        }
    }

    void clearwpcache(bool full = true)
    {
        for(int i = 0; i < WPCache_NumWPCaches; ++i)
        {
            if(full || invalidatedwpcaches&(1<<i))
            {
                wpcaches[i].clear();
                clearedwpcaches |= 1<<i;
            }
        }
        if(full || invalidatedwpcaches == (1<<WPCache_NumWPCaches)-1)
        {
            numinvalidatewpcaches = 0;
            lastwpcache = 0;
        }
        invalidatedwpcaches = 0;
    }
    ICOMMAND(clearwpcache, "", (), clearwpcache());

    avoidset wpavoid;

    void buildwpcache()
    {
        for(int i = 0; i < WPCache_NumWPCaches; ++i)
        {
            if(wpcaches[i].firstwp < 0)
            {
                wpcaches[i].build(i > 0 ? wpcaches[i-1].lastwp+1 : 1, i+1 >= WPCache_NumWPCaches || wpcaches[i+1].firstwp < 0 ? -1 : wpcaches[i+1].firstwp);
            }
        }
        clearedwpcaches = 0;
        lastwpcache = waypoints.length();

        wpavoid.clear();
        for(int i = 0; i < waypoints.length(); i++)
        {
            if(waypoints[i].weight < 0)
            {
                wpavoid.avoidnear(NULL, waypoints[i].o.z + waypointradius, waypoints[i].o, waypointradius);
            }
        }
    }

    struct wpcachestack
    {
        wpcache::node *node;
        float tmin, tmax;
    };

    vector<wpcache::node *> wpcachestack;

    int closestwaypoint(const vec &pos, float mindist, bool links, gameent *d)
    {
        if(waypoints.empty())
        {
            return -1;
        }
        if(clearedwpcaches)
        {
            buildwpcache();
        }
//==================================================================CHECKCLOSEST
        #define CHECKCLOSEST(index) do { \
            if(index < waypoints.length()) \
            { \
                const waypoint &w = waypoints[index]; \
                if(!links || w.links[0]) \
                { \
                    float dist = w.o.squaredist(pos); \
                    if(dist < mindist*mindist) \
                    { \
                        closest = index; \
                        mindist = sqrtf(dist); \
                    } \
                } \
            } \
        } while(0)
//=====
        int closest = -1;
        wpcache::node *curnode; //define current node
        for(int i = 0; i < WPCache_NumWPCaches; ++i) //WPCache_NumWPCaches = 2
        {
            if(wpcaches[i].firstwp >= 0) //if the first waypoint in the enumerated (by i) cache is not -1 (-1 set by clearing)
            {
                for(curnode = &wpcaches[i].nodes[0], wpcachestack.setsize(0);;) //for cache[i] node 0, set the cachestack size to 0
                {
                    int axis = curnode->axis(); //axis property from wpcache struct
                    float dist1 = pos[axis] - curnode->split[0],
                          dist2 = curnode->split[1] - pos[axis];
                    if(dist1 >= mindist)
                    {
                        if(dist2 < mindist)
                        {
                            if(!curnode->isleaf(1))
                            {
                                curnode += curnode->childindex(1);
                                continue;
                            }
                            CHECKCLOSEST(curnode->childindex(1));
                        }
                    }
                    else if(curnode->isleaf(0))
                    {
                        CHECKCLOSEST(curnode->childindex(0));
                        if(dist2 < mindist)
                        {
                            if(!curnode->isleaf(1))
                            {
                                curnode += curnode->childindex(1);
                                continue;
                            }
                            CHECKCLOSEST(curnode->childindex(1));
                        }
                    }
                    else
                    {
                        if(dist2 < mindist)
                        {
                            if(!curnode->isleaf(1))
                            {
                                wpcachestack.add(curnode + curnode->childindex(1));
                            }
                            else
                            {
                                CHECKCLOSEST(curnode->childindex(1));
                            }
                        }
                        curnode += curnode->childindex(0);
                        continue;
                    }
                    if(wpcachestack.empty())
                    {
                        break;
                    }
                    curnode = wpcachestack.pop();
                }
            }
        }
        for(int i = lastwpcache; i < waypoints.length(); i++)
        {
            CHECKCLOSEST(i);
        }
        return closest;
    }
#undef CHECKCLOSEST
//==============================================================================

    void findwaypointswithin(const vec &pos, float mindist, float maxdist, vector<int> &results)
    {
        if(waypoints.empty())
        {
            return;
        }
        if(clearedwpcaches)
        {
            buildwpcache();
        }

        float mindist2 = mindist*mindist,
              maxdist2 = maxdist*maxdist;
        #define CHECKWITHIN(index) \
        do { \
            int n = (index); \
            if(n < waypoints.length()) \
            { \
                const waypoint &w = waypoints[n]; \
                float dist = w.o.squaredist(pos); \
                if(dist > mindist2 && dist < maxdist2) \
                { \
                    results.add(n); \
                } \
            } \
        } while(0)
        wpcache::node *curnode;
        for(int which = 0; which < WPCache_NumWPCaches; ++which)
        {
            if(wpcaches[which].firstwp >= 0)
            {
                for(curnode = &wpcaches[which].nodes[0], wpcachestack.setsize(0);;)
                {
                    int axis = curnode->axis();
                    float dist1 = pos[axis] - curnode->split[0],
                          dist2 = curnode->split[1] - pos[axis];
                    if(dist1 >= maxdist)
                    {
                        if(dist2 < maxdist)
                        {
                            if(!curnode->isleaf(1))
                            {
                                curnode += curnode->childindex(1);
                                continue;
                            }
                            CHECKWITHIN(curnode->childindex(1));
                        }
                    }
                    else if(curnode->isleaf(0))
                    {
                        CHECKWITHIN(curnode->childindex(0));
                        if(dist2 < maxdist)
                        {
                            if(!curnode->isleaf(1))
                            {
                                curnode += curnode->childindex(1);
                                continue;
                            }
                            CHECKWITHIN(curnode->childindex(1));
                        }
                    }
                    else
                    {
                        if(dist2 < maxdist)
                        {
                            if(!curnode->isleaf(1))
                            {
                                wpcachestack.add(curnode + curnode->childindex(1));
                            }
                            else
                            {
                                CHECKWITHIN(curnode->childindex(1));
                            }
                        }
                        curnode += curnode->childindex(0);
                        continue;
                    }
                    if(wpcachestack.empty())
                    {
                        break;
                    }
                    curnode = wpcachestack.pop();
                }
            }
        }
        for(int i = lastwpcache; i < waypoints.length(); i++)
        {
            CHECKWITHIN(i);
        }
    }

    void avoidset::avoidnear(void *owner, float above, const vec &pos, float limit)
    {
        if(ai::waypoints.empty())
        {
            return;
        }
        if(clearedwpcaches)
        {
            buildwpcache();
        }

        float limit2 = limit*limit;
        #define CHECKNEAR(index) \
        do { \
            int n = (index); \
            if(n < ai::waypoints.length()) \
            { \
                const waypoint &w = ai::waypoints[n]; \
                if(w.o.squaredist(pos) < limit2) \
                { \
                    add(owner, above, n); \
                } \
            } \
        } while(0)
        wpcache::node *curnode;
        for(int which = 0; which < WPCache_NumWPCaches; ++which)
        {
            if(wpcaches[which].firstwp >= 0)
            {
                for(curnode = &wpcaches[which].nodes[0], wpcachestack.setsize(0);;)
                {
                    int axis = curnode->axis();
                    float dist1 = pos[axis] - curnode->split[0],
                          dist2 = curnode->split[1] - pos[axis];
                    if(dist1 >= limit)
                    {
                        if(dist2 < limit)
                        {
                            if(!curnode->isleaf(1))
                            {
                                curnode += curnode->childindex(1);
                                continue;
                            }
                            CHECKNEAR(curnode->childindex(1));
                        }
                    }
                    else if(curnode->isleaf(0))
                    {
                        CHECKNEAR(curnode->childindex(0));
                        if(dist2 < limit)
                        {
                            if(!curnode->isleaf(1))
                            {
                                curnode += curnode->childindex(1);
                                continue;
                            }
                            CHECKNEAR(curnode->childindex(1));
                        }
                    }
                    else
                    {
                        if(dist2 < limit)
                        {
                            if(!curnode->isleaf(1))
                            {
                                wpcachestack.add(curnode + curnode->childindex(1));
                            }
                            else
                            {
                                CHECKNEAR(curnode->childindex(1));
                            }
                        }
                        curnode += curnode->childindex(0);
                        continue;
                    }
                    if(wpcachestack.empty())
                    {
                        break;
                    }
                    curnode = wpcachestack.pop();
                }
            }
        }
        for(int i = lastwpcache; i < waypoints.length(); i++)
        {
            CHECKNEAR(i);
        }
    }

    int avoidset::remap(gameent *d, int n, vec &pos, bool retry)
    {
        if(!obstacles.empty())
        {
            int cur = 0;
            for(int i = 0; i < obstacles.length(); i++)
            {
                obstacle &ob = obstacles[i];
                int next = cur + ob.numwaypoints;
                if(ob.owner != d)
                {
                    for(; cur < next; cur++)
                    {
                        if(waypoints[cur] == n)
                        {
                            if(ob.above < 0)
                            {
                                return retry ? n : -1;
                            }
                            vec above(pos.x, pos.y, ob.above);
                            if(above.z-d->o.z >= ai::jumpmax)
                            {
                                return retry ? n : -1; // too much scotty
                            }
                            int node = closestwaypoint(above, ai::sightmin, true, d);
                            if(ai::iswaypoint(node) && node != n)
                            { // try to reroute above their head?
                                if(!find(node, d))
                                {
                                    pos = ai::waypoints[node].o;
                                    return node;
                                }
                                else
                                {
                                    return retry ? n : -1;
                                }
                            }
                            else
                            {
                                vec old = d->o;
                                d->o = vec(above).addz(d->eyeheight);
                                bool col = collide(d, vec(0, 0, 1));
                                d->o = old;
                                if(!col)
                                {
                                    pos = above;
                                    return n;
                                }
                                else
                                {
                                    return retry ? n : -1;
                                }
                            }
                        }
                    }
                }
                cur = next;
            }
        }
        return n;
    }

    static inline float heapscore(waypoint *q)
    {
        return q->score();
    }

    bool route(gameent *d, int node, int goal, vector<int> &route, const avoidset &obstacles, int retries)
    {
        if(waypoints.empty() || !iswaypoint(node) || !iswaypoint(goal) || goal == node || !waypoints[node].links[0])
        {
            return false;
        }
        static ushort routeid = 1;
        static vector<waypoint *> queue;

        if(!routeid)
        {
            for(int i = 0; i < waypoints.length(); i++)
            {
                waypoints[i].route = 0;
            }
            routeid = 1;
        }

        if(d)
        {
            if(retries <= 1 && d->ai)
            {
                for(int i = 0; i < ai::numprevnodes; ++i)
                {
                    if(d->ai->prevnodes[i] != node && iswaypoint(d->ai->prevnodes[i]))
                    {
                        waypoints[d->ai->prevnodes[i]].route = routeid;
                        waypoints[d->ai->prevnodes[i]].curscore = -1;
                        waypoints[d->ai->prevnodes[i]].estscore = 0;
                    }
                }
            }
            if(retries <= 0)
            {
                LOOP_AVOID(obstacles, d,
                {
                    if(iswaypoint(wp) && wp != node && wp != goal && waypoints[node].find(wp) < 0 && waypoints[goal].find(wp) < 0)
                    {
                        waypoints[wp].route = routeid;
                        waypoints[wp].curscore = -1;
                        waypoints[wp].estscore = 0;
                    }
                });
            }
        }

        waypoints[node].route = routeid;
        waypoints[node].curscore = waypoints[node].estscore = 0;
        waypoints[node].prev = 0;
        queue.setsize(0);
        queue.add(&waypoints[node]);
        route.setsize(0);

        int lowest = -1;
        while(!queue.empty())
        {
            waypoint &m = *queue.removeheap();
            float prevscore = m.curscore;
            m.curscore = -1;
            for(int i = 0; i < maxwaypointlinks; ++i)
            {
                int link = m.links[i];
                if(!link)
                {
                    break;
                }
                if(iswaypoint(link) && (link == node || link == goal || waypoints[link].links[0]))
                {
                    waypoint &n = waypoints[link];
                    int weight = max(n.weight, 1);
                    float curscore = prevscore + n.o.dist(m.o)*weight;
                    if(n.route == routeid && curscore >= n.curscore)
                    {
                        continue;
                    }
                    n.curscore = curscore;
                    n.prev = static_cast<ushort>(&m - &waypoints[0]);
                    if(n.route != routeid)
                    {
                        n.estscore = n.o.dist(waypoints[goal].o)*weight;
                        if(n.estscore <= waypointradius*4 && (lowest < 0 || n.estscore <= waypoints[lowest].estscore))
                        {
                            lowest = link;
                        }
                        n.route = routeid;
                        if(link == goal)
                        {
                            goto foundgoal;
                        }
                        queue.addheap(&n);
                    }
                    else
                    {
                        for(int j = 0; j < queue.length(); j++)
                        {
                            if(queue[j] == &n)
                            {
                                queue.upheap(j);
                                break;
                            }
                        }
                    }
                }
            }
        }
        foundgoal:

        routeid++;

        if(lowest >= 0) // otherwise nothing got there
        {
            for(waypoint *m = &waypoints[lowest]; m > &waypoints[0]; m = &waypoints[m->prev])
            {
                route.add(m - &waypoints[0]); // just keep it stored backward
            }
        }

        return !route.empty();
    }

    VARF(dropwaypoints, 0, 0, 1, { player1->lastnode = -1; });

    int addwaypoint(const vec &o, int weight = -1)
    {
        if(waypoints.length() > maxwaypoints)
        {
            return -1;
        }
        int n = waypoints.length();
        waypoints.add(waypoint(o, weight >= 0 ? weight : getweight(o)));
        invalidatewpcache(n);
        return n;
    }

    void linkwaypoint(waypoint &a, int n)
    {
        for(int i = 0; i < maxwaypointlinks; ++i)
        {
            if(a.links[i] == n)
            {
                return;
            }
            if(!a.links[i])
            {
                a.links[i] = n;
                return;
            }
        }
        a.links[randomint(maxwaypointlinks)] = n;
    }

    string loadedwaypoints = "";

    static inline bool shouldnavigate()
    {
        if(dropwaypoints)
        {
            return true;
        }
        for(int i = players.length(); --i >=0;) //note reverse iteration
        {
            if(players[i]->aitype != AI_None)
            {
                return true;
            }
        }
        return false;
    }

    static inline bool shoulddrop(gameent *d)
    {
        return !d->ai && (dropwaypoints || !loadedwaypoints[0]);
    }

    void inferwaypoints(gameent *d, const vec &o, const vec &v, float mindist)
    {
        if(!shouldnavigate())
        {
            return;
        }
        if(shoulddrop(d))
        {
            if(waypoints.empty())
            {
                seedwaypoints();
            }
            int from = closestwaypoint(o, mindist, false),
                to   = closestwaypoint(v, mindist, false);
            if(!iswaypoint(from))
            {
                from = addwaypoint(o);
            }
            if(!iswaypoint(to))
            {
                to = addwaypoint(v);
            }
            if(d->lastnode != from && iswaypoint(d->lastnode) && iswaypoint(from))
            {
                linkwaypoint(waypoints[d->lastnode], from);
            }
            if(iswaypoint(to))
            {
                if(from != to && iswaypoint(from) && iswaypoint(to))
                {
                    linkwaypoint(waypoints[from], to);
                }
                d->lastnode = to;
            }
        }
        else
        {
            d->lastnode = closestwaypoint(v, waypointradius*2, false, d);
        }
    }

    void navigate(gameent *d)
    {
        vec v(d->feetpos());
        if(d->state != ClientState_Alive)
        {
            d->lastnode = -1;
            return;
        }
        bool dropping = shoulddrop(d);
        int mat = lookupmaterial(v);
        if((mat&MatFlag_Clip) == Mat_Clip || mat&Mat_Death)
        {
            dropping = false;
        }
        float dist = dropping ? waypointradius : (d->ai ? waypointradius : sightmin);
        int curnode = closestwaypoint(v, dist, false, d), prevnode = d->lastnode;
        if(!iswaypoint(curnode) && dropping)
        {
            if(waypoints.empty())
            {
                seedwaypoints();
            }
            curnode = addwaypoint(v);
        }
        if(iswaypoint(curnode))
        {
            if(dropping && d->lastnode != curnode && iswaypoint(d->lastnode))
            {
                linkwaypoint(waypoints[d->lastnode], curnode);
                if(!d->timeinair)
                {
                    linkwaypoint(waypoints[curnode], d->lastnode);
                }
            }
            d->lastnode = curnode;
            if(d->ai && iswaypoint(prevnode) && d->lastnode != prevnode)
            {
                d->ai->addprevnode(prevnode);
            }
        }
        else if(!iswaypoint(d->lastnode) || waypoints[d->lastnode].o.squaredist(v) > sightmin*sightmin)
        {
            d->lastnode = closestwaypoint(v, sightmax, false, d);
        }
    }

    void navigate()
    {
        if(shouldnavigate())
        {
            for(int i = 0; i < players.length(); i++)
            {
                ai::navigate(players[i]);
            }
        }
        if(invalidatedwpcaches)
        {
            clearwpcache(false);
        }
    }

    //deletes all waypoints on the map
    void clearwaypoints(bool full)
    {
        waypoints.setsize(0);
        clearwpcache();
        if(full)
        {
            loadedwaypoints[0] = '\0';
            dropwaypoints = 0;
        }
    }
    ICOMMAND(clearwaypoints, "", (), clearwaypoints());

    void seedwaypoints()
    {
        if(waypoints.empty())
        {
            addwaypoint(vec(0, 0, 0));
        }
        for(int i = 0; i < entities::ents.length(); i++)
        {
            extentity &e = *entities::ents[i];
            switch(e.type)
            {
                case GamecodeEnt_Playerstart:
                case GamecodeEnt_Teleport:
                case GamecodeEnt_Jumppad:
                case GamecodeEnt_Flag:
                {
                    addwaypoint(e.o);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    void remapwaypoints()
    {
        vector<ushort> remap;
        int total = 0;
        for(int i = 0; i < waypoints.length(); i++)
        {
            remap.add(waypoints[i].links[1] == 0xFFFF ? 0 : total++);
        }
        total = 0;
        for(int j = 0; j < waypoints.length(); j++)
        {
            if(waypoints[j].links[1] == 0xFFFF)
            {
                continue;
            }
            waypoint &w = waypoints[total];
            if(j != total)
            {
                w = waypoints[j];
            }
            int k = 0;
            for(int i = 0; i < maxwaypointlinks; ++i)
            {
                int link = w.links[i];
                if(!link)
                {
                    break;
                }
                if((w.links[k] = remap[link]))
                {
                    k++;
                }
            }
            if(k < maxwaypointlinks)
            {
                w.links[k] = 0;
            }
            total++;
        }
        waypoints.setsize(total);
    }

    bool cleanwaypoints()
    {
        int cleared = 0;
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            if(clipped(w.o))
            {
                w.links[0] = 0;
                w.links[1] = 0xFFFF;
                cleared++;
            }
        }
        if(cleared)
        {
            player1->lastnode = -1;
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i])
                {
                    players[i]->lastnode = -1;
                }
            }
            remapwaypoints();
            clearwpcache();
            return true;
        }
        return false;
    }

    //returns true if there's a waypoint file with the same name as the map being edited
    bool getwaypointfile(const char *mname, char *wptname)
    {
        if(!mname || !*mname)
        {
            mname = getclientmap();
        }
        if(!*mname)
        {
            return false;
        }

        nformatstring(wptname, maxstrlen, "media/map/%s.wpt", mname);
        path(wptname);
        return true;
    }

    //loads waypoints from a file with extension .wpt created by running `savewaypoints`
    void loadwaypoints(bool force, const char *mname)
    {
        string wptname;
        if(!getwaypointfile(mname, wptname))
        {
            return;
        }
        if(!force && (waypoints.length() || !strcmp(loadedwaypoints, wptname)))
        {
            return;
        }
        stream *f = opengzfile(wptname, "rb");
        if(!f)
        {
            return;
        }
        char magic[4];
        if(f->read(magic, 4) < 4 || memcmp(magic, "OWPT", 4))
        {
            delete f;
            return;
        }

        copystring(loadedwaypoints, wptname);

        waypoints.setsize(0);
        waypoints.add(vec(0, 0, 0));
        ushort numwp = f->get<ushort>();
        for(int i = 0; i < numwp; ++i)
        {
            if(f->end())
            {
                break;
            }
            vec o;
            o.x = f->get<float>();
            o.y = f->get<float>();
            o.z = f->get<float>();
            waypoint &w = waypoints.add(waypoint(o, getweight(o)));
            int numlinks = f->getchar(), k = 0;
            for(int i = 0; i < numlinks; ++i)
            {
                if((w.links[k] = f->get<ushort>()))
                {
                    if(++k >= maxwaypointlinks)
                    {
                        break;
                    }
                }
            }
        }

        delete f;
        conoutf("loaded %d waypoints from %s", numwp, wptname);

        if(!cleanwaypoints())
        {
            clearwpcache();
        }
    }
    ICOMMAND(loadwaypoints, "s", (char *mname), loadwaypoints(true, mname));

    //writes the waypoints on the map to a file named <mapname>.wpt by default, can also write to a custom file name
    void savewaypoints(bool force, const char *mname)
    {
        if((!dropwaypoints && !force) || waypoints.empty())
        {
            return;
        }
        string wptname;
        if(!getwaypointfile(mname, wptname))
        {
            return;
        }
        stream *f = opengzfile(wptname, "wb");
        if(!f)
        {
            return;
        }
        f->write("OWPT", 4);
        f->put<ushort>(waypoints.length()-1);
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            f->put<float>(w.o.x);
            f->put<float>(w.o.y);
            f->put<float>(w.o.z);
            int numlinks = 0;
            for(int j = 0; j < maxwaypointlinks; ++j)
            {
                if(!w.links[j])
                {
                    break;
                }
                numlinks++;
            }
            f->putchar(numlinks);
            for(int j = 0; j < numlinks; ++j)
            {
                f->put<ushort>(w.links[j]);
            }
        }
        delete f;
        conoutf("saved %d waypoints to %s", waypoints.length()-1, wptname);
    }

    ICOMMAND(savewaypoints, "s", (char *mname), savewaypoints(true, mname));

    //deletes waypoints within bounds of selection (w.o.* >= 0.x etc. are bounds checks)
    void delselwaypoints()
    {
        if(noedit(true))
        {
            return;
        }
        vec o = vec(sel.o).sub(0.1f),
            s = vec(sel.s).mul(sel.grid).add(o).add(0.1f);
        int cleared = 0;
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            if(w.o.x >= o.x && w.o.x <= s.x && w.o.y >= o.y && w.o.y <= s.y && w.o.z >= o.z && w.o.z <= s.z)
            {
                w.links[0] = 0;
                w.links[1] = 0xFFFF;
                cleared++;
            }
        }
        if(cleared)
        {
            player1->lastnode = -1;
            remapwaypoints();
            clearwpcache();
        }
    }
    COMMAND(delselwaypoints, "");

    //moves waypoints by a linear translation in x,y,z
    //(obviously) does nothing if not in edit mode
    //deletes waypoints entirely if the dx/dy/dz would move a distance larger than map size
    void movewaypoints(const vec &d)
    {
        if(noedit(true))
        {
            return;
        }
        int worldsize = getworldsize();
        if(d.x < -worldsize || d.x > worldsize || d.y < -worldsize || d.y > worldsize || d.z < -worldsize || d.z > worldsize)
        {
            clearwaypoints();
            return;
        }
        int cleared = 0;
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            w.o.add(d);
            if(!insideworld(w.o))
            {
                w.links[0] = 0;
                w.links[1] = 0xFFFF;
                cleared++;
            }
        }
        if(cleared)
        {
            player1->lastnode = -1;
            remapwaypoints();
        }
        clearwpcache();
    }
    ICOMMAND(movewaypoints, "iii", (int *dx, int *dy, int *dz), movewaypoints(vec(*dx, *dy, *dz)));
}
