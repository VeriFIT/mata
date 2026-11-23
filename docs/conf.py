# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os

# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = "Mata"
copyright = "2025, Automata@FIT Group, BUT, FIT"
author = "Automata@FIT Group, BUT, FIT"


def get_version():
    """Parses the version of the library from the VERSION file"""
    version_file = os.path.join("..", "VERSION")
    if os.path.exists(version_file):
        with open(version_file, "r") as version_handle:
            return version_handle.read().split()[0]
    else:
        return ""


# The full version, including alpha/beta/rc tags
release = get_version()


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.doctest",
    "sphinx.ext.mathjax",
    "sphinx.ext.viewcode",
    "sphinx.ext.imgmath",
    "sphinx.ext.todo",
    "breathe",
    "myst_parser",
]

source_suffix = {".rst": "restructuredtext", ".md": "markdown"}

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "alabaster"

# Links from the sidebar to the pages built from the same branch.
website_url = os.environ.get("MATA_WEBSITE_URL", "https://verifit.github.io/mata/master/")
html_theme_options = {
    "extra_nav_links": {
        "Mata website": website_url,
        "Playground": f"{website_url}playground/",
    },
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]

breathe_projects = {project: "xml/"}
breathe_default_project = project
myst_heading_anchors = 7
# Symbols render on both the hand-written pages and the generated overview pages, so duplicates are by design.
suppress_warnings = ["duplicate_declaration.cpp", "duplicate_declaration.c"]


def _restore_doxygenfile_path_lookup():
    """Make ``doxygenfile:: nfa/types.hh`` resolve again under Breathe 5.

    Breathe 5 matches the path against the basename-only ``<name>`` from ``index.xml``, so any path with a directory
    never matches (https://github.com/breathe-doc/breathe/issues/1084). Match against the file location, as Breathe 4 did.
    """
    from pathlib import PurePosixPath

    try:
        from breathe import renderer
        from breathe.directives import file as file_directive
        from breathe.parser import DoxCompoundKind
    except ImportError:  # Breathe 4 matches by location already.
        return

    def file_finder_filter(filename, d_parser, project_info, index, matches):
        target = PurePosixPath(filename).parts
        for c in index.file_compounds:
            if PurePosixPath(c.name).name != target[-1]:
                continue
            for cd in d_parser.parse_compound(c.refid, project_info).root.compounddef:
                if (
                    cd.kind == DoxCompoundKind.file
                    and cd.location is not None
                    and PurePosixPath(cd.location.file).parts[-len(target) :] == target
                ):
                    matches.append([renderer.TaggedNode(None, cd)])

    file_directive.file_finder_filter = file_finder_filter


_restore_doxygenfile_path_lookup()
