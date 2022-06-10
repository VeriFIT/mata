from setuptools import setup, Extension

setup(
    ext_modules = [
        Extension(
            "pynfa", 
            sources=["pynfa.pyx", "nfa.cpp"], 
            language="c++", 
            extra_compile_args=["-std=c++11"],
        )
    ]
)
