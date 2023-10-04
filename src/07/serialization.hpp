#pragma once

#include <bit>
#include <cstring>
#include <iostream>
#include <meta>
#include <utility>

template <typename T>
concept Serializable = std::is_trivially_copyable_v<T> and not std::is_pointer_v<T>;

namespace detail
{
template <Serializable T>
constexpr void
serialize(const T& data, std::span<uint8_t> buffer, size_t& offset)
{
    if constexpr (std::is_array_v<T>) {
        for (size_t i{0u}; i < std::size(data); ++i) {
            serialize(data[i], buffer, offset);
        }
    }
    else if constexpr (std::is_enum_v<T>) {
        std::memcpy(&buffer[offset], &data, sizeof(std::underlying_type_t<T>));
        offset += sizeof(std::underlying_type_t<T>);
    }
    else if constexpr (not std::is_class_v<T>) {
        std::memcpy(&buffer[offset], &data, sizeof(T));
        offset += sizeof(T);
    }
    else {
        static constexpr auto bases
            = std::define_static_array(std::meta::bases_of(^^T, std::meta::access_context::current()));

        template for (constexpr auto& b : bases) {
            serialize(data.[:b:], buffer, offset);
        }

        static constexpr auto members
            = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

        template for (constexpr auto& m : members) {
            serialize(data.[:m:], buffer, offset);
        }
    }
}
}  // namespace detail

template <Serializable T>
constexpr void
serialize(const T& data, std::span<uint8_t> buffer)
{
    size_t offset{0u};

    detail::serialize(data, buffer, offset);
}

namespace detail
{
template <Serializable T>
constexpr void
deserialize(T& data, const std::span<uint8_t> buffer, size_t& offset)
{
    if constexpr (std::is_array_v<T>) {
        for (size_t i{0u}; i < std::size(data); ++i) {
            deserialize(data[i], buffer, offset);
        }
    }
    else if constexpr (std::is_enum_v<T>) {
        std::memcpy(&data, &buffer[offset], sizeof(std::underlying_type_t<T>));
        offset += sizeof(std::underlying_type_t<T>);
    }
    else if constexpr (not std::is_class_v<T>) {
        std::memcpy(&data, &buffer[offset], sizeof(T));

        offset += sizeof(T);
    }
    else {
        static constexpr auto bases
            = std::define_static_array(std::meta::bases_of(^^T, std::meta::access_context::current()));

        template for (constexpr auto& b : bases) {
            deserialize(data.[:b:], buffer, offset);
        }

        static constexpr auto members
            = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

        template for (constexpr auto& m : members) {
            deserialize(data.[:m:], buffer, offset);
        }
    }
}
}  // namespace detail

template <Serializable T>
constexpr void
deserialize(T& data, const std::span<uint8_t> buffer)
{
    size_t offset{0u};

    detail::deserialize(data, buffer, offset);
}

namespace detail
{
template <Serializable T>
constexpr void
serializedLength(const T& data, size_t& l)
{
    if constexpr (std::is_array_v<T>) {
        for (size_t i{0u}; i < std::size(data); ++i) {
            serializedLength(data[i], l);
        }
    }
    else if constexpr (std::is_enum_v<T>) {
        l += sizeof(std::underlying_type_t<T>);
    }
    else if constexpr (not std::is_class_v<T>) {
        l += sizeof(T);
    }
    else {
        static constexpr auto bases
            = std::define_static_array(std::meta::bases_of(^^T, std::meta::access_context::current()));

        template for (constexpr auto& b : bases) {
            serializedLength(data.[:b:], l);
        }

        static constexpr auto members
            = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

        template for (constexpr auto& m : members) {
            serializedLength(data.[:m:], l);
        }
    }
}
}  // namespace detail

template <Serializable T>
constexpr size_t
serializedLength(const T& data)
{
    size_t l{0u};

    detail::serializedLength(data, l);

    return l;
}
