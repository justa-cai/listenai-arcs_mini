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
GENERATED_ZH_PATHS := \
	zh/_static \
	zh/boards \
	zh/components \
	zh/demos \
	zh/drivers \
	zh/modules \
	zh/samples \
	zh/system \
	zh/tools \
	zh/CHANGELOG.md

.PHONY: all zh html clean

clean:
	@rm -rf output build $(GENERATED_ZH_PATHS)
	@if [ -d zh/labs ]; then find zh/labs -mindepth 1 -maxdepth 1 ! -name index.rst -exec rm -rf {} +; fi
	@find . -type d -name __pycache__ -prune -exec rm -rf {} +
