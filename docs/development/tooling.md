# Development Tooling

Use the devcontainer unless you already have the C++ and Python tools locally.

## Tools

- CMake + Ninja
- Eigen, OpenCV, spdlog, yaml-cpp
- CLI11 and reflect-cpp, resolved by CMake or vcpkg
- clang-format
- clang-tidy
- cppcheck
- ASan/LSan/UBSan
- Valgrind
- uv, Ruff, mdformat, pre-commit

The container uses Ubuntu packages for OpenCV to avoid long local builds. Native
vcpkg users can use `debug-vcpkg` or `release-vcpkg`.

## Commands

```bash
make format-check
make lint
make static-analysis
make python-check
make sanitize
make memcheck
make pre-commit
```

Full local gate:

```bash
make quality
```

Install hooks:

```bash
make pre-commit-install
```

## C++ Dependencies

Container packages:

- `libeigen3-dev`
- `libopencv-dev`
- `libspdlog-dev`
- `libyaml-cpp-dev`

Native vcpkg:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset debug-vcpkg
cmake --build --preset debug-vcpkg
```

Do not commit `build/`, `vcpkg_installed/`, cache directories, or local CMake
user presets.
