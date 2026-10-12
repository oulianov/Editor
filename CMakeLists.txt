cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW) # Support MSVC_RUNTIME_LIBRARY

project(EasyRPG_Editor VERSION 0.1.0
	DESCRIPTION "Editor for RPG Maker 2000/2003 games (fork of the EasyRPG Editor)"
	HOMEPAGE_URL "https://github.com/oulianov/Editor"
	LANGUAGES CXX)
if(APPLE)
	# Native macOS audio and application activation
	enable_language(OBJCXX)
endif()

set(REQUIRED_QT_VERSION "6.10.1")
# A dependency missing: where to get it (docs/Building.md)
macro(editor_find_package name)
	find_package(${name} ${ARGN} QUIET)
	if(NOT ${name}_FOUND)
		if(APPLE)
			set(_editor_hint "run builds/macos/build.sh, which builds it with the Editor")
		else()
			set(_editor_hint "use the vcpkg presets (cmake --preset vcpkg-liblcf-release)")
		endif()
		message(FATAL_ERROR "${name} was not found. To get it, ${_editor_hint}, "
			"or add its install folder to CMAKE_PREFIX_PATH. See docs/Building.md.")
	endif()
endmacro()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/builds/cmake/Modules")
include(EditorConfigureWindows)

# Dependencies provided by CMake Presets
option(EDITOR_FIND_ROOT_PATH_APPEND "When enabled the items from CMAKE_PREFIX_PATH are added to CMAKE_FIND_ROOT_PATH. Works around problematic CMake Toolchain files" OFF)
list(APPEND CMAKE_PREFIX_PATH "${EDITOR_PREFIX_PATH_APPEND}")
if(EDITOR_FIND_ROOT_PATH_APPEND)
	list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_PREFIX_PATH}")
endif()

# C++23 is required
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source Files
set(EDITOR_SOURCES
	src/common/dbstring.h
	src/common/editor_settings.cpp
	src/common/editor_settings.h
	src/common/translations.cpp
	src/common/translations.h
	src/common/audio_backend.h
	src/common/audio_player.cpp
	src/common/audio_player.h
	src/common/filefinder.cpp
	src/common/logging.cpp
	src/common/logging.h
	src/common/filefinder.h
	src/common/exfont.cpp
	src/common/game_font.cpp
	src/common/rpg_rt_exe.cpp
	src/common/ini_file.cpp
	src/common/ini_file.h
	src/common/exfont.h
	src/common/game_font.h
	src/common/rpg_rt_exe.h
	src/common/image_loader.cpp
	src/common/image_loader.h
	src/common/lcf_widget_binding.cpp
	src/common/lcf_widget_binding.h
	src/common/scope_guard.h
	src/common/signal_blocker.h
	src/common/sortfilter_proxy_models.cpp
	src/common/sortfilter_proxy_models.h
	src/core.cpp
	src/core.h
	src/defines.h
	src/main.cpp
	src/model/actor.cpp
	src/model/actor.h
	src/model/animation.cpp
	src/model/animation.h
	src/model/attribute.cpp
	src/model/attribute.h
	src/model/battleranimation.cpp
	src/model/battleranimation.h
	src/model/chipset.cpp
	src/model/chipset.h
	src/model/class.cpp
	src/model/class.h
	src/model/commonevent.cpp
	src/model/commonevent.h
	src/model/enemy.cpp
	src/model/enemy.h
	src/model/event_command_defaults.cpp
	src/model/event_command_defaults.h
	src/model/event_command_list.cpp
	src/model/move_route_codec.cpp
	src/model/move_route_codec.h
	src/model/event_command_list.h
	src/model/event_page_defaults.h
	src/model/export_files.cpp
	src/model/export_files.h
	src/model/export_references.cpp
	src/model/export_references.h
	src/model/game_export.cpp
	src/model/game_export.h
	src/model/web_player.cpp
	src/model/web_player.h
	src/model/item.cpp
	src/model/item.h
	src/model/map_tools.cpp
	src/model/map_tools.h
	src/model/maniac_expression.cpp
	src/model/maniac_expression.h
	src/model/dungeon_generator.cpp
	src/model/dungeon_generator.h
	src/common/script_link.cpp
	src/common/script_link.h
	src/model/data_merge.cpp
	src/model/data_merge.h
	src/model/game_patches.cpp
	src/model/game_patches.h
	src/model/project_convert.cpp
	src/model/project_convert.h
	src/model/project.cpp
	src/model/project.h
	src/model/project_data.cpp
	src/model/project_data.h
	src/model/project_watcher.cpp
	src/model/project_watcher.h
	src/model/rpg_base.cpp
	src/model/rpg_base.h
	src/model/rpg_reflect.h
	src/model/skill.cpp
	src/model/skill.h
	src/model/state.cpp
	src/model/state.h
	src/model/switch.cpp
	src/model/switch.h
	src/model/terrain.cpp
	src/model/terrain.h
	src/model/troop.cpp
	src/model/troop.h
	src/model/variable.cpp
	src/model/variable.h
	src/stringizer.cpp
	src/stringizer.h
	src/ui/commands/all_commands.h
	src/ui/commands/flow_widgets.cpp
	src/ui/commands/flow_widgets.h
	src/ui/commands/event_branch_helpers.cpp
	src/ui/commands/event_branch_helpers.h
	src/ui/commands/picture_widgets.cpp
	src/ui/commands/picture_widgets.h
	src/ui/commands/message_input_widgets.cpp
	src/ui/commands/message_input_widgets.h
	src/ui/commands/media_widgets.cpp
	src/ui/commands/media_widgets.h
	src/ui/commands/hero_widgets.cpp
	src/ui/commands/hero_widgets.h
	src/ui/commands/scene_widgets.cpp
	src/ui/commands/scene_widgets.h
	src/ui/commands/maniac_widgets.cpp
	src/ui/commands/maniac_widgets.h
	src/ui/commands/maniac_string_widgets.cpp
	src/ui/commands/location_widgets.cpp
	src/ui/commands/location_widgets.h
	src/ui/commands/change_actor_face_widget.cpp
	src/ui/commands/change_actor_face_widget.h
	src/ui/commands/change_actor_face_widget.ui
	src/ui/commands/change_actor_graphic_widget.cpp
	src/ui/commands/change_actor_graphic_widget.h
	src/ui/commands/change_actor_graphic_widget.ui
	src/ui/commands/change_actor_name_widget.cpp
	src/ui/commands/change_actor_name_widget.h
	src/ui/commands/change_actor_name_widget.ui
	src/ui/commands/change_actor_title_widget.cpp
	src/ui/commands/change_actor_title_widget.h
	src/ui/commands/change_actor_title_widget.ui
	src/ui/commands/change_experience_widget.cpp
	src/ui/commands/change_experience_widget.h
	src/ui/commands/change_experience_widget.ui
	src/ui/commands/change_item_widget.cpp
	src/ui/commands/change_item_widget.h
	src/ui/commands/change_item_widget.ui
	src/ui/commands/change_level_widget.cpp
	src/ui/commands/change_level_widget.h
	src/ui/commands/change_level_widget.ui
	src/ui/commands/change_money_widget.cpp
	src/ui/commands/change_money_widget.h
	src/ui/commands/change_money_widget.ui
	src/ui/commands/change_party_widget.cpp
	src/ui/commands/change_party_widget.h
	src/ui/commands/change_party_widget.ui
	src/ui/commands/change_vehicle_graphic_widget.cpp
	src/ui/commands/change_vehicle_graphic_widget.h
	src/ui/commands/change_vehicle_graphic_widget.ui
	src/ui/commands/enable_disable_widget.cpp
	src/ui/commands/enable_disable_widget.h
	src/ui/commands/enable_disable_widget.ui
	src/ui/commands/flash_screen_widget.cpp
	src/ui/commands/flash_screen_widget.h
	src/ui/commands/flash_screen_widget.ui
	src/ui/commands/full_heal_widget.cpp
	src/ui/commands/full_heal_widget.h
	src/ui/commands/full_heal_widget.ui
	src/ui/commands/message_options_widget.cpp
	src/ui/commands/message_options_widget.h
	src/ui/commands/message_options_widget.ui
	src/ui/commands/shake_screen_widget.cpp
	src/ui/commands/shake_screen_widget.h
	src/ui/commands/shake_screen_widget.ui
	src/ui/commands/message_code_popup.cpp
	src/ui/commands/message_code_popup.h
	src/ui/commands/show_message_widget.cpp
	src/ui/commands/simple_command_widgets.cpp
	src/ui/commands/simple_command_widgets.h
	src/ui/commands/show_message_widget.h
	src/ui/commands/show_message_widget.ui
	src/ui/commands/switch_operations_widget.cpp
	src/ui/commands/switch_operations_widget.h
	src/ui/commands/switch_operations_widget.ui
	src/ui/commands/timer_operation_widget.cpp
	src/ui/commands/timer_operation_widget.h
	src/ui/commands/timer_operation_widget.ui
	src/ui/commands/tint_screen_widget.cpp
	src/ui/commands/tint_screen_widget.h
	src/ui/commands/tint_screen_widget.ui
	src/ui/commands/variable_operations_widget.cpp
	src/ui/commands/variable_operations_widget.h
	src/ui/commands/weather_effects_widget.cpp
	src/ui/commands/weather_effects_widget.h
	src/ui/commands/weather_effects_widget.ui
	src/ui/common/edit_actions.cpp
	src/ui/common/edit_actions.h
	src/ui/common/edit_dialog.cpp
	src/ui/common/color_sample.h
	src/ui/common/message_text_edit.h
	src/ui/common/text_limit.h
	src/ui/common/page_scroll_area.h
	src/ui/common/preview_zoom.cpp
	src/ui/common/preview_zoom.h
	src/ui/common/wrap_check_box.cpp
	src/ui/common/wrap_check_box.h
	src/ui/common/path_field.h
	src/ui/common/resource_info.h
	src/ui/common/stale_file.h
	src/ui/common/toolbar_overflow.cpp
	src/ui/common/toolbar_overflow.h
	src/ui/common/editor_style.cpp
	src/ui/common/help.cpp
	src/ui/common/help.h
	src/ui/common/view_state.cpp
	src/ui/common/view_state.h
	src/ui/common/editor_style.h
	src/ui/common/edit_dialog.h
	src/ui/common/edit_dialog.ui
	src/ui/common/encounter_delegate.cpp
	src/ui/common/encounter_delegate.h
	src/ui/common/import_image_dialog.cpp
	src/ui/common/import_image_dialog.h
	src/ui/common/import_image_dialog.ui
	src/ui/common/import_item.cpp
	src/ui/common/import_item.h
	src/ui/common/operand_widget.cpp
	src/ui/common/operand_widget.h
	src/ui/common/palette_scene.cpp
	src/ui/common/palette_scene.h
	src/ui/common/map_preview.cpp
	src/ui/common/map_preview.h
	src/ui/common/picker_scene.cpp
	src/ui/common/picker_scene.h
	src/ui/common/rpg_combobox.h
	src/ui/common/search_combobox.cpp
	src/ui/common/search_combobox.h
	src/ui/common/rpg_model.h
	src/ui/common/rpg_audio_lineedit.cpp
	src/ui/common/rpg_audio_lineedit.h
	src/ui/common/rpg_slider.cpp
	src/ui/common/rpg_slider.h
	src/ui/common/rpg_spinbox.cpp
	src/ui/common/rpg_spinbox.h
	src/ui/common/system_color_combobox.cpp
	src/ui/common/system_color_combobox.h
	src/ui/common/widget_as_dialog_model_wrapper.h
	src/ui/common/widget_as_dialog_wrapper.h
	src/ui/database/actor_delegate.cpp
	src/ui/database/actor_delegate.h
	src/ui/database/actor_widget.cpp
	src/ui/database/actor_widget.h
	src/ui/database/actor_widget.ui
	src/ui/database/animation_tools.cpp
	src/ui/database/animation_tools.h
	src/ui/database/attribute_widget.cpp
	src/ui/database/attribute_widget.h
	src/ui/database/attribute_widget.ui
	src/ui/database/battle_animation2_widget.cpp
	src/ui/database/battle_animation2_widget.h
	src/ui/database/battle_animation_widget.cpp
	src/ui/database/battle_animation_widget.h
	src/ui/database/battle_screen_widget.cpp
	src/ui/database/battle_screen_widget.h
	src/ui/database/battle_screen_widget.ui
	src/ui/database/chipset_widget.cpp
	src/ui/database/chipset_widget.h
	src/ui/database/class_editors.cpp
	src/ui/database/class_editors.h
	src/ui/database/class_widget.cpp
	src/ui/database/class_widget.h
	src/ui/database/class_widget.ui
	src/ui/database/curve_dialog.cpp
	src/ui/database/curve_dialog.h
	src/ui/database/curve_editor.cpp
	src/ui/database/curve_editor.h
	src/ui/database/common_event_widget.cpp
	src/ui/database/common_event_widget.h
	src/ui/database/common_event_widget.ui
	src/ui/database/database_list_data.cpp
	src/ui/database/database_list_data.h
	src/ui/database/database_dialog.cpp
	src/ui/database/database_dialog.h
	src/ui/database/database_dialog.ui
	src/ui/database/database_split_widget.cpp
	src/ui/database/database_split_widget.h
	src/ui/database/database_split_widget.ui
	src/ui/database/enemy_action_dialog.cpp
	src/ui/database/enemy_action_dialog.h
	src/ui/database/enemy_graphics.cpp
	src/ui/database/enemy_graphics.h
	src/ui/database/enemy_group_view.cpp
	src/ui/database/enemy_group_view.h
	src/ui/database/enemy_picker_widget.cpp
	src/ui/database/enemy_picker_widget.h
	src/ui/database/enemy_group_widget.cpp
	src/ui/database/enemy_group_widget.h
	src/ui/database/enemy_group_widget.ui
	src/ui/database/enemy_widget.cpp
	src/ui/database/enemy_widget.h
	src/ui/database/enemy_widget.ui
	src/ui/database/experience_dialog.cpp
	src/ui/database/experience_dialog.h
	src/ui/database/item_widget.cpp
	src/ui/database/item_widget.h
	src/ui/database/skill_item_common.cpp
	src/ui/database/skill_item_common.h
	src/ui/database/skill_widget.cpp
	src/ui/database/skill_widget.h
	src/ui/database/state_widget.cpp
	src/ui/database/state_widget.h
	src/ui/database/state_widget.ui
	src/ui/database/switch_widget.cpp
	src/ui/database/switch_widget.h
	src/ui/database/switch_widget.ui
	src/ui/database/system2_widget.cpp
	src/ui/database/system2_widget.h
	src/ui/database/system_fields.cpp
	src/ui/database/system_fields.h
	src/ui/database/system_widget.cpp
	src/ui/database/system_widget.h
	src/ui/database/test_battle_dialog.cpp
	src/ui/database/test_battle_dialog.h
	src/ui/database/test_battle_party_widget.cpp
	src/ui/database/test_battle_party_widget.h
	src/ui/database/terrain_widget.cpp
	src/ui/database/terrain_widget.h
	src/ui/database/terrain_widget.ui
	src/ui/database/variable_widget.cpp
	src/ui/database/variable_widget.h
	src/ui/database/variable_widget.ui
	src/ui/database/vocabulary_widget.cpp
	src/ui/database/vocabulary_widget.h
	src/ui/event/battle_page_condition_dialog.cpp
	src/ui/event/battle_page_condition_dialog.h
	src/ui/event/battle_page_widget.cpp
	src/ui/event/battle_page_widget.h
	src/ui/event/battle_page_widget.ui
	src/ui/event/event_command_base_widget.cpp
	src/ui/event/event_command_base_widget.h
	src/ui/event/event_command_palette.cpp
	src/ui/event/event_command_palette.h
	src/ui/event/command_colors.cpp
	src/ui/event/command_colors.h
	src/ui/event/event_commands_widget.cpp
	src/ui/event/move_route_dialog.cpp
	src/ui/event/move_route_dialog.h
	src/ui/event/event_commands_widget.h
	src/ui/event/event_dialog.cpp
	src/ui/event/event_dialog.h
	src/ui/event/event_dialog.ui
	src/ui/event/event_page_widget.cpp
	src/ui/event/event_page_widget.h
	src/ui/event/event_page_widget.ui
	src/ui/event/event_raw_widget.cpp
	src/ui/event/event_raw_widget.h
	src/ui/event/event_raw_widget.ui
	src/ui/main_window.cpp
	src/ui/tools/screenshot.cpp
	src/ui/tools/screenshot.h
	src/ui/main_window.h
	src/ui/main_window.ui
	src/ui/map/map_cursors.cpp
	src/ui/map/map_cursors.h
	src/ui/map/map_scene.cpp
	src/ui/map/map_scene.h
	src/ui/map/undo_draw.cpp
	src/ui/map/undo_draw.h
	src/ui/map/undo_event.cpp
	src/ui/map/undo_event.h
	src/ui/maptree/area_boundaries_dialog.cpp
	src/ui/maptree/area_boundaries_dialog.h
	src/ui/maptree/chip_selector_dialog.cpp
	src/ui/maptree/chip_selector_dialog.h
	src/ui/maptree/map_location_dialog.cpp
	src/ui/maptree/map_location_dialog.h
	src/ui/maptree/area_properties_dialog.cpp
	src/ui/maptree/area_properties_dialog.h
	src/ui/maptree/encounter_list.cpp
	src/ui/maptree/encounter_list.h
	src/ui/maptree/shift_map_dialog.cpp
	src/ui/maptree/shift_map_dialog.h
	src/ui/maptree/generate_dungeon_dialog.cpp
	src/ui/maptree/generate_dungeon_dialog.h
	src/ui/maptree/map_properties_dialog.cpp
	src/ui/maptree/map_properties_dialog.h
	src/ui/maptree/map_properties_dialog.ui
	src/ui/other/convert_project_dialog.cpp
	src/ui/other/convert_project_dialog.h
	src/ui/other/import_project_dialog.cpp
	src/ui/other/import_project_dialog.h
	src/ui/other/import_project_dialog.ui
	src/ui/other/export_dialog.cpp
	src/ui/other/export_dialog.h
	src/ui/other/launcher_download.cpp
	src/ui/other/launcher_download.h
	src/ui/other/new_project_dialog.cpp
	src/ui/other/new_project_dialog.h
	src/ui/other/new_project_dialog.ui
	src/ui/other/open_project_dialog.cpp
	src/ui/other/open_project_dialog.h
	src/ui/other/open_project_dialog.ui
	src/ui/other/resource_manager_dialog.cpp
	src/ui/other/resource_manager_dialog.h
	src/ui/other/resource_manager_dialog.ui
	src/ui/other/jukebox_dialog.cpp
	src/ui/other/jukebox_dialog.h
	src/ui/other/project_settings_dialog.cpp
	src/ui/other/project_settings_dialog.h
	src/ui/other/settings_dialog.cpp
	src/ui/other/settings_dialog.h
	src/ui/other/run_game_dialog.cpp
	src/ui/other/run_game_dialog.h
	src/ui/other/run_game_dialog.ui
	src/ui/other/search_dialog.cpp
	src/ui/other/search_dialog.h
	src/ui/other/search_dialog.ui
	src/ui/picker/picker_audio_widget.cpp
	src/ui/picker/picker_audio_widget.h
	src/ui/picker/picker_audio_widget.ui
	src/ui/picker/picker_backdrop_widget.cpp
	src/ui/picker/picker_backdrop_widget.h
	src/ui/picker/picker_child_widget.cpp
	src/ui/picker/picker_child_widget.h
	src/ui/picker/picker_charset_widget.cpp
	src/ui/picker/picker_charset_widget.h
	src/ui/picker/picker_faceset_widget.cpp
	src/ui/picker/picker_faceset_widget.h
	src/ui/picker/picker_panorama_widget.cpp
	src/ui/picker/picker_panorama_widget.h
	src/ui/picker/picker_dialog.cpp
	src/ui/picker/picker_dialog.h
	src/ui/picker/picker_dialog.ui
	src/ui/viewer/battle_animation_graphics_item.cpp
	src/ui/viewer/battle_animation_graphics_item.h
	src/ui/viewer/charset_graphics_item.cpp
	src/ui/viewer/charset_graphics_item.h
	src/ui/viewer/faceset_graphics_item.cpp
	src/ui/viewer/faceset_graphics_item.h
	src/ui/viewer/rpg_graphics_view.cpp
	src/ui/viewer/rpg_graphics_view.h
	src/ui/viewer/stat_curve_graphics_item.cpp
	src/ui/viewer/stat_curve_graphics_item.h
)

# Dependencies
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED COMPONENTS
	Widgets Core Concurrent Multimedia Gui Svg Network GuiPrivate)
find_package(Qt6LinguistTools CONFIG QUIET)
find_package(ZLIB REQUIRED)

qt_standard_project_setup(
	REQUIRES "${REQUIRED_QT_VERSION}"
)

# Executable
if(APPLE)
	# Do not code sign
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
	set(CMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.easyrpg.editor")

	# App Icon
	set(EDITOR_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app/Editor.icns")
	set_source_files_properties(${EDITOR_BUNDLE_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

	set(EXE_NAME "EasyRPG-Editor.app")
	set(EXE_OUTPUT_NAME "EasyRPG Editor")
	qt_add_executable(${EXE_NAME} MACOSX_BUNDLE "src/main.cpp" ${EDITOR_SOURCES} ${EDITOR_BUNDLE_ICON} MANUAL_FINALIZATION)
	set_target_properties(${EXE_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/app/Info.plist.in")
	set_target_properties(${EXE_NAME} PROPERTIES OUTPUT_NAME ${EXE_OUTPUT_NAME})
	# the name of the app menu on macOS
	set_target_properties(${EXE_NAME} PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${EXE_OUTPUT_NAME})
	set_target_properties(${EXE_NAME} PROPERTIES
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
		MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
else()
	# Executable
	set(EXE_NAME ${PROJECT_NAME})
	qt_add_executable(${EXE_NAME} WIN32 "src/main.cpp" ${EDITOR_SOURCES} MANUAL_FINALIZATION)
	if(WIN32)
		# Open console for Debug builds
		if(CMAKE_BUILD_TYPE MATCHES "Debug")
			set_target_properties(${EXE_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)
		endif()
		set_target_properties(${EXE_NAME} PROPERTIES OUTPUT_NAME "Editor")
		# Add icon
		target_sources(${EXE_NAME} PRIVATE "resources/Resources.rc")
		# Set Visual Studio startup project
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${EXE_NAME})
	else()
		set_target_properties(${EXE_NAME} PROPERTIES OUTPUT_NAME "easyrpg-editor")
	endif()
endif()

if(MSVC)
	# the sources are UTF-8 (the symbols and the translations in the texts),
	# and std::min / std::max are not the macros of windows.h
	target_compile_options(${EXE_NAME} PRIVATE /utf-8)
	target_compile_definitions(${EXE_NAME} PRIVATE NOMINMAX)
endif()

# Qt Widgets and the database of liblcf, which most files include: parsed
# once for all of them
option(EDITOR_PRECOMPILE_HEADERS "Precompile the Qt and liblcf headers (faster builds)" ON)
if(EDITOR_PRECOMPILE_HEADERS)
	target_precompile_headers(${EXE_NAME} PRIVATE
		"$<$<COMPILE_LANGUAGE:CXX>:<QtWidgets$<ANGLE-R>>"
		"$<$<COMPILE_LANGUAGE:CXX>:<lcf/rpg/database.h$<ANGLE-R>>")
	# a function of it is named like Qt's emit macro
	set_source_files_properties(src/model/maniac_expression.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
		# ccache reuses files compiled with them only without their time
		target_compile_options(${EXE_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-Xclang -fno-pch-timestamp>")
	endif()
endif()

# Include directories
target_include_directories(${EXE_NAME}
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/src
	INTERFACE
		$<BUILD_INTERFACE:${AUTOGEN_INCLUDE_DIR}>
)

# IDE source grouping
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Editor" FILES ${EDITOR_SOURCES})

# Add autogen include dirs to include path, otherwise the includes are not found
# when used in the header (via https://stackoverflow.com/q/44746308/)
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_isMultiConfig)
	set(AUTOGEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_autogen/include_$<CONFIG>)
else()
	set(AUTOGEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_autogen/include)
endif()

# Qt Magic
set_target_properties(${EXE_NAME} PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
	AUTORCC ON # Enable resource compiler for exe
)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# liblcf
option(EDITOR_BUILD_LIBLCF "Instead of detecting liblcf the liblcf repository is cloned into lib/liblcf and built together with the Player. This is convenient for development" OFF)
set(EDITOR_BUILD_LIBLCF_GIT "https://github.com/EasyRPG/liblcf.git" CACHE STRING "Git repository of liblcf to clone when building liblcf. Requires EDITOR_BUILD_LIBLCF=ON.")

if(EDITOR_BUILD_LIBLCF)
	# liblcf is built as part of this cmake file
	set(LIBLCF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/liblcf")
	if(NOT EXISTS ${LIBLCF_PATH})
		find_package(Git REQUIRED)
		execute_process(COMMAND ${GIT_EXECUTABLE} clone "--depth=1"
			"${EDITOR_BUILD_LIBLCF_GIT}"
			"${LIBLCF_PATH}")
	endif()
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/liblcf/builds/cmake/Modules")

	option(BUILD_SHARED_LIBS "Not used. Supresses CMP0077 in liblcf" ON)

	# Always build static liblcf and do not install it
	function(add_liblcf)
		set(BUILD_SHARED_LIBS OFF)
		set(LIBLCF_ENABLE_INSTALL OFF)
		add_subdirectory(${LIBLCF_PATH})
	endfunction()
	add_liblcf()
	if(APPLE)
		# liblcf's vsprintf, deprecated by Apple's SDK: not ours to change
		target_compile_options(lcf PRIVATE -Wno-deprecated-declarations)
	endif()
	target_link_libraries(${EXE_NAME} PRIVATE lcf)
else()
	# Use system package
	find_package(liblcf REQUIRED)
	target_link_libraries(${EXE_NAME} PRIVATE liblcf::liblcf)
endif()

# Libraries
# shown in About
target_compile_definitions(${EXE_NAME} PRIVATE EDITOR_VERSION="${PROJECT_VERSION}")
target_link_libraries(${EXE_NAME} PRIVATE
	ZLIB::ZLIB
	Qt6::Widgets
	Qt6::Gui
	Qt6::Core
	Qt6::Concurrent
	Qt6::Multimedia
	Qt6::Network
	Qt6::Svg
)
if(APPLE)
	# audio of the Jukebox and the pickers: Apple's audio engine, with MIDI
	target_sources(${EXE_NAME} PRIVATE src/common/audio_player_mac.mm src/ui/other/run_game_dialog_mac.mm)
	set_source_files_properties(src/common/audio_player_mac.mm src/ui/other/run_game_dialog_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
	target_link_libraries(${EXE_NAME} PRIVATE "-framework AVFoundation" "-framework AudioToolbox" "-framework Foundation" "-framework AppKit")
endif()

# Add resources
target_sources(${EXE_NAME} PRIVATE "resources/Resources.qrc")

# Script runner (--script, docs/Automation.adoc), for tests and screenshots
target_sources(${EXE_NAME} PRIVATE src/ui/tools/automation.cpp src/ui/tools/automation.h)
# wheel: trackpad events injected like the platform's
target_link_libraries(${EXE_NAME} PRIVATE Qt6::GuiPrivate)

# Tests
enable_testing()
# the web Player folders the export accepts (a release, a Player build folder)
add_executable(editor_web_player_test tests/web_player_test.cpp src/model/web_player.cpp)
target_include_directories(editor_web_player_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(editor_web_player_test PRIVATE Qt6::Core)
add_test(NAME web_player COMMAND editor_web_player_test)
set(EDITOR_TEST_PROJECT "" CACHE PATH "Game the script tests run on (TestGame-2003 of https://github.com/EasyRPG/TestGame)")
if(EDITOR_TEST_PROJECT)
	file(GLOB EDITOR_TEST_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/*.txt")
	foreach(script ${EDITOR_TEST_SCRIPTS})
		get_filename_component(name ${script} NAME_WE)
		add_test(NAME script_${name}
			COMMAND $<TARGET_FILE:${EXE_NAME}> --script ${script} ${EDITOR_TEST_PROJECT}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
		# a dialog nobody closes fails the test instead of blocking the run
		set_tests_properties(script_${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
	endforeach()
endif()

# Translations
if(TARGET Qt6::lrelease)
	set(TRANSLATION_SOURCES
		translations/editor_de.ts
		translations/editor_fr.ts
	)
	set(TRANSLATION_RES ${CMAKE_CURRENT_BINARY_DIR}/translations/editor.qrc)
	configure_file(translations/editor.qrc ${TRANSLATION_RES} COPYONLY)
	set_source_files_properties(${TRANSLATION_SOURCES} PROPERTIES
		OUTPUT_LOCATION "translations")
	qt_add_translation(EDITOR_TRANSLATIONS ${TRANSLATION_SOURCES}
		OPTIONS -compress -nounfinished -removeidentical)
	target_sources(${EXE_NAME} PRIVATE ${TRANSLATION_RES} ${EDITOR_TRANSLATIONS})
	add_custom_target(translations
		COMMAND ${Qt6_LUPDATE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src -ts ${TRANSLATION_SOURCES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Create/Update translations..."
		COMMAND_EXPAND_LISTS VERBATIM)
else()
	message(STATUS "Disabling translation support")
endif()

qt_finalize_target(${EXE_NAME})

# Installation
include(GNUInstallDirs)

if(APPLE)
	install(TARGETS ${EXE_NAME}
		BUNDLE DESTINATION .
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
else()
	install(TARGETS ${EXE_NAME}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
	if(UNIX)
		# Install desktop entry and icon
		install(FILES resources/unix/easyrpg-editor.desktop
			DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
		# legacy path, remove 2030 :)
		install(FILES resources/app/easyrpg.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
			RENAME easyrpg-editor.png)
		# FIXME: use scalable icon
		install(FILES resources/app/easyrpg.png
			DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
			RENAME easyrpg-editor.png)
		# FIXME: Fill appdata file
		#install(FILES resources/unix/easyrpg-editor.appdata.xml
		#	DESTINATION ${CMAKE_INSTALL_DATADIR}/appdata)
	endif()
endif()

# Generate a deployment script to be executed at install time
if(WIN32 OR APPLE)
	# The script fails on Linux currently
	# On macOS the generated app does not run due to unsigned plugins
	qt_generate_deploy_app_script(
		TARGET ${EXE_NAME}
		OUTPUT_SCRIPT deploy_script
	)
	install(SCRIPT ${deploy_script})
endif()

if(WIN32)
	# Environment for the MSVC Debugger
	set_target_properties(${EXE_NAME}
		PROPERTIES VS_DEBUGGER_ENVIRONMENT
			"PATH=$ENV{Qt6_Path}/bin;%PATH%"
	)

	# Other dev environments
	configure_file("builds/cmake/setup_env.cmd.in" "setup_env.cmd")
	message(STATUS "When not using Visual Studio for development run setup_env.cmd to configure the environment")
endif()
