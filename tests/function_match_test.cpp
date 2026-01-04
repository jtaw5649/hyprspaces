#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "hyprspaces/function_match.hpp"

namespace {

    hyprspaces::FunctionMatch make_match(void* address, std::string signature, std::string demangled) {
        return hyprspaces::FunctionMatch{
            .address   = address,
            .signature = std::move(signature),
            .demangled = std::move(demangled),
        };
    }

} // namespace

TEST(FunctionMatch, PrefersClassAndSignature) {
    std::vector<hyprspaces::FunctionMatch> matches;
    matches.push_back(
        make_match(reinterpret_cast<void*>(0x1), "void CWindow::updateWindowData(SWorkspaceRule const&)", "Desktop::View::CWindow::updateWindowData(SWorkspaceRule const&)"));
    matches.push_back(make_match(reinterpret_cast<void*>(0x2), "void CWindow::updateWindowData()", "Desktop::View::CWindow::updateWindowData()"));

    const auto selected = hyprspaces::select_function_match(matches, "CWindow", "updateWindowData()");

    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->address, reinterpret_cast<void*>(0x2));
}

TEST(FunctionMatch, SelectsByClassWhenSignatureNotProvided) {
    std::vector<hyprspaces::FunctionMatch> matches;
    matches.push_back(make_match(reinterpret_cast<void*>(0x1), "void CHyprDwindleLayout::onWindowCreatedTiling()", "CHyprDwindleLayout::onWindowCreatedTiling()"));
    matches.push_back(make_match(reinterpret_cast<void*>(0x2), "void CHyprMasterLayout::onWindowCreatedTiling()", "CHyprMasterLayout::onWindowCreatedTiling()"));

    const auto selected = hyprspaces::select_function_match(matches, "CHyprMasterLayout", {});

    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->address, reinterpret_cast<void*>(0x2));
}

TEST(FunctionMatch, FallsBackToSignatureWhenClassMissing) {
    std::vector<hyprspaces::FunctionMatch> matches;
    matches.push_back(make_match(reinterpret_cast<void*>(0x1), "void onMap(void*)", "onMap"));
    matches.push_back(make_match(reinterpret_cast<void*>(0x2), "void onUpdateState(void*)", "onUpdateState"));

    const auto selected = hyprspaces::select_function_match(matches, "CWindow", "onUpdateState");

    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(selected->address, reinterpret_cast<void*>(0x2));
}

TEST(FunctionMatch, ReturnsNulloptWhenNoMatches) {
    std::vector<hyprspaces::FunctionMatch> matches;

    const auto                             selected = hyprspaces::select_function_match(matches, "CWindow", "updateWindowData()");

    EXPECT_FALSE(selected.has_value());
}
