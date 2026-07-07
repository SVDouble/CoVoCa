FROM docker.io/ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    clang-format \
    cmake \
    curl \
    g++ \
    git \
    libeigen3-dev \
    libglib2.0-0 \
    libgl1 \
    libopencv-contrib-dev \
    libopencv-dev \
    libyaml-cpp-dev \
    ninja-build \
    python3 \
    python3-venv \
    sudo \
    unzip \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -ms /bin/bash vscode \
    && echo "vscode ALL=(ALL) NOPASSWD:ALL" >/etc/sudoers.d/vscode \
    && chmod 0440 /etc/sudoers.d/vscode

USER vscode
RUN curl -LsSf https://astral.sh/uv/install.sh | sh

ENV PATH="/home/vscode/.local/bin:${PATH}"
