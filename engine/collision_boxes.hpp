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
};
