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
HTML_ASSETS_DIR = Path(__file__).resolve().parents[1] / "assets"
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
    "sphinx_tabs.tabs",
    "source_link"  # 自动添加源码链接
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
exclude_patterns = [
    # 示例章节文件（sample_build.rst, sample_flash.rst）仅用于 include，不作为独立页面
    "sample_build.rst",
    "sample_flash.rst",
]
html_extra_path = []

source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

external_content_contents = [
    (SDK_BASE, "docs/*.rst"),
    (SDK_BASE, "CHANGELOG.md"),
    (SDK_BASE / "docs/zh", "[!_]*"),
    (SDK_BASE, "drivers/**/*.rst"),
    (SDK_BASE, "drivers/**/*.md"),
    (SDK_BASE, "boards/*.rst"),
    (SDK_BASE, "boards/**/*.md"),
    (SDK_BASE, "components/**/*.rst"),
    (SDK_BASE, "components/**/*.md"),
    (SDK_BASE, "samples/**/*.rst"),
    (SDK_BASE, "samples/**/*.md"),
    (SDK_BASE, "demos/**/*.rst"),
    (SDK_BASE, "demos/**/*.md"),
    (SDK_BASE, "tools/**/*.rst"),
    (SDK_BASE, "tools/**/*.md"),
    ## modules目录下存在很多不规范的README.md，指定添加构建目录
    (SDK_BASE, "modules/fs/README.md"),
    (SDK_BASE, "modules/lisa_shell/README.md"),
    (SDK_BASE, "modules/usb/device/uvc/README.md"),
]

external_content_exclude = [
    "drivers/porting_docs/**/*.md",
    "drivers/lisa_device/**/*.md",
    "drivers/Devices_Docs_Spec.md",
    "samples/drivers/hal/**/README.md",
    "samples/Samples_Spec.md",
    "samples/drivers/devices/Devices_Samples_Spec.md",
    "samples/modules/sqlite3/README.md",
    "components/acomp/logger/**/README.md",
    "demos/face_detect/src/button/FlexibleButton/README.md",
]

# Keep template files from being deleted by external_content
# Note: CSS/JS files are in docs/assets/ which is outside the source dir
external_content_keep = [
    "_templates/*",
    "_templates/**/*",
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
html_static_path = [str(HTML_STATIC_DIR), str(HTML_ASSETS_DIR)]
html_last_updated_fmt = "%b %d, %Y"
html_domain_indices = False
html_split_index = True
html_show_sphinx = False

# Version switcher configuration
html_context = {
    # 当前版本
    'current_version': 'latest',
    # 版本列表
    'versions': [
        ('latest', 'https://docs2.listenai.com/arcs-sdk/latest/zh/html/index.html'),
        ('v0.1.0', 'https://docs2.listenai.com/arcs-sdk/v0.1.0/zh/html/index.html'),
        ('v0.1.1', 'https://docs2.listenai.com/arcs-sdk/v0.1.1/zh/html/index.html'),
        ('v0.1.2', 'https://docs2.listenai.com/arcs-sdk/v0.1.2/zh/html/index.html'),
    ],
    # 显示版本警告横幅(可选)
    'display_github': False,
}

# Add custom CSS and JS files
html_css_files = [
    'version-switcher.css',
    'feedback-widget.css',
]

html_js_files = [
    'version-switcher.js',
    'feedback-widget.js',
]

suppress_warnings = ['toc.excluded',
                    'toc.not_readable',
                    'toc.not_included',  # 允许 sample_build.rst、sample_flash.rst 等被 include 的文件不在 toctree 中
                    'toc.secnum','toc.circular','epub.duplicated_toc_entry','autosectionlabel.*',
                    'app.add_source_parser',
                    'myst.header',
                    'ref.doc',
                    'ref.ref',
                    'myst.domains',
                    'myst.xref_missing']

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

def setup(app):
    """Sphinx setup hook to add custom HTML for version switcher"""
    def add_version_switcher(app, pagename, templatename, context, doctree):
        """Add version switcher HTML to every page"""
        versions = context.get('versions', [])
        current_version = context.get('current_version', '')

        if versions:
            version_html = '''
<div class="rst-versions" data-toggle="rst-versions" role="note">
  <span class="rst-current-version" data-toggle="rst-current-version">
    <span class="fa fa-book"> 文档版本 </span>
    <span class="current-version-label">v: {}</span>
    <span class="fa fa-caret-down"></span>
  </span>
  <div class="rst-other-versions">
    <dl>
      <dt>版本</dt>
'''.format(current_version)

            for version_name, version_url in versions:
                if version_name == current_version:
                    version_html += f'      <dd><strong>{version_name}</strong></dd>\n'
                else:
                    version_html += f'      <dd><a href="{version_url}">{version_name}</a></dd>\n'

            version_html += '''    </dl>
  </div>
</div>
'''
            context['version_switcher_html'] = version_html

    app.connect('html-page-context', add_version_switcher)


# -- Source Link Configuration -----------------------------------------------
# 配置源码链接扩展

# 仓库基础 URL (仅配置到 /-/tree/ 之前的部分,分支名会自动根据版本添加)
# 支持 GitHub/GitLab 或内部 Git 仓库
# 例如: 'https://github.com/listenai/arcs-sdk' (会自动添加 /tree/分支名)
#      'https://cloud.listenai.com/CSKG836746/arcs-sdk/public/arcs-sdk' (会自动添加 /-/tree/分支名)
source_link_base_url = os.environ.get(
    "SOURCE_LINK_BASE_URL",
    "https://cloud.listenai.com/CSKG836746/arcs-sdk/public/arcs-sdk/-/tree/master"
)

# 文档版本到 Git 分支的映射
# 如果版本不在映射中,会使用以下规则:
# - 'latest' -> 'master'
# - 'v0.1.0' -> 'release/v0.1.0'
source_link_version_branches = {
    'latest': 'master',
    'v0.1.0': 'release/v0.1.0',
    'v0.1.1': 'release/v0.1.1',
    # 添加更多版本映射...
}

# 哪些文档需要添加源码链接 (支持通配符)
source_link_patterns = [
    "samples/**/*.md",
    "samples/**/*.rst",
    "demos/**/*.md",
    "demos/**/*.rst",
]

# 是否显示本地路径
source_link_show_local_path = True

# 源码位置标签文本
source_link_label = "📁 源码位置"

