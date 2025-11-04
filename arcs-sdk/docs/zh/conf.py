# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import os
import os.path
import subprocess
import shutil
import re
import sys
import datetime
from pathlib import Path
import sphinx_rtd_theme

SDK_BASE = Path(__file__).resolve().parents[2]
HTML_STATIC_DIR = Path(__file__).resolve().parents[0] / "_static"
DOXY_OUT = Path(__file__).resolve().parents[0] / "_static" / "api_doc"

sys.path.insert(0, str(SDK_BASE / "docs" / "_ext"))

os.environ["SDK_BASE"] = str(SDK_BASE)
project = 'Arcs Software Development Kit'
copyright = '2020-%s, LISTENAI' % datetime.date.today().year
author = '聆思科技固件组'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinx_rtd_theme',
    'myst_parser',
    "sphinx.ext.todo",
    "sphinx.ext.extlinks",
    'sphinx.ext.duration',
    "sphinx.ext.viewcode",
    'sphinxcontrib.moderncmakedomain',
    "external_content",
    "doxyrunner",
    "sphinx_tabs.tabs"
]

templates_path = ['_templates']

DOXY_OUT.mkdir(parents = True, exist_ok = True)
doxyrunner_doxygen = os.environ.get("DOXYGEN_EXECUTABLE", "doxygen")
doxyrunner_doxyfile = SDK_BASE / "docs" / "doxygen" / "Doxyfile" 
doxyrunner_outdir = DOXY_OUT
doxyrunner_fmt = True
doxyrunner_fmt_vars = {"SDK_BASE": str(SDK_BASE)}
doxyrunner_outdir_var = "DOXYGEN_OUTPUT_DIR"

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []
html_extra_path = []

source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

external_content_contents = [
    (SDK_BASE, "docs/*.rst"),
    (SDK_BASE / "docs/zh", "[!_]*"),
    (SDK_BASE, "components/**/*_zh.rst"),
    (SDK_BASE, "components/**/README.md"),
    (SDK_BASE, "samples/**/*_zh.rst",),
    (SDK_BASE, "samples/**/README.md"),
]


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    "logo_only": True,
    "prev_next_buttons_location": None
}
html_show_sphinx = False
html_logo = r'../assets/logo.svg'
html_static_path = [str(HTML_STATIC_DIR)]
html_last_updated_fmt = "%b %d, %Y"
html_domain_indices = False
html_split_index = True
html_show_sphinx = False

suppress_warnings = ['toc.excluded',
                    'toc.not_readable',
                    'toc.secnum','toc.circular','epub.duplicated_toc_entry','autosectionlabel.*',
                    'app.add_source_parser']

myst_heading_anchors = 2

myst_enable_extensions = [
    "amsmath",
    "colon_fence",
    "deflist",
    "dollarmath",
    "fieldlist",
    "html_admonition",
    "html_image",
    "replacements",
    "smartquotes",
    "strikethrough",
    "substitution",
    "tasklist",
]

