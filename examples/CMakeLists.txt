file(GLOB_RECURSE MATA_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
foreach(MATA_EXAMPLE ${MATA_EXAMPLES})
	file(RELATIVE_PATH MATA_EXAMPLE_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${MATA_EXAMPLE}")
	string(REPLACE "/" "-" MATA_EXAMPLE_NAME "${MATA_EXAMPLE_REL}")
	string(REPLACE ".cc" "" MATA_EXAMPLE_NAME "${MATA_EXAMPLE_NAME}")
	# Not built: is_in_lang_by_levels_repeat_symbol(), which it timed directly against MATA_BENCH_HAS_REPEAT_SYMBOL,
	# is no longer a public symbol. Left on disk for its scenario/query generation code, not currently buildable.
	if (${MATA_EXAMPLE_NAME} STREQUAL "bench_is_in_lang_by_levels")
		continue()
	endif ()
	add_executable(${MATA_EXAMPLE_NAME} ${MATA_EXAMPLE})
	target_link_libraries(${MATA_EXAMPLE_NAME} PRIVATE libmata)
endforeach()
