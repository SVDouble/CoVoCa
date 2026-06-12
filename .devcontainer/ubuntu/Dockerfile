FROM docker.io/ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    clang-format \
    clang-tidy \
    clangd \
    cmake \
    cppcheck \
    curl \
    g++ \
    gcc \
    gdb \
    git \
    libeigen3-dev \
    libopencv-dev \
    libspdlog-dev \
    libyaml-cpp-dev \
    make \
    ninja-build \
    pkg-config \
    python3 \
    texlive-fonts-recommended \
    texlive-latex-extra \
    unzip \
    valgrind \
    zip \
    && rm -rf /var/lib/apt/lists/*

ARG VCPKG_ROOT=/opt/vcpkg
RUN git clone --depth 1 https://github.com/microsoft/vcpkg "${VCPKG_ROOT}" \
    && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics

RUN useradd -ms /bin/bash vscode
RUN chown -R vscode:vscode "${VCPKG_ROOT}"

ENV VCPKG_ROOT="${VCPKG_ROOT}" \
    VCPKG_DISABLE_METRICS=1

USER vscode
RUN curl -LsSf https://astral.sh/uv/install.sh | sh

ENV PATH="/home/vscode/.local/bin:${PATH}"
