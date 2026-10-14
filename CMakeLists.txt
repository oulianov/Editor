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

# a Makefile generated here would replace the Makefile of the repository
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND CMAKE_GENERATOR MATCHES "Makefiles")
	message(FATAL_ERROR "Build in a folder of its own (cmake -B build), or run make. "
		"Remove the CMakeCache.txt and CMakeFiles written here.")
endif()

# ccache, when installed: a build folder compiles only what no build folder of
# this checkout (or of a worktree of it) compiled before. base_dir and hash_dir
# let the folders share. Precompiled headers are left off with it: a cached one
# names the folder it was built in, and fails once that folder is gone
option(EDITOR_CCACHE "Compile through ccache when it is installed" ON)
if(EDITOR_CCACHE AND (NOT CMAKE_CXX_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER MATCHES "(^|/)ccache$"))
	find_program(EDITOR_CCACHE_PROGRAM ccache)
	if(EDITOR_CCACHE_PROGRAM)
		set(_editor_ccache "${EDITOR_CCACHE_PROGRAM}"
			"sloppiness=time_macros,include_file_mtime,include_file_ctime"
			"base_dir=${CMAKE_SOURCE_DIR}" "hash_dir=false")
		set(CMAKE_CXX_COMPILER_LAUNCHER ${_editor_ccache})
		set(CMAKE_OBJCXX_COMPILER_LAUNCHER ${_editor_ccache})
		set(_editor_uses_ccache ON)
	endif()
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
# KDE's CMake modules: the QML module of the interface (ecm_add_qml_module)
editor_find_package(ECM ${REQUIRED_QT_VERSION})
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

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
	src/common/image_import.cpp
	src/common/image_import.h
	src/common/image_loader.cpp
	src/common/image_loader.h
	src/common/scope_guard.h
	src/common/search_match.cpp
	src/common/search_match.h
	src/common/sortfilter_proxy_models.cpp
	src/common/sortfilter_proxy_models.h
	src/controller/action_catalog.cpp
	src/controller/action_catalog.h
	src/controller/database_apply.cpp
	src/controller/database_apply.h
	src/controller/dialog_service.cpp
	src/controller/dialog_service.h
	src/controller/edit_locks.cpp
	src/controller/edit_locks.h
	src/controller/map_tabs_controller.cpp
	src/controller/map_tabs_controller.h
	src/controller/map_tree_drop.cpp
	src/controller/map_tree_drop.h
	src/controller/project_controller.cpp
	src/controller/project_controller.h
	src/controller/shell_controller.cpp
	src/controller/shell_controller.h
	src/controller/shell_dialogs.h
	src/controller/shortcut_catalog.cpp
	src/controller/shortcut_catalog.h
	src/controller/workspace_store.cpp
	src/controller/workspace_store.h
	src/core.cpp
	src/core.h
	src/defines.h
	src/main.cpp
	src/model/actor.cpp
	src/model/actor.h
	src/model/animation.cpp
	src/model/animation.h
	src/model/animation_editor.cpp
	src/model/animation_editor.h
	src/model/animation_tools.cpp
	src/model/animation_tools.h
	src/model/attribute.cpp
	src/model/attribute.h
	src/model/autotile.cpp
	src/model/autotile.h
	src/model/battleranimation.cpp
	src/model/battleranimation.h
	src/model/battler_animation_editor.cpp
	src/model/battler_animation_editor.h
	src/model/chipset.cpp
	src/model/chipset.h
	src/model/chipset_editor.cpp
	src/model/chipset_editor.h
	src/model/class.cpp
	src/model/class.h
	src/model/command_catalog.cpp
	src/model/command_catalog.h
	src/model/command_colors.cpp
	src/model/command_colors.h
	src/model/command_schema.cpp
	src/model/command_schema.h
	src/model/commonevent.cpp
	src/model/commonevent.h
	src/model/editor_state.cpp
	src/model/editor_state.h
	src/model/enemy.cpp
	src/model/enemy.h
	src/model/event_branches.cpp
	src/model/event_branches.h
	src/model/event_command_defaults.cpp
	src/model/event_command_defaults.h
	src/model/event_command_list.cpp
	src/model/move_route_codec.cpp
	src/model/move_route_codec.h
	src/model/move_route_model.cpp
	src/model/move_route_model.h
	src/model/event_command_list.h
	src/model/event_command_list_model.cpp
	src/model/event_command_list_model.h
	src/model/event_editor_controller.cpp
	src/model/event_editor_controller.h
	src/model/event_page_defaults.h
	src/model/experience_curve.cpp
	src/model/experience_curve.h
	src/model/export_files.cpp
	src/model/export_files.h
	src/model/export_references.cpp
	src/model/export_references.h
	src/model/game_export.cpp
	src/model/game_export.h
	src/model/export_controller.cpp
	src/model/export_controller.h
	src/model/file_picker_model.cpp
	src/model/file_picker_model.h
	src/model/launcher_download.cpp
	src/model/launcher_download.h
	src/model/player_download.cpp
	src/model/player_download.h
	src/model/web_player.cpp
	src/model/web_player.h
	src/model/resource_library.cpp
	src/model/resource_library.h
	src/model/audio_library.cpp
	src/model/audio_library.h
	src/model/item.cpp
	src/model/item.h
	src/model/map_document.cpp
	src/model/map_document.h
	src/model/map_document_file.cpp
	src/model/map_tools.cpp
	src/model/map_tools.h
	src/model/encounter_list_model.cpp
	src/model/encounter_list_model.h
	src/model/map_tree_model.cpp
	src/model/map_tree_model.h
	src/model/map_undo.cpp
	src/model/map_undo.h
	src/model/maniac_expression.cpp
	src/model/maniac_expression.h
	src/model/dungeon_generator.cpp
	src/model/dungeon_generator.h
	src/common/script_link.cpp
	src/common/script_link.h
	src/model/data_merge.cpp
	src/model/data_merge.h
	src/model/game_patches.cpp
	src/model/game_runner.cpp
	src/model/game_runner.h
	src/model/game_patches.h
	src/model/project_convert.cpp
	src/model/project_convert.h
	src/model/project.cpp
	src/model/project.h
	src/model/project_data.cpp
	src/model/project_data.h
	src/model/project_list.cpp
	src/model/project_list.h
	src/model/project_search.cpp
	src/model/project_search.h
	src/model/project_settings.cpp
	src/model/project_settings.h
	src/model/search_results_model.cpp
	src/model/search_results_model.h
	src/model/search_suggestions.cpp
	src/model/search_suggestions.h
	src/model/project_watcher.cpp
	src/model/project_watcher.h
	src/model/stale_choice.h
	src/model/rpg_base.cpp
	src/model/rpg_base.h
	src/model/rpg_reflect.h
	src/model/rpg_metatypes.h
	src/model/skill.cpp
	src/model/skill.h
	src/model/state.cpp
	src/model/state.h
	src/model/switch.cpp
	src/model/switch.h
	src/model/terrain.cpp
	src/model/terrain.h
	src/model/test_battle.cpp
	src/model/test_battle.h
	src/model/test_play_options.cpp
	src/model/test_play_options.h
	src/model/transparent_color.cpp
	src/model/transparent_color.h
	src/model/troop.cpp
	src/model/troop.h
	src/model/troop_layout.cpp
	src/model/troop_layout.h
	src/model/variable.cpp
	src/model/variable.h
	src/model/vocabulary_fields.cpp
	src/model/vocabulary_fields.h
	src/render/chipset_editor_painter.cpp
	src/render/chipset_editor_painter.h
	src/render/animation_clock.cpp
	src/render/animation_clock.h
	src/render/chipset_tiles.cpp
	src/render/chipset_tiles.h
	src/render/curve_painter.cpp
	src/render/curve_painter.h
	src/render/game_graphic.cpp
	src/render/game_graphic.h
	src/render/panorama.cpp
	src/render/panorama.h
	src/render/picture_cache.cpp
	src/render/picture_cache.h
	src/render/tile_atlas.cpp
	src/render/tile_atlas.h
	src/render/tile_painter.cpp
	src/render/tile_painter.h
	src/stringizer.cpp
	src/stringizer.h
	src/ui/common/help.cpp
	src/ui/common/help.h
	src/model/database_list_data.cpp
	src/model/database_list_data.h
	src/model/enemy_graphics.cpp
	src/model/enemy_graphics.h
	src/ui/event/command_editing.cpp
	src/ui/event/command_editing.h
	src/ui/event/command_editor_registry.cpp
	src/ui/event/command_editor_registry.h
	src/ui/event/event_window.cpp
	src/ui/event/event_window.h
	src/ui/tools/screenshot.cpp
	src/ui/tools/screenshot.h
	src/ui/map/map_canvas_host.cpp
	src/ui/map/map_canvas_host.h
	src/ui/map/map_cursors.cpp
	src/ui/map/map_cursors.h
	src/ui/map/map_editor.cpp
	src/ui/map/map_editor.h
	src/ui/maptree/map_tree_dialogs.cpp
	src/ui/maptree/map_tree_dialogs.h
	src/ui/other/search_window_host.cpp
	src/ui/other/search_window_host.h
)

# The interface in QML (the module org.easyrpg.editor): its files...
set(EDITOR_QML_UI
	src/ui/commands/ChangeActorFaceEditor.qml
	src/ui/commands/ChangeActorGraphicEditor.qml
	src/ui/commands/ChangeFaceGraphicEditor.qml
	src/ui/commands/ChangeVehicleGraphicEditor.qml
	src/ui/commands/CommandEditorWindow.qml
	src/ui/commands/CommandField.qml
	src/ui/commands/CommandRadio.qml
	src/ui/commands/CommentEditor.qml
	src/ui/commands/ConditionalBranchEditor.qml
	src/ui/commands/ControlStringsEditor.qml
	src/ui/commands/EnemyEncounterEditor.qml
	src/ui/commands/ErasePictureEditor.qml
	src/ui/commands/KeyInputProcEditor.qml
	src/ui/commands/MessageCodePopup.qml
	src/ui/commands/MessageTextArea.qml
	src/ui/commands/MoveEventEditor.qml
	src/ui/commands/OpenShopEditor.qml
	src/ui/commands/PictureEditor.qml
	src/ui/commands/RawCommandEditor.qml
	src/ui/commands/SchemaField.qml
	src/ui/commands/SchemaForm.qml
	src/ui/commands/SchemaRows.qml
	src/ui/commands/ShowBattleAnimationEditor.qml
	src/ui/commands/ShowChoiceEditor.qml
	src/ui/commands/ShowMessageEditor.qml
	src/ui/commands/ShowStringPictureEditor.qml
	src/ui/commands/VariableOperationsEditor.qml
	src/ui/common/ArraySizeDialog.qml
	src/ui/common/ArrayTable.qml
	src/ui/common/BitCheckBox.qml
	src/ui/common/Button.qml
	src/ui/common/AudioField.qml
	src/ui/common/ButtonBox.qml
	src/ui/common/CheckBox.qml
	src/ui/common/CheckSection.qml
	src/ui/common/ColumnList.qml
	src/ui/common/ComboBox.qml
	src/ui/common/ContextMenu.qml
	src/ui/common/Dialog.qml
	src/ui/common/DialogButton.qml
	src/ui/common/DialogWindow.qml
	src/ui/common/DownloadBar.qml
	src/ui/common/EditorIcon.qml
	src/ui/common/EntryListPane.qml
	src/ui/common/EntryPage.qml
	src/ui/common/FieldButton.qml
	src/ui/common/FileField.qml
	src/ui/common/Form.qml
	src/ui/common/FormLabel.qml
	src/ui/common/HelpWindow.qml
	src/ui/common/Label.qml
	src/ui/common/LogView.qml
	src/ui/common/MessageDialog.qml
	src/ui/common/MessageWindow.qml
	src/ui/common/Metrics.qml
	src/ui/common/Note.qml
	src/ui/common/NumberDialog.qml
	src/ui/common/PathField.qml
	src/ui/common/ProgressBar.qml
	src/ui/common/RadioButton.qml
	src/ui/common/RadioGroup.qml
	src/ui/common/RefComboBox.qml
	src/ui/common/Section.qml
	src/ui/common/Slider.qml
	src/ui/common/SliderField.qml
	src/ui/common/TableHeader.qml
	src/ui/common/SpinBox.qml
	src/ui/common/SpriteField.qml
	src/ui/common/SystemColorComboBox.qml
	src/ui/common/TextArea.qml
	src/ui/common/TextField.qml
	src/ui/common/ToolTip.qml
	src/ui/common/ZoomButtons.qml
	src/ui/common/ZoomView.qml
	src/ui/database/ActorEasyRpgDialog.qml
	src/ui/database/ActorPage.qml
	src/ui/database/AnimationTimingDialog.qml
	src/ui/database/AttributePage.qml
	src/ui/database/AutocompleteDialog.qml
	src/ui/database/BattleAnimation2Page.qml
	src/ui/database/BattleAnimationPage.qml
	src/ui/database/BattleCommandBoxes.qml
	src/ui/database/BattleScreenPage.qml
	src/ui/database/BattlerAnimationDialog.qml
	src/ui/database/BattlerView.qml
	src/ui/database/BitTable.qml
	src/ui/database/ChipsetPage.qml
	src/ui/database/ClassPage.qml
	src/ui/database/CommonEventPage.qml
	src/ui/database/CurveDialog.qml
	src/ui/database/DatabaseSplit.qml
	src/ui/database/DatabaseView.qml
	src/ui/database/DatabaseWindow.qml
	src/ui/database/EnemyActionDialog.qml
	src/ui/database/EnemyPage.qml
	src/ui/database/ExperienceDialog.qml
	src/ui/database/ExperienceSummary.qml
	src/ui/database/FrameRange.qml
	src/ui/database/ItemPage.qml
	src/ui/database/MassCopyClearDialog.qml
	src/ui/database/MassModifyDialog.qml
	src/ui/database/ParameterGraphs.qml
	src/ui/database/RankTable.qml
	src/ui/database/RichPage.qml
	src/ui/database/SetUniformTerrainDialog.qml
	src/ui/database/SkillPage.qml
	src/ui/database/SkillTable.qml
	src/ui/database/StatePage.qml
	src/ui/database/SwitchPage.qml
	src/ui/database/System2Page.qml
	src/ui/database/SystemPage.qml
	src/ui/database/TerrainPage.qml
	src/ui/database/TestBattleDialog.qml
	src/ui/database/TestBattlePartyView.qml
	src/ui/database/TroopPage.qml
	src/ui/database/VariablePage.qml
	src/ui/database/VocabularyPage.qml
	src/ui/event/BattleConditionDialog.qml
	src/ui/event/BattlePageEditor.qml
	src/ui/event/CommandList.qml
	src/ui/event/CommandPalette.qml
	src/ui/event/EventEditorWindow.qml
	src/ui/event/EventPageEditor.qml
	src/ui/event/MoveRouteEditor.qml
	src/ui/event/RefChoice.qml
	src/ui/event/UpperTileDialog.qml
	src/ui/map/MapCanvas.qml
	src/ui/map/MapCanvasWindow.qml
	src/ui/map/MapContextMenu.qml
	src/ui/map/MapPreview.qml
	src/ui/map/Palette.qml
	src/ui/maptree/AreaBoundariesWindow.qml
	src/ui/maptree/AreaPropertiesWindow.qml
	src/ui/maptree/ChipSelectorWindow.qml
	src/ui/maptree/EncounterDialog.qml
	src/ui/maptree/EncounterTable.qml
	src/ui/maptree/GenerateDungeonWindow.qml
	src/ui/maptree/MapCellView.qml
	src/ui/maptree/MapLocationPicker.qml
	src/ui/maptree/MapPropertiesWindow.qml
	src/ui/maptree/ShiftMapWindow.qml
	src/ui/other/ConvertProjectWindow.qml
	src/ui/other/ExportWindow.qml
	src/ui/other/ImportImageWindow.qml
	src/ui/other/ImportProjectWindow.qml
	src/ui/other/ImportProgressWindow.qml
	src/ui/other/JukeboxWindow.qml
	src/ui/other/NewProjectWindow.qml
	src/ui/other/OpenProjectWindow.qml
	src/ui/other/PlayerMissingWindow.qml
	src/ui/other/ProjectSettingsWindow.qml
	src/ui/other/ResourceManagerWindow.qml
	src/ui/other/RunGameWindow.qml
	src/ui/other/SearchWindow.qml
	src/ui/other/SettingsWindow.qml
	src/ui/picker/EntryPicker.qml
	src/ui/picker/FilePicker.qml
	src/ui/picker/PickerFieldsWindow.qml
	src/ui/picker/PickerList.qml
	src/ui/shell/Actions.qml
	src/ui/shell/CentralArea.qml
	src/ui/shell/ControlsWindow.qml
	src/ui/shell/DatabaseControlsWindow.qml
	src/ui/shell/EventControlsWindow.qml
	src/ui/shell/MainMenuBar.qml
	src/ui/shell/MainToolBar.qml
	src/ui/shell/MapTabs.qml
	src/ui/shell/MapTree.qml
	src/ui/shell/NativeMenuBar.qml
	src/ui/shell/SmokeWindow.qml
	src/ui/shell/StatusBar.qml
	src/ui/MainWindow.qml
)
# the design system: one object for all the windows
set_source_files_properties(src/ui/common/Metrics.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
list(APPEND EDITOR_SOURCES src/ui/shell/qml_engine.cpp src/ui/shell/qml_engine.h
	src/ui/shell/editor_window.cpp src/ui/shell/editor_window.h
	src/ui/shell/database_windows.cpp src/ui/shell/database_windows.h
	src/ui/shell/qml_shell_dialogs.cpp src/ui/shell/qml_shell_dialogs.h)

# ...and the C++ types they use: the project's data as JSON (glaze), the
# painted sprites
set(EDITOR_QML_INTERFACE
	src/qmlbinding/battler_animation_preview.cpp
	src/qmlbinding/battler_animation_preview.h
	src/qmlbinding/command_edit_session.cpp
	src/qmlbinding/command_edit_session.h
	src/qmlbinding/database_entry.cpp
	src/qmlbinding/database_entry.h
	src/qmlbinding/database_session.cpp
	src/qmlbinding/database_session.h
	src/qmlbinding/dialog_qml_types.h
	src/qmlbinding/dialog_scope.cpp
	src/qmlbinding/dialog_scope.h
	src/qmlbinding/directory_model.cpp
	src/qmlbinding/directory_model.h
	src/qmlbinding/editor_bridge.cpp
	src/qmlbinding/editor_bridge.h
	src/qmlbinding/entry_filter_model.cpp
	src/qmlbinding/entry_filter_model.h
	src/qmlbinding/json.cpp
	src/qmlbinding/json.h
	src/qmlbinding/json_field.cpp
	src/qmlbinding/json_field.h
	src/qmlbinding/json_list_view.cpp
	src/qmlbinding/json_list_view.h
	src/qmlbinding/json_node.h
	src/qmlbinding/json_t.h
	src/qmlbinding/json_view.cpp
	src/qmlbinding/json_view.h
	src/qmlbinding/lcf_glaze.h
	src/qmlbinding/project_data_gadget.cpp
	src/qmlbinding/project_data_gadget.h
	src/qmlbinding/preview_image.cpp
	src/qmlbinding/preview_image.h
	src/qmlbinding/project_image_provider.cpp
	src/qmlbinding/project_image_provider.h
	src/qmlbinding/project_windows.cpp
	src/qmlbinding/project_windows.h
	src/qmlbinding/qml_dialog_service.cpp
	src/qmlbinding/qml_dialog_service.h
	src/qmlbinding/qml_host.cpp
	src/qmlbinding/qml_host.h
	src/qmlbinding/qml_player_download.cpp
	src/qmlbinding/qml_player_download.h
	src/qmlbinding/resource_qml_types.h
	src/qmlbinding/resource_windows.cpp
	src/qmlbinding/resource_windows.h
	src/qmlbinding/test_battle_model.cpp
	src/qmlbinding/test_battle_model.h
	src/qmlbinding/test_battle_party_model.cpp
	src/qmlbinding/test_battle_party_model.h
	src/qmlbinding/test_play.cpp
	src/qmlbinding/test_play.h
	src/qmlbinding/troop_editor.cpp
	src/qmlbinding/troop_editor.h
	src/qmlbinding/vocabulary_table.cpp
	src/qmlbinding/vocabulary_table.h
	src/ui/canvas/animation_canvas.cpp
	src/ui/canvas/animation_canvas.h
	src/ui/canvas/chipset_canvas.cpp
	src/ui/canvas/chipset_canvas.h
	src/ui/canvas/curve_canvas.cpp
	src/ui/canvas/curve_canvas.h
	src/ui/canvas/curves_bridge.cpp
	src/ui/canvas/curves_bridge.h
	src/ui/canvas/image_canvas.cpp
	src/ui/canvas/image_canvas.h
	src/ui/canvas/troop_canvas.cpp
	src/ui/canvas/troop_canvas.h
	src/ui/canvas/zoom_image_view.cpp
	src/ui/canvas/zoom_image_view.h
	src/ui/event/battle_page.cpp
	src/ui/event/battle_page.h
	src/ui/event/command_list_editor.cpp
	src/ui/event/command_list_editor.h
	src/ui/event/event_qml_types.h
	src/ui/event/event_tools.cpp
	src/ui/event/event_tools.h
	src/ui/map/map_events_item.cpp
	src/ui/map/map_events_item.h
	src/ui/map/map_layer_item.cpp
	src/ui/map/map_layer_item.h
	src/ui/map/map_overlay_item.cpp
	src/ui/map/map_overlay_item.h
	src/ui/map/map_panorama_item.cpp
	src/ui/map/map_panorama_item.h
	src/ui/map/map_qml_types.h
	src/ui/map/map_texture_cache.cpp
	src/ui/map/map_texture_cache.h
	src/ui/map/tile_palette.cpp
	src/ui/map/tile_palette.h
	src/ui/maptree/location_picker.cpp
	src/ui/maptree/location_picker.h
	src/ui/maptree/map_tree_forms.cpp
	src/ui/maptree/map_tree_forms.h
	src/ui/maptree/maptree_qml_types.h
	src/ui/picker/charset_painted_item.cpp
	src/ui/picker/charset_painted_item.h
	src/ui/picker/faceset_painted_item.cpp
	src/ui/picker/faceset_painted_item.h
	src/ui/picker/monster_painted_item.cpp
	src/ui/picker/monster_painted_item.h
	src/ui/picker/pickers.cpp
	src/ui/picker/pickers.h
	src/ui/picker/sprite_painted_item.cpp
	src/ui/picker/sprite_painted_item.h
	src/ui/shell/platform_menu.cpp
	src/ui/shell/platform_menu.h
	src/ui/shell/qml_shell.cpp
	src/ui/shell/qml_shell.h
	src/ui/shell/shell_qml_types.h
)
# The reflection of the data by glaze (JsonT), slow to compile: built once
# for the Editor and the tests
set(EDITOR_JSON_TYPES
	src/qmlbinding/json_internals/json_node_impl.h
	src/qmlbinding/json_internals/json_t_database.cpp
	src/qmlbinding/json_internals/json_t_impl.h
	src/qmlbinding/json_internals/json_t_map.cpp
	src/qmlbinding/json_internals/json_t_mapinfo.cpp
	src/qmlbinding/json_internals/json_t_treemap.cpp
)

# Dependencies
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 ${REQUIRED_QT_VERSION} CONFIG REQUIRED COMPONENTS
	Core Concurrent Multimedia Gui Svg Network GuiPrivate
	Qml Quick QuickControls2 QuickPrivate Test)
find_package(Qt6LinguistTools CONFIG QUIET)
find_package(ZLIB REQUIRED)
# The QML interface: KDE's Kirigami with its Breeze style, the docks of
# KDDockWidgets, and glaze for the project data as JSON (as upstream)
editor_find_package(glaze)
editor_find_package(KF6 6.0 COMPONENTS Kirigami)
editor_find_package(QQC2BreezeStyle)
find_package(KDBindings QUIET) # only required by the static build
editor_find_package(KDDockWidgets-qt6)

qt_standard_project_setup(
	REQUIRES "${REQUIRED_QT_VERSION}"
)

# The core of the Editor, without its interface: the project and its data,
# the editing logic (model, controllers) and the drawing of the tiles,
# which the tests use too: it is built without Qt Quick
set(EDITOR_CORE_SOURCES ${EDITOR_SOURCES})
set(EDITOR_CORE_REGEX "^src/(model|common|render|controller)/|^src/(core|stringizer|defines)\\.")
list(FILTER EDITOR_CORE_SOURCES INCLUDE REGEX "${EDITOR_CORE_REGEX}")
list(FILTER EDITOR_SOURCES EXCLUDE REGEX "${EDITOR_CORE_REGEX}")
qt_add_library(editor_core STATIC ${EDITOR_CORE_SOURCES})
set_target_properties(editor_core PROPERTIES AUTOMOC ON)
target_include_directories(editor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(editor_core PUBLIC
	ZLIB::ZLIB
	Qt6::Gui
	Qt6::Core
	Qt6::Concurrent
	Qt6::Multimedia
	Qt6::Network
	Qt6::Svg
)
if(MSVC)
	target_compile_options(editor_core PRIVATE /utf-8)
	target_compile_definitions(editor_core PUBLIC NOMINMAX)
endif()
if(APPLE)
	# audio of the Jukebox and the pickers: Apple's audio engine, with MIDI;
	# the Player brought to the front when a test play starts
	target_sources(editor_core PRIVATE src/common/audio_player_mac.mm src/model/game_runner_mac.mm)
	set_source_files_properties(src/common/audio_player_mac.mm src/model/game_runner_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
	target_link_libraries(editor_core PUBLIC "-framework AVFoundation" "-framework AudioToolbox" "-framework Foundation" "-framework AppKit")
endif()

# The JsonT types of the QML binding (the Editor and the binding test)
add_library(editor_json_types OBJECT ${EDITOR_JSON_TYPES})
set_target_properties(editor_json_types PROPERTIES AUTOMOC OFF)
target_include_directories(editor_json_types PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/qmlbinding)
target_link_libraries(editor_json_types PRIVATE editor_core Qt6::Qml glaze::glaze)
if(MSVC)
	target_compile_options(editor_json_types PRIVATE /utf-8 /bigobj)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
	# the template recursion of glaze over the database
	target_compile_options(editor_json_types PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fbracket-depth=1024>")
endif()

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
	# the reflection of the whole database by glaze
	target_compile_options(${EXE_NAME} PRIVATE /bigobj)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang$")
	# the template recursion of glaze over the database
	target_compile_options(${EXE_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fbracket-depth=1024>")
endif()
if(CMAKE_BUILD_TYPE MATCHES "Debug")
	# the QML debugger (Qt Creator)
	target_compile_definitions(${EXE_NAME} PRIVATE QT_QML_DEBUG=1)
endif()

# Qt Quick and the database of liblcf, which most files include: parsed
# once for all of them
option(EDITOR_PRECOMPILE_HEADERS "Precompile the Qt and liblcf headers (faster builds, unless ccache is used)" ON)
if(EDITOR_PRECOMPILE_HEADERS AND NOT _editor_uses_ccache)
	target_precompile_headers(${EXE_NAME} PRIVATE
		"$<$<COMPILE_LANGUAGE:CXX>:<QtQuick$<ANGLE-R>>"
		"$<$<COMPILE_LANGUAGE:CXX>:<lcf/rpg/database.h$<ANGLE-R>>")
	target_precompile_headers(editor_core PRIVATE
		"$<$<COMPILE_LANGUAGE:CXX>:<QtGui$<ANGLE-R>>"
		"$<$<COMPILE_LANGUAGE:CXX>:<lcf/rpg/database.h$<ANGLE-R>>")
	# a function of it is named like Qt's emit macro
	set_source_files_properties(src/model/maniac_expression.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

# Include directories
target_include_directories(${EXE_NAME}
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/src
		# the headers of the QML types, for the type registration of the module
		${CMAKE_CURRENT_SOURCE_DIR}/src/qmlbinding
		${CMAKE_CURRENT_SOURCE_DIR}/src/ui/canvas
		${CMAKE_CURRENT_SOURCE_DIR}/src/ui/event
		${CMAKE_CURRENT_SOURCE_DIR}/src/ui/map
		${CMAKE_CURRENT_SOURCE_DIR}/src/ui/maptree
		${CMAKE_CURRENT_SOURCE_DIR}/src/ui/picker
		${CMAKE_CURRENT_SOURCE_DIR}/src/ui/shell
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
	AUTORCC ON # Enable resource compiler for exe
)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# The QML module: every file at its root, whatever its folder
include(ECMQmlModule)
# where the QML modules of KDE (Kirigami, Breeze) are under each prefix:
# ECM gives them to qmllint and qmlimportscanner
if(NOT DEFINED KDE_INSTALL_QMLDIR)
	set(KDE_INSTALL_QMLDIR "lib/qml")
endif()
ecm_add_qml_module(${EXE_NAME}
	URI "org.easyrpg.editor"
	VERSION 1.0
	GENERATE_PLUGIN_SOURCE
	SOURCES
		${EDITOR_QML_INTERFACE}
)
ecm_target_qml_sources(${EXE_NAME}
	SOURCES ${EDITOR_QML_UI}
)
# the title bars of the docks: loaded by their URL for KDDockWidgets
# (qml_engine.cpp), apart from the types of the module, whose base (in
# KDDockWidgets' resources) qmllint does not know
set_source_files_properties(src/ui/shell/DockTitleBar.qml PROPERTIES QT_QML_SKIP_QMLDIR_ENTRY TRUE)
ecm_target_qml_sources(${EXE_NAME}
	SOURCES src/ui/shell/DockTitleBar.qml
	PATH kddockwidgets
)

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
	set(EDITOR_LCF_TARGET lcf)
	target_link_libraries(editor_core PUBLIC lcf)
else()
	# Use system package
	find_package(liblcf REQUIRED)
	set(EDITOR_LCF_TARGET liblcf::liblcf)
	target_link_libraries(editor_core PUBLIC liblcf::liblcf)
endif()

# Libraries
# shown in About
target_compile_definitions(${EXE_NAME} PRIVATE EDITOR_VERSION="${PROJECT_VERSION}")
target_link_libraries(${EXE_NAME} PRIVATE
	editor_core
	editor_json_types
	ZLIB::ZLIB
	Qt6::Gui
	Qt6::Core
	Qt6::Concurrent
	Qt6::Multimedia
	Qt6::Network
	Qt6::Svg
	Qt6::Qml
	Qt6::Quick
	Qt6::QuickPrivate
	Qt6::QuickControls2
	glaze::glaze
	KDAB::kddockwidgets
	KF6::KirigamiPlatform
)

# Kirigami and Breeze built as static libraries (vcpkg on Windows): their QML
# plugins are linked in by hand
get_target_property(KIRIGAMI_TYPE KF6::Kirigami TYPE)
if(KIRIGAMI_TYPE STREQUAL "STATIC_LIBRARY")
	target_link_libraries(${EXE_NAME} PRIVATE
		KF6::Kirigami
		KF6Kirigamiplugin
		KF6KirigamiTemplatesplugin
		KF6KirigamiDialogsplugin
		KF6KirigamiDelegatesplugin
		KF6KirigamiLayoutsplugin
		KF6KirigamiLayoutsPrivateplugin
		KF6KirigamiPrimitivesplugin
		KF6KirigamiPrivateplugin
		KF6KirigamiPolyfillplugin
	)
	target_compile_definitions(${EXE_NAME} PUBLIC KIRIGAMI_STATIC=1)
endif()
get_target_property(BREEZE_TYPE org.kde.breeze TYPE)
if(BREEZE_TYPE STREQUAL "STATIC_LIBRARY")
	target_link_libraries(${EXE_NAME} PRIVATE BreezeStyle BreezeImpl)
endif()
qt_import_qml_plugins(${EXE_NAME})

# Kirigami and Breeze outside Qt's folders (built for this Qt by
# builds/macos/build.sh): where the Editor finds their QML modules and the
# Kirigami platform plugin when it runs from its build folder. A packaged
# Editor has them inside (macdeployqt, the deploy script)
get_target_property(KIRIGAMI_LIBRARY KF6::Kirigami LOCATION)
if(KIRIGAMI_LIBRARY)
	get_filename_component(KIRIGAMI_LIBDIR "${KIRIGAMI_LIBRARY}" DIRECTORY)
	if(EXISTS "${KIRIGAMI_LIBDIR}/qml/org/kde/kirigami")
		target_compile_definitions(${EXE_NAME} PRIVATE
			EDITOR_KDE_QML_DIR="${KIRIGAMI_LIBDIR}/qml"
			EDITOR_KDE_PLUGIN_DIR="${KIRIGAMI_LIBDIR}/plugins")
	endif()
endif()


# Add resources
target_sources(${EXE_NAME} PRIVATE "resources/Resources.qrc")

# Script runner (--script, docs/Automation.adoc), for tests and screenshots
target_sources(${EXE_NAME} PRIVATE src/ui/tools/automation.cpp src/ui/tools/automation.h
	src/ui/tools/automation_quick_driver.cpp src/ui/tools/automation_quick_driver.h)
# its commands of the QML map canvas
target_sources(${EXE_NAME} PRIVATE src/ui/tools/automation_quick_map.cpp src/ui/tools/automation_quick_map.h)
# wheel: trackpad events injected like the platform's
target_link_libraries(${EXE_NAME} PRIVATE Qt6::GuiPrivate)

# Tests
enable_testing()
# the web Player folders the export accepts (a release, a Player build folder)
add_executable(editor_web_player_test tests/web_player_test.cpp src/model/web_player.cpp)
target_include_directories(editor_web_player_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(editor_web_player_test PRIVATE Qt6::Core)
add_test(NAME web_player COMMAND editor_web_player_test)
# the labels of the QML windows fade when disabled: Ez.Labels, not raw ones
# (the label of Ez.Label and Ez.ToolTip themselves; the smoke test of the
# KDE stack)
string(REPLACE ";" "|" EDITOR_QML_LABEL_FILES "${EDITOR_QML_UI}")
add_test(NAME qml_labels COMMAND ${CMAKE_COMMAND}
	"-DFILES=${EDITOR_QML_LABEL_FILES}"
	"-DALLOWED=Label.qml|ToolTip.qml|SmokeWindow.qml"
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_labels.cmake
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# no Qt Widgets: none of its headers in the sources and the tests, nor its
# library in the build
add_test(NAME no_widgets COMMAND ${CMAKE_COMMAND}
	"-DDIRS=${CMAKE_CURRENT_SOURCE_DIR}/src|${CMAKE_CURRENT_SOURCE_DIR}/tests"
	"-DCMAKE_FILES=${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
	-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_no_widgets.cmake)
# the texts of Move Event routes, in the code page of the game
add_executable(editor_move_route_test tests/move_route_codec_test.cpp src/model/move_route_codec.cpp)
target_include_directories(editor_move_route_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(editor_move_route_test PRIVATE ${EDITOR_LCF_TARGET})
add_test(NAME move_route_codec COMMAND editor_move_route_test)
# the editing of a map without its view (MapDocument), on a map in memory:
# its file and the project are left to the script tests
add_executable(editor_map_document_test tests/map_document_test.cpp
	src/model/map_document.cpp src/model/map_undo.cpp src/model/autotile.cpp src/model/map_tools.cpp)
target_include_directories(editor_map_document_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_map_document_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_map_document_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Gui Qt6::Test)
add_test(NAME map_document COMMAND editor_map_document_test)
# the map tree as an item model (MapTreeModel), on a tree in memory
add_executable(editor_map_tree_model_test tests/map_tree_model_test.cpp src/model/map_tree_model.cpp)
target_include_directories(editor_map_tree_model_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_map_tree_model_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_map_tree_model_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Gui Qt6::Test)
add_test(NAME map_tree_model COMMAND editor_map_tree_model_test)
# the command list of an event page as a list model (EventCommandListModel),
# on a page in memory, the texts of the commands from a stand-in of the
# Stringizer; the clipboard of the system needs a (offscreen) display
add_executable(editor_event_command_list_model_test tests/event_command_list_model_test.cpp
	src/model/event_command_list_model.cpp src/model/event_command_defaults.cpp src/model/command_colors.cpp
	src/model/command_schema.cpp)
target_include_directories(editor_event_command_list_model_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_event_command_list_model_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_event_command_list_model_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Gui Qt6::Test)
add_test(NAME event_command_list_model COMMAND editor_event_command_list_model_test)
set_tests_properties(event_command_list_model PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# Project Settings without its window (ProjectSettings), on a copy of the
# files of the test game (skipped without it), which it writes
add_executable(editor_project_settings_test tests/project_settings_test.cpp)
target_include_directories(editor_project_settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_project_settings_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_project_settings_test PRIVATE editor_core Qt6::Test)
add_test(NAME project_settings COMMAND editor_project_settings_test)
set_tests_properties(project_settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# the search of a project (ProjectSearch) and its results (SearchResultsModel),
# on a copy of the database and maps of the test game (skipped without it)
# with events added in memory
add_executable(editor_project_search_test tests/project_search_test.cpp)
target_include_directories(editor_project_search_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_project_search_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_project_search_test PRIVATE editor_core Qt6::Test)
add_test(NAME project_search COMMAND editor_project_search_test)
set_tests_properties(project_search PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# the editing rules of the Chipset, Monster Group, curve and Test Battle
# windows without their QML views, on data in memory; the painter of the
# chipset's marks needs a (offscreen) display for its fonts
add_executable(editor_database_tools_test tests/database_tools_test.cpp
	src/model/chipset_editor.cpp src/render/chipset_editor_painter.cpp src/model/troop_layout.cpp
	src/model/experience_curve.cpp src/model/test_battle.cpp)
target_include_directories(editor_database_tools_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_database_tools_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_database_tools_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Gui Qt6::Test)
add_test(NAME database_tools COMMAND editor_database_tools_test)
set_tests_properties(database_tools PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# the map drawn by the scene graph (MapLayerView) against TilePainter, on a
# map of the test game (skipped without it): with the software scene graph,
# then with the default one (Metal, Direct3D, OpenGL) on a screen of twice
# the resolution; the backend is chosen once per process. The offscreen
# platform has only the software one: the second test uses the system's
# (a window never shown, the Editor not in the Dock), skipped without it
add_executable(editor_map_layer_test tests/map_layer_test.cpp
	src/ui/map/map_layer_item.cpp src/ui/map/map_layer_item.h src/ui/map/map_texture_cache.cpp)
target_include_directories(editor_map_layer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_map_layer_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_map_layer_test PRIVATE editor_core Qt6::Quick Qt6::Test)
add_test(NAME map_layer COMMAND editor_map_layer_test)
set_tests_properties(map_layer PROPERTIES ENVIRONMENT
	"QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
add_test(NAME map_layer_rhi COMMAND editor_map_layer_test)
if(APPLE OR WIN32)
	set(EDITOR_RHI_TEST_PLATFORM "QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1")
else()
	set(EDITOR_RHI_TEST_PLATFORM "QT_QPA_PLATFORM=offscreen")
endif()
set_tests_properties(map_layer_rhi PROPERTIES ENVIRONMENT
	"${EDITOR_RHI_TEST_PLATFORM};EDITOR_TEST_RHI=1;QT_SCALE_FACTOR=2;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# the editing of the battle animations without their pages (AnimationTools,
# AnimationEditor, BattlerAnimationEditor), on animations in memory
add_executable(editor_animation_editor_test tests/animation_editor_test.cpp
	src/model/animation_editor.cpp src/model/animation_tools.cpp src/model/battler_animation_editor.cpp)
target_include_directories(editor_animation_editor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_animation_editor_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_animation_editor_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Core Qt6::Test)
add_test(NAME animation_editor COMMAND editor_animation_editor_test)
# the Resource Manager without its window (ResourceLibraryModel) on a copy
# of the test game (skipped without it): files, import, export, delete
add_executable(editor_resource_library_test tests/resource_library_test.cpp)
target_include_directories(editor_resource_library_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_resource_library_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_resource_library_test PRIVATE editor_core Qt6::Test)
add_test(NAME resource_library COMMAND editor_resource_library_test)
set_tests_properties(resource_library PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# the resource pickers without their window (FilePickerModel), on files
# made in a temporary folder: the list, the file selected, the choice
add_executable(editor_file_picker_model_test tests/file_picker_model_test.cpp)
target_include_directories(editor_file_picker_model_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_file_picker_model_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_file_picker_model_test PRIVATE editor_core Qt6::Test)
add_test(NAME file_picker_model COMMAND editor_file_picker_model_test)
set_tests_properties(file_picker_model PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# the Jukebox without its window (AudioLibraryModel) on the music and
# sounds of the test game (skipped without it); nothing is played
add_executable(editor_audio_library_test tests/audio_library_test.cpp)
target_include_directories(editor_audio_library_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_audio_library_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_audio_library_test PRIVATE editor_core Qt6::Test)
add_test(NAME audio_library COMMAND editor_audio_library_test)
set_tests_properties(audio_library PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# the transparent color of an image imported (TransparentColorChoice, the
# Import Image window without it)
add_executable(editor_transparent_color_test tests/transparent_color_test.cpp)
target_include_directories(editor_transparent_color_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_transparent_color_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_transparent_color_test PRIVATE editor_core Qt6::Test)
add_test(NAME transparent_color COMMAND editor_transparent_color_test)
set_tests_properties(transparent_color PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# Open Project without its window (ProjectListModel) on projects made of the
# test game's files (skipped without it): the list, Delete, Settings
add_executable(editor_project_list_test tests/project_list_test.cpp)
target_include_directories(editor_project_list_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_project_list_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_project_list_test PRIVATE editor_core Qt6::Test)
add_test(NAME project_list COMMAND editor_project_list_test)
set_tests_properties(project_list PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# Export Game without its window (ExportController): a copy of the test game
# (skipped without it) exported into a temporary folder; nothing downloaded
add_executable(editor_export_controller_test tests/export_controller_test.cpp)
target_include_directories(editor_export_controller_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_export_controller_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_export_controller_test PRIVATE editor_core Qt6::Test)
add_test(NAME export_controller COMMAND editor_export_controller_test)
set_tests_properties(export_controller PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}" TIMEOUT 300)
# The Player of this computer downloaded for test play (PlayerDownload), from
# fake archives (no network) into a temporary folder
include(tests/fake_player.cmake)
add_executable(editor_player_download_test tests/player_download_test.cpp)
target_include_directories(editor_player_download_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_player_download_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_player_download_test PRIVATE editor_core Qt6::Test)
add_test(NAME player_download COMMAND editor_player_download_test)
set_tests_properties(player_download PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_FAKE_PLAYER_URL=${EDITOR_FAKE_PLAYER_URL}")
# the schemas the QML command editors are generated from (CommandSchema):
# one for each command with an editor, their parameters, bits and
# rules on new commands, their texts in the French translation
add_executable(editor_command_schema_test tests/command_schema_test.cpp
	src/model/command_schema.cpp src/model/event_command_defaults.cpp)
target_include_directories(editor_command_schema_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(editor_command_schema_test PRIVATE EDITOR_TRANSLATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/translations")
set_target_properties(editor_command_schema_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_command_schema_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Core Qt6::Test)
add_test(NAME command_schema COMMAND editor_command_schema_test)
# the editing of a map with the mouse and the keys without its view
# (MapEditor), in cells, on a map in memory; the shortcuts of the platform
# need a (offscreen) display
add_executable(editor_map_editor_test tests/map_editor_test.cpp
	src/ui/map/map_editor.cpp src/ui/map/map_editor.h src/model/editor_state.cpp
	src/model/map_document.cpp src/model/map_undo.cpp src/model/autotile.cpp src/model/map_tools.cpp)
target_include_directories(editor_map_editor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_map_editor_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_map_editor_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Gui Qt6::Test)
add_test(NAME map_editor COMMAND editor_map_editor_test)
set_tests_properties(map_editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# the event window without its QML view (EventEditorController, the settings
# of a page, the battle page conditions) and the moves of a route as a list
# model (MoveRouteModel), on events in memory, a new event put on a map in
# memory (MapDocument); the texts of the moves from a stand-in of the
# Stringizer, the colors of the list need a (offscreen) display
add_executable(editor_event_editor_test tests/event_editor_test.cpp
	src/model/event_editor_controller.cpp src/model/move_route_model.cpp src/model/move_route_codec.cpp
	src/controller/edit_locks.cpp
	src/model/map_document.cpp src/model/map_undo.cpp src/model/autotile.cpp src/model/map_tools.cpp)
target_include_directories(editor_event_editor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_event_editor_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_event_editor_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Gui Qt6::Test)
add_test(NAME event_editor COMMAND editor_event_editor_test)
set_tests_properties(event_editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# the rest of the map drawn by the scene graph (the panorama, the events
# and the start positions, MapPreview.qml) on maps of the test game
# (skipped without it): what is drawn where, the updates, the textures
# shared by the views of a window, the zoom snapped. Like map_layer, with
# the software scene graph, then with the system's (map_view_rhi). With
# EDITOR_TEST_OUTPUT set, it saves pictures of the maps to look at
add_executable(editor_map_view_test tests/map_view_test.cpp
	src/ui/map/map_events_item.cpp src/ui/map/map_layer_item.cpp src/ui/map/map_panorama_item.cpp
	src/ui/map/map_texture_cache.cpp)
target_include_directories(editor_map_view_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(editor_map_view_test PRIVATE EDITOR_MAP_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/ui/map")
set_target_properties(editor_map_view_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_map_view_test PRIVATE editor_core Qt6::Quick Qt6::Test)
add_test(NAME map_view COMMAND editor_map_view_test)
set_tests_properties(map_view PROPERTIES ENVIRONMENT
	"QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
add_test(NAME map_view_rhi COMMAND editor_map_view_test)
set_tests_properties(map_view_rhi PROPERTIES ENVIRONMENT
	"${EDITOR_RHI_TEST_PLATFORM};EDITOR_TEST_RHI=1;QT_SCALE_FACTOR=2;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
set(EDITOR_TEST_PROJECT "" CACHE PATH "Game the script tests run on (TestGame-2003 of https://github.com/EasyRPG/TestGame)")
# OK and Apply of the database windows (DatabaseApply) on a copy of the
# database of the test game (skipped without it), which another program
# changes meanwhile; the project's file (core()) is left to the script tests
add_executable(editor_database_apply_test tests/database_apply_test.cpp
	src/controller/database_apply.cpp src/controller/dialog_service.cpp src/model/data_merge.cpp
	src/model/database_list_data.cpp src/model/project_watcher.cpp src/common/logging.cpp)
target_include_directories(editor_database_apply_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_database_apply_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_database_apply_test PRIVATE ${EDITOR_LCF_TARGET} Qt6::Core Qt6::Test)
add_test(NAME database_apply COMMAND editor_database_apply_test)
set_tests_properties(database_apply PROPERTIES ENVIRONMENT "EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# the workspace of a project (tabs, zoom, tree, layout) in settings of its
# own, and what older versions saved
add_executable(editor_workspace_store_test tests/workspace_store_test.cpp src/controller/workspace_store.cpp)
target_include_directories(editor_workspace_store_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_workspace_store_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_workspace_store_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME workspace_store COMMAND editor_workspace_store_test)
# the edit lock of a modal window (ModalWindowLocks): held while it is shown,
# none for the other windows; the dialogs themselves: script edit_locks
add_executable(editor_edit_locks_test tests/edit_locks_test.cpp src/controller/edit_locks.cpp)
target_include_directories(editor_edit_locks_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_edit_locks_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_edit_locks_test PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME edit_locks COMMAND editor_edit_locks_test)
set_tests_properties(edit_locks PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# every action of the main window (ActionCatalog): its texts and their
# translations, its keys, groups, menus and icons
add_executable(editor_action_catalog_test tests/action_catalog_test.cpp
	src/controller/action_catalog.cpp src/controller/shortcut_catalog.cpp)
target_include_directories(editor_action_catalog_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(editor_action_catalog_test PRIVATE EDITOR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(editor_action_catalog_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_action_catalog_test PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME action_catalog COMMAND editor_action_catalog_test)
set_tests_properties(action_catalog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# the map tabs without a window (MapTabsController) on a copy of the test
# game (skipped without it)
add_executable(editor_map_tabs_controller_test tests/map_tabs_controller_test.cpp)
target_include_directories(editor_map_tabs_controller_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_map_tabs_controller_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_map_tabs_controller_test PRIVATE editor_core Qt6::Test)
add_test(NAME map_tabs_controller COMMAND editor_map_tabs_controller_test)
set_tests_properties(map_tabs_controller PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# what the main window does without it (ShellController), on copies of
# the test game (skipped without it)
add_executable(editor_shell_controller_test tests/shell_controller_test.cpp)
target_include_directories(editor_shell_controller_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_shell_controller_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_shell_controller_test PRIVATE editor_core Qt6::Test)
add_test(NAME shell_controller COMMAND editor_shell_controller_test)
set_tests_properties(shell_controller PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# where a map dragged in the map tree lands (MapTreeDrop), on a tree in memory
add_executable(editor_map_tree_drop_test tests/map_tree_drop_test.cpp)
target_include_directories(editor_map_tree_drop_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(editor_map_tree_drop_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_map_tree_drop_test PRIVATE editor_core Qt6::Test)
add_test(NAME map_tree_drop COMMAND editor_map_tree_drop_test)
# the QML binding (Json, its views and lists, JsonField, DatabaseSession,
# EntryFilterModel) on a database in memory, and on a copy of the test
# game's when there is one
add_executable(editor_qmlbinding_test tests/qmlbinding_test.cpp
	src/qmlbinding/database_session.cpp src/qmlbinding/database_session.h
	src/qmlbinding/entry_filter_model.cpp src/qmlbinding/entry_filter_model.h
	src/qmlbinding/json.cpp src/qmlbinding/json.h
	src/qmlbinding/json_field.cpp src/qmlbinding/json_field.h
	src/qmlbinding/json_list_view.cpp src/qmlbinding/json_list_view.h
	src/qmlbinding/json_view.cpp src/qmlbinding/json_view.h
	src/qmlbinding/project_data_gadget.cpp src/qmlbinding/project_data_gadget.h)
target_include_directories(editor_qmlbinding_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/qmlbinding)
set_target_properties(editor_qmlbinding_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_qmlbinding_test PRIVATE editor_json_types editor_core Qt6::Qml Qt6::Test)
add_test(NAME qmlbinding COMMAND editor_qmlbinding_test)
set_tests_properties(qmlbinding PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EDITOR_TEST_PROJECT=${EDITOR_TEST_PROJECT}")
# The Test Play Options without their window (TestPlayOptions: what the
# Player is given) and the party of the battle tests for QML
# (TestBattlePartyModel), on a project in memory
add_executable(editor_test_play_options_test tests/test_play_options_test.cpp
	src/qmlbinding/database_session.cpp src/qmlbinding/database_session.h
	src/qmlbinding/json.cpp src/qmlbinding/json.h
	src/qmlbinding/json_list_view.cpp src/qmlbinding/json_list_view.h
	src/qmlbinding/json_view.cpp src/qmlbinding/json_view.h
	src/qmlbinding/project_data_gadget.cpp src/qmlbinding/project_data_gadget.h
	src/qmlbinding/test_battle_party_model.cpp src/qmlbinding/test_battle_party_model.h)
target_include_directories(editor_test_play_options_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/qmlbinding)
set_target_properties(editor_test_play_options_test PROPERTIES AUTOMOC ON)
target_link_libraries(editor_test_play_options_test PRIVATE editor_json_types editor_core Qt6::Qml Qt6::Test)
add_test(NAME test_play_options COMMAND editor_test_play_options_test)
set_tests_properties(test_play_options PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
if(EDITOR_TEST_PROJECT)
	file(GLOB EDITOR_TEST_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/scripts/*.txt")
	foreach(script ${EDITOR_TEST_SCRIPTS})
		get_filename_component(name ${script} NAME_WE)
		add_test(NAME script_${name}
			COMMAND $<TARGET_FILE:${EXE_NAME}> --script ${script} ${EDITOR_TEST_PROJECT}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
		# a dialog nobody closes fails the test instead of blocking the run
		set_tests_properties(script_${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;EASYRPG_EDITOR_LAUNCHER_BASE=${EDITOR_FAKE_PLAYER_URL}" TIMEOUT 120)
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
		COMMAND $<TARGET_FILE:Qt6::lupdate> ${CMAKE_CURRENT_SOURCE_DIR}/src -ts ${TRANSLATION_SOURCES} -no-obsolete -locations relative
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
	qt_generate_deploy_qml_app_script(
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
