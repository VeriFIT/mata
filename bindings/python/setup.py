import os
import errno
import shutil
import platform
import shlex
import subprocess
import re

from setuptools import setup, Extension
from Cython.Build import cythonize
from distutils.command.sdist import sdist as _sdist
from distutils.command.build_ext import build_ext as _build_ext

if 'microsoft' in platform.uname().release.lower():
    #Patch for calling the python from WSL. The copystat fails on copied directories
    orig_copyxattr = shutil._copyxattr
    def patched_copyxattr(src, dst, *, follow_symlinks=True):
        try:
            orig_copyxattr(src, dst, follow_symlinks=follow_symlinks)
        except OSError as ex:
            if ex.errno != errno.EACCES: raise
    shutil._copyxattr = patched_copyxattr

root_dir = os.path.abspath(os.path.dirname(__file__))
project_dir = os.path.abspath(os.path.join(os.path.join(root_dir, "..", "..")))
sdist_dir = os.path.join(os.path.join(root_dir, "mata"))
src_dir = sdist_dir if os.path.exists(sdist_dir) else project_dir
mata_build_dir = "build"

# Build stuff
project_library_dirs = [
    os.path.join(src_dir, mata_build_dir, "src"),
    os.path.join(src_dir, mata_build_dir, "3rdparty", "cudd"),
    os.path.join(src_dir, mata_build_dir, "3rdparty", "simlib"),
    os.path.join(src_dir, mata_build_dir, "3rdparty", "re2"),
]

with open(os.path.join(src_dir, "README.md")) as readme_handle:
    README_MD = readme_handle.read()

project_includes = [
    os.path.join(src_dir, "include"),
    os.path.join(src_dir, "3rdparty"),
    os.path.join(src_dir, "3rdparty", "re2"),
    os.path.join(src_dir, "3rdparty", "simlib", "include"),
    os.path.join(src_dir, "3rdparty", "cudd", "include")
]

extensions = [
    Extension(
        f"libmata.{pkg}",
        sources=[f"libmata{os.sep}{pkg.replace('.', os.sep)}.pyx"],
        include_dirs=project_includes,
        libraries=['mata'],
        library_dirs=project_library_dirs,
        language="c++",
        extra_compile_args=["-std=c++20", "-DNO_THROW_DISPATCHER"],
    ) for pkg in (
        'nfa.nfa', 'alphabets', 'utils', 'parser', 'nfa.strings', 'plotting'
    )
]


def _clean_up():
    """Removes old files"""
    shutil.rmtree(sdist_dir, ignore_errors=True)


def _copy_sources():
    """Copies sources to specified `mata` folder.

    This is used in `dist` command, to copy sources that will be included in the
    tar.gz and that it will be able to compile on other systems using pip.

    Inspired by z3 setup.py
    """
    _clean_up()
    os.mkdir(sdist_dir)

    version_file = os.path.join(project_dir, 'VERSION')
    version_file_was_created = False
    if not os.path.exists(version_file):
        version = get_version()
        with open(version_file, 'w') as version_handle:
            version_handle.write(version)
            version_file_was_created = True
    shutil.copy(version_file, sdist_dir)
    shutil.copy(os.path.join(project_dir, 'LICENSE'), sdist_dir)
    shutil.copy(os.path.join(project_dir, 'README.md'), sdist_dir)
    shutil.copy(os.path.join(project_dir, 'Makefile'), sdist_dir)
    shutil.copy(os.path.join(project_dir, 'CMakeLists.txt'), sdist_dir)
    shutil.copy(os.path.join(project_dir, 'Doxyfile.in'), sdist_dir)
    shutil.copy(os.path.join(project_dir, 'cmake_uninstall.cmake.in'), sdist_dir)
    for mata_dir in (
            'src', 'include', '3rdparty', 'cmake'
    ):
        shutil.copytree(
            os.path.join(project_dir, mata_dir),
            os.path.join(sdist_dir, mata_dir),
        )
    if version_file_was_created:
        os.remove(version_file)


class sdist(_sdist):
    def run(self):
        self.execute(_copy_sources, (), msg="Copying source files")
        _sdist.run(self)
        self.execute(_clean_up, (), msg="Cleaning up")


class build_ext(_build_ext):
    def run(self):
        self.execute(_build_mata, (), msg="Building libmata library")
        _build_ext.run(self)


def get_version():
    """Parses the version of the library from the VERSION file"""
    # Note: The VERSION file should be present ONLY in the following cases:
    # 1. the binding is installed from github action (one of the action automatically
    # creates this file)
    # 2. the binding is installed from sdist
    version_file = os.path.join(src_dir, "VERSION")
    if os.path.exists(version_file):
        with open(version_file, 'r') as version_handle:
            return version_handle.read().split()[0]
    else:
        version, _ = run_safely_external_command("git describe --tags --abbrev=0 HEAD")
        assert re.match(r"\d+\.\d+\.\d+(.*)", version)
        return version.strip()


def _build_mata():
    """Builds mata library"""
    with subprocess.Popen(
        shlex.split(f"make release-lib BUILD_DIR={mata_build_dir}"),
        cwd=src_dir, bufsize=1, universal_newlines=True, stdout=subprocess.PIPE, shell=False
    ) as p:
        for line in p.stdout:
            print(line, end='')


def run_safely_external_command(cmd: str, check_results=True, quiet=True, timeout=None, **kwargs):
    """Safely runs the piped command, without executing of the shell

    Courtesy of: https://blog.avinetworks.com/tech/python-best-practices

    :param str cmd: string with command that we are executing
    :param bool check_results: check correct command exit code and raise exception in case of fail
    :param bool quiet: if set to False, then it will print the output of the command
    :param int timeout: timeout of the command
    :param dict kwargs: additional args to subprocess call
    :return: returned standard output and error
    :raises subprocess.CalledProcessError: when any of the piped commands fails
    """
    # Split
    unpiped_commands = list(map(str.strip, cmd.split(" | ")))
    cmd_no = len(unpiped_commands)

    # Run the command through pipes
    objects: list[subprocess.Popen] = []
    for i in range(cmd_no):
        executed_command = shlex.split(unpiped_commands[i])

        # set streams
        stdin = None if i == 0 else objects[i-1].stdout
        stderr = subprocess.STDOUT if i < (cmd_no - 1) else subprocess.PIPE

        # run the piped command and close the previous one
        piped_command = subprocess.Popen(
            executed_command,
            shell=False, stdin=stdin, stdout=subprocess.PIPE, stderr=stderr, **kwargs
        )
        if i != 0:
            objects[i-1].stdout.close()  # type: ignore
        objects.append(piped_command)

    try:
        # communicate with the last piped object
        cmdout, cmderr = objects[-1].communicate(timeout=timeout)

        for i in range(len(objects) - 1):
            objects[i].wait(timeout=timeout)

    except subprocess.TimeoutExpired:
        for process in objects:
            process.terminate()
        raise

    # collect the return codes
    if check_results:
        for i in range(cmd_no):
            if objects[i].returncode:
                if not quiet and (cmdout or cmderr):
                    print(f"captured stdout: {cmdout.decode('utf-8')}", 'red')
                    print(f"captured stderr: {cmderr.decode('utf-8')}", 'red')
                raise subprocess.CalledProcessError(
                    objects[i].returncode, unpiped_commands[i]
                )

    return cmdout.decode('utf-8'), cmderr.decode('utf-8')


setup(
    name="libmata",
    packages=["libmata", "libmata.nfa"],
    package_dir={'libmata': 'libmata', 'libmata.nfa': os.path.join('libmata', 'nfa')},
    version=get_version(),
    ext_modules=cythonize(
        extensions,
        compiler_directives={'language_level': "3"}
    ),
    description="The automata library",
    author="Lukáš Holík <holik@fit.vutbr.cz>, "
                "Ondřej Lengál <lengal@fit.vutbr.cz>, "
                "Martin Hruška <ihruskam@fit.vutbr.cz>, "
                "Tomáš Fiedor <ifiedortom@fit.vutbr.cz>, "
                "David Chocholatý <chocholaty.david@protonmail.com>, "
                "Juraj Síč <sicjuraj@fit.vutbr.cz>, "
                "Tomáš Vojnar <vojnar@fit.vutbr.cz>",
    author_email="holik@fit.vutbr.cz",
    long_description=README_MD,
    long_description_content_type="text/markdown",
    keywords="automata, finite automata, alternating automata",
    url="https://github.com/verifit/mata",
    cmdclass={
        'sdist': sdist,
        'build_ext': build_ext
    },
    # Requirements from install for pip
    install_requires=[
        'Cython>=0.29.32',
        'tabulate>=0.8.10',
        'ipython>=7.9.0',
        'pandas>=1.3.5',
        'networkx>=2.6.3',
        'graphviz>=0.10.1',
    ]
)
