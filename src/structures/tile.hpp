#pragma once

#include <span>
#include <vector>

#include <glm/glm.hpp>

struct uv_data {
    glm::u16vec2 pos;
    glm::u16vec2 size;

    union {
        struct {
            bool collides_left : 1;
            bool collides_right : 1;
            bool collides_up : 1;
            bool collides_down : 1;

            bool not_placeable : 1;
            bool additive : 1;

            bool obscures : 1;
            bool contiguous : 1;
            bool blocks_light : 1;
            bool self_contiguous : 1;
            bool hidden : 1;
            bool dirt : 1;
            bool has_normals : 1;
            bool uv_light : 1;
        };
        uint16_t flags;
    };
};
static_assert(sizeof(uv_data) == 10);

inline std::vector<uv_data> parse_uvs(std::span<const uint8_t> data) {
    if(data.size() < 0xC) {
        throw std::runtime_error("invalid uv header size");
    }

    auto magic = *(uint32_t *)data.data();
    if(magic != 0x00B00B00) {
        throw std::runtime_error("invalid uv header");
    }
    auto count = *(uint32_t *)(data.data() + 4);
    assert(*(uint32_t *)(data.data() + 8) == 0); // null in the given asset

    if(data.size() < 0xC + count * sizeof(uv_data)) {
        throw std::runtime_error("invalid uv data size");
    }

    auto ptr = (uv_data *)(data.data() + 0xC);
    return std::vector(ptr, ptr + count);
}

inline std::vector<uint8_t> save_uvs(std::span<const uv_data> uvs) {
    std::vector<uint8_t> data(0xC + uvs.size() * sizeof(uv_data));

    *((uint32_t*)data.data() + 0) = 0x00B00B00;
    *((uint32_t*)data.data() + 1) = uvs.size(); // 1024
    *((uint32_t*)data.data() + 2) = 0;

    std::memcpy(data.data() + 0xC, uvs.data(), uvs.size() * sizeof(uv_data));

    return data;
}
