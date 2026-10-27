#pragma once

#include "eb/direct_scene.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace eb {
// Consecutive primitives may share one submission, but their order and triangle
// diagonals must stay identical to the individual source triangle strips.
// Flush before changing any shader, stencil or scissor state.
class SceneDrawBatch {
public:
    explicit SceneDrawBatch(std::vector<float>& vertices) : vertices_(vertices) {
        bind_buffer_ = reinterpret_cast<PFNGLBINDBUFFERPROC>(SDL_GL_GetProcAddress("glBindBuffer"));
        vertex_attrib_pointer_ = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(SDL_GL_GetProcAddress("glVertexAttribPointer"));
        enable_vertex_attrib_array_ = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
        disable_vertex_attrib_array_ = reinterpret_cast<PFNGLDISABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glDisableVertexAttribArray"));
        if (!bind_buffer_ || !vertex_attrib_pointer_ || !enable_vertex_attrib_array_ || !disable_vertex_attrib_array_)
            throw std::runtime_error("Scene batching requires programmable vertex attributes");
        vertices_.clear();
        bind_buffer_(GL_ARRAY_BUFFER, 0);
        enable_vertex_attrib_array_(0);
        enable_vertex_attrib_array_(1);
    }
    ~SceneDrawBatch() {
        disable_vertex_attrib_array_(1);
        disable_vertex_attrib_array_(0);
    }
    SceneDrawBatch(const SceneDrawBatch&) = delete;
    SceneDrawBatch& operator=(const SceneDrawBatch&) = delete;

    void append(const DirectSceneFrame::Quad& quad, DirectScenePicture::Offset offset,
                const DirectSceneFrame& scene) {
        const float left = (quad.x + offset.x) * 2 / scene.width - 1;
        const float right = left + quad.width * 2.f / scene.width;
        const float top = 1 - (quad.y + offset.y) * 2 / 224;
        const float bottom = top - quad.height * 2.f / 224;
        const float z = .8f - quad.priority * .1f;
        // Preserve floor-based sampling at exact nearest-texel boundaries.
        const auto uv = [](float v) { return std::nextafter(v, std::numeric_limits<float>::infinity()); };
        const float u0 = uv(float(quad.u) / scene.atlas_width), u1 = uv(float(quad.u + quad.width) / scene.atlas_width);
        const float v0 = uv(float(quad.v) / scene.atlas_height), v1 = uv(float(quad.v + quad.height) / scene.atlas_height);
        vertices_.insert(vertices_.end(), {
            left, bottom, z, u0, v1, right, bottom, z, u1, v1, left, top, z, u0, v0,
            left, top, z, u0, v0, right, bottom, z, u1, v1, right, top, z, u1, v0});
    }
    void flush() {
        if (vertices_.empty()) return;
        vertex_attrib_pointer_(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices_.data());
        vertex_attrib_pointer_(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices_.data() + 3);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size() / 5));
        vertices_.clear();
    }
private:
    PFNGLBINDBUFFERPROC bind_buffer_{};
    PFNGLVERTEXATTRIBPOINTERPROC vertex_attrib_pointer_{};
    PFNGLENABLEVERTEXATTRIBARRAYPROC enable_vertex_attrib_array_{};
    PFNGLDISABLEVERTEXATTRIBARRAYPROC disable_vertex_attrib_array_{};
    std::vector<float>& vertices_;
};
} // namespace eb
