# Fake EasyRPG Player archives, for the tests of the Player downloads without
# the network: a folder given as EASYRPG_EDITOR_LAUNCHER_BASE (file://),
# with the archive of each platform as easyrpg.org names it, holding a
# program that does nothing. Sets EDITOR_FAKE_PLAYER_URL.

# the version downloaded (Launchers::version)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/model/launcher_download.cpp" version_line
	REGEX "kVersion\\[\\] = \"[0-9.]+\"")
string(REGEX MATCH "[0-9][0-9.]*" fake_player_version "${version_line}")

set(fake_player_dir "${CMAKE_CURRENT_BINARY_DIR}/fake-player")
set(fake_player_src "${fake_player_dir}/src")
file(REMOVE_RECURSE "${fake_player_dir}")
set(program "#!/bin/sh\nexit 0\n")
set(executable FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# macOS: the application bundle
file(MAKE_DIRECTORY "${fake_player_src}/macos/EasyRPG Player.app/Contents/MacOS")
file(WRITE "${fake_player_src}/macos/EasyRPG Player.app/Contents/MacOS/EasyRPG Player" "${program}")
file(CHMOD "${fake_player_src}/macos/EasyRPG Player.app/Contents/MacOS/EasyRPG Player" ${executable})
execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf "${fake_player_dir}/easyrpg-player-${fake_player_version}-macos.app.zip"
	--format=zip "EasyRPG Player.app" WORKING_DIRECTORY "${fake_player_src}/macos")
# Windows: Player.exe
file(MAKE_DIRECTORY "${fake_player_src}/windows")
file(WRITE "${fake_player_src}/windows/Player.exe" "${program}")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf "${fake_player_dir}/easyrpg-player-${fake_player_version}-windows-x64.zip"
	--format=zip "Player.exe" WORKING_DIRECTORY "${fake_player_src}/windows")
# Linux: the program in a tarball
file(MAKE_DIRECTORY "${fake_player_src}/linux")
file(WRITE "${fake_player_src}/linux/easyrpg-player" "${program}")
file(CHMOD "${fake_player_src}/linux/easyrpg-player" ${executable})
execute_process(COMMAND ${CMAKE_COMMAND} -E tar czf "${fake_player_dir}/easyrpg-player-${fake_player_version}-linux.tar.gz"
	"easyrpg-player" WORKING_DIRECTORY "${fake_player_src}/linux")

if(fake_player_dir MATCHES "^/")
	set(EDITOR_FAKE_PLAYER_URL "file://${fake_player_dir}/")
else()
	set(EDITOR_FAKE_PLAYER_URL "file:///${fake_player_dir}/")
endif()
