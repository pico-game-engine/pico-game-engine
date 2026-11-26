#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

// Optional asset-owned collider data, never allocated for entities without it.
// PCB1: magic + LE uint32 count + count * 7 LE float32 values:
// center x/y/z, half extents x/y/z, yaw radians. Y is vertical.
class CollisionBoxes {
public:
    static constexpr unsigned MAX_BOXES = 4;
    static constexpr size_t MAX_BYTES = 8 + MAX_BOXES * 28;
    struct Box { float x,y,z,hx,hy,hz,yaw; };
private:
    Box boxes_[MAX_BOXES] = {};
    unsigned count_ = 0;
    static uint32_t word(const uint8_t *p) {
        return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24);
    }
    static bool segmentRect(float x0,float z0,float x1,float z1,float hx,float hz) {
        float enter=0,leave=1;
        const float start[2]={x0,z0}, delta[2]={x1-x0,z1-z0}, extent[2]={hx,hz};
        for(unsigned axis=0;axis<2;++axis) {
            if(delta[axis]==0) { if(fabsf(start[axis])>extent[axis]) return false; }
            else {
                float a=(-extent[axis]-start[axis])/delta[axis];
                float b=(extent[axis]-start[axis])/delta[axis];
                if(a>b) {const float swap=a;a=b;b=swap;}
                enter=fmaxf(enter,a);leave=fminf(leave,b);
                if(enter>leave) return false;
            }
        }
        return true;
    }
    static bool segmentCircle(float x0,float z0,float x1,float z1,float cx,float cz,float radius) {
        const float dx=x1-x0,dz=z1-z0,length2=dx*dx+dz*dz;
        const float t=length2>0 ? fminf(1,fmaxf(0,((cx-x0)*dx+(cz-z0)*dz)/length2)) : 0;
        const float ex=x0+t*dx-cx,ez=z0+t*dz-cz;
        return ex*ex+ez*ez<=radius*radius;
    }
public:
    unsigned count() const { return count_; }
    // Invalid input leaves the previous valid collider set unchanged.
    bool load(const uint8_t *data, size_t size) {
        static_assert(sizeof(float)==4, "PCB1 requires 32-bit floats");
        if (!data || size<8 || memcmp(data,"PCB1",4)) return false;
        const unsigned n=word(data+4);
        if(n>MAX_BOXES || size!=8+n*28) return false;
        Box pending[MAX_BOXES] = {};
        for(unsigned i=0;i<n;++i) {
            float f[7];
            for(unsigned j=0;j<7;++j) {
                uint32_t bits=word(data+8+i*28+j*4);
                memcpy(&f[j],&bits,4);
                if(!isfinite(f[j]) || fabsf(f[j])>1e30f) return false;
            }
            if(f[3]<=0 || f[4]<=0 || f[5]<=0) return false;
            pending[i]={f[0],f[1],f[2],f[3],f[4],f[5],f[6]};
        }
        memcpy(boxes_,pending,sizeof(boxes_)); count_=n; return true;
    }
    // Circle against oriented ground footprints. Asset translation, yaw and
    // positive uniform scale are independent of its mesh/billboard rendering.
    // Height intentionally ignored: games choose whether a prop blocks ground movement.
    bool blocksGround(float x,float z,float radius,float assetX=0,float assetZ=0,
                      float assetYaw=0,float scale=1) const {
        if(!isfinite(x)||!isfinite(z)||!isfinite(radius)||radius<0 ||
           !isfinite(assetX)||!isfinite(assetZ)||!isfinite(assetYaw)||!isfinite(scale)||scale<=0) return false;
        const float c=cosf(assetYaw),s=sinf(assetYaw);
        const float dx=x-assetX,dz=z-assetZ;
        const float lx=(c*dx+s*dz)/scale,lz=(-s*dx+c*dz)/scale,r=radius/scale;
        for(unsigned i=0;i<count_;++i) {
            const Box &b=boxes_[i];
            const float bc=cosf(b.yaw),bs=sinf(b.yaw),bx=lx-b.x,bz=lz-b.z;
            const float ex=fmaxf(fabsf(bc*bx+bs*bz)-b.hx,0);
            const float ez=fmaxf(fabsf(-bs*bx+bc*bz)-b.hz,0);
            if(ex*ex+ez*ez<=r*r) return true;
        }
        return false;
    }
    // Swept circle versus the full ground footprint: prevents tunnelling when
    // one frame's movement is wider than a thin collider. Corners remain rounded.
    bool blocksGroundSegment(float x0,float z0,float x1,float z1,float radius,
                             float assetX=0,float assetZ=0,float assetYaw=0,float scale=1) const {
        if(!isfinite(x0)||!isfinite(z0)||!isfinite(x1)||!isfinite(z1)||
           !isfinite(radius)||radius<0||!isfinite(assetX)||!isfinite(assetZ)||
           !isfinite(assetYaw)||!isfinite(scale)||scale<=0) return false;
        // Canonical endpoint order avoids direction-dependent float rounding at tangencies.
        if(x0>x1 || (x0==x1 && z0>z1)) {
            float swap=x0;x0=x1;x1=swap;swap=z0;z0=z1;z1=swap;
        }
        const float c=cosf(assetYaw),s=sinf(assetYaw),r=radius/scale;
        const float dx0=x0-assetX,dz0=z0-assetZ,dx1=x1-assetX,dz1=z1-assetZ;
        const float lx0=(c*dx0+s*dz0)/scale,lz0=(-s*dx0+c*dz0)/scale;
        const float lx1=(c*dx1+s*dz1)/scale,lz1=(-s*dx1+c*dz1)/scale;
        for(unsigned i=0;i<count_;++i) {
            const Box &b=boxes_[i];
            const float bc=cosf(b.yaw),bs=sinf(b.yaw);
            const float bx0=lx0-b.x,bz0=lz0-b.z,bx1=lx1-b.x,bz1=lz1-b.z;
            const float ax=bc*bx0+bs*bz0,az=-bs*bx0+bc*bz0;
            const float ex=bc*bx1+bs*bz1,ez=-bs*bx1+bc*bz1;
            // Rectangle expanded by a disk = two strips plus four corner disks.
            if(segmentRect(ax,az,ex,ez,b.hx+r,b.hz) ||
               segmentRect(ax,az,ex,ez,b.hx,b.hz+r)) return true;
            for(int sx=-1;sx<=1;sx+=2) for(int sz=-1;sz<=1;sz+=2)
                if(segmentCircle(ax,az,ex,ez,sx*b.hx,sz*b.hz,r)) return true;
        }
        return false;
    }

};
