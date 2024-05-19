#include <stdio.h>

#include "glStuff.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>  // Will drag system OpenGL headers

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "parsing.hpp"

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

float gScale = 1;
// Camera matrix
glm::mat4 view = glm::lookAt(
    glm::vec3(0, 0, 3),  // Camera is at (0, 0, 3), in World Space
    glm::vec3(0, 0, 0),  // and looks at the origin
    glm::vec3(0, 1, 0)   // Head is up
);
glm::mat4 projection;
glm::vec2 mousePos = glm::vec2(-1);

glm::vec2 screenSize;

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    glm::vec2 newPos{xpos, ypos};
    if (mousePos == newPos)
        return;

    if (mousePos == glm::vec2(-1)) {
        mousePos = newPos;
        return;
    }
    auto delta = mousePos - newPos;
    mousePos = newPos;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
        view = glm::translate(view, glm::vec3(-delta / gScale, 0));
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float scale = 1;
    if (yoffset > 0) {
        scale = 1.1;
    } else if (yoffset < 0) {
        scale = 1 / 1.1;
    }

    auto mp = glm::vec4(((mousePos - screenSize / 2.0f) / screenSize) * 2.0f, 0, 1);
    mp.y = -mp.y;
    auto wp = glm::vec2(glm::inverse(projection * view) * mp);

    gScale *= scale;
    view = glm::translate(view, glm::vec3(wp, 0));
    view = glm::scale(view, glm::vec3(scale, scale, 1));
    view = glm::translate(view, glm::vec3(-wp, 0));
}

static void onResize(GLFWwindow* window, int width, int height) {
    screenSize = glm::vec2(width, height);
    glViewport(0, 0, width, height);
    projection = glm::ortho<float>(0, width, height, 0, 0.0f, 100.0f);
    // projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 100.0f); // In world coordinates
}

static bool isSolid(uint16_t id) {
    return id == 81 ||
           id == 356 || id == 314 || id == 299 || id == 140 || id == 180 || id == 308 || id == 546 || id == 676 ||
           id == 215 || id == 1 || id == 357 || id == 390 || id == 452 || id == 309 || id == 389 || id == 307 || id == 727;
}

std::vector<glm::vec4> renderMap(const Map& map, int layer, std::map<uint32_t, SpriteData>& sprites, std::vector<uv_data>& uvs) {
    auto atlasSize = glm::vec2(1024, 2048);

    std::vector<glm::vec4> vertecies;

    for (auto&& room : map.rooms) {
        for (int y2 = 0; y2 < 22; y2++) {
            for (int x2 = 0; x2 < 40; x2++) {
                auto tile = room.tiles[layer][y2][x2];
                // assert((tile & 0xFFFF0000) == 0);
                if (tile.tile_id == 0 || tile.tile_id >= 0x400) continue;

                auto pos = glm::vec2(x2 + room.x * 40, y2 + room.y * 22);
                pos *= 8;
                auto uv = uvs[tile.tile_id];

                if (sprites.contains(tile.tile_id)) {
                    auto& sprite = sprites[tile.tile_id];
                    // __debugbreak();

                    int composition_id = 0;

                    for (int j = 0; j < sprite.layer_count; ++j) {
                        auto subsprite_id = sprite.compositions[composition_id * sprite.layer_count + j];
                        if (subsprite_id >= sprite.subsprite_count)
                            continue;

                        auto& layer = sprite.layers[j];
                        if (layer.is_normals1 || layer.is_normals2 || !layer.is_visible) continue;

                        auto& subsprite = sprite.sub_sprites[subsprite_id];

                        auto aUv = glm::vec2(uv.pos + subsprite.atlas_pos);
                        auto size = glm::vec2(subsprite.size);
                        auto ap = pos + glm::vec2(subsprite.composite_pos);

                        if (tile.vertical_mirror) {
                            ap.y = pos.y + (sprite.composite_size.y - (subsprite.composite_pos.y + subsprite.size.y));
                            aUv.y += size.y;
                            size.y = -size.y;
                        }
                        if (tile.horizontal_mirror) {
                            ap.x = pos.x + (sprite.composite_size.x - (subsprite.composite_pos.x + subsprite.size.x));
                            aUv.x += size.x;
                            size.x = -size.x;
                        }
                        assert(!tile.rotate_90 && !tile.rotate_180, "sprite rotation not implemented");

                        vertecies.emplace_back(ap, aUv / atlasSize);                                                                // tl
                        vertecies.emplace_back(ap + glm::vec2(subsprite.size.x, 0), glm::vec2(aUv.x + size.x, aUv.y) / atlasSize);  // tr
                        vertecies.emplace_back(ap + glm::vec2(0, subsprite.size.y), glm::vec2(aUv.x, aUv.y + size.y) / atlasSize);  // bl

                        vertecies.emplace_back(ap + glm::vec2(subsprite.size.x, 0), glm::vec2(aUv.x + size.x, aUv.y) / atlasSize);  // tr
                        vertecies.emplace_back(ap + glm::vec2(subsprite.size), (aUv + size) / atlasSize);                           // br
                        vertecies.emplace_back(ap + glm::vec2(0, subsprite.size.y), glm::vec2(aUv.x, aUv.y + size.y) / atlasSize);  // bl
                    }
                } else {
                    auto right = glm::vec2(uv.size.x, 0);
                    auto down = glm::vec2(0, uv.size.y);

                    bool self_contiguous = (uv.flags & (1 << 9)) != 0;
                    bool contiguous = (uv.flags & (1 << 7)) != 0;

                    if (contiguous || self_contiguous) {
                        bool l, r, u, d;

                        auto l_ = map.getTile(layer, x2 + room.x * 40 - 1, y2 + room.y * 22);
                        auto r_ = map.getTile(layer, x2 + room.x * 40 + 1, y2 + room.y * 22);
                        auto u_ = map.getTile(layer, x2 + room.x * 40, y2 + room.y * 22 - 1);
                        auto d_ = map.getTile(layer, x2 + room.x * 40, y2 + room.y * 22 + 1);

                        if (self_contiguous) {
                            l = l_.has_value() && l_.value().tile_id == tile.tile_id;
                            r = r_.has_value() && r_.value().tile_id == tile.tile_id;
                            u = u_.has_value() && u_.value().tile_id == tile.tile_id;
                            d = d_.has_value() && d_.value().tile_id == tile.tile_id;
                        } else {
                            l = !l_.has_value() || isSolid(l_.value().tile_id);
                            r = !r_.has_value() || isSolid(r_.value().tile_id);
                            u = !u_.has_value() || isSolid(u_.value().tile_id);
                            d = !d_.has_value() || isSolid(d_.value().tile_id);
                        }

                        if (l && r && u && d) {
                            uv.pos.x += 8;
                            uv.pos.y += 8;
                        } else if (l && r && d) {
                            uv.pos.x += 8;
                        } else if (l && u && d) {
                            uv.pos.x += 16;
                            uv.pos.y += 8;
                        } else if (l && u && r) {
                            uv.pos.x += 8;
                            uv.pos.y += 16;
                        } else if (u && d && r) {
                            uv.pos.y += 8;
                        } else if (l && u) {
                            uv.pos.x += 16;
                            uv.pos.y += 16;
                        } else if (l && d) {
                            uv.pos.x += 16;
                        } else if (u && r) {
                            uv.pos.y += 16;
                        } else if (d && r) {
                            // default
                        } else if (l && r) {
                            uv.pos.x += 8;
                            uv.pos.y += 24;
                        } else if (u && d) {
                            uv.pos.x += 24;
                            uv.pos.y += 8;
                        } else if (l) {
                            uv.pos.x += 16;
                            uv.pos.y += 24;
                        } else if (r) {
                            uv.pos.y += 24;
                        } else if (u) {
                            uv.pos.x += 24;
                            uv.pos.y += 16;
                        } else if (d) {
                            uv.pos.x += 24;
                        } else {
                            uv.pos.x += 24;
                            uv.pos.y += 24;
                        }
                    } else {  // flags ignore for tiling
                        if (tile.vertical_mirror) {
                            uv.pos += down;
                            down = -down;
                        }
                        if (tile.horizontal_mirror) {
                            uv.pos += right;
                            right = -right;
                        }
                        if (tile.rotate_90) {
                            std::tie(uv.size.x, uv.size.y) = std::tie(uv.size.y, uv.size.x);  // flip size
                            uv.pos += down;
                            std::tie(down, right) = std::pair(right, -down);
                        }
                        if (tile.rotate_180) {
                            uv.pos += down + right;
                            down = -down;
                            right = -right;
                        }
                    }
                    auto uvp = glm::vec2(uv.pos);

                    vertecies.emplace_back(pos                          , uvp / atlasSize);           // tl
                    vertecies.emplace_back(pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize); // tr
                    vertecies.emplace_back(pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);  // bl

                    vertecies.emplace_back(pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize);   // tr
                    vertecies.emplace_back(pos + glm::vec2(uv.size), (uvp + down + right) / atlasSize); // br
                    vertecies.emplace_back(pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);    // bl
                }
            }
        }
    }

    return vertecies;
}

// Main code
int main(int, char**) {
#pragma region glfw/opengl setup
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Animal Well map viewer", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync

    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        printf("Failed to initialize OpenGL context\n");
        return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetFramebufferSizeCallback(window, onResize);
    onResize(window, 1280, 720);
#pragma endregion

#pragma region imgui setup
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != nullptr);
#pragma endregion

#pragma region map rendering
    auto gameData = loadGameAssets();

    Texture atlas;
    atlas.Load(gameData[255].data);

    bool show_fg = true;
    bool show_bg = true;

    glm::vec4 bg_color{0.8, 0.8, 0.8, 1};
    glm::vec4 fg_color{1, 1, 1, 1};

    int selectedMap = 0;
    static int mapIds[5] = {300, 193, 52, 222, 157};
    static const char* mapNames[5] = {"Overworld", "Space", "Bunny temple", "Time Capsule", "CE temple"};
    Map maps[5]{};
    for (size_t i = 0; i < 5; i++) {
        maps[i] = parse_map(gameData[mapIds[i]]);
    }

    std::map<uint32_t, SpriteData> sprites;
    for (auto el : spriteMapping) {
        sprites[el.tile_id] = parse_sprite(gameData[el.asset_id]);
    }

    std::vector<uv_data> uvs = parse_uvs(gameData[254]);
    // position.xy/uv.zw
    std::vector<glm::vec4> vertecies_fg = renderMap(maps[selectedMap], 0, sprites, uvs);
    std::vector<glm::vec4> vertecies_bg = renderMap(maps[selectedMap], 1, sprites, uvs);

#pragma endregion

#pragma region opgenl buffer setup
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShaderProgram shader("shaders/shader.vs", "shaders/shader.fs");
    shader.Use();

    VAO VAO_fg{};
    VBO VBO_fg{GL_ARRAY_BUFFER, GL_STATIC_DRAW};
    VAO_fg.Bind();
    VBO_fg.Bind();
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    VBO_fg.BufferData(vertecies_fg.data(), vertecies_fg.size() * sizeof(glm::vec4));

    VAO VAO_bg{};
    VBO VBO_bg{GL_ARRAY_BUFFER, GL_STATIC_DRAW};
    VAO_bg.Bind();
    VBO_bg.Bind();
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    VBO_bg.BufferData(vertecies_bg.data(), vertecies_bg.size() * sizeof(glm::vec4));
#pragma endregion

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto MVP = projection * view;

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        // if(show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        {
            ImGui::Begin("Window");

            if (ImGui::Combo("Map", &selectedMap, mapNames, 5)) {
                vertecies_fg = renderMap(maps[selectedMap], 0, sprites, uvs);
                vertecies_bg = renderMap(maps[selectedMap], 1, sprites, uvs);

                VBO_fg.BufferData(vertecies_fg.data(), vertecies_fg.size() * sizeof(glm::vec4));
                VBO_bg.BufferData(vertecies_bg.data(), vertecies_bg.size() * sizeof(glm::vec4));
            }
            ImGui::Checkbox("Render Foreground", &show_fg);
            ImGui::ColorEdit4("Foreground color", &fg_color.r);
            ImGui::Checkbox("Render Background", &show_bg);
            ImGui::ColorEdit4("Background color", &bg_color.r);

            auto mp = glm::vec4(((mousePos - screenSize / 2.0f) / screenSize) * 2.0f, 0, 1);
            mp.y = -mp.y;
            auto wp = glm::vec2(glm::inverse(MVP) * mp);

            // ImGui::Text("mouse pos %f %f", mousePos.x, mousePos.y);
            ImGui::Text("world pos %f %f", (wp.x / 8), (wp.y / 8));

            auto room_size = glm::vec2(40 * 8, 22 * 8);
            auto rp = glm::ivec2(wp / room_size);
            auto room = maps[selectedMap](rp.x, rp.y);

            if (wp.x >= 0 && wp.y >= 0 && room != nullptr) {
                ImGui::NewLine();
                ImGui::Text("room position %i %i", room->x, room->y);
                ImGui::Text("room water level %i", room->waterLevel);
                ImGui::Text("room background id %i", room->bgId);

                auto tp = glm::ivec2(glm::mod(wp, room_size) / 8.0f);
                auto tile = room->tiles[0][tp.y][tp.x];
                int tile_layer = 0;

                if (show_fg && tile.tile_id != 0) {
                    tile_layer = 0;
                } else {
                    tile = room->tiles[1][tp.y][tp.x];
                    tile_layer = 1;
                    if (!show_bg || tile.tile_id == 0) {
                        tile = {};
                        tile_layer = 2;
                    }
                }

                ImGui::NewLine();
                ImGui::Text("tile");
                ImGui::Text("offset %i %i %s", tp.x, tp.y, tile_layer == 0 ? "Foreground" : tile_layer == 1 ? "Background" : "N/A");
                ImGui::Text("id %i", tile.tile_id);
                ImGui::Text("param %i", tile.param);

                ImGui::BeginDisabled();
                int flags = tile.flags;
                ImGui::CheckboxFlags("horizontal_mirror", &flags, 1);
                ImGui::CheckboxFlags("vertical_mirror", &flags, 2);
                ImGui::CheckboxFlags("rotate_90", &flags, 4);
                ImGui::CheckboxFlags("rotate_180", &flags, 8);

                int uv_flags = uvs[tile.tile_id].flags;
                ImGui::NewLine();
                ImGui::Text("uv flags");

                ImGui::CheckboxFlags("collides_left", &uv_flags, 1 << 0);   // correct
                ImGui::CheckboxFlags("collides_right", &uv_flags, 1 << 1);  // correct
                ImGui::CheckboxFlags("collides_up", &uv_flags, 1 << 2);     // correct
                ImGui::CheckboxFlags("collides_down", &uv_flags, 1 << 3);   // correct
                ImGui::CheckboxFlags("not_placeable", &uv_flags, 1 << 4);
                ImGui::CheckboxFlags("additive", &uv_flags, 1 << 5);
                ImGui::CheckboxFlags("obscures", &uv_flags, 1 << 6);
                ImGui::CheckboxFlags("contiguous", &uv_flags, 1 << 7); // correct
                ImGui::CheckboxFlags("blocks_light", &uv_flags, 1 << 8);
                ImGui::CheckboxFlags("self_contiguous", &uv_flags, 1 << 9); // correct
                ImGui::CheckboxFlags("hidden", &uv_flags, 1 << 10);
                ImGui::CheckboxFlags("dirt", &uv_flags, 1 << 11);
                ImGui::CheckboxFlags("has_normals", &uv_flags, 1 << 12);  // correct
                ImGui::CheckboxFlags("uv_light", &uv_flags, 1 << 13);     // correct

                ImGui::EndDisabled();

                if (sprites.contains(tile.tile_id)) {
                    auto sprite = sprites[tile.tile_id];

                    ImGui::NewLine();
                    ImGui::Text("sprite");
                    ImGui::Text("composite_size %i %i", sprite.composite_size.x, sprite.composite_size.y);
                    ImGui::Text("layer_count %i", sprite.layer_count);
                    ImGui::Text("composition_count %i", sprite.composition_count);
                    ImGui::Text("subsprite_count %i", sprite.subsprite_count);
                    ImGui::Text("animation_count %i", sprite.animation_count);
                }
            }

            ImGui::End();
        }

        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.setMat4("MVP", MVP);

        glActiveTexture(GL_TEXTURE0);
        atlas.Bind();

        if (show_bg) {
            shader.setVec4("color", bg_color);
            VAO_bg.Bind();
            glDrawArrays(GL_TRIANGLES, 0, vertecies_bg.size());
        }
        if (show_fg) {
            shader.setVec4("color", fg_color);
            VAO_fg.Bind();
            glDrawArrays(GL_TRIANGLES, 0, vertecies_fg.size());
        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
