# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information
import sys
import os
from pathlib import Path

LSCHAT_BASE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(LSCHAT_BASE / "docs" / "source" / "_extensions"))

project = 'lschat'
copyright = '2024, ListenAI'
author = 'ListenAI'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
  "breathe",
  "sphinx_rtd_theme",
  'sphinx_tabs.tabs',
  'external_content',
]

templates_path = ['_templates']
exclude_patterns = []

breathe_projects = {
  "lschat": "doxygen/xml",
}
breathe_default_project = "lschat"
breathe_domain_by_extension = {
    "h": "c",
    "c": "c",
}
breathe_show_enumvalue_initializer = True
breathe_default_members = ("members", )

language = 'zh_CN'

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_logo = "_static/img/logo.svg"
html_theme_options = {
    "logo_only": False,
    "prev_next_buttons_location": None
}
html_domain_indices = False
html_split_index = True
html_show_sourcelink = False
html_show_sphinx = False
html_theme = "sphinx_rtd_theme"
html_static_path = ['_static']


external_content_contents = [
    (LSCHAT_BASE / "doc", "[!_]*"),
    (LSCHAT_BASE, "samples/**/*.rst"),
]
