import os

from setuptools import setup, Extension
from Cython.Build import cythonize

project_dir = os.path.abspath(os.path.join("..", ".."))
include_dir = os.path.join(project_dir, "include")
source_dir = os.path.join(project_dir, "src")

third_party_include_dir = os.path.join(project_dir, "3rdparty")
re2_include_dir = os.path.join(project_dir, "3rdparty", "re2")
re2_source_dir = os.path.join(project_dir, "3rdparty", "re2")
simlib_include_dir = os.path.join(project_dir, "3rdparty", "simlib", "include")
simlib_source_dir = os.path.join(project_dir, "3rdparty", "simlib", "src")

with open(os.path.join(project_dir, "README.md")) as readme_handle:
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
            if not file.startswith(('.', "test")) and ext in ('.cpp', '.cc'):
                sources.append(os.path.join(root, file))
    return sorted(sources)


extensions = [
    Extension(
        "libmata",
        sources=["libmata.pyx"]
                + get_cpp_sources(source_dir)
                + get_cpp_sources(re2_source_dir)
                + get_cpp_sources(simlib_source_dir),
        include_dirs=[include_dir, third_party_include_dir, re2_include_dir, simlib_include_dir],
        language="c++",
        extra_compile_args=["-std=c++14", "-DNO_THROW_DISPATCHER"],
    ),
]


def get_version():
    """Parses the version of the library from the VERSION file"""
    version_file = os.path.join("..", "..", "VERSION")
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
)
