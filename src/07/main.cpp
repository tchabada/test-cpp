#include "serialization.hpp"

enum class EntityType : uint16_t
{
    Human = 0x1u,
    Car   = 0x2u,
};


struct Entity
{
    uint32_t   id;
    EntityType entityType;
    float      position[3];

    auto operator<=>(const Entity&) const = default;
};

enum class HumanType : uint16_t
{
    Player = 0x1u,
    Enemy  = 0x2u,
    Ally   = 0x3u,
};

struct Human : public Entity
{
    HumanType humanType;

    auto operator<=>(const Human&) const = default;
};

int
main()
{
    const Human humans[]{{{1u, EntityType::Human, {0.0f, 0.0f, 0.0f}}, HumanType::Player},
                         {
                             {2u, EntityType::Human, {1.0f, 1.0f, 1.0f}},
                             HumanType::Enemy,
                         },
                         {
                             {3u, EntityType::Human, {2.0f, 2.0f, 2.0f}},
                             HumanType::Ally,
                         }};

    for (const auto& h : humans) {
        std::cout << "id: " << h.id << ", entity type: " << static_cast<uint16_t>(h.entityType) << ", position: {"
                  << h.position[0] << ", " << h.position[1] << ", " << h.position[2] << "}"
                  << ", human type: " << static_cast<uint16_t>(h.humanType) << std::endl;
    }

    std::array<uint8_t, serializedLength(humans)> buffer;

    serialize(humans, buffer);

    std::cout << std::endl;

    Human temp[3];
    deserialize(temp, buffer);

    for (const auto& h : temp) {
        std::cout << "id: " << h.id << ", entity type: " << static_cast<uint16_t>(h.entityType) << ", position: {"
                  << h.position[0] << ", " << h.position[1] << ", " << h.position[2] << "}"
                  << ", human type: " << static_cast<uint16_t>(h.humanType) << std::endl;
    }

    return 0;
}
