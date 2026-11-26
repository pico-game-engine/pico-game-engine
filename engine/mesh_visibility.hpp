#pragma once
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Per-mesh visibility, using one depth/color scanline rather than a screen-sized
// depth buffer. Vertices retain affine UVs to match the existing texture backend.
class MeshVisibility
{
public:
    struct Vertex { float x, y, z, u, v; };
    struct Triangle { Vertex v[3]; uint16_t tint; int16_t first, last; };
    MeshVisibility() : blocks(nullptr), blockCount(0), pointerCapacity(0), count(0), row(nullptr), rowCapacity(0) {}
    ~MeshVisibility()
    {
        for (size_t i = 0; i < blockCount; ++i) ENGINE_MEM_FREE(blocks[i]);
        ENGINE_MEM_FREE(blocks);
        ENGINE_MEM_FREE(row);
    }
    MeshVisibility(const MeshVisibility &) = delete;
    MeshVisibility &operator=(const MeshVisibility &) = delete;

    bool reserve(size_t n, uint16_t width)
    {
        count = 0;
        if (n > SIZE_MAX - (blockSize - 1)) return false;
        const size_t required = (n + blockSize - 1) / blockSize;
        if (required > SIZE_MAX / sizeof(Triangle *)) return false;
        if (required > pointerCapacity)
        {
            Triangle **replacement = static_cast<Triangle **>(ENGINE_MEM_MALLOC(required * sizeof(Triangle *)));
            if (!replacement) return false;
            if (blockCount) memcpy(replacement, blocks, blockCount * sizeof(Triangle *));
            ENGINE_MEM_FREE(blocks);
            blocks = replacement;
            pointerCapacity = required;
        }
        // Small retained blocks avoid a large contiguous allocation and avoid
        // holding both old and new triangle arrays during a camera-angle change.
        while (blockCount < required)
        {
            Triangle *block = static_cast<Triangle *>(ENGINE_MEM_MALLOC(blockSize * sizeof(Triangle)));
            if (!block) return false;
            blocks[blockCount++] = block;
        }
        if (width > rowCapacity)
        {
            void *replacement = ENGINE_MEM_MALLOC(static_cast<size_t>(width) * (sizeof(float) + sizeof(uint16_t)));
            if (!replacement) return false;
            ENGINE_MEM_FREE(row); row = replacement; rowCapacity = width;
        }
        return true;
    }

    // Clip against the same near plane as Level's existing projection path.
    static int project(const Vertex input[3], uint16_t tint, uint16_t width,
                       uint16_t height, Triangle output[2])
    {
        Vertex clipped[4]; int n = 0;
        Vertex previous = input[2]; bool previousInside = previous.z >= .1f;
        for (int i = 0; i < 3; ++i)
        {
            Vertex current = input[i]; bool inside = current.z >= .1f;
            if (inside != previousInside)
            {
                float t = (.1f - previous.z) / (current.z - previous.z);
                clipped[n++] = {previous.x + t * (current.x - previous.x),
                    previous.y + t * (current.y - previous.y), .1f,
                    previous.u + t * (current.u - previous.u), previous.v + t * (current.v - previous.v)};
            }
            if (inside) clipped[n++] = current;
            previous = current; previousInside = inside;
        }
        int result = 0;
        for (int fan = 1; fan + 1 < n; ++fan)
        {
            Triangle t; t.tint = tint;
            int indices[3] = {0, fan, fan + 1};
            float minX = width, maxX = 0, minY = height, maxY = 0;
            bool finite = true;
            for (int i = 0; i < 3; ++i)
            {
                Vertex v = clipped[indices[i]];
                float inv = 1.f / v.z;
                t.v[i] = {v.x * height * inv + width * .5f,
                          -v.y * height * inv + height * .5f, inv, v.u, v.v};
                finite = finite && isfinite(t.v[i].x) && isfinite(t.v[i].y) && isfinite(v.u) && isfinite(v.v);
                minX = fminf(minX, t.v[i].x); maxX = fmaxf(maxX, t.v[i].x);
                minY = fminf(minY, t.v[i].y); maxY = fmaxf(maxY, t.v[i].y);
            }
            if (!finite || maxX <= 0 || minX >= width || maxY <= 0 || minY >= height) continue;
            float area = (t.v[1].x-t.v[0].x)*(t.v[2].y-t.v[0].y) - (t.v[1].y-t.v[0].y)*(t.v[2].x-t.v[0].x);
            if (fabsf(area) < 1e-8f) continue;
            t.first = static_cast<int16_t>(ceilf(fmaxf(0, minY - .5f)));
            t.last = static_cast<int16_t>(fminf(height - 1, ceilf(maxY - .5f) - 1));
            if (t.first <= t.last) output[result++] = t;
        }
        return result;
    }
    void append(const Triangle &t)
    {
        if (count < blockCount * blockSize) { triangle(count) = t; ++count; }
    }

    template<class Blit>
    void render(uint16_t width, uint16_t height, const uint16_t *texture,
                uint16_t textureWidth, uint16_t textureHeight, Blit blit)
    {
        if (!count || !row || !texture || !textureWidth || !textureHeight) return;
        float *depth = static_cast<float *>(row);
        uint16_t *colors = reinterpret_cast<uint16_t *>(depth + rowCapacity);
        int first = height, last = -1;
        for (size_t i=0; i<count; ++i) { if(triangle(i).first<first) first=triangle(i).first; if(triangle(i).last>last) last=triangle(i).last; }
        for (int y=first; y<=last; ++y)
        {
            memset(depth, 0, width * sizeof(float));
            float sy = y + .5f;
            for (size_t i=0; i<count; ++i)
            {
                const Triangle &t = triangle(i);
                if (y<t.first || y>t.last) continue;
                Vertex ends[2]; int found=0;
                for(int j=0; j<3; ++j)
                {
                    const Vertex &a=t.v[j], &b=t.v[(j+1)%3];
                    if ((sy>=a.y && sy<b.y) || (sy>=b.y && sy<a.y))
                    {
                        float f=(sy-a.y)/(b.y-a.y);
                        ends[found++]={a.x+f*(b.x-a.x), sy, a.z+f*(b.z-a.z), a.u+f*(b.u-a.u), a.v+f*(b.v-a.v)};
                    }
                }
                if(found!=2) continue;
                if(ends[0].x>ends[1].x) { Vertex swap=ends[0]; ends[0]=ends[1]; ends[1]=swap; }
                float span=ends[1].x-ends[0].x;
                if(span<=0) continue;
                int left=static_cast<int>(ceilf(fmaxf(0, fminf(width, ends[0].x-.5f))));
                int right=static_cast<int>(ceilf(fmaxf(0, fminf(width, ends[1].x-.5f))))-1;
                for(int x=left; x<=right; ++x)
                {
                    float f=(x+.5f-ends[0].x)/span;
                    float z=ends[0].z+f*(ends[1].z-ends[0].z);
                    if(z<=depth[x]) continue;
                    float u=ends[0].u+f*(ends[1].u-ends[0].u), v=ends[0].v+f*(ends[1].v-ends[0].v);
                    int tx=static_cast<int>(fmaxf(0, fminf(textureWidth-1, u*textureWidth)));
                    int ty=static_cast<int>(fmaxf(0, fminf(textureHeight-1, v*textureHeight)));
                    uint16_t texel=texture[static_cast<size_t>(ty)*textureWidth+tx];
                    uint16_t r=(texel>>11)&31, g=(texel>>5)&63, b=texel&31;
                    uint16_t tr=(t.tint>>11)&31, tg=(t.tint>>5)&63, tb=t.tint&31;
                    r=(((r<<3)|(r>>2))*((tr<<3)|(tr>>2)))/2040;
                    g=(((g<<2)|(g>>4))*((tg<<2)|(tg>>4)))/1020;
                    b=(((b<<3)|(b>>2))*((tb<<3)|(tb>>2)))/2040;
                    colors[x]=(r<<11)|(g<<5)|b; depth[x]=z;
                }
            }
            for(int x=0; x<width;)
            {
                while(x<width && depth[x]==0) ++x;
                int begin=x;
                while(x<width && depth[x]>0) ++x;
                if(x>begin) blit(begin,y,colors+begin,x-begin);
            }
        }
    }
private:
    static const size_t blockSize = 64;
    Triangle &triangle(size_t index) { return blocks[index / blockSize][index % blockSize]; }
    Triangle **blocks;
    size_t blockCount, pointerCapacity, count;
    void *row; uint16_t rowCapacity;
};
