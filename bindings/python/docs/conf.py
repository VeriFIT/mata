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
html_static_path = ["_static"]
html_theme = "alabaster"

# Links from the sidebar to the pages built from the same branch.
website_url = os.environ.get("MATA_WEBSITE_URL", "https://verifit.github.io/mata/master/")
html_theme_options = {
    "extra_nav_links": {
        "Mata website": website_url,
        "All documentation": f"{website_url}docs/",
        "Playground": f"{website_url}playground/",
    },
}
