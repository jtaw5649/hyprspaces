#ifndef HYPRSPACES_MARKS_HPP
#define HYPRSPACES_MARKS_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hyprspaces {

    class MarksStore {
      public:
        void                       clear();
        void                       set(std::string name, std::string address);
        std::optional<std::string> get(std::string_view name) const;
        bool                       erase(std::string_view name);
        std::vector<std::string>   list_names() const;

      private:
        std::unordered_map<std::string, std::string> marks_;
    };

} // namespace hyprspaces

#endif // HYPRSPACES_MARKS_HPP
