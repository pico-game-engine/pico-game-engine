#include "sprite3d.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

#ifdef ENGINE_STORAGE_INCLUDE
#include ENGINE_STORAGE_INCLUDE
#endif

#ifdef ENGINE_LOG_INCLUDE
#include ENGINE_LOG_INCLUDE
#endif

// Shade an RGB565 color by a factor (< 1.0 darkens, > 1.0 lightens)
static uint16_t shadeColor565(uint16_t color, float factor)
{
    uint8_t r = (uint8_t)(((color >> 11) & 0x1F) * factor);
    uint8_t g = (uint8_t)(((color >> 5) & 0x3F) * factor);
    uint8_t b = (uint8_t)((color & 0x1F) * factor);
    if (r > 0x1F)
        r = 0x1F;
    if (g > 0x3F)
        g = 0x3F;
    if (b > 0x1F)
        b = 0x1F;
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}

Sprite3D::Sprite3D() : triangles(nullptr), triangle_count(0), triangle_capacity(0), position(Vector(0, 0)), rotation_y(0),
                       scale_factor(1.0f), type(SPRITE_CUSTOM), active(false)
{
    triangles = nullptr;
}

Sprite3D::~Sprite3D()
{
    clearTriangles();
}

bool Sprite3D::reserveTriangles(uint16_t capacity)
{
    if (capacity > ENGINE_MAX_TRIANGLES_PER_SPRITE)
        return false;
    if (capacity <= triangle_capacity)
        return true;
    Triangle3D *new_block = (Triangle3D *)ENGINE_MEM_MALLOC(capacity * sizeof(Triangle3D));
    if (!new_block)
        return false;
    if (triangle_count > 0)
        memcpy(new_block, triangles, triangle_count * sizeof(Triangle3D));
    if (triangles != nullptr)
        ENGINE_MEM_FREE(triangles);
    triangles = new_block;
    triangle_capacity = capacity;
    return true;
}

bool Sprite3D::addTriangle(const Triangle3D &triangle)
{
    if (triangle_count >= ENGINE_MAX_TRIANGLES_PER_SPRITE)
        return false;
    // Keep a value copy in case the argument aliases the buffer being grown.
    const Triangle3D appended = triangle;
    if (triangle_count == triangle_capacity)
    {
        // Geometric growth avoids a full allocation/copy on every append.
        // 1.5x limits spare capacity on small embedded heaps.
        uint32_t capacity = triangle_capacity ? triangle_capacity + triangle_capacity / 2 : 8;
        if (capacity <= triangle_count)
            capacity = triangle_count + 1;
        if (capacity > ENGINE_MAX_TRIANGLES_PER_SPRITE)
            capacity = ENGINE_MAX_TRIANGLES_PER_SPRITE;
        if (!reserveTriangles((uint16_t)capacity))
            return false;
    }
    triangles[triangle_count++] = appended;
    return true;
}

bool Sprite3D::addTriangle(float x1, float y1, float z1,
                           float x2, float y2, float z2,
                           float x3, float y3, float z3,
                           uint16_t color, bool wireframe)
{
    return addTriangle(Triangle3D(x1, y1, z1, x2, y2, z2, x3, y3, z3,
                                  color, wireframe));
}

bool Sprite3D::bakeTransform()
{
    if (rotation_y == 0.0f && scale_factor == 1.0f)
        return true;

    const float cos_a = cosf(rotation_y);
    const float sin_a = sinf(rotation_y);

    for (uint16_t i = 0; i < triangle_count; i++)
    {
        Triangle3D &t = triangles[i];

        t.x1 *= scale_factor;
        t.y1 *= scale_factor;
        t.z1 *= scale_factor;
        t.x2 *= scale_factor;
        t.y2 *= scale_factor;
        t.z2 *= scale_factor;
        t.x3 *= scale_factor;
        t.y3 *= scale_factor;
        t.z3 *= scale_factor;

        float ox = t.x1;
        t.x1 = ox * cos_a - t.z1 * sin_a;
        t.z1 = ox * sin_a + t.z1 * cos_a;
        ox = t.x2;
        t.x2 = ox * cos_a - t.z2 * sin_a;
        t.z2 = ox * sin_a + t.z2 * cos_a;
        ox = t.x3;
        t.x3 = ox * cos_a - t.z3 * sin_a;
        t.z3 = ox * sin_a + t.z3 * cos_a;
    }

    rotation_y = 0.0f;
    scale_factor = 1.0f;
    return true;
}

void Sprite3D::clearTriangles()
{
    if (triangles != nullptr)
        ENGINE_MEM_FREE(triangles);
    triangles = nullptr;
    triangle_count = 0;
    triangle_capacity = 0;
}

bool Sprite3D::createCube(float x, float y, float z, float width, float height, float depth, uint16_t color, bool wireframe)
{
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;

    // Render 4 most important faces (skip top and bottom to save triangles)
    // This gives 8 triangles per cube instead of 12

    // Front face (2 triangles)
    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z + hd,
            x + hw, y - hh, z + hd,
            x + hw, y + hh, z + hd, color, wireframe)))
        return false;

    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z + hd,
            x + hw, y + hh, z + hd,
            x - hw, y + hh, z + hd, color, wireframe)))
        return false;

    // Back face (2 triangles)
    if (!addTriangle(Triangle3D(
            x + hw, y - hh, z - hd,
            x - hw, y - hh, z - hd,
            x - hw, y + hh, z - hd, color, wireframe)))
        return false;

    if (!addTriangle(Triangle3D(
            x + hw, y - hh, z - hd,
            x - hw, y + hh, z - hd,
            x + hw, y + hh, z - hd, color, wireframe)))
        return false;

    // Right face (2 triangles)
    if (!addTriangle(Triangle3D(
            x + hw, y - hh, z + hd,
            x + hw, y - hh, z - hd,
            x + hw, y + hh, z - hd, color, wireframe)))
        return false;

    if (!addTriangle(Triangle3D(
            x + hw, y - hh, z + hd,
            x + hw, y + hh, z - hd,
            x + hw, y + hh, z + hd, color, wireframe)))
        return false;

    // Left face (2 triangles)
    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z - hd,
            x - hw, y - hh, z + hd,
            x - hw, y + hh, z + hd, color, wireframe)))
        return false;

    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z - hd,
            x - hw, y + hh, z + hd,
            x - hw, y + hh, z - hd, color, wireframe)))
        return false;

    return true;
}

bool Sprite3D::createCylinder(float x, float y, float z, float radius, float height, uint8_t segments, uint16_t color, bool wireframe)
{
    float hh = height * 0.5f;

    // Limit segments to prevent too many triangles
    if (segments > 6)
        segments = 6;

    // Only side faces - no caps to save triangles
    for (uint8_t i = 0; i < segments; i++)
    {
        float angle1 = (float)i * 2.0f * M_PI / segments;
        float angle2 = (float)(i + 1) * 2.0f * M_PI / segments;

        float x1 = x + radius * cosf(angle1);
        float z1 = z + radius * sinf(angle1);
        float x2 = x + radius * cosf(angle2);
        float z2 = z + radius * sinf(angle2);

        // Side face triangles only
        if (!addTriangle(Triangle3D(
                x1, y - hh, z1,
                x2, y - hh, z2,
                x2, y + hh, z2, color, wireframe)))
            return false;

        if (!addTriangle(Triangle3D(
                x1, y - hh, z1,
                x2, y + hh, z2,
                x1, y + hh, z1, color, wireframe)))
            return false;
    }

    return true;
}

bool Sprite3D::createHouse(float width, float height, uint16_t color, bool wireframe)
{
    clearTriangles();
    type = SPRITE_HOUSE;

    float wall_height = height * 0.7f;
    float roof_height = height * 0.3f;
    float house_width = width * 1.3f;
    float house_depth = width * 1.1f;

    // House base (cube)
    if (!createCube(0, wall_height / 2, 0, house_width, wall_height, house_depth, color, wireframe))
        return false;

    // Roof (triangular prism)
    return createTriangularPrism(0, wall_height + roof_height / 2, 0, house_width, roof_height, house_depth, shadeColor565(color, 0.6f), wireframe);
}

bool Sprite3D::createHumanoid(float height, uint16_t color, bool wireframe)
{
    clearTriangles();
    type = SPRITE_HUMANOID;

    float head_radius = height * 0.12f;
    float torso_width = height * 0.20f;
    float torso_height = height * 0.35f;
    float leg_height = height * 0.45f;
    float arm_length = height * 0.25f;

    // Head (sphere) - positioned at top
    if (!createSphere(0, height - head_radius, 0, head_radius, 4, shadeColor565(color, 1.25f), wireframe))
        return false;

    // Torso - positioned in middle, wider and deeper
    if (!createCube(0, leg_height + torso_height / 2, 0, torso_width, torso_height, torso_width * 0.8f, color, wireframe))
        return false;

    // Arms - positioned at shoulder level
    float arm_width = torso_width * 0.35f;
    float arm_y = leg_height + torso_height - arm_length / 2;
    const uint16_t arm_color = shadeColor565(color, 0.75f);
    if (!createCube(-torso_width * 0.8f, arm_y, 0, arm_width, arm_length, arm_width, arm_color, wireframe))
        return false;
    if (!createCube(torso_width * 0.8f, arm_y, 0, arm_width, arm_length, arm_width, arm_color, wireframe))
        return false;

    // Legs - positioned so their bottoms touch ground (y=0)
    float leg_width = torso_width * 0.45f;
    const uint16_t leg_color = shadeColor565(color, 0.55f);
    if (!createCube(-leg_width * 0.7f, leg_height / 2, 0, leg_width, leg_height, leg_width, leg_color, wireframe))
        return false;
    if (!createCube(leg_width * 0.7f, leg_height / 2, 0, leg_width, leg_height, leg_width, leg_color, wireframe))
        return false;

    return true;
}

bool Sprite3D::createPillar(float height, float radius, uint16_t color, bool wireframe)
{
    clearTriangles();
    type = SPRITE_PILLAR;
    float pillar_radius = radius * 1.5f;

    // Main cylinder - 6 segments = 12 triangles
    if (!createCylinder(0, height / 2, 0, pillar_radius, height, 6, color, wireframe))
        return false;

    // Base - 4 segments = 8 triangles
    if (!createCylinder(0, pillar_radius * 0.4f, 0, pillar_radius * 1.4f, pillar_radius * 0.8f, 4, color, wireframe))
        return false;

    // Top - 4 segments = 8 triangles
    return createCylinder(0, height - pillar_radius * 0.4f, 0, pillar_radius * 1.4f, pillar_radius * 0.8f, 4, color, wireframe);
}

bool Sprite3D::createSphere(float x, float y, float z, float radius, uint8_t segments, uint16_t color, bool wireframe)
{
    // limit segments for sphere to prevent triangle explosion
    if (segments > 4)
        segments = 4;

    for (uint8_t lat = 0; lat < segments / 2; lat++)
    {
        float theta1 = (float)lat * M_PI / (segments / 2);
        float theta2 = (float)(lat + 1) * M_PI / (segments / 2);

        for (uint8_t lon = 0; lon < segments; lon++)
        {
            float phi1 = (float)lon * 2.0f * M_PI / segments;
            float phi2 = (float)(lon + 1) * 2.0f * M_PI / segments;

            // Calculate vertices
            float x1 = x + radius * sinf(theta1) * cosf(phi1);
            float y1 = y + radius * cosf(theta1);
            float z1 = z + radius * sinf(theta1) * sinf(phi1);
            //
            float x2 = x + radius * sinf(theta1) * cosf(phi2);
            float y2 = y + radius * cosf(theta1);
            float z2 = z + radius * sinf(theta1) * sinf(phi2);
            //
            float x3 = x + radius * sinf(theta2) * cosf(phi1);
            float y3 = y + radius * cosf(theta2);
            float z3 = z + radius * sinf(theta2) * sinf(phi1);
            //
            float x4 = x + radius * sinf(theta2) * cosf(phi2);
            float y4 = y + radius * cosf(theta2);
            float z4 = z + radius * sinf(theta2) * sinf(phi2);

            // Add triangles
            if (lat > 0)
            {
                if (!addTriangle(Triangle3D(x1, y1, z1, x2, y2, z2, x3, y3, z3, color, wireframe)))
                    return false;
            }
            if (lat < segments / 2 - 1)
            {
                if (!addTriangle(Triangle3D(x2, y2, z2, x4, y4, z4, x3, y3, z3, color, wireframe)))
                    return false;
            }
        }
    }
    return true;
}

bool Sprite3D::createTree(float height, uint16_t color, bool wireframe)
{
    clearTriangles();
    type = SPRITE_TREE;

    float trunk_width = height * 0.18f;
    float trunk_height = height * 0.4f;
    float crown_width = height * 0.65f;
    float crown_height = height * 0.6f;

    // Trunk (simple cube) - positioned so bottom touches ground (y=0) - brown
    if (!createCube(0, trunk_height / 2, 0, trunk_width, trunk_height, trunk_width, 0x9A60, wireframe))
        return false;

    // Crown (simple cube representing foliage) - positioned on top of trunk
    return createCube(0, trunk_height + crown_height / 2, 0, crown_width, crown_height, crown_width, color, wireframe);
}

bool Sprite3D::createTriangularPrism(float x, float y, float z, float width, float height, float depth, uint16_t color, bool wireframe)
{
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;

    // Front triangle
    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z + hd,
            x + hw, y - hh, z + hd,
            x, y + hh, z + hd, color, wireframe)))
        return false;

    // Back triangle
    if (!addTriangle(Triangle3D(
            x + hw, y - hh, z - hd,
            x - hw, y - hh, z - hd,
            x, y + hh, z - hd, color, wireframe)))
        return false;

    // Bottom face
    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z - hd,
            x + hw, y - hh, z - hd,
            x + hw, y - hh, z + hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z - hd,
            x + hw, y - hh, z + hd,
            x - hw, y - hh, z + hd, color, wireframe)))
        return false;

    // Side faces
    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z + hd,
            x, y + hh, z + hd,
            x, y + hh, z - hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(
            x - hw, y - hh, z + hd,
            x, y + hh, z - hd,
            x - hw, y - hh, z - hd, color, wireframe)))
        return false;

    if (!addTriangle(Triangle3D(
            x, y + hh, z + hd,
            x + hw, y - hh, z + hd,
            x + hw, y - hh, z - hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(
            x, y + hh, z + hd,
            x + hw, y - hh, z - hd,
            x, y + hh, z - hd, color, wireframe)))
        return false;

    return true;
}

bool Sprite3D::createWall(float x, float y, float z, float width, float height, float depth, uint16_t color, bool wireframe)
{
    // Wall segment using raw triangles, offset by (x, y, z)
    const float hw = width / 2, hh = height / 2, hd = depth / 2;
    // Front face
    if (!addTriangle(Triangle3D(x - hw, y - hh, z + hd, x + hw, y - hh, z + hd, x + hw, y + hh, z + hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(x - hw, y - hh, z + hd, x + hw, y + hh, z + hd, x - hw, y + hh, z + hd, color, wireframe)))
        return false;
    // Back face
    if (!addTriangle(Triangle3D(x + hw, y - hh, z - hd, x - hw, y - hh, z - hd, x - hw, y + hh, z - hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(x + hw, y - hh, z - hd, x - hw, y + hh, z - hd, x + hw, y + hh, z - hd, color, wireframe)))
        return false;
    // Left, right, top caps
    if (!addTriangle(Triangle3D(x - hw, y - hh, z - hd, x - hw, y - hh, z + hd, x - hw, y + hh, z + hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(x - hw, y - hh, z - hd, x - hw, y + hh, z + hd, x - hw, y + hh, z - hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(x + hw, y - hh, z + hd, x + hw, y - hh, z - hd, x + hw, y + hh, z - hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(x + hw, y - hh, z + hd, x + hw, y + hh, z - hd, x + hw, y + hh, z + hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(x - hw, y + hh, z + hd, x + hw, y + hh, z + hd, x + hw, y + hh, z - hd, color, wireframe)))
        return false;
    if (!addTriangle(Triangle3D(x - hw, y + hh, z + hd, x + hw, y + hh, z - hd, x - hw, y + hh, z - hd, color, wireframe)))
        return false;
    return true;
}

bool Sprite3D::fromPath(const char *path, bool wireframe)
{
#ifdef ENGINE_STORAGE_READ
    clearTriangles();

    size_t file_size = ENGINE_STORAGE_SIZE(path);
    if (file_size == 0)
    {
        ENGINE_LOG_INFO("Sprite3D::fromPath failed to read triangle data from path: %s\n", path);
        return false;
    }

    uint16_t max_triangles = file_size / sizeof(Triangle3D);
    if (max_triangles > ENGINE_MAX_TRIANGLES_PER_SPRITE)
    {
        max_triangles = ENGINE_MAX_TRIANGLES_PER_SPRITE;
    }

    Triangle3D *buf = (Triangle3D *)ENGINE_MEM_MALLOC(max_triangles * sizeof(Triangle3D));
    if (!buf)
    {
        ENGINE_LOG_INFO("Sprite3D::fromPath failed to allocate memory for %u triangles\n", max_triangles);
        return false;
    }

    size_t bytes = ENGINE_STORAGE_READ(path, buf, max_triangles * sizeof(Triangle3D));
    if (bytes == 0)
    {
        ENGINE_MEM_FREE(buf);
        ENGINE_LOG_INFO("Sprite3D::fromPath failed to read triangle data from path: %s\n", path);
        return false;
    }

    uint16_t count = bytes / sizeof(Triangle3D);
    if (!reserveTriangles(count))
    {
        ENGINE_MEM_FREE(buf);
        return false;
    }
    for (uint16_t i = 0; i < count; i++)
    {
        buf[i].wireframe = wireframe;
        if (!addTriangle(buf[i]))
        {
            ENGINE_MEM_FREE(buf);
            return false;
        }
    }
    ENGINE_MEM_FREE(buf);
    return true;
#else
    return false;
#endif
}

bool Sprite3D::getTriangle(uint16_t index, Triangle3D &out) const
{
    if (index >= triangle_count)
        return false;
    out = triangles[index];
    return true;
}

bool Sprite3D::getTriangle(uint16_t index, float &x1, float &y1, float &z1,
                           float &x2, float &y2, float &z2,
                           float &x3, float &y3, float &z3, uint16_t &color, bool &wireframe) const
{
    if (index >= triangle_count)
        return false;
    const Triangle3D &tri = triangles[index];
    x1 = tri.x1;
    y1 = tri.y1;
    z1 = tri.z1;
    x2 = tri.x2;
    y2 = tri.y2;
    z2 = tri.z2;
    x3 = tri.x3;
    y3 = tri.y3;
    z3 = tri.z3;
    color = tri.color;
    wireframe = tri.wireframe;
    return true;
}

bool Sprite3D::getWorldTriangle(uint16_t index, Triangle3D &out) const
{
    if (index >= triangle_count)
        return false;

    out = triangles[index];
    if (rotation_y == 0.0f && scale_factor == 1.0f)
    {
        out.x1 += position.x;
        out.y1 += position.z;
        out.z1 += position.y;
        out.x2 += position.x;
        out.y2 += position.z;
        out.z2 += position.y;
        out.x3 += position.x;
        out.y3 += position.z;
        out.z3 += position.y;
        return true;
    }

    const float cos_a = cosf(rotation_y);
    const float sin_a = sinf(rotation_y);
    transformVertex(out.x1, out.y1, out.z1, cos_a, sin_a, out.x1, out.y1, out.z1);
    transformVertex(out.x2, out.y2, out.z2, cos_a, sin_a, out.x2, out.y2, out.z2);
    transformVertex(out.x3, out.y3, out.z3, cos_a, sin_a, out.x3, out.y3, out.z3);
    return true;
}

bool Sprite3D::getTransformedTriangle(uint16_t index, const Vector &camera_pos, Triangle3D &out) const
{
    if (!getWorldTriangle(index, out))
        return false;

    bool isSet = false;

    // Back-face culling: check if triangle faces the camera
    {
        // Calculate triangle normal using cross product of two edge vectors
        const float e1x = out.x2 - out.x1;
        const float e1y = out.y2 - out.y1;
        const float e1z = out.z2 - out.z1;
        const float e2x = out.x3 - out.x1;
        const float e2y = out.y3 - out.y1;
        const float e2z = out.z3 - out.z1;

        // Cross product: normal = e1 × e2
        const float nx = e1y * e2z - e1z * e2y;
        const float ny = e1z * e2x - e1x * e2z;
        const float nz = e1x * e2y - e1y * e2x;

        // Vector from triangle center to camera
        const float cx = (out.x1 + out.x2 + out.x3) * (1.0f / 3.0f);
        const float cy = (out.y1 + out.y2 + out.y3) * (1.0f / 3.0f);
        const float cz = (out.z1 + out.z2 + out.z3) * (1.0f / 3.0f);

        const float tox = camera_pos.x - cx;
        const float toy = 0.5f - cy;         // Camera height offset
        const float toz = camera_pos.y - cz; // camera_pos.y is Z in world space

        // Dot product: if positive, triangle faces camera
        isSet = (nx * tox + ny * toy + nz * toz) > 0.0f;
    }
    return isSet;
}

bool Sprite3D::initializeAsHouse(Vector pos, float width, float height, float rot, uint16_t color, bool wireframe)
{
    position = pos;
    rotation_y = rot;
    clearTriangles();
    type = SPRITE_HOUSE;
    active = true;
    return createHouse(width, height, color, wireframe);
}

bool Sprite3D::initializeAsHumanoid(Vector pos, float height, float rot, uint16_t color, bool wireframe)
{
    position = pos;
    rotation_y = rot;
    clearTriangles();
    type = SPRITE_HUMANOID;
    active = true;
    return createHumanoid(height, color, wireframe);
}

bool Sprite3D::initializeAsPillar(Vector pos, float height, float radius, uint16_t color, bool wireframe)
{
    position = pos;
    rotation_y = 0;
    clearTriangles();
    type = SPRITE_PILLAR;
    active = true;
    return createPillar(height, radius, color, wireframe);
}

bool Sprite3D::initializeAsTree(Vector pos, float height, uint16_t color, bool wireframe)
{
    position = pos;
    rotation_y = 0;
    clearTriangles();
    type = SPRITE_TREE;
    active = true;
    return createTree(height, color, wireframe);
}

void Sprite3D::setWireframe(bool wireframe)
{
    for (uint16_t i = 0; i < triangle_count; i++)
    {
        triangles[i].wireframe = wireframe;
    }
}

bool Sprite3D::toPath(const char *path) const
{
#ifdef ENGINE_STORAGE_WRITE
    if (triangle_count == 0)
        return false;
    if (triangle_count == ENGINE_MAX_TRIANGLES_PER_SPRITE)
        return ENGINE_STORAGE_WRITE(path, triangles, sizeof(Triangle3D) * triangle_count);
    Triangle3D *buf = ENGINE_MEM_NEW Triangle3D[triangle_count];
    if (!buf)
        return false;
    for (uint16_t i = 0; i < triangle_count; i++)
        buf[i] = triangles[i];
    const bool ok = ENGINE_STORAGE_WRITE(path, buf, sizeof(Triangle3D) * triangle_count);
    ENGINE_MEM_DELETE[] buf;
    return ok;
#else
    return false;
#endif
}

void Sprite3D::transformVertex(float x, float y, float z, float cos_a, float sin_a,
                               float &out_x, float &out_y, float &out_z) const
{
    x *= scale_factor;
    y *= scale_factor;
    z *= scale_factor;
    const float original_x = x;
    x = original_x * cos_a - z * sin_a;
    z = original_x * sin_a + z * cos_a;
    out_x = x + position.x;
    out_y = y + position.z;
    out_z = z + position.y;
}

bool Sprite3D::updateTriangle(uint16_t index, const Triangle3D &triangle)
{
    if (index >= triangle_count)
        return false;
    triangles[index] = triangle;
    return true;
}

bool Sprite3D::updateTriangle(uint16_t index, float x1, float y1, float z1,
                              float x2, float y2, float z2,
                              float x3, float y3, float z3, uint16_t color, bool wireframe)
{
    if (index >= triangle_count)
        return false;
    triangles[index].x1 = x1;
    triangles[index].y1 = y1;
    triangles[index].z1 = z1;
    triangles[index].x2 = x2;
    triangles[index].y2 = y2;
    triangles[index].z2 = z2;
    triangles[index].x3 = x3;
    triangles[index].y3 = y3;
    triangles[index].z3 = z3;
    triangles[index].color = color;
    triangles[index].wireframe = wireframe;
    return true;
}
namespace sprite3d_buffer
{
using Result = Sprite3D::BufferResult;
const size_t recordSize = 40;

static bool coordinate(double value) { return std::isfinite(value) && std::fabs(value) <= 1e12; }

static float readFloat(const uint8_t *data)
{
    uint32_t bits =
        uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static void writeFloat(uint8_t *data, float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (unsigned i = 0; i < 4; ++i)
        data[i] = uint8_t(bits >> (8 * i));
}

static void values(const uint8_t *record, float *out)
{
    for (unsigned i = 0; i < 9; ++i)
        out[i] = readFloat(record + i * 4);
}

static Result validate(const void *records, size_t size)
{
    if ((!records && size) || size % recordSize)
        return Result::InvalidBuffer;
    if (size / recordSize > ENGINE_MAX_TRIANGLES_PER_SPRITE || size / recordSize > UINT16_MAX)
        return Result::TriangleLimit;
    const uint8_t *data = static_cast<const uint8_t *>(records);
    for (size_t offset = 0; offset < size; offset += recordSize)
    {
        for (unsigned i = 0; i < 9; ++i)
            if (!coordinate(readFloat(data + offset + i * 4)))
                return Result::InvalidValue;
        if (data[offset + 38] > 1)
            return Result::InvalidValue;
    }
    return Result::Ok;
}

static Sprite3D::Bounds initialBounds(bool empty)
{
    Sprite3D::Bounds bounds = {{-.5, 0, -.5}, {.5, 1, .5}};
    if (!empty)
        for (unsigned i = 0; i < 3; ++i)
        {
            bounds.low[i] = INFINITY;
            bounds.high[i] = -INFINITY;
        }
    return bounds;
}

static void extend(Sprite3D::Bounds &bounds, const float *point)
{
    for (unsigned i = 0; i < 3; ++i)
    {
        bounds.low[i] = std::fmin(bounds.low[i], point[i]);
        bounds.high[i] = std::fmax(bounds.high[i], point[i]);
    }
}

static bool overlaps(const void *a, size_t aSize, const void *b, size_t bSize)
{
    if (!aSize || !bSize)
        return false;
    uintptr_t aa = reinterpret_cast<uintptr_t>(a), bb = reinterpret_cast<uintptr_t>(b);
    return aa > bb ? aa - bb < bSize : bb - aa < aSize;
}

struct Transform
{
    Sprite3D::TransformKind kind;
    double amount[3], pivot[3], cosine[3], sine[3];
    unsigned axes;
    const uint8_t *mask;

    bool point(const float *value, size_t vertex, float *out) const
    {
        double x = value[0], y = value[1], z = value[2];
        if (!mask || mask[vertex])
        {
            x -= pivot[0];
            y -= pivot[1];
            z -= pivot[2];
            if (kind == Sprite3D::TransformKind::Move)
            {
                x += amount[0];
                y += amount[1];
                z += amount[2];
            }
            else if (kind == Sprite3D::TransformKind::Scale)
            {
                x *= amount[0];
                y *= amount[1];
                z *= amount[2];
            }
            else
            {
                // Preserve XYZ order and skip identity rotations per vertex.
                if (axes & 1u)
                {
                    double t = y * cosine[0] - z * sine[0];
                    z = y * sine[0] + z * cosine[0];
                    y = t;
                }
                if (axes & 2u)
                {
                    double t = x * cosine[1] + z * sine[1];
                    z = -x * sine[1] + z * cosine[1];
                    x = t;
                }
                if (axes & 4u)
                {
                    double t = x * cosine[2] - y * sine[2];
                    y = x * sine[2] + y * cosine[2];
                    x = t;
                }
            }
            x += pivot[0];
            y += pivot[1];
            z += pivot[2];
        }
        if (!coordinate(x) || !coordinate(y) || !coordinate(z))
            return false;
        out[0] = float(x);
        out[1] = float(y);
        out[2] = float(z);
        return coordinate(out[0]) && coordinate(out[1]) && coordinate(out[2]);
    }
};

static bool validOptions(const Sprite3D::PreviewOptions &o)
{
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!coordinate(o.center[i]))
            return false;
        for (unsigned j = 0; j < 3; ++j)
            if (!coordinate(o.basis[i][j]))
                return false;
    }
    if (!coordinate(o.scale) || o.scale <= 0 || !coordinate(o.distance) || o.wireframe < -1 ||
        o.wireframe > 1)
        return false;
    if (o.orthographic && (!coordinate(o.orthoDistance) || o.orthoDistance <= 0))
        return false;
    if (o.panel && (!coordinate(o.panelScale) || o.panelScale <= 0 || !coordinate(o.offsetX) ||
                    !coordinate(o.offsetY) || !coordinate(o.aspect) || o.aspect <= 0 || !coordinate(o.edge) ||
                    o.edge <= 0 || o.distance <= 0 || (!o.orthographic && !std::isfinite(.11 / o.scale))))
        return false;
    return true;
}
struct Point
{
    double x, y, z;
};
struct Depth
{
    double depth;
    uint16_t index;
};
static int compare_depth(const void *aa, const void *bb)
{
    const Depth &a = *static_cast<const Depth *>(aa), &b = *static_cast<const Depth *>(bb);
    if (a.depth != b.depth)
        return a.depth > b.depth ? -1 : 1;
    return a.index < b.index ? -1 : a.index != b.index;
}
static unsigned clip(Point *points, unsigned count, const double *plane)
{
    if (!count)
        return 0;
    Point result[9], previous = points[count - 1];
    double pv = plane[0] * previous.x + plane[1] * previous.y + plane[2] * previous.z + plane[3];
    unsigned n = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        Point point = points[i];
        double value = plane[0] * point.x + plane[1] * point.y + plane[2] * point.z + plane[3];
        if ((value >= 0) != (pv >= 0))
        {
            double t = pv / (pv - value);
            result[n++] = {previous.x + t * (point.x - previous.x), previous.y + t * (point.y - previous.y),
                           previous.z + t * (point.z - previous.z)};
        }
        if (value >= 0)
            result[n++] = point;
        previous = point;
        pv = value;
    }
    memcpy(points, result, n * sizeof(Point));
    return n;
}
static unsigned polygon(const uint8_t *record, const Sprite3D::PreviewOptions &o, Point *points)
{
    float v[9];
    values(record, v);
    for (unsigned j = 0; j < 3; ++j)
    {
        double x = v[j * 3] - o.center[0], y = v[j * 3 + 1] - o.center[1], z = v[j * 3 + 2] - o.center[2];
        points[j] = {x * o.basis[0][0] + y * o.basis[0][1] + z * o.basis[0][2],
                     x * o.basis[1][0] + y * o.basis[1][1] + z * o.basis[1][2],
                     x * o.basis[2][0] + y * o.basis[2][1] + z * o.basis[2][2]};
    }
    if (o.facing)
    {
        Point a = points[0], b = points[1], c = points[2];
        double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        double facing = o.orthographic ? nz : nx * a.x + ny * a.y + nz * (a.z + o.distance);
        if (facing == 0 || (o.culling && facing < 0))
            return 0;
        if (facing < 0)
        {
            points[0] = c;
            points[2] = a;
        }
    }
    unsigned count = 3;
    if (o.panel)
    {
        double a = o.aspect, e = o.edge, d = o.distance;
        if (o.orthographic)
        {
            const double planes[4][4] = {
                {1, 0, 0, a * d}, {-1, 0, 0, a * d}, {0, 1, 0, e * d}, {0, -1, 0, e * d}};
            for (unsigned j = 0; j < 4; ++j)
                count = clip(points, count, planes[j]);
        }
        else
        {
            const double planes[5][4] = {{0, 0, 1, d - .11 / o.scale},
                                         {1, 0, a, a * d},
                                         {-1, 0, a, a * d},
                                         {0, 1, e, e * d},
                                         {0, -1, e, e * d}};
            for (unsigned j = 0; j < 5; ++j)
                count = clip(points, count, planes[j]);
        }
    }
    return count;
}
static Point render_point(Point p, const Sprite3D::PreviewOptions &o)
{
    if (o.orthographic)
    {
        double d = fmax(.22, o.orthoDistance);
        p.z = .5 * p.z / (1 + fabs(p.z) / d);
        double factor = (d + p.z) / o.orthoDistance;
        p.x *= factor;
        p.y *= factor;
    }
    if (o.panel)
    {
        double d = o.orthographic ? fmax(.22, o.distance) : o.distance;
        p.x = p.x * o.panelScale + o.offsetX * (d + p.z);
        p.y = p.y * o.panelScale + o.offsetY * (d + p.z);
    }
    if (!o.orthographic)
    {
        p.x *= o.scale;
        p.y *= o.scale;
        p.z *= o.scale;
    }
    return {p.z, p.y + .5, p.x};
}
} // namespace sprite3d_buffer

Sprite3D::BufferResult Sprite3D::loadBuffer(const void *records, size_t size, Bounds *bounds)
{
    using namespace sprite3d_buffer;
    if (triangle_count)
        return BufferResult::NotEmpty;
    BufferResult result = validate(records, size);
    if (result != BufferResult::Ok)
        return result;
    size_t count = size / recordSize;
    if (!reserveTriangles(uint16_t(count)))
        return BufferResult::OutOfMemory;
    Bounds calculated = initialBounds(count == 0);
    const uint8_t *data = static_cast<const uint8_t *>(records);
    for (size_t i = 0; i < count; ++i)
    {
        const uint8_t *record = data + i * recordSize;
        float v[9];
        values(record, v);
        for (unsigned j = 0; j < 3; ++j)
            extend(calculated, v + j * 3);
        triangles[i] = Triangle3D(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8],
                                  uint16_t(record[36]) | uint16_t(record[37]) << 8, record[38] != 0);
    }
    triangle_count = uint16_t(count);
    if (bounds)
        *bounds = calculated;
    return BufferResult::Ok;
}

Sprite3D::BufferResult Sprite3D::transformBuffer(const void *source, size_t size, void *target,
                                                 size_t targetSize, TransformKind kind,
                                                 const double amount[3], const double pivot[3],
                                                 const uint8_t *mask, size_t maskSize, Bounds *bounds)
{
    using namespace sprite3d_buffer;
    BufferResult result = validate(source, size);
    if (result != BufferResult::Ok)
        return result;
    size_t count = size / recordSize;
    if (targetSize != size || (!target && size) || !amount || !pivot ||
        overlaps(source, size, target, size) || (mask ? maskSize != count * 3 : maskSize != 0) ||
        overlaps(mask, maskSize, target, size) || overlaps(amount, sizeof(double) * 3, target, size) ||
        overlaps(pivot, sizeof(double) * 3, target, size) ||
        (bounds && overlaps(bounds, sizeof(Bounds), target, size)))
        return BufferResult::InvalidBuffer;
    if (kind != TransformKind::Move && kind != TransformKind::Scale && kind != TransformKind::Rotate)
        return BufferResult::InvalidValue;
    Transform transform = {};
    transform.kind = kind;
    transform.mask = mask;
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!coordinate(amount[i]) || !coordinate(pivot[i]) ||
            (kind == TransformKind::Scale && (amount[i] <= 0 || amount[i] > 1e6)))
            return BufferResult::InvalidValue;
        transform.amount[i] = amount[i];
        transform.pivot[i] = pivot[i];
        if (kind == TransformKind::Rotate && amount[i] != 0)
        {
            transform.axes |= 1u << i;
            double angle = amount[i] * 3.14159265358979323846 / 180.0;
            transform.cosine[i] = std::cos(angle);
            transform.sine[i] = std::sin(angle);
        }
    }
    const uint8_t *data = static_cast<const uint8_t *>(source);
    Bounds calculated = initialBounds(count == 0);
    for (size_t i = 0; i < count; ++i)
    {
        float v[9];
        values(data + i * recordSize, v);
        for (unsigned j = 0; j < 3; ++j)
        {
            float point[3];
            if (!transform.point(v + j * 3, i * 3 + j, point))
                return BufferResult::InvalidValue;
            extend(calculated, point);
        }
    }
    // All validation has finished. Retain material, wireframe and padding bytes.
    uint8_t *out = static_cast<uint8_t *>(target);
    if (size)
        std::memcpy(out, data, size);
    for (size_t i = 0; i < count; ++i)
    {
        if (mask && !(mask[i * 3] || mask[i * 3 + 1] || mask[i * 3 + 2]))
            continue;
        float v[9];
        values(data + i * recordSize, v);
        for (unsigned j = 0; j < 3; ++j)
        {
            if (mask && !mask[i * 3 + j])
                continue;
            float point[3];
            transform.point(v + j * 3, i * 3 + j, point);
            for (unsigned k = 0; k < 3; ++k)
                writeFloat(out + i * recordSize + (j * 3 + k) * 4, point[k]);
        }
    }
    if (bounds)
        *bounds = calculated;
    return BufferResult::Ok;
}

Sprite3D::BufferResult Sprite3D::buildPreview(const void *records, size_t size, const PreviewOptions &o)
{
    using namespace sprite3d_buffer;
    if (triangle_count)
        return BufferResult::NotEmpty;
    BufferResult result = validate(records, size);
    if (result != BufferResult::Ok)
        return result;
    if (!validOptions(o))
        return BufferResult::InvalidValue;
    size_t count = size / recordSize;
    if (!count)
    {
        active = true;
        return BufferResult::Ok;
    }
    const uint8_t *data = static_cast<const uint8_t *>(records);
    Depth *order = static_cast<Depth *>(ENGINE_MEM_MALLOC(count * sizeof(Depth)));
    if (!order)
        return BufferResult::OutOfMemory;
    for (size_t i = 0; i < count; ++i)
    {
        float v[9];
        values(data + i * recordSize, v);
        order[i] = {((double(v[0]) + v[3]) + v[6]) * o.basis[2][0] +
                        ((double(v[1]) + v[4]) + v[7]) * o.basis[2][1] +
                        ((double(v[2]) + v[5]) + v[8]) * o.basis[2][2],
                    uint16_t(i)};
    }
    std::qsort(order, count, sizeof(Depth), compare_depth);
    // Stable depth order via source-index tie breaking; transform/clip each face once.
    for (size_t i = 0; i < count && result == BufferResult::Ok; ++i)
    {
        const uint8_t *record = data + order[i].index * recordSize;
        Point points[9];
        unsigned n = polygon(record, o, points);
        uint16_t color = uint16_t(record[36]) | uint16_t(record[37]) << 8;
        bool wire = o.wireframe < 0 ? record[38] != 0 : o.wireframe == 1;
        for (unsigned j = 1; j + 1 < n; ++j)
        {
            Point a = render_point(points[0], o), b = render_point(points[j], o),
                  c = render_point(points[j + 1], o);
            if (!coordinate(a.x) || !coordinate(a.y) || !coordinate(a.z) || !coordinate(b.x) ||
                !coordinate(b.y) || !coordinate(b.z) || !coordinate(c.x) || !coordinate(c.y) ||
                !coordinate(c.z))
            {
                result = BufferResult::InvalidValue;
                break;
            }
            if (triangle_count >= ENGINE_MAX_TRIANGLES_PER_SPRITE)
            {
                result = BufferResult::TriangleLimit;
                break;
            }
            if (!addTriangle(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, color, wire))
            {
                result = BufferResult::OutOfMemory;
                break;
            }
        }
    }
    ENGINE_MEM_FREE(order);
    if (result != BufferResult::Ok)
        clearTriangles();
    else
        active = true;
    return result;
}
