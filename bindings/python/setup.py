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

if 'microsoft' in platform.uname().release.lower():
    """Patch for calling the python from WSL. The copystat fails on copied directories"""
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
mata_include_dir = os.path.join(src_dir, "include")
mata_source_dir = os.path.join(src_dir, "src")

# TODO: Refactor this to include more 3rd party release
third_party_include_dir = os.path.join(src_dir, "3rdparty")
re2_include_dir = os.path.join(src_dir, "3rdparty", "re2")
re2_source_dir = os.path.join(src_dir, "3rdparty", "re2")
simlib_include_dir = os.path.join(src_dir, "3rdparty", "simlib", "include")
simlib_source_dir = os.path.join(src_dir, "3rdparty", "simlib", "src")
cudd_include_dir = os.path.join(src_dir, "3rdparty", "cudd-min", "include")
cudd_source_dir = os.path.join(src_dir, "3rdparty", "cudd-min")

with open(os.path.join(src_dir, "README.md")) as readme_handle:
    README_MD = readme_handle.read()


def get_cpp_sources(src_dir):
    """
    Finds all sources that ends either with .cc or .cpp

    :return: list of c++ sources
    """
    sources = []
    for root, _, files in os.walk(src_dir):
        for file in files:
            ext = os.path.splitext(file)[1]
            if not file.startswith(('.', "test")) and ext in ('.cpp', '.cc', '.c'):
                sources.append(os.path.join(root, file))
    return sorted(sources)


extensions = [
    Extension(
        "libmata",
        sources=["libmata.pyx"]
                + get_cpp_sources(mata_source_dir)
                + get_cpp_sources(re2_source_dir)
                + get_cpp_sources(simlib_source_dir)
                + get_cpp_sources(cudd_source_dir),
        include_dirs=[
            mata_include_dir,
            third_party_include_dir,
            re2_include_dir,
            simlib_include_dir,
            cudd_include_dir,
        ],
        language="c++",
        extra_compile_args=["-std=c++17", "-DNO_THROW_DISPATCHER"],
    ),
]


def _copy_sources():
    """Copies sources to specified `mata` folder.

    This is used in `dist` command, to copy sources that will be included in the
    tar.gz and that it will be able to compile on other systems using pip.

    Inspired by z3 setup.py
    """
    shutil.rmtree(sdist_dir, ignore_errors=True)
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
    shutil.copytree(
        os.path.join(project_dir, 'src'),
        os.path.join(sdist_dir, 'src'),
        ignore=shutil.ignore_patterns("*.txt")
    )
    shutil.copytree(
        os.path.join(project_dir, 'include'),
        os.path.join(sdist_dir, 'include'),
        ignore=shutil.ignore_patterns("*.txt")
    )
    shutil.copytree(
        os.path.join(project_dir, '3rdparty'),
        os.path.join(sdist_dir, '3rdparty'),
        ignore=shutil.ignore_patterns("*.txt")
    )
    if version_file_was_created:
        os.remove(version_file)


class sdist(_sdist):
    def run(self):
        self.execute(_copy_sources, (), msg="Copying source files")
        _sdist.run(self)


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
    objects: List[subprocess.Popen] = []
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
    version=get_version(),
    ext_modules=cythonize(extensions),
    description="The automata library",
    author="Lukáš Holík <holik@fit.vutbr.cz>, "
                "Ondřej Lengál <lengal@fit.vutbr.cz>, "
                "Martin Hruška <ihruskam@fit.vutbr.cz>, "
                "Tomáš Fiedor <ifiedortom@fit.vutbr.cz>, "
                "David Chocholatý <chocholaty.david@protonmail.com>, "
                "Juraj Síč <sicjuraj@fit.vutbr.cz>, "
                "Tomáš Vojnar <vojnar@fit.vutbr.cz>",
    author_email="lengal@fit.vutbr.cz",
    long_description=README_MD,
    long_description_content_type="text/markdown",
    keywords="automata, finite automata, alternating automata",
    url="https://github.com/verifit/mata",
    cmdclass={'sdist': sdist},
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
