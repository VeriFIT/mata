# Write `src/config.cc` with the hash of the current git HEAD. Run in script mode (`cmake -P`) as a
# build step of `libmata`, never at configure time.
#
# Reading the hash while configuring (what `get_git_head_revision()` did) registers `${GIT_DIR}/HEAD`
#  as a configure dependency of the build directory, so the first build after any commit, checkout or
#  `jj` operation re-runs the whole configure step before compiling anything. Here the hash is read on
#  every build, and the result is copied over the previous `config.cc` only when it differs, so a new
#  HEAD costs one recompiled object and an unchanged HEAD costs one `git rev-parse`.
#
# Expects: SOURCE_DIR, CONFIG_IN, CONFIG_OUT. Optional GIT_EXECUTABLE: a usable git program name/path
#  to prefer. Discovery uses a fresh variable, so a stale configured path cannot block it.

# A removed stale cached executable must not suppress discovery of a usable Git on PATH.
set (GIT_PROGRAM "")
if (GIT_EXECUTABLE AND EXISTS "${GIT_EXECUTABLE}")
	set (GIT_PROGRAM "${GIT_EXECUTABLE}")
endif ()
if (NOT GIT_PROGRAM)
	unset (GIT_PROGRAM)
	find_program (GIT_PROGRAM NAMES git NO_CACHE)
endif ()

set (GIT_SHA1 "")
if (GIT_PROGRAM)
	# Only a repository rooted at the source tree identifies Mata's sources. Otherwise a release
	#  archive unpacked inside another Git checkout would report the enclosing repository's HEAD.
	#  GIT_DIR/GIT_WORK_TREE are cleared for the subprocess so an inherited environment cannot
	#  redirect this check; `rev-parse --show-toplevel` follows a worktree's `.git` file normally.
	execute_process (
		COMMAND "${CMAKE_COMMAND}" -E env --unset=GIT_DIR --unset=GIT_WORK_TREE
			"${GIT_PROGRAM}" rev-parse --show-toplevel
		WORKING_DIRECTORY "${SOURCE_DIR}"
		OUTPUT_VARIABLE GIT_TOPLEVEL
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
		RESULT_VARIABLE GIT_RESULT
	)
	file (REAL_PATH "${SOURCE_DIR}" SOURCE_REAL_PATH)
	if (GIT_RESULT EQUAL 0 AND "${GIT_TOPLEVEL}" STREQUAL "${SOURCE_REAL_PATH}")
		execute_process (
			COMMAND "${CMAKE_COMMAND}" -E env --unset=GIT_DIR --unset=GIT_WORK_TREE
				"${GIT_PROGRAM}" rev-parse HEAD
			WORKING_DIRECTORY "${SOURCE_DIR}"
			OUTPUT_VARIABLE GIT_SHA1
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			RESULT_VARIABLE GIT_RESULT
		)
	endif ()
	# A source tree that is not a repository (release tarball, Python sdist) has no hash. Leave
	#  `mata::g_GIT_SHA1` empty there, which is what the configure-time lookup also did.
	if (NOT GIT_RESULT EQUAL 0)
		set (GIT_SHA1 "")
	endif ()
endif ()

configure_file ("${CONFIG_IN}" "${CONFIG_OUT}.tmp" @ONLY)
execute_process (
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CONFIG_OUT}.tmp" "${CONFIG_OUT}"
	RESULT_VARIABLE COPY_RESULT
)
if (NOT COPY_RESULT EQUAL 0)
	message (FATAL_ERROR "Failed to write '${CONFIG_OUT}'")
endif ()
