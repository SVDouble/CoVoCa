PROJECT_PRESET ?= debug
PROJECT_BUILD_PRESET ?= debug-voxel-demo

DOC_DIR := docs/proposal
DOC_MAIN := main
DOC_BUILD_DIR := build/docs/proposal
DOC_PDF := $(DOC_BUILD_DIR)/$(DOC_MAIN).pdf

DOC_SOURCES := \
	$(DOC_DIR)/main.tex \
	$(DOC_DIR)/main.bib \
	$(DOC_DIR)/cvpr.sty \
	$(DOC_DIR)/ieeenat_fullname.bst \
	$(wildcard $(DOC_DIR)/sec/*.tex)

DOC_ENV := \
	TEXINPUTS="$(abspath $(DOC_DIR))//:" \
	BIBINPUTS="$(abspath $(DOC_DIR))//:" \
	BSTINPUTS="$(abspath $(DOC_DIR))//:"

.PHONY: all project configure build release run docs clean clean-project clean-docs help

all: project docs

project: build

configure:
	cmake --preset $(PROJECT_PRESET)

build: configure
	cmake --build --preset $(PROJECT_BUILD_PRESET)

release:
	cmake --preset release
	cmake --build --preset release-voxel-demo

run: build
	./build/debug/voxel_demo

docs: $(DOC_PDF)

$(DOC_PDF): $(DOC_SOURCES)
	mkdir -p $(DOC_BUILD_DIR)
	$(DOC_ENV) pdflatex -interaction=nonstopmode -halt-on-error \
		-output-directory="$(abspath $(DOC_BUILD_DIR))" \
		"$(abspath $(DOC_DIR))/$(DOC_MAIN).tex"
	cd $(DOC_BUILD_DIR) && $(DOC_ENV) bibtex $(DOC_MAIN)
	$(DOC_ENV) pdflatex -interaction=nonstopmode -halt-on-error \
		-output-directory="$(abspath $(DOC_BUILD_DIR))" \
		"$(abspath $(DOC_DIR))/$(DOC_MAIN).tex"
	$(DOC_ENV) pdflatex -interaction=nonstopmode -halt-on-error \
		-output-directory="$(abspath $(DOC_BUILD_DIR))" \
		"$(abspath $(DOC_DIR))/$(DOC_MAIN).tex"
	$(DOC_ENV) pdflatex -interaction=nonstopmode -halt-on-error \
		-output-directory="$(abspath $(DOC_BUILD_DIR))" \
		"$(abspath $(DOC_DIR))/$(DOC_MAIN).tex"
	@echo "Built $(DOC_PDF)"

clean: clean-project clean-docs

clean-project:
	rm -rf build/debug build/release

clean-docs:
	rm -rf build/docs

help:
	@echo "Targets:"
	@echo "  make project       Configure and build the default debug project target"
	@echo "  make release       Configure and build the release project target"
	@echo "  make run           Build and run ./build/debug/voxel_demo"
	@echo "  make docs          Build the proposal PDF at $(DOC_PDF)"
	@echo "  make all           Build project and docs"
	@echo "  make clean         Remove project and docs build outputs"
