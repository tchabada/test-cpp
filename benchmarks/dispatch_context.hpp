#pragma once

#include "dispatch_compute.hpp"

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

class Human final : public Entity
{
public:
    void tick() final;
};

class Car final : public Entity
{
public:
    void tick() final;
};
}  // namespace virtual_call

namespace crtp
{
// Old CRTP
// template <class Derived>
// struct Entity
// {
//     void
//     tick()
//     {
//         static_cast<Derived*>(this)->tickImplementation();
//     }

// protected:
//     Entity() = default;

// protected:
//     Vector3 position_{};
//     Vector3 velocity_{1.0f, 1.0f, 1.0f};

//     std::byte padding_[64]{};
// };


// struct Human : public Entity<Human>
// {
//     void
//     tickImplementation();
// };
// struct Car : public Entity<Car>
// {
//     void
//     tickImplementation();
// };

// C++23 CRTP
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

struct Human : Entity
{
    void tickImplementation();
};

struct Car : Entity
{
    void tickImplementation();
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

class Human
{
public:
    void tick();

private:
    Vector3 position_{};
    Vector3 velocity_{1.0f, 1.0f, 1.0f};

    std::byte padding_[64]{};
};

class Car
{
public:
    void tick();

private:
    Vector3 position_{};
    Vector3 velocity_{1.0f, 1.0f, 1.0f};

    std::byte padding_[64]{};
};
}  // namespace variant
