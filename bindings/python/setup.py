import os
import errno
import shutil
import platform

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
cudd_include_dir = os.path.join(src_dir, "3rdparty", "cudd-min")
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
        extra_compile_args=["-std=c++14", "-DNO_THROW_DISPATCHER"],
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

    shutil.copy(os.path.join(project_dir, 'VERSION'), sdist_dir)
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


class sdist(_sdist):
    def run(self):
        self.execute(_copy_sources, (), msg="Copying source files")
        _sdist.run(self)


def get_version():
    """Parses the version of the library from the VERSION file"""
    version_file = os.path.join(src_dir, "VERSION")
    if os.path.exists(version_file):
        with open(version_file, 'r') as version_handle:
            return version_handle.read().split()[0]
    else:
        return ""


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
