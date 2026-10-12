#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cli.hpp"
#include "cropsim/renderer.hpp"

namespace {

struct Instance {
    float x;
    float y;
    float radius_x;
    float radius_y;
    float cosine;
    float sine;
    float red;
    float green;
    float blue;
};

struct RailVertex {
    float x;
    float y;
    float red;
    float green;
    float blue;
};

struct Camera {
    double x{};
    double y{};
    double half_height{1.0};
};

std::vector<std::uint8_t> read_binary(const std::string& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("failed to open shader: " + path);
    }
    const auto size = stream.tellg();
    if (size < 0) {
        throw std::runtime_error("failed to read shader size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!stream) {
        throw std::runtime_error("failed to read shader");
    }
    return bytes;
}

SDL_GPUShader* load_shader(SDL_GPUDevice* device, const std::string& path,
                           const SDL_GPUShaderStage stage, const std::uint32_t uniforms) {
    const auto bytes = read_binary(path);
    const SDL_GPUShaderCreateInfo info{
        bytes.size(), bytes.data(), "main", SDL_GPU_SHADERFORMAT_SPIRV, stage, 0U, 0U,
        0U,           uniforms,     0U};
    auto* shader = SDL_CreateGPUShader(device, &info);
    if (shader == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }
    return shader;
}

std::string shader_path(const std::string& filename) {
    const auto* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        throw std::runtime_error("failed to determine viewer executable directory: " +
                                 std::string(SDL_GetError()));
    }
    return (std::filesystem::path(base_path) / filename).string();
}

cropsim::View2D camera_view(const Camera& camera, const int width, const int height) {
    const auto aspect =
        static_cast<double>(std::max(width, 1)) / static_cast<double>(std::max(height, 1));
    const auto half_width = camera.half_height * aspect;
    return {camera.x - half_width, camera.y - camera.half_height, camera.x + half_width,
            camera.y + camera.half_height};
}

Camera framed_camera(const cropsim::World& world, const int width, const int height) {
    if (world.size() == 0U && world.rails().empty() && world.crop_lines().empty()) {
        return {};
    }
    auto bounds = world.size() == 0U
                      ? cropsim::Aabb{std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::infinity(),
                                     -std::numeric_limits<double>::infinity(),
                                     -std::numeric_limits<double>::infinity()}
                      : world.spatial_index().bounds();
    for (const auto& rail : world.rails()) {
        for (const auto point : rail.points()) {
            bounds.min_x = std::min(bounds.min_x, point.x);
            bounds.min_y = std::min(bounds.min_y, point.y);
            bounds.max_x = std::max(bounds.max_x, point.x);
            bounds.max_y = std::max(bounds.max_y, point.y);
        }
    }
    for (const auto& line : world.crop_lines()) {
        for (const auto point : line.points()) {
            bounds.min_x = std::min(bounds.min_x, point.x); bounds.min_y = std::min(bounds.min_y, point.y);
            bounds.max_x = std::max(bounds.max_x, point.x); bounds.max_y = std::max(bounds.max_y, point.y);
        }
    }
    const auto world_width = std::max(bounds.max_x - bounds.min_x, 0.001);
    const auto world_height = std::max(bounds.max_y - bounds.min_y, 0.001);
    const auto aspect =
        static_cast<double>(std::max(width, 1)) / static_cast<double>(std::max(height, 1));
    return {(bounds.min_x + bounds.max_x) * 0.5, (bounds.min_y + bounds.max_y) * 0.5,
            std::max(world_height * 0.55, world_width * 0.55 / aspect)};
}

std::array<float, 3> diagnostic_color(const std::uint64_t crop_id, const std::size_t leaf_index) {
    auto value = crop_id ^ (static_cast<std::uint64_t>(leaf_index) + 0x9e3779b97f4a7c15ULL);
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    const auto channel = [value](const unsigned int shift) {
        return 0.25F + 0.7F * static_cast<float>((value >> shift) & 0xffU) / 255.0F;
    };
    return {channel(0U), channel(8U), channel(16U)};
}

class Viewer final {
  public:
    Viewer(const cropsim::World& world, const cropsim::viewer::ViewMode view_mode,
           const bool show_rails, const bool show_crop_lines)
        : world_(world), rails_visible_(show_rails), crop_lines_visible_(show_crop_lines), view_mode_(view_mode) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }
        window_ = SDL_CreateWindow("cropsim", 1280, 720, SDL_WINDOW_RESIZABLE);
        device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
        if (window_ == nullptr || device_ == nullptr ||
            !SDL_ClaimWindowForGPUDevice(device_, window_)) {
            throw std::runtime_error(SDL_GetError());
        }
        create_pipeline();
        SDL_GetWindowSizeInPixels(window_, &width_, &height_);
        camera_ = framed_camera(world_, width_, height_);
        instances_dirty_ = true;
    }

    ~Viewer() {
        if (device_ != nullptr) {
            SDL_WaitForGPUIdle(device_);
            if (instance_buffer_ != nullptr)
                SDL_ReleaseGPUBuffer(device_, instance_buffer_);
            if (pipeline_ != nullptr)
                SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
            if (rail_buffer_ != nullptr)
                SDL_ReleaseGPUBuffer(device_, rail_buffer_);
            if (rail_pipeline_ != nullptr)
                SDL_ReleaseGPUGraphicsPipeline(device_, rail_pipeline_);
            SDL_ReleaseWindowFromGPUDevice(device_, window_);
            SDL_DestroyGPUDevice(device_);
        }
        if (window_ != nullptr)
            SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void run(const bool single_frame) {
        auto title_time = std::chrono::steady_clock::now();
        std::size_t frames{};
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                running = handle_event(event);
            }
            if (instances_dirty_)
                update_instances();
            draw();
            ++frames;
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration<double>(now - title_time).count();
            if (elapsed >= 0.5) {
                const auto title =
                    "cropsim - " +
                    std::to_string(static_cast<int>(static_cast<double>(frames) / elapsed)) +
                    " FPS - " + std::to_string(instances_.size()) + " leaves - " +
                    std::to_string(world_.rails().size()) + " rails (" + (rails_visible_ ? "on" : "off") + "), " +
                    std::to_string(world_.crop_lines().size()) + " crop lines (" + (crop_lines_visible_ ? "on)" : "off)");
                SDL_SetWindowTitle(window_, title.c_str());
                frames = 0U;
                title_time = now;
            }
            if (single_frame)
                running = false;
        }
    }

  private:
    void create_pipeline() {
        auto* vertex =
            load_shader(device_, shader_path("crop.vert.spv"), SDL_GPU_SHADERSTAGE_VERTEX, 1U);
        auto* fragment =
            load_shader(device_, shader_path("crop.frag.spv"), SDL_GPU_SHADERSTAGE_FRAGMENT, 0U);
        const SDL_GPUVertexBufferDescription buffer_description{
            0U, sizeof(Instance), SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0U};
        const std::array<SDL_GPUVertexAttribute, 4> attributes{{
            {0U, 0U, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Instance, x)},
            {1U, 0U, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Instance, radius_x)},
            {2U, 0U, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Instance, cosine)},
            {3U, 0U, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Instance, red)},
        }};
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.vertex_input_state = {&buffer_description, 1U, attributes.data(), attributes.size()};
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        SDL_GPUColorTargetDescription color_target{};
        color_target.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
        info.target_info.color_target_descriptions = &color_target;
        info.target_info.num_color_targets = 1U;
        pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
        SDL_ReleaseGPUShader(device_, vertex);
        SDL_ReleaseGPUShader(device_, fragment);
        if (pipeline_ == nullptr)
            throw std::runtime_error(SDL_GetError());

        vertex = load_shader(device_, shader_path("rail.vert.spv"),
                             SDL_GPU_SHADERSTAGE_VERTEX, 1U);
        fragment = load_shader(device_, shader_path("rail.frag.spv"),
                               SDL_GPU_SHADERSTAGE_FRAGMENT, 0U);
        const SDL_GPUVertexBufferDescription rail_buffer_description{
            0U, sizeof(RailVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0U};
        const std::array<SDL_GPUVertexAttribute, 2> rail_attributes{{
            {0U, 0U, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(RailVertex, x)},
            {1U, 0U, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(RailVertex, red)},
        }};
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.vertex_input_state = {&rail_buffer_description, 1U,
                                   rail_attributes.data(), rail_attributes.size()};
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
        rail_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
        SDL_ReleaseGPUShader(device_, vertex);
        SDL_ReleaseGPUShader(device_, fragment);
        if (rail_pipeline_ == nullptr)
            throw std::runtime_error(SDL_GetError());
    }

    bool handle_event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_QUIT)
            return false;
        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            SDL_GetWindowSizeInPixels(window_, &width_, &height_);
            instances_dirty_ = true;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                   event.button.button == SDL_BUTTON_LEFT) {
            dragging_ = true;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                   event.button.button == SDL_BUTTON_LEFT) {
            dragging_ = false;
        } else if (event.type == SDL_EVENT_MOUSE_MOTION && dragging_) {
            const auto view = camera_view(camera_, width_, height_);
            camera_.x -= static_cast<double>(event.motion.xrel) * (view.max_x - view.min_x) /
                         static_cast<double>(std::max(width_, 1));
            camera_.y += static_cast<double>(event.motion.yrel) * (view.max_y - view.min_y) /
                         static_cast<double>(std::max(height_, 1));
            instances_dirty_ = true;
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            camera_.half_height *= std::pow(1.15, -static_cast<double>(event.wheel.y));
            camera_.half_height = std::clamp(camera_.half_height, 1.0e-8, 1.0e12);
            instances_dirty_ = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_Q)
                return false;
            if (event.key.key == SDLK_F && !event.key.repeat) {
                camera_ = framed_camera(world_, width_, height_);
                instances_dirty_ = true;
            } else if (event.key.key == SDLK_C && !event.key.repeat) {
                view_mode_ = view_mode_ == cropsim::viewer::ViewMode::occupancy
                                 ? cropsim::viewer::ViewMode::diagnostic_leaves
                                 : cropsim::viewer::ViewMode::occupancy;
                instances_dirty_ = true;
            } else if (event.key.key == SDLK_R && !event.key.repeat) {
                rails_visible_ = !rails_visible_;
                instances_dirty_ = true;
            } else if (event.key.key == SDLK_L && !event.key.repeat) {
                crop_lines_visible_ = !crop_lines_visible_;
                instances_dirty_ = true;
            } else {
                const auto view = camera_view(camera_, width_, height_);
                const auto pan_x = (view.max_x - view.min_x) * 0.1;
                const auto pan_y = (view.max_y - view.min_y) * 0.1;
                switch (event.key.key) {
                case SDLK_LEFT:
                case SDLK_H:
                    camera_.x -= pan_x;
                    break;
                case SDLK_RIGHT:
                    camera_.x += pan_x;
                    break;
                case SDLK_UP:
                case SDLK_K:
                    camera_.y += pan_y;
                    break;
                case SDLK_DOWN:
                case SDLK_J:
                    camera_.y -= pan_y;
                    break;
                case SDLK_PLUS:
                case SDLK_EQUALS:
                case SDLK_KP_PLUS:
                    camera_.half_height /= 1.15;
                    break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                    camera_.half_height *= 1.15;
                    break;
                default:
                    return true;
                }
                camera_.half_height = std::clamp(camera_.half_height, 1.0e-8, 1.0e12);
                instances_dirty_ = true;
            }
        }
        return true;
    }

    void update_instances() {
        instances_.clear();
        const auto view = camera_view(camera_, width_, height_);
        const auto indices = world_.query(view);
        std::size_t leaf_count{};
        for (const auto index : indices)
            leaf_count += world_.crops()[index].leaves.size();
        instances_.reserve(leaf_count);
        for (const auto index : indices) {
            const auto& crop = world_.crops()[index];
            for (std::size_t leaf_index = 0; leaf_index < crop.leaves.size(); ++leaf_index) {
                const auto& leaf = crop.leaves[leaf_index];
                const auto color = view_mode_ == cropsim::viewer::ViewMode::occupancy
                                       ? std::array<float, 3>{0.18F, 0.78F, 0.28F}
                                       : diagnostic_color(crop.id, leaf_index);
                instances_.push_back(
                    {static_cast<float>(crop.x + leaf.x - camera_.x),
                     static_cast<float>(crop.y + leaf.y - camera_.y),
                     static_cast<float>(leaf.radius_x), static_cast<float>(leaf.radius_y),
                     static_cast<float>(std::cos(leaf.rotation)),
                     static_cast<float>(std::sin(leaf.rotation)), color[0], color[1], color[2]});
            }
        }
        const auto required_size =
            std::max<std::size_t>(sizeof(Instance), instances_.size() * sizeof(Instance));
        if (required_size > instance_capacity_) {
            SDL_WaitForGPUIdle(device_);
            if (instance_buffer_ != nullptr)
                SDL_ReleaseGPUBuffer(device_, instance_buffer_);
            const SDL_GPUBufferCreateInfo buffer_info{SDL_GPU_BUFFERUSAGE_VERTEX,
                                                      static_cast<Uint32>(required_size), 0U};
            instance_buffer_ = SDL_CreateGPUBuffer(device_, &buffer_info);
            instance_capacity_ = required_size;
        }
        if (!instances_.empty()) {
            const SDL_GPUTransferBufferCreateInfo transfer_info{
                SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                static_cast<Uint32>(instances_.size() * sizeof(Instance)), 0U};
            auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
            auto* mapped = SDL_MapGPUTransferBuffer(device_, transfer, false);
            std::memcpy(mapped, instances_.data(), instances_.size() * sizeof(Instance));
            SDL_UnmapGPUTransferBuffer(device_, transfer);
            auto* command = SDL_AcquireGPUCommandBuffer(device_);
            auto* copy = SDL_BeginGPUCopyPass(command);
            const SDL_GPUTransferBufferLocation source{transfer, 0U};
            const SDL_GPUBufferRegion destination{
                instance_buffer_, 0U, static_cast<Uint32>(instances_.size() * sizeof(Instance))};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
            SDL_EndGPUCopyPass(copy);
            SDL_SubmitGPUCommandBuffer(command);
            SDL_ReleaseGPUTransferBuffer(device_, transfer);
        }
        rail_vertices_.clear();
        if (rails_visible_) {
            std::size_t vertex_count{};
            for (const auto& rail : world_.rails()) {
                if (rail.points().size() - 1U >
                    (std::numeric_limits<std::size_t>::max() - vertex_count) / 2U) {
                    throw std::overflow_error("rail viewer vertex count overflow");
                }
                vertex_count += (rail.points().size() - 1U) * 2U;
            }
            rail_vertices_.reserve(vertex_count);
            for (const auto& rail : world_.rails()) {
                for (std::size_t index = 1U; index < rail.points().size(); ++index) {
                    const std::array<std::array<float,3>,4> colors{{{{0.95F,0.75F,0.15F}},{{0.15F,0.9F,1.0F}},{{0.7F,0.35F,0.95F}},{{0.2F,0.95F,0.4F}}}};
                    const auto color=colors[static_cast<std::size_t>(rail.kind())];
                    const auto append = [&](const cropsim::RailPoint point) {
                        rail_vertices_.push_back({
                            static_cast<float>(point.x - camera_.x),
                            static_cast<float>(point.y - camera_.y),
                            color[0], color[1], color[2]});
                    };
                    append(rail.points()[index - 1U]);
                    append(rail.points()[index]);
                }
            }
        }
        if(crop_lines_visible_){for(const auto &line:world_.crop_lines()){for(std::size_t i=1;i<line.points().size();++i){for(const auto p:{line.points()[i-1],line.points()[i]})rail_vertices_.push_back({static_cast<float>(p.x-camera_.x),static_cast<float>(p.y-camera_.y),1.0F,0.3F,0.25F});}}}
        const auto rail_size = std::max<std::size_t>(
            sizeof(RailVertex), rail_vertices_.size() * sizeof(RailVertex));
        if (rail_size > std::numeric_limits<Uint32>::max() ||
            rail_vertices_.size() > std::numeric_limits<Uint32>::max()) {
            throw std::overflow_error("rail viewer GPU buffer limit exceeded");
        }
        if (rail_size > rail_capacity_) {
            SDL_WaitForGPUIdle(device_);
            if (rail_buffer_ != nullptr)
                SDL_ReleaseGPUBuffer(device_, rail_buffer_);
            const SDL_GPUBufferCreateInfo buffer_info{
                SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(rail_size), 0U};
            rail_buffer_ = SDL_CreateGPUBuffer(device_, &buffer_info);
            rail_capacity_ = rail_size;
        }
        if (!rail_vertices_.empty()) {
            const auto byte_size = rail_vertices_.size() * sizeof(RailVertex);
            const SDL_GPUTransferBufferCreateInfo transfer_info{
                SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<Uint32>(byte_size), 0U};
            auto* transfer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
            auto* mapped = SDL_MapGPUTransferBuffer(device_, transfer, false);
            std::memcpy(mapped, rail_vertices_.data(), byte_size);
            SDL_UnmapGPUTransferBuffer(device_, transfer);
            auto* command = SDL_AcquireGPUCommandBuffer(device_);
            auto* copy = SDL_BeginGPUCopyPass(command);
            const SDL_GPUTransferBufferLocation source{transfer, 0U};
            const SDL_GPUBufferRegion destination{
                rail_buffer_, 0U, static_cast<Uint32>(byte_size)};
            SDL_UploadToGPUBuffer(copy, &source, &destination, false);
            SDL_EndGPUCopyPass(copy);
            SDL_SubmitGPUCommandBuffer(command);
            SDL_ReleaseGPUTransferBuffer(device_, transfer);
        }
        instances_dirty_ = false;
    }

    void draw() {
        auto* command = SDL_AcquireGPUCommandBuffer(device_);
        SDL_GPUTexture* swapchain{};
        Uint32 width{};
        Uint32 height{};
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, window_, &swapchain, &width, &height) ||
            swapchain == nullptr) {
            SDL_CancelGPUCommandBuffer(command);
            return;
        }
        const auto view = camera_view(camera_, static_cast<int>(width), static_cast<int>(height));
        const std::array<float, 2> half_extents{
            static_cast<float>((view.max_x - view.min_x) * 0.5),
            static_cast<float>((view.max_y - view.min_y) * 0.5)};
        SDL_PushGPUVertexUniformData(command, 0U, half_extents.data(), sizeof(half_extents));
        SDL_GPUColorTargetInfo target{};
        target.texture = swapchain;
        target.clear_color = {0.02F, 0.025F, 0.02F, 1.0F};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        auto* pass = SDL_BeginGPURenderPass(command, &target, 1U, nullptr);
        SDL_BindGPUGraphicsPipeline(pass, pipeline_);
        if (!instances_.empty()) {
            const SDL_GPUBufferBinding binding{instance_buffer_, 0U};
            SDL_BindGPUVertexBuffers(pass, 0U, &binding, 1U);
            SDL_DrawGPUPrimitives(pass, 6U, static_cast<Uint32>(instances_.size()), 0U, 0U);
        }
        if (!rail_vertices_.empty()) {
            SDL_BindGPUGraphicsPipeline(pass, rail_pipeline_);
            const SDL_GPUBufferBinding binding{rail_buffer_, 0U};
            SDL_BindGPUVertexBuffers(pass, 0U, &binding, 1U);
            SDL_DrawGPUPrimitives(pass, static_cast<Uint32>(rail_vertices_.size()),
                                  1U, 0U, 0U);
        }
        SDL_EndGPURenderPass(pass);
        SDL_SubmitGPUCommandBuffer(command);
    }

    const cropsim::World& world_;
    SDL_Window* window_{};
    SDL_GPUDevice* device_{};
    SDL_GPUGraphicsPipeline* pipeline_{};
    SDL_GPUGraphicsPipeline* rail_pipeline_{};
    SDL_GPUBuffer* instance_buffer_{};
    SDL_GPUBuffer* rail_buffer_{};
    std::size_t instance_capacity_{};
    std::size_t rail_capacity_{};
    std::vector<Instance> instances_;
    std::vector<RailVertex> rail_vertices_;
    Camera camera_;
    int width_{1280};
    int height_{720};
    bool dragging_{};
    bool instances_dirty_{};
    bool rails_visible_{};
    bool crop_lines_visible_{};
    cropsim::viewer::ViewMode view_mode_;
};

} // namespace

int main(const int argc, const char* const* argv) {
    try {
        const auto options = cropsim::viewer::parse_options(argc, argv);
        const auto world = cropsim::viewer::load_world(options);
        if (options.check_only) {
            std::cout << "loaded " << world.size() << " crops\n";
            return 0;
        }
        Viewer viewer(world, options.view_mode, options.show_rails, options.show_crop_lines);
        viewer.run(options.smoke_test);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cropsim_viewer: " << error.what() << '\n';
        return 1;
    }
}
