#ifndef HYPRSPACES_DEBOUNCE_HPP
#define HYPRSPACES_DEBOUNCE_HPP

#include <chrono>
#include <optional>

namespace hyprspaces {

    class RebalanceDebounce {
      public:
        explicit RebalanceDebounce(std::chrono::milliseconds min_interval);

        bool record_event(std::chrono::steady_clock::time_point now);
        bool flush(std::chrono::steady_clock::time_point now);

      private:
        bool                                                 should_run_now(std::chrono::steady_clock::time_point now) const;

        std::chrono::milliseconds                            min_interval_;
        std::optional<std::chrono::steady_clock::time_point> last_rebalance_;
        std::optional<std::chrono::steady_clock::time_point> last_event_;
        bool                                                 pending_;
    };

    class PendingDebounce {
      public:
        explicit PendingDebounce(std::chrono::milliseconds min_interval);

        void mark(std::chrono::steady_clock::time_point now);
        bool flush(std::chrono::steady_clock::time_point now);

      private:
        std::chrono::milliseconds                            min_interval_;
        std::optional<std::chrono::steady_clock::time_point> last_event_;
        bool                                                 pending_;
    };

    class FocusSwitchDebounce {
      public:
        explicit FocusSwitchDebounce(std::chrono::milliseconds min_interval);

        bool should_switch(std::chrono::steady_clock::time_point now, int workspace);

      private:
        std::chrono::milliseconds                            min_interval_;
        std::optional<std::chrono::steady_clock::time_point> last_switch_;
        std::optional<int>                                   last_workspace_;
    };

} // namespace hyprspaces

#endif // HYPRSPACES_DEBOUNCE_HPP
