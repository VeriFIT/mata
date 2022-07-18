import os

from setuptools import setup, Extension
from Cython.Build import cythonize

project_dir = os.path.abspath(os.path.join("..", ".."))
include_dir = os.path.join(project_dir, "include")
third_party_include_dir = os.path.join(project_dir, "3rdparty")
re2_include_dir = os.path.join(project_dir, "3rdparty", "re2")
source_dir = os.path.join(project_dir, "src")
re2_source_dir = os.path.join(project_dir, "3rdparty", "re2")


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
        "mata",
        sources=["mata.pyx"] + get_cpp_sources(source_dir) + get_cpp_sources(re2_source_dir),
        include_dirs=[include_dir, third_party_include_dir, re2_include_dir],
        language="c++",
        extra_compile_args=["-std=c++14", "-DNO_THROW_DISPATCHER"],
    )
]

setup(
    name="mata",
    ext_modules=cythonize(extensions),
    version=0.1
)
