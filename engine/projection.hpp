#pragma once
#include "vector.hpp"
#include <math.h>

class Projection
{
public:
    // A triangle gains at most one vertex per clipping plane.
    static constexpr int MAX_VERTICES = 8;

    // Clip before perspective division and unsigned LCD conversion.
    // clamp retains the preview API's vertex-pinning behavior at screen edges.
    static int project(const Vector triangle[3], float width, float height,
                       bool clamp, Vector output[MAX_VERTICES])
    {
        if (!isfinite(width) || !isfinite(height) || width < 1 || height < 1)
            return 0;
        for (int i = 0; i < 3; ++i)
        {
            if (!isfinite(triangle[i].x) || !isfinite(triangle[i].y) || !isfinite(triangle[i].z))
                return 0;
            output[i] = triangle[i];
        }
        // Reuse the caller's output as one clipping buffer; no heap allocation.
        Vector scratch[MAX_VERTICES];
        Vector *input = output;
        Vector *target = scratch;
        const float half_width = width * 0.5f, half_height = height * 0.5f;
        int count = 3;
        for (int plane = 0; plane < (clamp ? 1 : 5); ++plane)
        {
            Vector normal;
            float offset = 0;
            switch (plane)
            {
            case 0:
                normal.z = 1;
                offset = -0.1f;
                break;
            case 1:
                normal = Vector(height, 0, half_width);
                break;
            case 2:
                normal = Vector(-height, 0, width - 1 - half_width);
                break;
            case 3:
                normal = Vector(0, -height, half_height);
                break;
            default:
                normal = Vector(0, height, height - 1 - half_height);
                break;
            }
            count = clip(input, count, target, normal, offset);
            if (count < 3)
                return 0;
            Vector *previous = input;
            input = target;
            target = previous;
        }
        for (int i = 0; i < count; ++i)
        {
            const Vector &v = input[i];
            // Intersections may round slightly below the near plane.
            const float scale = height / (v.z < 0.1f ? 0.1f : v.z);
            output[i] = Vector(bound(v.x * scale + half_width, width - 1),
                               bound(-v.y * scale + half_height, height - 1));
        }
        return count;
    }

private:
    static float bound(float value, float maximum)
    {
        return value < 0 ? 0 : (value > maximum ? maximum : value);
    }

    static float distance(const Vector &vertex, const Vector &normal, float offset)
    {
        return normal.x * vertex.x + normal.y * vertex.y + normal.z * vertex.z + offset;
    }

    static int clip(const Vector *input, int count, Vector *output,
                    const Vector &normal, float offset)
    {
        int written = 0;
        Vector previous = input[count - 1];
        float previous_distance = distance(previous, normal, offset);
        for (int i = 0; i < count; ++i)
        {
            const Vector &current = input[i];
            const float current_distance = distance(current, normal, offset);
            if ((current_distance > 0 && previous_distance < 0) ||
                (current_distance < 0 && previous_distance > 0))
            {
                if (written == MAX_VERTICES)
                    return 0;
                const float t = previous_distance / (previous_distance - current_distance);
                output[written++] = Vector(
                    previous.x + t * (current.x - previous.x),
                    previous.y + t * (current.y - previous.y),
                    previous.z + t * (current.z - previous.z));
            }
            if (current_distance >= 0)
            {
                if (written == MAX_VERTICES)
                    return 0;
                output[written++] = current;
            }
            previous = current;
            previous_distance = current_distance;
        }
        return written;
    }
};
