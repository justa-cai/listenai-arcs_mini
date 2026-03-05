"""
Source Link Extension
######################

This Sphinx extension automatically adds source code repository links to documentation pages.
It's especially useful for sample documentation, allowing readers to easily locate the source
code in the repository.

Features
========

- Automatically injects a "View Source Code" link at the top of specified documents
- Supports GitHub, GitLab, and local path displays
- Configurable patterns to match which documents should get source links
- Minimal configuration required

Configuration options
=====================

- ``source_link_base_url``: Base URL for the source repository (e.g.,
  'https://github.com/your-org/repo/tree/master'). If not set, only shows local paths.
- ``source_link_patterns``: List of glob patterns for files that should have source links
  (defaults to ['samples/**/*.md', 'samples/**/*.rst'])
- ``source_link_show_local_path``: Whether to show the local file path (default: True)
- ``source_link_label``: Label text for the link (default: '📁 源码位置')
"""

import os
from pathlib import Path
from typing import Dict, Any, List
from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx.application import Sphinx
from sphinx.util.docutils import SphinxDirective


__version__ = "0.1.0"


def get_source_path(app: Sphinx, docname: str) -> str:
    """
    Get the source file path relative to SDK_BASE.

    Args:
        app: Sphinx application instance
        docname: Document name (without extension)

    Returns:
        Relative path from SDK_BASE to the source file, or empty string if not applicable
    """
    srcdir = Path(app.srcdir).resolve()
    sdk_base = Path(os.environ.get("SDK_BASE", srcdir.parent)).resolve()

    # Try to find the actual source file (.md or .rst)
    for ext in ['.md', '.rst']:
        source_file = srcdir / f"{docname}{ext}"
        if source_file.exists():
            try:
                rel_path = source_file.relative_to(srcdir)
                # Get directory path (remove filename if it's README.md or similar)
                if source_file.name.lower() in ['readme.md', 'readme.rst', 'index.md', 'index.rst']:
                    return str(rel_path.parent)
                else:
                    return str(rel_path.parent)
            except ValueError:
                pass

    return ""


def should_add_source_link(app: Sphinx, docname: str) -> bool:
    """
    Check if a document should have a source link based on configured patterns.

    Args:
        app: Sphinx application instance
        docname: Document name

    Returns:
        True if the document matches any of the configured patterns
    """
    patterns = app.config.source_link_patterns

    for pattern in patterns:
        # Convert glob pattern to simple matching (could be enhanced with fnmatch)
        if pattern.endswith('**/*.md') or pattern.endswith('**/*.rst'):
            prefix = pattern.rsplit('/', 1)[0].replace('**', '')
            if docname.startswith(prefix):
                return True
        elif pattern in docname:
            return True

    return False


def get_version_branch(app: Sphinx) -> str:
    """
    Get the Git branch name based on the current documentation version.

    Args:
        app: Sphinx application instance

    Returns:
        Branch name to use in repository URLs
    """
    # Get current version from html_context
    current_version = app.config.html_context.get('current_version', 'latest')

    # Get version-to-branch mapping from config
    version_branch_map = getattr(app.config, 'source_link_version_branches', {})

    # Return mapped branch, or construct default branch name
    if current_version in version_branch_map:
        return version_branch_map[current_version]
    elif current_version == 'latest':
        return 'master'
    elif current_version.startswith('v'):
        # Convert v0.1.0 to release/v0.1.0
        return f"release/{current_version}"
    else:
        return 'master'


def create_source_link_node(app: Sphinx, source_path: str) -> nodes.container:
    """
    Create a docutils node containing the source link information.

    Args:
        app: Sphinx application instance
        source_path: Relative path to the source directory

    Returns:
        A docutils container node with the source link
    """
    container = nodes.container()
    container['classes'].append('source-link-box')

    paragraph = nodes.paragraph()

    # Add emoji/icon
    icon = nodes.inline(text=app.config.source_link_label.split()[0] + ' ')
    icon['classes'].append('source-link-icon')
    paragraph += icon

    # Add label
    label_text = ' '.join(app.config.source_link_label.split()[1:]) + ': '
    label = nodes.strong(text=label_text)
    paragraph += label

    # Add local path
    if app.config.source_link_show_local_path:
        path_text = nodes.literal(text=source_path)
        path_text['classes'].append('source-link-path')
        paragraph += path_text

    # Add repository link if configured
    if app.config.source_link_base_url:
        if app.config.source_link_show_local_path:
            paragraph += nodes.Text(' ')

        # Get version-specific branch
        branch = get_version_branch(app)

        # Create clickable link with version-specific branch
        base_url = app.config.source_link_base_url.rstrip('/')
        # Replace the branch/tag in the URL
        # Handle both GitHub style (/tree/BRANCH) and GitLab style (/-/tree/BRANCH)
        if '/-/tree/' in base_url:
            # GitLab style
            base_parts = base_url.rsplit('/-/tree/', 1)
            ref_url = f"{base_parts[0]}/-/tree/{branch}/{source_path}"
        elif '/tree/' in base_url:
            # GitHub style
            base_parts = base_url.rsplit('/tree/', 1)
            ref_url = f"{base_parts[0]}/tree/{branch}/{source_path}"
        else:
            # Fallback: append branch and path
            ref_url = f"{base_url}/{branch}/{source_path}"

        reference = nodes.reference('', '查看源码', refuri=ref_url)
        reference['classes'].append('source-link-button')
        paragraph += reference

    container += paragraph

    return container


def inject_source_link(app: Sphinx, doctree, docname: str) -> None:
    """
    Inject source link at the beginning of the document.

    Args:
        app: Sphinx application instance
        doctree: Document tree
        docname: Document name
    """
    if not should_add_source_link(app, docname):
        return

    source_path = get_source_path(app, docname)
    if not source_path:
        return

    # Create the source link node
    source_link_node = create_source_link_node(app, source_path)

    # Insert at the beginning of the document
    # Find the first section or insert at the very beginning
    if len(doctree.children) > 0:
        # Insert after the title but before the first section content
        for i, child in enumerate(doctree.children):
            if isinstance(child, nodes.section) and len(child.children) > 0:
                # Insert after the section title
                child.insert(1, source_link_node)
                break
        else:
            # No section found, insert at the beginning
            doctree.insert(0, source_link_node)


def add_source_link_css(app: Sphinx, config) -> None:
    """
    Add CSS for source link styling.

    Args:
        app: Sphinx application instance
        config: Sphinx config
    """
    # CSS will be added via static files
    app.add_css_file('source-link.css')


def setup(app: Sphinx) -> Dict[str, Any]:
    """
    Setup function for the Sphinx extension.

    Args:
        app: Sphinx application instance

    Returns:
        Extension metadata
    """
    # Add configuration values
    app.add_config_value("source_link_base_url", "", "html")
    app.add_config_value("source_link_patterns", ["samples/**/*.md", "samples/**/*.rst"], "html")
    app.add_config_value("source_link_show_local_path", True, "html")
    app.add_config_value("source_link_label", "📁 源码位置", "html")
    app.add_config_value("source_link_version_branches", {}, "html")

    # Connect event handlers
    app.connect("doctree-resolved", inject_source_link)
    app.connect("config-inited", add_source_link_css)

    return {
        "version": __version__,
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
