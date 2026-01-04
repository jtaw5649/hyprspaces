#include "hyprspaces/pairing.hpp"

namespace hyprspaces {

    int normalize_workspace(int workspace_id, int offset) {
        if (offset <= 0) {
            return workspace_id;
        }
        return ((workspace_id - 1) % offset) + 1;
    }

    int cycle_target(int base, int offset, CycleDirection direction, bool wrap) {
        switch (direction) {
            case CycleDirection::kNext:
                if (wrap) {
                    return (base % offset) + 1;
                }
                if (base >= offset) {
                    return offset;
                }
                return base + 1;
            case CycleDirection::kPrev:
                if (wrap) {
                    return ((base + offset - 2) % offset) + 1;
                }
                if (base <= 1) {
                    return 1;
                }
                return base - 1;
        }
        return base;
    }

} // namespace hyprspaces
