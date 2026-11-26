#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <vector>
#include <algorithm>
#include <cmath>
static bool failAllocation=false;
static size_t maximumAllocation=static_cast<size_t>(-1);
static int allocationsRemaining=-1;
static void *allocate(size_t n) { if (failAllocation || n>maximumAllocation || allocationsRemaining==0) return nullptr;
    if (allocationsRemaining>0) --allocationsRemaining;
    return malloc(n); }
#define ENGINE_MEM_MALLOC allocate
#define ENGINE_MEM_FREE free
#include "../engine/mesh_visibility.hpp"

static const int W=64,H=48;
using V=MeshVisibility::Vertex;
using T=MeshVisibility::Triangle;
static std::vector<uint16_t> render(const std::vector<T> &triangles)
{
    MeshVisibility raster; assert(raster.reserve(triangles.size(),W));
    for(const auto &t:triangles) raster.append(t);
    std::vector<uint16_t> pixels(W*H,0); uint16_t texture=0xffff;
    raster.render(W,H,&texture,1,1,[&](int x,int y,const uint16_t *p,int n){
        assert(x>=0 && x+n<=W && y>=0 && y<H);
        std::copy(p,p+n,pixels.begin()+y*W+x);
    });
    return pixels;
}
static T screenTriangle(float za,float zb,float zc,uint16_t color)
{
    T t={{{5,5,za,0,0},{59,5,zb,0,0},{32,44,zc,0,0}},color,5,43}; return t;
}
int main()
{
    // Same projected footprint, crossing depth planes: neither whole-triangle
    // order is correct. Both sides must select their nearest surface.
    T red=screenTriangle(.8f,.2f,.5f,0xf800), green=screenTriangle(.3f,.7f,.5f,0x07e0);
    auto a=render({red,green}), b=render({green,red});
    assert(a[10*W+15]==0xf800 && a[10*W+49]==0x07e0);
    for(int y=0;y<H;++y) for(int x=0;x<W;++x)
        if(x!=31 && x!=32) assert(a[y*W+x]==b[y*W+x]);
    // Independent barycentric depth reference over random overlapping meshes.
    srand(7);
    for(int round=0;round<40;++round)
    {
        std::vector<T> triangles;
        for(int i=0;i<30;++i)
        {
            T t; t.tint=static_cast<uint16_t>(1+rand()%65534);t.first=0;t.last=H-1;
            for(auto &v:t.v) v={float(rand()%90-13)+.123f,float(rand()%70-11)+.234f,.1f+float(rand()%100)/100,0,0};
            triangles.push_back(t);
        }
        auto pixels=render(triangles);
        for(int y=0;y<H;++y) for(int x=0;x<W;++x)
        {
            float closest=0;uint16_t expected=0;bool boundary=false;
            for(const T &t:triangles)
            {
                const auto &a=t.v[0],&b=t.v[1],&c=t.v[2];
                float d=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
                if(fabsf(d)<1e-8f) continue;
                float u=((b.y-c.y)*(x+.5f-c.x)+(c.x-b.x)*(y+.5f-c.y))/d;
                float v=((c.y-a.y)*(x+.5f-c.x)+(a.x-c.x)*(y+.5f-c.y))/d;
                float w=1-u-v;
                if(u>=0 && v>=0 && w>=0)
                {
                    if(fminf(u,fminf(v,w))<1e-5f) boundary=true;
                    float depth=u*a.z+v*b.z+w*c.z;
                    if(fabsf(depth-closest)<1e-5f) boundary=true;
                    if(depth>closest) { closest=depth;expected=t.tint; }
                }
            }
            if(!boundary) assert(pixels[y*W+x]==expected);
        }
    }
    T out[2];V near[3]={{-.02f,-.02f,.05f,0,0},{.02f,-.02f,.2f,1,0},{0,.02f,.2f,.5f,1}};
    assert(MeshVisibility::project(near,0xffff,W,H,out)==2);
    for(auto &v:near)v.z=-1;
    assert(MeshVisibility::project(near,0xffff,W,H,out)==0);
    V degenerate[3]={{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0}};
    assert(MeshVisibility::project(degenerate,0xffff,W,H,out)==0);
    MeshVisibility raster;failAllocation=true;assert(!raster.reserve(3,W));failAllocation=false;
    assert(raster.reserve(3,W));assert(raster.reserve(0,W));
    maximumAllocation=5000;
    MeshVisibility fragmented;
    assert(fragmented.reserve(400,320));
    assert(fragmented.reserve(1092,320));
    maximumAllocation=static_cast<size_t>(-1);
    MeshVisibility partial;
    allocationsRemaining=2;
    assert(!partial.reserve(192,W));
    allocationsRemaining=-1;
    assert(partial.reserve(192,W));
    printf("PASS: crossing depths, winding/order independence, 122880 reference pixels, near clipping, degenerates, allocation failure; triangle bytes=%zu\n",sizeof(T));
}
