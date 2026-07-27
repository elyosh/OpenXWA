# Microsoft's Linux DXC release binaries are available for x86-64 only.
ARG XWA_BUILD_PLATFORM=linux/amd64

FROM --platform=${XWA_BUILD_PLATFORM} debian:bookworm-slim AS build-base

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        file \
        git \
        libasound2-dev \
        libdbus-1-dev \
        libdecor-0-dev \
        libdrm-dev \
        libegl1-mesa-dev \
        libgbm-dev \
        libgl-dev \
        libibus-1.0-dev \
        libncurses-dev \
        libpipewire-0.3-dev \
        libpulse-dev \
        libudev-dev \
        liburing-dev \
        libvulkan-dev \
        libwayland-dev \
        libx11-dev \
        libxcursor-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxkbcommon-dev \
        libxml2-dev \
        libxrandr-dev \
        libxss-dev \
        libxtst-dev \
        nasm \
        ninja-build \
        patchelf \
        pkg-config \
        python3 \
        wayland-protocols \
        xz-utils \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

ARG SDL_VERSION=3.4.0
ARG ZSTD_VERSION=1.5.7
ARG FFMPEG_VERSION=8.0.1
ARG SHADERCROSS_COMMIT=6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99

# The dependencies build as independent stages so that a version bump only
# rebuilds the affected dependency and so that cold builds run concurrently.

FROM build-base AS sdl-build

RUN git clone --branch "release-${SDL_VERSION}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && cmake -S /deps/SDL -B /build/sdl -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-linux \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
    && cmake --build /build/sdl --parallel \
    && cmake --install /build/sdl

FROM build-base AS zstd-build

RUN git clone --branch "v${ZSTD_VERSION}" --depth 1 \
        https://github.com/facebook/zstd.git /deps/zstd \
    && cmake -S /deps/zstd/build/cmake -B /build/zstd -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-linux \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=ON \
        -DZSTD_BUILD_STATIC=OFF \
        -DZSTD_BUILD_TESTS=OFF \
    && cmake --build /build/zstd --parallel \
    && cmake --install /build/zstd

FROM build-base AS ffmpeg-build

RUN git clone --branch "n${FFMPEG_VERSION}" --depth 1 \
        https://github.com/FFmpeg/FFmpeg.git /deps/FFmpeg \
    && cd /deps/FFmpeg \
    && ./configure \
        --prefix=/opt/xwa-linux \
        --disable-autodetect \
        --disable-avdevice \
        --disable-avfilter \
        --disable-debug \
        --disable-doc \
        --disable-everything \
        --disable-network \
        --disable-programs \
        --disable-static \
        --enable-decoder=adpcm_vima \
        --enable-decoder=sanm \
        --enable-demuxer=smush \
        --enable-pic \
        --enable-pthreads \
        --enable-shared \
    && make -j"$(nproc)" \
    && make install

FROM build-base AS shader-tools

COPY packaging/common/shadercross-fsr3.patch /tmp/shadercross-fsr3.patch

# DXC 1.9 requires glibc 2.38; keep the Debian 12-compatible 1.8 release.
ARG DXC_VERSION=1.8.2502
ARG DXC_LINUX_X64_ARCHIVE=linux_dxc_2025_02_20.x86_64.tar.gz
ARG DXC_LINUX_X64_SHA256=e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71

ADD --checksum=sha256:${DXC_LINUX_X64_SHA256} \
    https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_VERSION}/${DXC_LINUX_X64_ARCHIVE} \
    /tmp/dxc.tar.gz

# shadercross links SDL but needs neither zstd nor FFmpeg.
COPY --from=sdl-build /opt/xwa-linux /opt/xwa-linux

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
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-tools \
        -DSPIRV_CROSS_CLI=OFF \
        -DSPIRV_CROSS_ENABLE_TESTS=OFF \
        -DSPIRV_CROSS_SHARED=ON \
        -DSPIRV_CROSS_STATIC=OFF \
    && cmake --build /build/spirv-cross --parallel \
    && cmake --install /build/spirv-cross \
    && cmake -S /deps/SDL_shadercross -B /build/shadercross -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-tools \
        -DCMAKE_PREFIX_PATH="/opt/xwa-linux;/opt/xwa-tools" \
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
        /opt/xwa-tools/lib/

FROM build-base AS xwa-build

ARG XWA_BUILD_TYPE=Release
ARG XWA_VERSION=0.0.0-dev

COPY --from=sdl-build /opt/xwa-linux /opt/xwa-linux
COPY --from=zstd-build /opt/xwa-linux /opt/xwa-linux
COPY --from=ffmpeg-build /opt/xwa-linux /opt/xwa-linux
COPY --from=sdl-build /deps/SDL/LICENSE.txt /opt/xwa-licenses/SDL3.txt
COPY --from=zstd-build /deps/zstd/LICENSE /opt/xwa-licenses/zstd.txt
COPY --from=ffmpeg-build /deps/FFmpeg/COPYING.LGPLv2.1 /opt/xwa-licenses/FFmpeg-LGPL-2.1.txt
COPY --from=shader-tools /opt/xwa-tools /opt/xwa-tools
COPY CMakeLists.txt /src/CMakeLists.txt
COPY README.md /src/README.md
COPY aeron /src/aeron
COPY cmake /src/cmake
COPY resources /src/resources
COPY shaders /src/shaders
COPY src /src/src

ENV LD_LIBRARY_PATH=/opt/xwa-tools/lib:/opt/xwa-linux/lib
ENV PKG_CONFIG_PATH=/opt/xwa-linux/lib/pkgconfig

RUN case "${XWA_BUILD_TYPE}" in \
        Release) xwa_debug_ui=OFF ;; \
        Debug) xwa_debug_ui=ON ;; \
        *) echo "XWA_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;; \
    esac \
    && cmake -S /src -B /build/xwa -G Ninja \
        -DCMAKE_BUILD_TYPE="${XWA_BUILD_TYPE}" \
        -DCMAKE_PREFIX_PATH=/opt/xwa-linux \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xwa-tools/bin/shadercross \
        -DXWA_BUILD_TOOLS=OFF \
        -DXWA_ENABLE_DEBUG_UI="${xwa_debug_ui}" \
    && cmake --build /build/xwa --target xwa --parallel

RUN mkdir -p /build/package/openxwa-linux-x86_64/licenses \
        /build/package/openxwa-linux-x86_64/lib \
        /build/package/openxwa-linux-x86_64/shaders \
    && cp /build/xwa/OpenXWA /build/package/openxwa-linux-x86_64/ \
    && cp /build/xwa/shaders/*.spv \
        /build/package/openxwa-linux-x86_64/shaders/ \
    && cp -a /src/resources /build/package/openxwa-linux-x86_64/ \
    && cp /src/README.md \
        /build/package/openxwa-linux-x86_64/README.md \
    && cp /opt/xwa-licenses/SDL3.txt \
        /build/package/openxwa-linux-x86_64/licenses/SDL3.txt \
    && cp /opt/xwa-licenses/zstd.txt \
        /build/package/openxwa-linux-x86_64/licenses/zstd.txt \
    && cp /opt/xwa-licenses/FFmpeg-LGPL-2.1.txt \
        /build/package/openxwa-linux-x86_64/licenses/FFmpeg-LGPL-2.1.txt \
    && cp -L /usr/share/doc/libgcc-s1/copyright \
        /build/package/openxwa-linux-x86_64/licenses/libgcc-s1.txt \
    && cp -L /usr/share/doc/libstdc++6/copyright \
        /build/package/openxwa-linux-x86_64/licenses/libstdc++6.txt \
    && cp /usr/share/common-licenses/GPL-3 \
        /build/package/openxwa-linux-x86_64/licenses/GPL-3.txt \
    && cp -L /opt/xwa-linux/lib/libSDL3.so.0 \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L /opt/xwa-linux/lib/libavcodec.so.62 \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L /opt/xwa-linux/lib/libavformat.so.62 \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L /opt/xwa-linux/lib/libavutil.so.60 \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L /opt/xwa-linux/lib/libswresample.so.6 \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L /opt/xwa-linux/lib/libswscale.so.9 \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L /opt/xwa-linux/lib/libzstd.so.1 \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L "$(g++ -print-file-name=libgcc_s.so.1)" \
        /build/package/openxwa-linux-x86_64/lib/ \
    && cp -L "$(g++ -print-file-name=libstdc++.so.6)" \
        /build/package/openxwa-linux-x86_64/lib/ \
    && patchelf --set-rpath '$ORIGIN/lib' \
        /build/package/openxwa-linux-x86_64/OpenXWA \
    && for library in /build/package/openxwa-linux-x86_64/lib/*.so*; do \
        patchelf --set-rpath '$ORIGIN' "$library"; \
    done

RUN test -f /build/package/openxwa-linux-x86_64/shaders/hyperspace_streak.vert.spv \
    && test -f /build/package/openxwa-linux-x86_64/shaders/hyperspace_streak.frag.spv \
    && test -f /build/package/openxwa-linux-x86_64/resources/remaster/flight/render.yaml \
    && file /build/package/openxwa-linux-x86_64/OpenXWA \
    && env -u LD_LIBRARY_PATH ldd /build/package/openxwa-linux-x86_64/OpenXWA \
    && ! env -u LD_LIBRARY_PATH \
        ldd /build/package/openxwa-linux-x86_64/OpenXWA | grep -q "not found" \
    && if [ "${XWA_BUILD_TYPE}" = Debug ]; then \
        readelf --sections /build/package/openxwa-linux-x86_64/OpenXWA \
            | grep -q '\.debug_info'; \
    fi \
    && if [ "${XWA_BUILD_TYPE}" = Debug ]; then \
        artifact_variant=debug; \
    else \
        artifact_variant=release; \
    fi \
    && package_name="openxwa-${XWA_VERSION}-linux-x86_64-${artifact_variant}" \
    && mv /build/package/openxwa-linux-x86_64 "/build/package/${package_name}" \
    && mkdir -p /build/artifacts \
    && tar \
        --sort=name \
        --mtime="@0" \
        --owner=0 \
        --group=0 \
        --numeric-owner \
        -C /build/package \
        -cJf "/build/artifacts/${package_name}.tar.xz" \
        "${package_name}" \
    && tar -tJf "/build/artifacts/${package_name}.tar.xz"

FROM scratch AS artifact
COPY --from=xwa-build /build/artifacts/ /
