#ifndef HYPRSPACES_FAILSAFE_HPP
#define HYPRSPACES_FAILSAFE_HPP

#include <exception>
#include <string_view>
#include <utility>

namespace hyprspaces::failsafe {

    template <typename F, typename OnError>
    [[nodiscard]] bool guard(F&& fn, OnError&& on_error, std::string_view context) noexcept {
        try {
            std::forward<F>(fn)();
            return true;
        } catch (const std::exception& ex) {
            try {
                std::forward<OnError>(on_error)(context, ex.what());
            } catch (...) {}
        } catch (...) {
            try {
                std::forward<OnError>(on_error)(context, "unknown exception");
            } catch (...) {}
        }
        return false;
    }

} // namespace hyprspaces::failsafe

#endif // HYPRSPACES_FAILSAFE_HPP
