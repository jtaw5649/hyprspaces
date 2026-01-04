#include "hyprspaces/function_match.hpp"

namespace hyprspaces {
    namespace {

        bool contains(std::string_view haystack, std::string_view needle) {
            if (needle.empty()) {
                return true;
            }
            return haystack.find(needle) != std::string_view::npos;
        }

        bool match_any(const FunctionMatch& match, std::string_view needle) {
            return contains(match.signature, needle) || contains(match.demangled, needle);
        }

        template <typename Predicate>
        std::optional<FunctionMatch> first_match(const std::vector<FunctionMatch>& matches, Predicate predicate) {
            for (const auto& match : matches) {
                if (predicate(match)) {
                    return match;
                }
            }
            return std::nullopt;
        }

    } // namespace

    std::optional<FunctionMatch> select_function_match(const std::vector<FunctionMatch>& matches, std::string_view class_tag, std::string_view signature_hint) {
        if (matches.empty()) {
            return std::nullopt;
        }

        const bool has_class     = !class_tag.empty();
        const bool has_signature = !signature_hint.empty();

        auto       class_ok = [&](const FunctionMatch& match) { return !has_class || match_any(match, class_tag); };
        auto       sig_ok   = [&](const FunctionMatch& match) { return !has_signature || match_any(match, signature_hint); };

        if (auto both = first_match(matches, [&](const FunctionMatch& match) { return class_ok(match) && sig_ok(match); })) {
            return both;
        }

        if (has_signature) {
            if (auto by_signature = first_match(matches, [&](const FunctionMatch& match) { return sig_ok(match); })) {
                return by_signature;
            }
        }

        if (has_class) {
            if (auto by_class = first_match(matches, [&](const FunctionMatch& match) { return class_ok(match); })) {
                return by_class;
            }
        }

        return std::nullopt;
    }

} // namespace hyprspaces
