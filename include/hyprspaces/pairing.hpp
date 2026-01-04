#ifndef HYPRSPACES_PAIRING_HPP
#define HYPRSPACES_PAIRING_HPP

namespace hyprspaces {

    enum class CycleDirection {
        kNext,
        kPrev,
    };

    int normalize_workspace(int workspace_id, int offset);
    int cycle_target(int base, int offset, CycleDirection direction, bool wrap);

} // namespace hyprspaces

#endif // HYPRSPACES_PAIRING_HPP
