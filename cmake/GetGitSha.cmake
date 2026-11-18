# Write `src/config.cc` with the hash of the current git HEAD. Run in script mode (`cmake -P`) as a
# build step of `libmata`, never at configure time.
#
# Reading the hash while configuring (what `get_git_head_revision()` did) registers `${GIT_DIR}/HEAD`
#  as a configure dependency of the build directory, so the first build after any commit, checkout or
#  `jj` operation re-runs the whole configure step before compiling anything. Here the hash is read on
#  every build, and `configure_file()` rewrites the output only when its content changed (it keeps the
#  timestamp of an unchanged output), so a new HEAD costs one recompiled object and an unchanged HEAD
#  costs one `git rev-parse`.
#
# Expects: SOURCE_DIR, CONFIG_IN, CONFIG_OUT, GIT_EXECUTABLE (may be empty or gone).

# Bind the lookup to the repository of SOURCE_DIR. An unbound `rev-parse` would walk up from the
#  working directory and happily report the hash of an enclosing checkout that has nothing to do with
#  these sources (a release archive unpacked inside one), and an inherited `GIT_DIR` environment
#  variable could redirect it anywhere. In a linked worktree or a submodule, `.git` is a file pointing
#  at the real metadata directory ('gitdir: <path>') instead.
set (GIT_SHA1 "")
set (GIT_DIR_ARG "${SOURCE_DIR}/.git")
if (NOT IS_DIRECTORY "${SOURCE_DIR}/.git")
	if (EXISTS "${SOURCE_DIR}/.git")
		file (READ "${SOURCE_DIR}/.git" GIT_FILE LIMIT 512)
		string (REGEX MATCH "gitdir: *([^\r\n]+)" _ "${GIT_FILE}")
		if (NOT CMAKE_MATCH_1 STREQUAL "")
			set (GIT_DIR_ARG "${CMAKE_MATCH_1}")
			# The pointer can be relative to the directory of the `.git` file (a submodule's
			#  `../.git/modules/<name>`), so make it absolute before use.
			cmake_path (ABSOLUTE_PATH GIT_DIR_ARG BASE_DIRECTORY "${SOURCE_DIR}" NORMALIZE)
		else ()
			set (GIT_DIR_ARG "")
		endif ()
	else ()
		# No `.git` at all: a release tarball or a Python sdist, never a repository of its own.
		set (GIT_DIR_ARG "")
	endif ()
endif ()

if (GIT_DIR_ARG)
	# The configured path can point at a git that no longer exists (a Nix store path removed by
	#  garbage collection), and `find_program()` keeps returning such a path from its cache. Search
	#  into a fresh variable instead, so a still-installed git on the PATH is found again.
	set (GIT_BIN "${GIT_EXECUTABLE}")
	if (NOT EXISTS "${GIT_BIN}")
		unset (GIT_BIN)
		find_program (GIT_BIN NAMES git)
	endif ()
	if (GIT_BIN)
		execute_process (
			# Not quoted: in `cmake -P` script mode a quoted `--git-dir=${…}` reaches git with the
			#  quote characters as part of the path.
			COMMAND ${GIT_BIN} --git-dir=${GIT_DIR_ARG} rev-parse HEAD
			OUTPUT_VARIABLE GIT_SHA1
			OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
			RESULT_VARIABLE GIT_RESULT
		)
		# A `git init` without a commit has no HEAD hash yet; the same empty result covers it as
		#  what the configure-time lookup produced for a non-repository.
		if (NOT GIT_RESULT EQUAL 0)
			set (GIT_SHA1 "")
		endif ()
	else ()
		set (GIT_SHA1 "")
	endif ()
endif ()
configure_file ("${CONFIG_IN}" "${CONFIG_OUT}" @ONLY)
