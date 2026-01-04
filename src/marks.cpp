#include "hyprspaces/marks.hpp"

namespace hyprspaces {

    void MarksStore::clear() {
        marks_.clear();
    }

    void MarksStore::set(std::string name, std::string address) {
        marks_[std::move(name)] = std::move(address);
    }

    std::optional<std::string> MarksStore::get(std::string_view name) const {
        const auto it = marks_.find(std::string(name));
        if (it == marks_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool MarksStore::erase(std::string_view name) {
        return marks_.erase(std::string(name)) > 0;
    }

    std::vector<std::string> MarksStore::list_names() const {
        std::vector<std::string> names;
        names.reserve(marks_.size());
        for (const auto& entry : marks_) {
            names.push_back(entry.first);
        }
        return names;
    }

} // namespace hyprspaces
