#pragma once

#include <cstddef>
#include <type_traits>

template <typename T, size_t N>
struct Vector
{
    constexpr Vector<T, N>
    operator+(const Vector<T, N>& other) const
    {
        Vector<T, N> result;

        for (size_t i = 0; i < N; ++i) {
            result.elements[i] = elements[i] + other.elements[i];
        }

        return result;
    }

    constexpr void
    operator+=(const Vector<T, N>& other)
    {
        for (size_t i = 0; i < N; ++i) {
            elements[i] += other.elements[i];
        }
    }


    template <typename S>
    constexpr Vector<T, N>
    operator*(S scalar) const
        requires std::is_arithmetic_v<S>
    {
        Vector<T, N> result;

        for (size_t i = 0; i < N; ++i) {
            result.elements[i] = elements[i] * scalar;
        }

        return result;
    }

    template <typename S>
    constexpr void
    operator*=(S scalar)
        requires std::is_arithmetic_v<S>
    {
        for (size_t i = 0; i < N; ++i) {
            elements[i] *= scalar;
        }
    }

    T elements[N];
};

using Vector3 = Vector<float, 3>;

void compute(Vector3& position, const Vector3& velocity);