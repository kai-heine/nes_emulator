#ifndef NES_SDL_HELPER_HPP
#define NES_SDL_HELPER_HPP

#include <SDL2/SDL.h>
#include <memory>

namespace nes::sdl {

struct deleter {
    void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
    void operator()(SDL_Renderer* renderer) const noexcept { SDL_DestroyRenderer(renderer); }
    void operator()(SDL_Surface* surface) const noexcept { SDL_FreeSurface(surface); }
    void operator()(SDL_Texture* texture) const noexcept { SDL_DestroyTexture(texture); }
};

template <typename T>
concept destructable = requires(T* t) {
    deleter{}(t);
};

template <destructable sdl_type>
[[nodiscard]] constexpr auto make_scoped(sdl_type* handle) {
    if (handle == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }
    return std::unique_ptr<sdl_type, deleter>{handle};
}

struct initializer {
    [[nodiscard]] initializer(std::uint32_t flags) {
        if (SDL_Init(flags) < 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ~initializer() noexcept { SDL_Quit(); }
};

} // namespace nes::sdl

#endif
