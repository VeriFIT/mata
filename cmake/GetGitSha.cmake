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
#  variable could redirect it anywhere. Git accepts the `.git` path itself even when it is a gitfile
#  (a linked worktree or submodule), so the pointer's length never matters here.
set (GIT_SHA1 "")
set (GIT_DIR_ARG "${SOURCE_DIR}/.git")
if (NOT EXISTS "${GIT_DIR_ARG}")
	# No `.git` at all: a release tarball or a Python sdist, never a repository of its own.
	set (GIT_DIR_ARG "")
endif ()

if (GIT_DIR_ARG)
	# A configured path can point at a git that no longer exists (a Nix store path removed by
	#  garbage collection). Search into a fresh variable, so a still-installed git on PATH is found.
	set (GIT_BIN "${GIT_EXECUTABLE}")
	if (NOT EXISTS "${GIT_BIN}")
		unset (GIT_BIN)
		find_program (GIT_BIN NAMES git)
	endif ()
	if (GIT_BIN)
		execute_process (
			# Drop inherited repository-location variables; `--git-dir` then selects exactly the
			#  checkout of the sources. `rev-parse --verify` keeps an unborn HEAD on the fallback
			#  even if a file called HEAD happens to sit in the working directory.
			COMMAND "${CMAKE_COMMAND}" -E env
				--unset=GIT_DIR --unset=GIT_WORK_TREE --unset=GIT_COMMON_DIR
				"${GIT_BIN}" "--git-dir=${GIT_DIR_ARG}" rev-parse --verify HEAD
			WORKING_DIRECTORY "${SOURCE_DIR}"
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
