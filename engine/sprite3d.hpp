#pragma once
#include "triangle3d.hpp"
#include "vector.hpp"
#include "../engine_config.hpp"
#include "math.h"
#include <cstddef>
#include <cstdint>

// Bindings can compile against older engine snapshots without these optional APIs.
#define ENGINE_SPRITE3D_BUFFER_API 1

#include ENGINE_MEM_INCLUDE

#ifndef ENGINE_MAX_TRIANGLES_PER_SPRITE
#define ENGINE_MAX_TRIANGLES_PER_SPRITE 64
#endif

typedef enum
{
    SPRITE_HUMANOID = 0,
    SPRITE_TREE = 1,
    SPRITE_HOUSE = 2,
    SPRITE_PILLAR = 3,
    SPRITE_CUSTOM = 4
} SpriteType;

class Sprite3D
{
private:
    Triangle3D *triangles;
    uint16_t triangle_count;
    uint16_t triangle_capacity;
    Vector position;
    float rotation_y;
    float scale_factor;
    SpriteType type;
    bool active;

    void transformVertex(float x, float y, float z, float cos_a, float sin_a,
                         float &out_x, float &out_y, float &out_z) const;

public:
    enum class BufferResult
    {
        Ok,
        InvalidBuffer,
        InvalidValue,
        TriangleLimit,
        OutOfMemory,
        NotEmpty
    };

    enum class TransformKind { Move, Scale, Rotate };

    struct Bounds
    {
        double low[3];
        double high[3];
    };

    // View-space basis rows map model coordinates to screen X, screen Y, depth.
    // Orthographic previews compensate for the engine's perspective projection.
    struct PreviewOptions
    {
        double center[3] = {0, 0, 0};
        double basis[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        double distance = 1;
        double orthoDistance = 1;
        double scale = 1;
        double panelScale = 1;
        double offsetX = 0;
        double offsetY = 0;
        double aspect = 1;
        double edge = 1;
        bool panel = false;
        bool orthographic = false;
        bool culling = false;
        bool facing = false;
        int wireframe = -1; // -1 preserves each record; 0 solid; 1 wireframe.
    };

    Sprite3D();
    ~Sprite3D();

    // Allocation failure preserves geometry. clearTriangles releases capacity.
    bool reserveTriangles(uint16_t capacity);
    bool addTriangle(const Triangle3D &triangle);
    bool addTriangle(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, uint16_t color = 0x0000, bool wireframe = true);
    bool bakeTransform();
    void clearTriangles();
    bool createHumanoid(float height = 1.8f, uint16_t color = 0x0000, bool wireframe = true);
    bool createTree(float height = 2.0f, uint16_t color = 0x0000, bool wireframe = true);
    bool createHouse(float width = 2.0f, float height = 2.5f, uint16_t color = 0x0000, bool wireframe = true);
    bool createPillar(float height = 3.0f, float radius = 0.3f, uint16_t color = 0x0000, bool wireframe = true);
    bool createWall(float x, float y, float z, float width = 4.0f, float height = 1.5f, float depth = 0.2f, uint16_t color = 0x0000, bool wireframe = true);
    bool createCube(float x, float y, float z, float width, float height, float depth, uint16_t color = 0x0000, bool wireframe = true);
    bool createCylinder(float x, float y, float z, float radius, float height, uint8_t segments, uint16_t color = 0x0000, bool wireframe = true);
    bool createSphere(float x, float y, float z, float radius, uint8_t segments, uint16_t color = 0x0000, bool wireframe = true);
    bool createTriangularPrism(float x, float y, float z, float width, float height, float depth, uint16_t color = 0x0000, bool wireframe = true);
    bool fromPath(const char *path, bool wireframe = true);

    // Packed records: nine little-endian IEEE-754 floats, RGB565 uint16,
    // wireframe byte (0/1), and one opaque byte: 40 bytes per triangle.
    // Coordinates must be finite and within +/-1e12. Unaligned data is allowed.
    // Construction requires an empty mesh and leaves it empty on failure.
    BufferResult loadBuffer(const void *records, size_t size, Bounds *bounds = nullptr);
    BufferResult buildPreview(const void *records, size_t size, const PreviewOptions &options);

    // No allocation. Source and destination must be disjoint, equal-sized buffers.
    // Optional mask has one byte per vertex. Zero means leave that vertex unchanged.
    // Rotation uses degrees in XYZ order; scale components must be in (0, 1e6].
    // Validate the whole result before writing; failure leaves target/bounds unchanged.
    // Values, pivot and bounds must not overlap the destination buffer.
    static BufferResult transformBuffer(const void *source, size_t size, void *target,
                                       size_t targetSize, TransformKind kind,
                                       const double values[3], const double pivot[3],
                                       const uint8_t *mask = nullptr, size_t maskSize = 0,
                                       Bounds *bounds = nullptr);

    Vector getPosition() const { return position; }
    bool getTriangle(uint16_t index, Triangle3D &out) const;
    bool getTriangle(uint16_t index, float &x1, float &y1, float &z1,
                     float &x2, float &y2, float &z2,
                     float &x3, float &y3, float &z3, uint16_t &color, bool &wireframe) const;
    bool getWorldTriangle(uint16_t index, Triangle3D &out) const;
    float getRotation() const { return rotation_y; }
    float getScale() const { return scale_factor; }
    bool getTransformedTriangle(uint16_t index, const Vector &camera_pos, Triangle3D &out) const;
    uint16_t getTriangleCount() const { return triangle_count; }
    SpriteType getType() const { return type; }
    bool initializeAsHouse(Vector pos, float width, float height, float rot, uint16_t color = 0x0000, bool wireframe = true);
    bool initializeAsHumanoid(Vector pos, float height, float rot, uint16_t color = 0x0000, bool wireframe = true);
    bool initializeAsPillar(Vector pos, float height, float radius, uint16_t color = 0x0000, bool wireframe = true);
    bool initializeAsTree(Vector pos, float height, uint16_t color = 0x0000, bool wireframe = true);
    bool isActive() const { return active; }
    void setActive(bool state) { active = state; }
    void setPosition(Vector pos) { position = pos; }
    void setRotation(float rot) { rotation_y = rot; }
    void setScale(float scale) { scale_factor = scale; }
    void setWireframe(bool wireframe);
    bool toPath(const char *path) const;
    bool updateTriangle(uint16_t index, const Triangle3D &triangle);
    bool updateTriangle(uint16_t index, float x1, float y1, float z1,
                        float x2, float y2, float z2,
                        float x3, float y3, float z3, uint16_t color = 0x0000, bool wireframe = true);
};
