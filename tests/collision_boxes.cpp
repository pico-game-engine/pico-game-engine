#include "../engine/collision_boxes.hpp"
#include <assert.h>
#include <stdio.h>
#include <vector>
static std::vector<uint8_t> fixture(unsigned n) {
    std::vector<uint8_t> d(8+n*28);memcpy(d.data(),"PCB1",4); d[4]=n;
    for(unsigned i=0;i<n;++i) {
        float f[]={float(i*4),.5f,0,1,.5f,.25f,0};
        for(unsigned j=0;j<7;++j){uint32_t u;memcpy(&u,f+j,4); for(unsigned k=0;k<4;++k)d[8+i*28+j*4+k]=u>>(8*k);}
    } return d;
}
int main(int argc,char **argv) {
    CollisionBoxes b;auto d=fixture(1);assert(b.load(d.data(),d.size()));
    assert(b.blocksGround(0,0,0));assert(!b.blocksGround(0,.5f,.1f));
    assert(b.blocksGround(1.1f,0,.2f));assert(!b.blocksGround(1.15f,.4f,.2f)); // rounded corner
    assert(b.blocksGround(3,4.9f,.01f,3,4,1.57079632679f));
    assert(!b.blocksGround(3.9f,4,.01f,3,4,1.57079632679f));
    assert(b.blocksGround(1.9f,0,0,0,0,0,2));
    // Both endpoints clear but the full movement crosses the thin box.
    assert(!b.blocksGround(0,.51f,.25f) && !b.blocksGround(0,-.89f,.25f));
    assert(b.blocksGroundSegment(0,.51f,0,-.89f,.25f));
    assert(!b.blocksGroundSegment(1.15f,.4f,1.2f,.4f,.2f)); // rounded corner clearance
    assert(b.blocksGroundSegment(1.1f,.3f,1.1f,.5f,.2f));
    assert(b.blocksGroundSegment(-2,0,2,0,0));
    assert(!b.blocksGroundSegment(-2,1,2,1,.1f));
    assert(b.blocksGroundSegment(3,1,3,7,.25f,3,4,1.57079632679f,2));
    assert(!b.blocksGroundSegment(0,0,1,1,-1));
    for(int ix=-30;ix<=30;++ix) for(int iz=-20;iz<=20;++iz) {
        const float x=ix*.07f,z=iz*.07f;
        assert(b.blocksGroundSegment(x,z,x,z,.2f)==b.blocksGround(x,z,.2f));
    }
    for(int i=0;i<400;++i) {
        const float ax=((i*17)%101-50)*.05f,az=((i*29)%101-50)*.05f;
        const float ex=((i*31)%101-50)*.05f,ez=((i*43)%101-50)*.05f;
        const bool hit=b.blocksGroundSegment(ax,az,ex,ez,.2f);
        assert(hit==b.blocksGroundSegment(ex,ez,ax,az,.2f));
        for(int k=0;k<=200;++k) {
            const float t=k/200.f;
            if(b.blocksGround(ax+(ex-ax)*t,az+(ez-az)*t,.2f)) assert(hit);
        }
    }
    d=fixture(4);assert(b.load(d.data(),d.size())&&b.count()==4&&b.blocksGround(12,0,0));
    d=fixture(5);assert(!b.load(d.data(),d.size())&&b.count()==4);
    d=fixture(1);assert(!b.load(d.data(),d.size()-1));d[0]='X';assert(!b.load(d.data(),d.size()));
    d=fixture(1);memset(d.data()+8+3*4,0,4);assert(!b.load(d.data(),d.size()));
    d=fixture(1);d[8+3]=0x7f;d[8+2]=0xc0;assert(!b.load(d.data(),d.size()));
    d=fixture(0);assert(b.load(d.data(),d.size())&&b.count()==0&&!b.blocksGround(0,0,1));
    if(argc>1) { FILE *f=fopen(argv[1],"rb");assert(f);uint8_t buf[CollisionBoxes::MAX_BYTES];size_t n=fread(buf,1,sizeof(buf),f);fclose(f);assert(b.load(buf,n)&&b.count()==1);assert(b.blocksGround(2,-3,0));assert(!b.blocksGround(20,20,.1f)); }
    puts("PASS: collider limits, malformed files, rotated/scaled footprints, corners, swept movement, point equivalence, 80400 reference samples and Blender fixture");
}
