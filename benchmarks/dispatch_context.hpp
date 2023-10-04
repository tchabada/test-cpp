#pragma once

#include "dispatch_compute.hpp"

#include <benchmark/benchmark.h>

inline void
computeInline(Vector3& position, const Vector3& velocity)
{
    position += velocity * 2.0f;

    benchmark::DoNotOptimize(position);
}

namespace virtual_call
{
class Entity
{
public:
    Entity()          = default;
    virtual ~Entity() = default;

    Entity(const Entity& other)                = default;
    Entity& operator=(const Entity& other)     = default;
    Entity(Entity&& other) noexcept            = default;
    Entity& operator=(Entity&& other) noexcept = default;

    virtual void tick() = 0;

protected:
    Vector3 position_{};
    Vector3 velocity_{1.0f, 1.0f, 1.0f};

    std::byte padding_[64]{};
};

template <bool I>
class Human final : public Entity
{
public:
    void
    tick() final
    {
        if constexpr (I) {
            computeInline(position_, velocity_);
        }
        else {
            compute(position_, velocity_);
        }
    }
};

template <bool I>
class Car final : public Entity
{
public:
    void
    tick() final
    {
        if constexpr (I) {
            computeInline(position_, velocity_);
        }
        else {
            compute(position_, velocity_);
        }
    }
};
}  // namespace virtual_call

namespace crtp
{
class Entity
{
public:
    void
    tick(this auto&& self)
    {
        self.tickImplementation();
    }

protected:
    Entity() = default;

protected:
    Vector3 position_{};
    Vector3 velocity_{1.0f, 1.0f, 1.0f};

    std::byte padding_[64]{};
};

template <bool I>
struct Human : Entity
{
    void
    tickImplementation()
    {
        if constexpr (I) {
            computeInline(position_, velocity_);
        }
        else {
            compute(position_, velocity_);
        }
    }
};

template <bool I>
struct Car : Entity
{
    void
    tickImplementation()
    {
        if constexpr (I) {
            computeInline(position_, velocity_);
        }
        else {
            compute(position_, velocity_);
        }
    }
};
}  // namespace crtp

namespace variant
{
template <class... Ts>
struct overload : Ts...
{
    using Ts::operator()...;
};

template <class... Ts>
overload(Ts...) -> overload<Ts...>;

template <bool I>
class Human
{
public:
    void
    tick()
    {
        if constexpr (I) {
            computeInline(position_, velocity_);
        }
        else {
            compute(position_, velocity_);
        }
    }

private:
    Vector3 position_{};
    Vector3 velocity_{1.0f, 1.0f, 1.0f};

    std::byte padding_[64]{};
};

template <bool I>
class Car
{
public:
    void
    tick()
    {
        if constexpr (I) {
            computeInline(position_, velocity_);
        }
        else {
            compute(position_, velocity_);
        }
    }

private:
    Vector3 position_{};
    Vector3 velocity_{1.0f, 1.0f, 1.0f};

    std::byte padding_[64]{};
};
}  // namespace variant
