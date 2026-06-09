FROM docker.io/ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    clang-format \
    clangd \
    cmake \
    g++ \
    gcc \
    gdb \
    git \
    libeigen3-dev \
    make \
    ninja-build \
    texlive-fonts-recommended \
    texlive-latex-extra \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -ms /bin/bash vscode
USER vscode
