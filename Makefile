PROJECT_PRESET ?= debug
PROJECT_BUILD_PRESET ?= debug-voxel-demo
PYTHON_TOOLS_DIR := gsplat_toolkit
CXX_FORMAT_FILES := $(shell find apps include src -type f \( -name '*.cpp' -o -name '*.h' \) 2>/dev/null | sort)
CXX_LINT_FILES := $(shell find apps src -type f -name '*.cpp' 2>/dev/null | sort)

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

.PHONY: all project configure build release run format format-check lint static-analysis sanitize memcheck quality python-format python-format-check python-lint python-check pre-commit pre-commit-install python-tools-build download-sample-data docs clean clean-project clean-docs help

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

format:
	clang-format -i $(CXX_FORMAT_FILES)

format-check:
	clang-format --dry-run --Werror $(CXX_FORMAT_FILES)

lint:
	cmake --preset debug
	clang-tidy --quiet -p build/debug $(CXX_LINT_FILES)

static-analysis:
	cmake --preset debug
	cppcheck --project=build/debug/compile_commands.json \
		--enable=warning,style,performance,portability \
		--inline-suppr \
		--suppress=missingIncludeSystem \
		--error-exitcode=1

sanitize:
	cmake --preset debug-asan
	cmake --build --preset debug-asan
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/debug-asan/voxel_demo

memcheck: build
	valgrind --leak-check=full --show-leak-kinds=definite,indirect --error-exitcode=1 ./build/debug/voxel_demo

quality: format-check lint static-analysis python-check sanitize memcheck

python-format:
	uv run --project $(PYTHON_TOOLS_DIR) --group dev --python 3.14 ruff format $(PYTHON_TOOLS_DIR)/src

python-format-check:
	uv run --project $(PYTHON_TOOLS_DIR) --group dev --python 3.14 ruff format --check $(PYTHON_TOOLS_DIR)/src

python-lint:
	uv run --project $(PYTHON_TOOLS_DIR) --group dev --python 3.14 ruff check $(PYTHON_TOOLS_DIR)/src

python-check: python-format-check python-lint

pre-commit:
	uv run --project $(PYTHON_TOOLS_DIR) --group dev --python 3.14 pre-commit run --all-files

pre-commit-install:
	uv run --project $(PYTHON_TOOLS_DIR) --group dev --python 3.14 pre-commit install

python-tools-build:
	uv build --project $(PYTHON_TOOLS_DIR)

download-sample-data:
	uv run --project $(PYTHON_TOOLS_DIR) --python 3.14 download-aruco-sample

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
	rm -rf build/debug build/debug-asan build/debug-tidy build/debug-vcpkg build/release build/release-vcpkg

clean-docs:
	rm -rf build/docs

help:
	@echo "Targets:"
	@echo "  make project       Configure and build the default debug project target"
	@echo "  make release       Configure and build the release project target"
	@echo "  make run           Build and run ./build/debug/voxel_demo"
	@echo "  make format        Format C++ sources with clang-format"
	@echo "  make format-check  Verify C++ formatting"
	@echo "  make lint          Run clang-tidy over C++ sources"
	@echo "  make static-analysis"
	@echo "                     Run cppcheck over the CMake compile database"
	@echo "  make sanitize      Build and smoke-test with ASan/LSan/UBSan"
	@echo "  make memcheck      Run Valgrind leak checks"
	@echo "  make quality       Run format, lint, static analysis, Python checks, sanitizers, and memcheck"
	@echo "  make python-format Format Python helper sources with Ruff"
	@echo "  make python-check  Run Ruff formatting and lint checks"
	@echo "  make pre-commit    Run formatting pre-commit hooks on all files"
	@echo "  make pre-commit-install"
	@echo "                     Install formatting hooks into .git/hooks"
	@echo "  make python-tools-build"
	@echo "                     Build the gsplat-toolkit Python package with uv"
	@echo "  make download-sample-data"
	@echo "                     Download public ArUco sample images into data/sample_aruco"
	@echo "  make docs          Build the proposal PDF at $(DOC_PDF)"
	@echo "  make all           Build project and docs"
	@echo "  make clean         Remove project and docs build outputs"
