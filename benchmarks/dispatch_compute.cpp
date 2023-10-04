#include "dispatch_compute.hpp"

#include <benchmark/benchmark.h>

void
compute(Vector3& position, const Vector3& velocity)
{
    position += velocity * 2.0f;

    benchmark::DoNotOptimize(position);
}
