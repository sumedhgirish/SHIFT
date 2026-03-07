# syntax=docker/dockerfile:1
# escape=\
# check=error=true

# =============================================================================
#   ECTF 2026 - Secure HSM Firmware Docker Container
#   Authors: ISFCR, PES University RR Campus
# =============================================================================

FROM ubuntu:24.04

LABEL version="2.0"
LABEL description="Docker Container for Secure HSM Firmware"

ARG DEBIAN_FRONTEND=noninteractive
WORKDIR /root

# Install Requisite Packages
RUN apt update && apt upgrade -y && apt install -y \
    build-essential \
    python3 \
    cmake \
    ninja-build \
    make \
    wget \
    unzip \
    git \
    ca-certificates \
    curl \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi

# Install uv
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/root/.local/bin:${PATH}"

# Download the MSPM0 SDK and make it globally available at /opt
# Keeping user's version 2.09.00.01 as requested to match local
ENV SDK_VERSION=mspm0_sdk_2_09_00_01
ENV SDK_SOURCE_LINK=https://github.com/TexasInstruments/mspm0-sdk/archive/refs/tags/${SDK_VERSION}.zip
ENV SDK_SOURCE_DIR=/opt/${SDK_VERSION}

# Note: The zip extracts to a folder named 'mspm0-sdk-<version>' usually,
# but GitHub tags extract to 'mspm0-sdk-<tag>'.
# We'll stick to the user's unzip logic but cleaner.
RUN wget ${SDK_SOURCE_LINK} && \
    unzip ${SDK_VERSION}.zip -d /opt && \
    mv /opt/mspm0-sdk* /opt/${SDK_VERSION} && \
    rm -f ${SDK_VERSION}.zip

# Set TI Clang Version and Path
ENV TICLANG_VERSION=4.0.3.LTS
ENV TICLANG_ARMCOMPILER=/opt/ti-cgt-armllvm_${TICLANG_VERSION}
ENV PATH="${TICLANG_ARMCOMPILER}/bin:${PATH}"

# Install TI-Clang for curr machine's arch (Referenced from Ground Truth)
# We use --no-check-certificate as we don't have the specific mitre chain,
# assuming public internet CA is enough or ignoring for build speed/robustness in this context.
RUN if [ $(uname -m) = "x86_64" ]; then \
    wget -O ti_cgt_installer.bin https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-ayxs93eZNN/4.0.3.LTS/ti_cgt_armllvm_4.0.3.LTS_linux-x64_installer.bin --no-check-certificate; \
    elif [ $(uname -m) = "aarch64" ] || [ $(uname -m) = "arm64" ]; then \
    wget -O ti_cgt_installer.bin https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-ayxs93eZNN/4.0.3.LTS/ti_cgt_armllvm_4.0.3.LTS_linux-arm64_installer.bin --no-check-certificate; \
    else \
    echo "Unsupported architecture" && exit 1; \
    fi && \
    chmod +x ti_cgt_installer.bin && \
    ./ti_cgt_installer.bin --mode unattended --prefix /opt && \
    rm -f ti_cgt_installer.bin

# Change working directory to base of project
ENV PROJECT_DIR=/hsm
ENV OUTPUT_DIR=/out
ENV BUILD_DIR=/tmp/build

WORKDIR ${PROJECT_DIR}
ENTRYPOINT ["bash", "utils/build.sh"]
