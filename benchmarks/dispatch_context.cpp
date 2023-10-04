#include "dispatch_context.hpp"

#include <benchmark/benchmark.h>

namespace virtual_call
{
void
Human::tick()
{
    compute(position_, velocity_);
}

void
Car::tick()
{
    compute(position_, velocity_);
}
}  // namespace virtual_call

namespace crtp
{
void
Human::tickImplementation()
{
    compute(position_, velocity_);
}

void
Car::tickImplementation()
{
    compute(position_, velocity_);
}
}  // namespace crtp

namespace variant
{
void
Human::tick()
{
    compute(position_, velocity_);
}

void
Car::tick()
{
    compute(position_, velocity_);
}
}  // namespace variant
