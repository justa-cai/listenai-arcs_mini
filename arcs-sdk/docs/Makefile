# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
#SPHINXBUILD   ?= sphinx-build
SPHINXBUILD   ?= python3 -m sphinx.cmd.build
#CMAKE_RST_GEN ?= python3 ../scripts/generate_cmake_rst.py --skip_private --skip_undocumented --headline="SDK CMake API Reference" ../cmake

all: html

#cmake_intro:
#	@$(CMAKE_RST_GEN) -o cmake_intro.rst

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
en: Makefile
	@$(SPHINXBUILD) -M html "en" "output/en" $(SPHINXOPTS) $(O)

zh: Makefile
	@$(SPHINXBUILD) -M html "zh" "output/zh" $(SPHINXOPTS) $(O)

html: zh
.PHONY: all zh html clean

clean:
	@rm -rf output
	@rm -rf zh/samples
	@rm -rf zh/components
	@rm -rf zh/_static
	@rm -rf zh/drivers
	@rm -rf zh/modules
	@rm -rf zh/boards
	@rm -rf zh/CHANGELOG.md
	@rm -rf zh/tools
	@rm -rf zh/demos
