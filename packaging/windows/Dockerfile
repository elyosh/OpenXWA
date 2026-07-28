# Microsoft's Linux DXC release binaries are available for x86-64 only.
ARG XWA_BUILD_PLATFORM=linux/amd64

FROM --platform=${XWA_BUILD_PLATFORM} debian:bookworm-slim AS build-base

RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mingw-w64-x86-64 \
        build-essential \
        ca-certificates \
        cmake \
        g++-mingw-w64-x86-64-posix \
        gcc-mingw-w64-x86-64-posix \
        git \
        libncurses-dev \
        libxml2-dev \
        ninja-build \
        nasm \
        pkg-config \
        python3 \
        unzip \
        zip \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

ARG SDL_VERSION=3.4.0
ARG ZSTD_VERSION=1.5.7
ARG FFMPEG_VERSION=8.0.1
ARG SHADERCROSS_COMMIT=6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99

FROM build-base AS host-tools

RUN git clone --branch "release-${SDL_VERSION}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && cmake -S /deps/SDL -B /build/sdl-host -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-host \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_UNIX_CONSOLE_BUILD=ON \
    && cmake --build /build/sdl-host --parallel \
    && cmake --install /build/sdl-host

COPY packaging/common/shadercross-fsr3.patch /tmp/shadercross-fsr3.patch

# DXC 1.9 requires glibc 2.38; keep the Debian 12-compatible 1.8 release.
ARG DXC_VERSION=1.8.2502
ARG DXC_LINUX_X64_ARCHIVE=linux_dxc_2025_02_20.x86_64.tar.gz
ARG DXC_LINUX_X64_SHA256=e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71

ADD --checksum=sha256:${DXC_LINUX_X64_SHA256} \
    https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_VERSION}/${DXC_LINUX_X64_ARCHIVE} \
    /tmp/dxc.tar.gz

RUN git clone https://github.com/libsdl-org/SDL_shadercross.git /deps/SDL_shadercross \
    && git -C /deps/SDL_shadercross checkout "${SHADERCROSS_COMMIT}" \
    && git -C /deps/SDL_shadercross apply /tmp/shadercross-fsr3.patch \
    && git -C /deps/SDL_shadercross submodule update --init --depth 1 \
        external/SPIRV-Cross \
    && mkdir -p \
        /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux \
    && tar -xzf /tmp/dxc.tar.gz \
        -C /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux \
    && cmake -S /deps/SDL_shadercross/external/SPIRV-Cross \
        -B /build/spirv-cross -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-host \
        -DSPIRV_CROSS_CLI=OFF \
        -DSPIRV_CROSS_ENABLE_TESTS=OFF \
        -DSPIRV_CROSS_SHARED=ON \
        -DSPIRV_CROSS_STATIC=OFF \
    && cmake --build /build/spirv-cross --parallel \
    && cmake --install /build/spirv-cross \
    && cmake -S /deps/SDL_shadercross -B /build/shadercross -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-host \
        -DCMAKE_PREFIX_PATH="/opt/xwa-host" \
        -DDirectXShaderCompiler_ROOT=/deps/SDL_shadercross/external/DirectXShaderCompiler-binaries \
        -DSDLSHADERCROSS_CLI=ON \
        -DSDLSHADERCROSS_DXC=ON \
        -DSDLSHADERCROSS_INSTALL=ON \
        -DSDLSHADERCROSS_INSTALL_RUNTIME=OFF \
        -DSDLSHADERCROSS_SHARED=ON \
        -DSDLSHADERCROSS_STATIC=OFF \
        -DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON \
        -DSDLSHADERCROSS_VENDORED=OFF \
    && cmake --build /build/shadercross --parallel \
    && cmake --install /build/shadercross \
    && cp /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux/lib/libdxcompiler.so \
        /deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/linux/lib/libdxil.so \
        /opt/xwa-host/lib/

# The Windows target dependencies build as independent stages so that a
# version bump only rebuilds the affected dependency and so that cold builds
# run concurrently.

FROM build-base AS sdl-windows

COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xwa-toolchain.cmake
COPY packaging/common/sdl-gpu-d3d12-hdr.patch /tmp/sdl-gpu-d3d12-hdr.patch

# SDL's D3D12 SDL_GPU backend queries HDR swapchain composition support against
# the live SDR swapchain, which DXGI can never report scRGB as presentable on,
# so HDR output is unreachable on that backend. Only the Windows target build
# has a D3D12 backend, so the host SDL above is left unpatched.
RUN git clone --branch "release-${SDL_VERSION}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && git -C /deps/SDL apply /tmp/sdl-gpu-d3d12-hdr.patch \
    && cmake -S /deps/SDL -B /build/sdl-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-windows \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xwa-toolchain.cmake \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
    && cmake --build /build/sdl-windows --parallel \
    && cmake --install /build/sdl-windows

FROM build-base AS zstd-windows

COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xwa-toolchain.cmake

RUN git clone --branch "v${ZSTD_VERSION}" --depth 1 \
        https://github.com/facebook/zstd.git /deps/zstd \
    && cmake -S /deps/zstd/build/cmake -B /build/zstd-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-windows \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xwa-toolchain.cmake \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=ON \
        -DZSTD_BUILD_STATIC=OFF \
        -DZSTD_BUILD_TESTS=OFF \
    && cmake --build /build/zstd-windows --parallel \
    && cmake --install /build/zstd-windows

FROM build-base AS ffmpeg-windows

RUN git clone --branch "n${FFMPEG_VERSION}" --depth 1 \
        https://github.com/FFmpeg/FFmpeg.git /deps/FFmpeg \
    && cd /deps/FFmpeg \
    && ./configure \
        --prefix=/opt/xwa-windows \
        --target-os=mingw32 \
        --arch=x86_64 \
        --cross-prefix=x86_64-w64-mingw32- \
        --disable-autodetect \
        --disable-avdevice \
        --disable-avfilter \
        --disable-debug \
        --disable-doc \
        --disable-everything \
        --disable-network \
        --disable-programs \
        --disable-static \
        --enable-shared \
        --enable-decoder=adpcm_vima \
        --enable-decoder=sanm \
        --enable-demuxer=smush \
        --enable-w32threads \
    && make -j"$(nproc)" \
    && make install

FROM build-base AS xwa-host-tools

COPY --from=host-tools /opt/xwa-host /opt/xwa-host
COPY aeron /src/aeron

ENV LD_LIBRARY_PATH=/opt/xwa-host/lib

RUN cmake -S /src/aeron -B /build/aeron-host -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-host \
        -DCMAKE_PREFIX_PATH=/opt/xwa-host \
        -DAERON_HOST_TOOLS_ONLY=ON \
    && cmake --build /build/aeron-host --parallel \
    && cmake --install /build/aeron-host

FROM build-base AS xwa-build

ARG XWA_BUILD_TYPE=Release
ARG XWA_VERSION=0.0.0-dev

COPY --from=xwa-host-tools /opt/xwa-host /opt/xwa-host
COPY --from=sdl-windows /opt/xwa-windows /opt/xwa-windows
COPY --from=zstd-windows /opt/xwa-windows /opt/xwa-windows
COPY --from=ffmpeg-windows /opt/xwa-windows /opt/xwa-windows
COPY --from=sdl-windows /deps/SDL/LICENSE.txt /opt/xwa-licenses/SDL3.txt
COPY --from=zstd-windows /deps/zstd/LICENSE /opt/xwa-licenses/zstd.txt
COPY --from=ffmpeg-windows /deps/FFmpeg/COPYING.LGPLv2.1 /opt/xwa-licenses/FFmpeg-LGPL-2.1.txt
COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xwa-toolchain.cmake
COPY CMakeLists.txt /src/CMakeLists.txt
COPY README.md /src/README.md
COPY aeron /src/aeron
COPY cmake /src/cmake
COPY resources /src/resources
COPY shaders /src/shaders
COPY src /src/src

ENV LD_LIBRARY_PATH=/opt/xwa-host/lib

RUN case "${XWA_BUILD_TYPE}" in \
        Release) xwa_debug_ui=OFF ;; \
        Debug) xwa_debug_ui=ON ;; \
        *) echo "XWA_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;; \
    esac \
    && cmake -S /src -B /build/xwa -G Ninja \
        -DCMAKE_BUILD_TYPE="${XWA_BUILD_TYPE}" \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xwa-toolchain.cmake \
        -DAERON_FSR3_SHADERGEN_EXECUTABLE=/opt/xwa-host/bin/aeron_fsr3_shadergen \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xwa-host/bin/shadercross \
        -DXWA_BUILD_TOOLS=OFF \
        -DXWA_ENABLE_DEBUG_UI="${xwa_debug_ui}" \
        -DXWA_VERSION="${XWA_VERSION}" \
    && cmake --build /build/xwa --target xwa --parallel

RUN mkdir -p /build/package/openxwa-windows-x86_64/licenses \
        /build/package/openxwa-windows-x86_64/shaders \
    && cp /build/xwa/OpenXWA.exe /build/package/openxwa-windows-x86_64/ \
    && cp /build/xwa/shaders/*.dxil /build/xwa/shaders/*.spv \
        /build/package/openxwa-windows-x86_64/shaders/ \
    && cp -a /src/resources /build/package/openxwa-windows-x86_64/ \
    && cp /src/README.md \
        /build/package/openxwa-windows-x86_64/README.md \
    && cp /opt/xwa-licenses/SDL3.txt \
        /build/package/openxwa-windows-x86_64/licenses/SDL3.txt \
    && cp /opt/xwa-licenses/zstd.txt \
        /build/package/openxwa-windows-x86_64/licenses/zstd.txt \
    && cp /opt/xwa-licenses/FFmpeg-LGPL-2.1.txt \
        /build/package/openxwa-windows-x86_64/licenses/FFmpeg-LGPL-2.1.txt \
    && cp -L /usr/share/doc/gcc-mingw-w64-x86-64-posix-runtime/copyright \
        /build/package/openxwa-windows-x86_64/licenses/GCC-MinGW-w64.txt \
    && cp -L /usr/share/doc/mingw-w64-x86-64-dev/copyright \
        /build/package/openxwa-windows-x86_64/licenses/MinGW-w64.txt \
    && cp /opt/xwa-windows/bin/*.dll /build/package/openxwa-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32-g++ -print-file-name=libgcc_s_seh-1.dll)" \
        /build/package/openxwa-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32-g++ -print-file-name=libstdc++-6.dll)" \
        /build/package/openxwa-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32-g++ -print-file-name=libwinpthread-1.dll)" \
        /build/package/openxwa-windows-x86_64/

RUN test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.vert.dxil \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.frag.dxil \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.vert.spv \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.frag.spv \
    && test -f /build/package/openxwa-windows-x86_64/resources/remaster/flight/render.yaml \
    && x86_64-w64-mingw32-objdump -p \
        /build/package/openxwa-windows-x86_64/OpenXWA.exe \
        | grep -Eq 'Subsystem[[:space:]]+00000002' \
    && if [ "${XWA_BUILD_TYPE}" = Debug ]; then \
        x86_64-w64-mingw32-objdump -h \
            /build/package/openxwa-windows-x86_64/OpenXWA.exe \
            | grep -q '\.debug_info'; \
    fi \
    && if [ "${XWA_BUILD_TYPE}" = Debug ]; then \
        artifact_variant=debug; \
    else \
        artifact_variant=release; \
    fi \
    && package_name="openxwa-${XWA_VERSION}-windows-x86_64-${artifact_variant}" \
    && mv /build/package/openxwa-windows-x86_64 "/build/package/${package_name}" \
    && find "/build/package/${package_name}" \
        -exec touch -h -d "@315532800" {} + \
    && mkdir -p /build/artifacts \
    && cd /build/package \
    && find "${package_name}" -print > /tmp/openxwa-windows-files \
    && LC_ALL=C sort -o /tmp/openxwa-windows-files /tmp/openxwa-windows-files \
    && TZ=UTC zip -X -9 -q "/build/artifacts/${package_name}.zip" \
        -@ < /tmp/openxwa-windows-files \
    && unzip -tq "/build/artifacts/${package_name}.zip"

FROM scratch AS artifact
COPY --from=xwa-build /build/artifacts/ /
