# The texts of the QML windows are Ez.Labels (Ez.FormLabel, Ez.Note...):
# they fade with the controls when disabled (Ez.Metrics.disabledOpacity),
# where Breeze's label and the labels of Kirigami's FormLayout keep the
# color of an enabled one. Fails for each raw label in FILES (the files of
# the QML module, separated by "|") but those of ALLOWED.
#
#   cmake -DFILES="a.qml|b.qml" -DALLOWED="Label.qml|ToolTip.qml" -P check_labels.cmake
string(REPLACE "|" ";" files "${FILES}")
string(REPLACE "|" ";" allowed "${ALLOWED}")
set(found 0)
foreach(file IN LISTS files)
	get_filename_component(name "${file}" NAME)
	if(name IN_LIST allowed)
		continue()
	endif()
	file(STRINGS "${file}" lines)
	set(number 0)
	foreach(line IN LISTS lines)
		math(EXPR number "${number} + 1")
		if(line MATCHES "(Controls|QQC2)\\.Label[ \t]*{" OR line MATCHES "Kirigami\\.FormData\\.label")
			message(WARNING "${file}:${number}: a raw label (use Ez.Label, or Ez.Form and Ez.FormLabel): ${line}")
			math(EXPR found "${found} + 1")
		endif()
	endforeach()
endforeach()
if(found GREATER 0)
	message(FATAL_ERROR "${found} raw label(s)")
endif()
message(STATUS "no raw label")
