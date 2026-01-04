#include "hyprspaces/debounce.hpp"

namespace hyprspaces {

    RebalanceDebounce::RebalanceDebounce(std::chrono::milliseconds min_interval) : min_interval_(min_interval), pending_(false) {}

    bool RebalanceDebounce::record_event(std::chrono::steady_clock::time_point now) {
        last_event_ = now;
        if (should_run_now(now)) {
            last_rebalance_ = now;
            pending_        = false;
            return true;
        }
        pending_ = true;
        return false;
    }

    bool RebalanceDebounce::flush(std::chrono::steady_clock::time_point now) {
        if (!pending_) {
            return false;
        }
        if (!last_event_) {
            return false;
        }
        if (now - *last_event_ < min_interval_) {
            return false;
        }
        if (!should_run_now(now)) {
            return false;
        }
        pending_        = false;
        last_rebalance_ = now;
        return true;
    }

    bool RebalanceDebounce::should_run_now(std::chrono::steady_clock::time_point now) const {
        if (!last_rebalance_) {
            return true;
        }
        return now - *last_rebalance_ >= min_interval_;
    }

    PendingDebounce::PendingDebounce(std::chrono::milliseconds min_interval) : min_interval_(min_interval), pending_(false) {}

    void PendingDebounce::mark(std::chrono::steady_clock::time_point now) {
        last_event_ = now;
        pending_    = true;
    }

    bool PendingDebounce::flush(std::chrono::steady_clock::time_point now) {
        if (!pending_) {
            return false;
        }
        if (!last_event_) {
            return false;
        }
        if (now - *last_event_ < min_interval_) {
            return false;
        }
        pending_ = false;
        return true;
    }

    FocusSwitchDebounce::FocusSwitchDebounce(std::chrono::milliseconds min_interval) : min_interval_(min_interval) {}

    bool FocusSwitchDebounce::should_switch(std::chrono::steady_clock::time_point now, int workspace) {
        if (last_switch_ && last_workspace_ && *last_workspace_ == workspace && now - *last_switch_ < min_interval_) {
            return false;
        }
        last_switch_    = now;
        last_workspace_ = workspace;
        return true;
    }

} // namespace hyprspaces
