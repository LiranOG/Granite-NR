# Configuration file for the Sphinx documentation builder.

project = "GRANITE"
copyright = "2026, Liran M. Schwartz"
author = "Liran M. Schwartz"
release = "0.6.7"

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.mathjax",
    "sphinx.ext.viewcode",
    "sphinx.ext.napoleon",
    "sphinx.ext.intersphinx",
    "sphinxcontrib.bibtex",
    "breathe",  # C++ API from Doxygen
]

bibtex_bibfiles = ['citation.bib']

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

# -- Breathe configuration (C++ API) -----------------------------------------

breathe_projects = {"granite": "../build/xml/"}
breathed_default_project = "granite"

# -- Options for HTML output --------------------------------------------------

html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]
html_logo = "_static/granite_logo.svg"
html_favicon = "_static/favicon.ico"

html_theme_options = {
    "logo_only": True,
    "display_version": True,
    "prev_next_buttons_location": "bottom",
    "style_nav_header_background": "#7c3aed",
}

# -- MathJax configuration ---------------------------------------------------

mathjax3_config = {
    "tex": {
        "macros": {
            "dd": r"\mathrm{d}",
            "pde": [r"\frac{\partial #1}{\partial #2}", 2],
            "Diff": [r"D_{#1}", 1],
        }
    }
}

# -- Intersphinx mapping ------------------------------------------------------

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "h5py": ("https://docs.h5py.org/en/stable/", None),
}
