# Sphinx configuration of the reference of the Python bindings. Autodoc imports 'libmata', so build the bindings
#  in place first (`just bindings python build`).

import os
import sys

sys.path.insert(0, os.path.abspath(".."))
# Document the compiled modules, not their '.pyi' stubs: executing a stub in place of its module breaks the circular
#  imports between the Cython modules, and the modules carry more docstrings.
os.environ["SPHINX_AUTODOC_IGNORE_NATIVE_MODULE_TYPE_STUBS"] = "1"

project = "libmata"
project_copyright = "Mata authors"
extensions = ["sphinx.ext.autodoc"]
autodoc_default_options = {
    "members": True,
    "undoc-members": True,
    "show-inheritance": True,
}
exclude_patterns = ["_build"]
templates_path = ["_templates"]
html_theme = "furo"

# Links from the sidebar to the pages built from the same branch. Furo has no extra_nav_links: the links come from
#  the custom sidebar template 'links.html', listed in 'html_sidebars' below.
website_url = os.environ.get("MATA_WEBSITE_URL", "https://verifit.github.io/mata/master/")
html_context = {"website_url": website_url}
html_sidebars = {
    "**": [
        "sidebar/brand.html",
        "sidebar/search.html",
        "sidebar/scroll-start.html",
        "sidebar/navigation.html",
        "links.html",
        "sidebar/scroll-end.html",
    ],
}
