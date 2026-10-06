#include "../../app/app.hpp"
#include "runtime.hpp"

#include <SDL.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

/**
 * @brief Interactive SDL2 simulator GUI. Input mapping matches the old
 * port/sdl/m5_gui.c: Up/Down move the rotary, Enter/Space push it, mouse
 * click taps, mouse wheel turns the rotary, Left/Right/Escape/Backspace/`m`
 * map to the directional/back/menu keys. Renders the real M5Paper logical
 * profile (540x960 GRAY4) by dogfooding the exact board::sim::runtime the
 * headless tests use -- this is not a separate code path.
 */
namespace {

constexpr int window_width = board::sim::width;
constexpr int window_height = board::sim::height;

struct gui {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  std::array<std::uint8_t, static_cast<std::size_t>(board::sim::width) * board::sim::height * 3>
      rgb{};
};

void present(gui& g, const board::sim::runtime& sim) {
  for (int y = 0; y < board::sim::height; ++y) {
    for (int x = 0; x < board::sim::width; ++x) {
      const auto packed = sim.framebuffer[static_cast<std::size_t>(y) * board::sim::width / 2 + x / 2];
      const auto nibble = (x & 1) == 0 ? static_cast<std::uint8_t>(packed >> 4U)
                                       : static_cast<std::uint8_t>(packed & 0x0fU);
      const auto gray = static_cast<unsigned char>(nibble * 17U);
      const auto offset = (static_cast<std::size_t>(y) * board::sim::width + x) * 3;
      g.rgb[offset + 0] = gray;
      g.rgb[offset + 1] = gray;
      g.rgb[offset + 2] = gray;
    }
  }

  SDL_UpdateTexture(g.texture, nullptr, g.rgb.data(), board::sim::width * 3);
  int output_w = 0;
  int output_h = 0;
  SDL_GetRendererOutputSize(g.renderer, &output_w, &output_h);
  float scale = static_cast<float>(output_w) / board::sim::width;
  const float scale_h = static_cast<float>(output_h) / board::sim::height;
  if (scale_h < scale) scale = scale_h;
  SDL_Rect dst{static_cast<int>((output_w - board::sim::width * scale) / 2),
               static_cast<int>((output_h - board::sim::height * scale) / 2),
               static_cast<int>(board::sim::width * scale),
               static_cast<int>(board::sim::height * scale)};
  SDL_SetRenderDrawColor(g.renderer, 255, 255, 255, 255);
  SDL_RenderClear(g.renderer);
  SDL_RenderCopy(g.renderer, g.texture, nullptr, &dst);
  SDL_RenderPresent(g.renderer);
}

void inject_key(board::sim::runtime& sim, SDL_Keycode key) {
  switch (key) {
    case SDLK_UP: board::sim::rotary_left(sim); break;
    case SDLK_DOWN: board::sim::rotary_right(sim); break;
    case SDLK_LEFT: board::sim::inject(sim, event::key(event::key_code::left)); break;
    case SDLK_RIGHT: board::sim::inject(sim, event::key(event::key_code::right)); break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE: board::sim::rotary_push(sim); break;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE: board::sim::inject(sim, event::key(event::key_code::back)); break;
    case SDLK_m: board::sim::inject(sim, event::key(event::key_code::menu)); break;
    default: break;
  }
}

}  // namespace

int main() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "simulator_gui: SDL_Init failed: %s\n", SDL_GetError());
    return 2;
  }

  gui g;
  g.window = SDL_CreateWindow("Ebook Reader - M5Paper Simulator", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, window_width, window_height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  g.renderer = g.window ? SDL_CreateRenderer(g.window, -1, SDL_RENDERER_ACCELERATED) : nullptr;
  g.texture = g.renderer ? SDL_CreateTexture(g.renderer, SDL_PIXELFORMAT_RGB24,
                                             SDL_TEXTUREACCESS_STREAMING, window_width,
                                             window_height)
                        : nullptr;
  if (g.texture == nullptr) {
    std::fprintf(stderr, "simulator_gui: SDL setup failed: %s\n", SDL_GetError());
    if (g.renderer != nullptr) SDL_DestroyRenderer(g.renderer);
    if (g.window != nullptr) SDL_DestroyWindow(g.window);
    SDL_Quit();
    return 2;
  }

  board::sim::runtime sim{};
  board::sim::init(sim);

  const char* root = std::getenv("XREADER_SDCARD");
  char default_root[storage::path_max];
  if (root == nullptr) {
    const char* home = std::getenv("HOME");
    std::snprintf(default_root, sizeof(default_root), "%s/data/sdcard", home != nullptr ? home : ".");
    root = default_root;
  }
  board::sim::mount(sim, root);

  app::context application{};
  app::pages nav{};
  state::store memory{};
  state::store persistent{};
  static constexpr shell::theme theme{
      &xr_font_alegreya_14, &xr_font_alegreya_18,      &xr_font_alegreya_bold_18,
      &xr_font_alegreya_bold_26, &xr_font_alegreya_20, 44,
      64,                   16,                        72,
  };
  if (!app::init(application, nav, sim.capabilities, memory, persistent, theme, root)) {
    std::fprintf(stderr, "simulator_gui: app::init failed (check XREADER_SDCARD=%s)\n", root);
    return 3;
  }
  shell::tick(application.shell);
  shell::flush(application.shell);
  app::scan_library(application);

  present(g, sim);

  bool running = true;
  while (running) {
    SDL_Event event{};
    bool need_redraw = false;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        inject_key(sim, event.key.keysym.sym);
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          board::sim::touch(sim, event.button.x, event.button.y);
        }
      } else if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0) board::sim::rotary_left(sim);
        else if (event.wheel.y < 0) board::sim::rotary_right(sim);
      } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED) {
        need_redraw = true;
      }
    }

    const int refresh_before = sim.refresh_count;
    board::sim::advance(sim, 10);
    app::pump(application);
    if (sim.refresh_count != refresh_before) need_redraw = true;
    if (need_redraw) present(g, sim);
    SDL_Delay(10);
  }

  app::checkpoint(application);
  SDL_DestroyTexture(g.texture);
  SDL_DestroyRenderer(g.renderer);
  SDL_DestroyWindow(g.window);
  SDL_Quit();
  return 0;
}
