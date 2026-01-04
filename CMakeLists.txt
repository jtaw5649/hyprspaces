cmake_minimum_required(VERSION 3.28)

project(hyprspaces VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Werror -Wpedantic)
endif()

option(HYPRSPACES_BUILD_TESTS "Build tests" ON)
option(HYPRSPACES_BUILD_PLUGIN "Build Hyprland plugin" ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(hyprspaces_core STATIC
  src/strings.cpp
  src/config_value.cpp
  src/json_utils.cpp
  src/pairing.cpp
  src/dispatch_result.cpp
  src/dispatch.cpp
  src/config.cpp
  src/persistent_workspaces.cpp
  src/autostart.cpp
  src/logging.cpp
  src/status.cpp
  src/paths.cpp
  src/command.cpp
  src/registration.cpp
  src/hyprctl.cpp
  src/history.cpp
  src/actions.cpp
  src/runtime.cpp
  src/runtime_utils.cpp
  src/marks.cpp
  src/function_match.cpp
  src/process_utils.cpp
  src/debounce.cpp
  src/events.cpp
  src/notifications.cpp
  src/monitor_groups.cpp
  src/monitor_profiles.cpp
  src/profiles.cpp
  src/protocol_session.cpp
  src/layout_tree.cpp
  src/session_protocol_store.cpp
  src/session_protocol_state.cpp
  src/setup.cpp
  src/waybar.cpp
  src/waybar_socket.cpp
  src/workspace_rules.cpp
  src/workspace_switch.cpp
)

target_include_directories(hyprspaces_core PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(hyprspaces_core PUBLIC nlohmann_json::nlohmann_json)
set_target_properties(hyprspaces_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
get_target_property(NLOHMANN_INCLUDE_DIRS nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
if(NLOHMANN_INCLUDE_DIRS)
  target_include_directories(hyprspaces_core SYSTEM PUBLIC ${NLOHMANN_INCLUDE_DIRS})
endif()

if(HYPRSPACES_BUILD_PLUGIN)
  if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    message(FATAL_ERROR "hyprspaces plugin supports x86_64 only")
  endif()
  if(NOT HYPRLAND_HEADERS_DIR)
    find_path(HYPRLAND_HEADERS_DIR NAMES hyprland/src/plugins/PluginAPI.hpp)
  endif()
  if(NOT HYPRLAND_HEADERS_DIR)
    message(WARNING "Hyprland headers not found; skipping plugin target.")
  else()
    set(HYPRLAND_REQUIRED_HEADER "hyprland/src/desktop/state/FocusState.hpp")
    if(NOT EXISTS "${HYPRLAND_HEADERS_DIR}/${HYPRLAND_REQUIRED_HEADER}")
      message(WARNING
        "Hyprland headers missing ${HYPRLAND_REQUIRED_HEADER}; skipping plugin target.")
    else()
      if(NOT HYPRLAND_PROTOCOLS_DIR)
        find_path(HYPRLAND_PROTOCOLS_DIR NAMES content-type-v1.hpp
          PATHS ${HYPRLAND_HEADERS_DIR}
          PATH_SUFFIXES hyprland/protocols hyprland/src/protocols/types
        )
      endif()
      if(NOT HYPRLAND_PROTOCOLS_DIR)
        message(WARNING "Hyprland protocol headers not found; skipping plugin target.")
      else()
        if(NOT PIXMAN_INCLUDE_DIR)
          find_path(PIXMAN_INCLUDE_DIR NAMES pixman.h PATH_SUFFIXES pixman-1)
        endif()
        if(NOT PIXMAN_INCLUDE_DIR)
          message(WARNING "pixman headers not found; skipping plugin target.")
        else()
          if(NOT DRM_INCLUDE_DIR)
            find_path(DRM_INCLUDE_DIR NAMES drm_fourcc.h PATH_SUFFIXES libdrm drm)
          endif()
          if(NOT DRM_INCLUDE_DIR)
            message(WARNING "libdrm headers not found; skipping plugin target.")
          else()
        add_library(hyprspaces_plugin SHARED
          src/plugin.cpp
          src/plugin_entry.cpp
          src/session_protocol_server.cpp
          src/layout_capture.cpp
          src/layout_restore.cpp
          src/state_provider.cpp
          src/plugin_state.cpp
          protocols/xx-session-management-v1-protocol.c
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
          target_compile_options(hyprspaces_plugin PRIVATE -Wno-error)
        endif()
        target_link_libraries(hyprspaces_plugin PRIVATE hyprspaces_core)
        target_include_directories(hyprspaces_plugin SYSTEM PRIVATE
          ${HYPRLAND_HEADERS_DIR}
          ${HYPRLAND_PROTOCOLS_DIR}
          ${PIXMAN_INCLUDE_DIR}
          ${DRM_INCLUDE_DIR}
        )
        target_include_directories(hyprspaces_plugin PRIVATE
          ${PROJECT_SOURCE_DIR}/protocols
        )
            set_target_properties(hyprspaces_plugin PROPERTIES OUTPUT_NAME "hyprspaces" PREFIX "")
          endif()
        endif()
      endif()
    endif()
  endif()
endif()

if(HYPRSPACES_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  )
  FetchContent_MakeAvailable(googletest)

  enable_testing()

  add_executable(hyprspaces_tests
    tests/pairing_test.cpp
    tests/dispatch_result_test.cpp
    tests/dispatch_test.cpp
    tests/config_test.cpp
    tests/config_value_test.cpp
    tests/persistent_workspaces_test.cpp
    tests/logging_test.cpp
    tests/status_test.cpp
    tests/paths_test.cpp
    tests/failsafe_test.cpp
    tests/autostart_test.cpp
    tests/command_test.cpp
    tests/plugin_entry_test.cpp
    tests/plugin_registration_test.cpp
    tests/hyprctl_parse_test.cpp
    tests/hyprctl_client_test.cpp
    tests/actions_test.cpp
    tests/history_test.cpp
    tests/runtime_test.cpp
    tests/runtime_utils_test.cpp
    tests/process_utils_test.cpp
    tests/session_registry_test.cpp
    tests/session_protocol_store_test.cpp
    tests/session_protocol_state_test.cpp
    tests/layout_tree_test.cpp
    tests/layout_capture_test.cpp
    tests/layout_restore_test.cpp
    tests/layout_restore_tracker_test.cpp
    tests/toplevel_state_tracker_test.cpp
    tests/function_match_test.cpp
    tests/hyprland_api_stub.cpp
    tests/debounce_test.cpp
    tests/events_test.cpp
    tests/notifications_test.cpp
    tests/setup_test.cpp
    tests/monitor_groups_test.cpp
    tests/monitor_profiles_test.cpp
    tests/profile_catalog_test.cpp
    tests/profiles_test.cpp
    tests/plugin_install_script_test.cpp
    tests/waybar_test.cpp
    tests/waybar_socket_test.cpp
    tests/workspace_rules_test.cpp
    tests/strings_test.cpp
    tests/json_utils_test.cpp
    tests/file_descriptor_test.cpp
    tests/headless_cleanup_script_test.cpp
  )
  target_link_libraries(hyprspaces_tests PRIVATE hyprspaces_core GTest::gtest_main)
  target_compile_definitions(hyprspaces_core PRIVATE HYPRSPACES_TESTING)
  target_compile_definitions(hyprspaces_tests PRIVATE HYPRSPACES_TESTING)

  add_test(NAME hyprspaces_tests COMMAND hyprspaces_tests)
endif()
