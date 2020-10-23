#ifndef NES_SDL_HELPER_HPP
#define NES_SDL_HELPER_HPP

#include <SDL2/SDL.h>
#include <memory>
#include <type_traits>

namespace nes::sdl {

// naive bare minimum implementation
template <typename T, typename D>
struct [[nodiscard]] unique_resource {
    explicit unique_resource(T t) noexcept : handle{t} {}
    ~unique_resource() noexcept { D{}(handle); }
    T& get() noexcept { return handle; }
    T const& get() const noexcept { return handle; }

  private:
    T handle{};
};

struct deleter {
    void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
    void operator()(SDL_Renderer* renderer) const noexcept { SDL_DestroyRenderer(renderer); }
    void operator()(SDL_Surface* surface) const noexcept { SDL_FreeSurface(surface); }
    void operator()(SDL_Texture* texture) const noexcept { SDL_DestroyTexture(texture); }
    void operator()(SDL_GameController* controller) const noexcept {
        SDL_GameControllerClose(controller);
    }
    void operator()(SDL_AudioDeviceID device_id) const noexcept { SDL_CloseAudioDevice(device_id); }
};

template <typename T>
concept destructable = requires(T* t) {
    deleter{}(t);
};

template <typename T>
concept closable = requires(T t) {
    deleter{}(t);
}
&&!std::is_pointer_v<T>;

template <destructable sdl_type>
using ptr = std::unique_ptr<sdl_type, deleter>;

template <destructable sdl_type>
[[nodiscard]] constexpr auto make_scoped(sdl_type* pointer) {
    if (pointer == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }
    return std::unique_ptr<sdl_type, deleter>{pointer};
}

template <closable sdl_handle>
[[nodiscard]] constexpr auto make_scoped(sdl_handle handle) {
    if (handle == 0) {
        throw std::runtime_error(SDL_GetError());
    }
    return unique_resource<sdl_handle, deleter>{handle};
}

struct initializer {
    [[nodiscard]] initializer(std::uint32_t flags) {
        if (SDL_Init(flags) < 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ~initializer() noexcept { SDL_Quit(); }
};

constexpr void checked(int return_value) {
    if (return_value == -1) {
        throw std::runtime_error(SDL_GetError());
    }
}

} // namespace nes::sdl

#endif
