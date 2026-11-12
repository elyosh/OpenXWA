# Microsoft's Linux DXC release binaries are available for x86-64 only.
# The Windows target uses Debian's GCC MinGW-w64 for UCRT, the C runtime of
# Windows 10 and later. Its maths functions come from UCRT. They are much
# faster than MinGW-w64's own versions, which the older MSVCRT target uses.
ARG XWA_BUILD_PLATFORM=linux/amd64

FROM --platform=${XWA_BUILD_PLATFORM} debian:trixie-slim AS build-base

RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mingw-w64-ucrt64 \
        build-essential \
        ca-certificates \
        cmake \
        g++-mingw-w64-ucrt64 \
        gcc-mingw-w64-ucrt64 \
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

# The versions and patches of aeron's dependencies come from
# aeron/packaging/dependencies; set these to build another version.
ARG SDL_VERSION=
ARG ZSTD_VERSION=
ARG FFMPEG_VERSION=
ARG SHADERCROSS_COMMIT=

FROM build-base AS host-tools

COPY aeron/packaging/dependencies/apply-patches.sh /tmp/aeron/apply-patches.sh
COPY aeron/packaging/dependencies/sdl /tmp/aeron/sdl

RUN git clone --branch "release-${SDL_VERSION:-$(cat /tmp/aeron/sdl/version)}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && /tmp/aeron/apply-patches.sh /tmp/aeron/sdl /deps/SDL \
    && cmake -S /deps/SDL -B /build/sdl-host -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-host \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_UNIX_CONSOLE_BUILD=ON \
    && cmake --build /build/sdl-host --parallel \
    && cmake --install /build/sdl-host

COPY aeron/packaging/dependencies/shadercross /tmp/aeron/shadercross

# The DXC release that compiles the shaders. Releases from 1.9 on need
# glibc 2.38 or later.
ARG DXC_VERSION=1.8.2502
ARG DXC_LINUX_X64_ARCHIVE=linux_dxc_2025_02_20.x86_64.tar.gz
ARG DXC_LINUX_X64_SHA256=e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71

ADD --checksum=sha256:${DXC_LINUX_X64_SHA256} \
    https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_VERSION}/${DXC_LINUX_X64_ARCHIVE} \
    /tmp/dxc.tar.gz

RUN git clone https://github.com/libsdl-org/SDL_shadercross.git /deps/SDL_shadercross \
    && git -C /deps/SDL_shadercross checkout "${SHADERCROSS_COMMIT:-$(cat /tmp/aeron/shadercross/version)}" \
    && /tmp/aeron/apply-patches.sh /tmp/aeron/shadercross /deps/SDL_shadercross \
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

# aeron's SDL patches fix the D3D12 backend: HDR swapchain compositions and
# descriptor heap overflows (aeron/packaging/dependencies/README.md).
COPY aeron/packaging/dependencies/apply-patches.sh /tmp/aeron/apply-patches.sh
COPY aeron/packaging/dependencies/sdl /tmp/aeron/sdl

RUN git clone --branch "release-${SDL_VERSION:-$(cat /tmp/aeron/sdl/version)}" --depth 1 \
        https://github.com/libsdl-org/SDL.git /deps/SDL \
    && /tmp/aeron/apply-patches.sh /tmp/aeron/sdl /deps/SDL \
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

COPY aeron/packaging/dependencies/zstd /tmp/aeron/zstd

RUN git clone --branch "v${ZSTD_VERSION:-$(cat /tmp/aeron/zstd/version)}" --depth 1 \
        https://github.com/facebook/zstd.git /deps/zstd \
    && cmake -S /deps/zstd/build/cmake -B /build/zstd-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/xwa-windows \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xwa-toolchain.cmake \
        -DZSTD_BUILD_PROGRAMS=OFF \
        -DZSTD_BUILD_SHARED=ON \
        -DZSTD_BUILD_STATIC=ON \
        -DZSTD_BUILD_TESTS=OFF \
    && cmake --build /build/zstd-windows --parallel \
    && cmake --install /build/zstd-windows

FROM build-base AS ffmpeg-windows

COPY aeron/packaging/dependencies/ffmpeg /tmp/aeron/ffmpeg

RUN git clone --branch "n${FFMPEG_VERSION:-$(cat /tmp/aeron/ffmpeg/version)}" --depth 1 \
        https://github.com/FFmpeg/FFmpeg.git /deps/FFmpeg \
    && cd /deps/FFmpeg \
    && ./configure \
        --prefix=/opt/xwa-windows \
        --target-os=mingw32 \
        --arch=x86_64 \
        --cross-prefix=x86_64-w64-mingw32ucrt- \
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
        --enable-bsf=vp9_superframe_split \
        --enable-decoder=aac \
        --enable-decoder=adpcm_vima \
        --enable-decoder=h264 \
        --enable-decoder=sanm \
        --enable-decoder=vp9 \
        --enable-demuxer=matroska \
        --enable-demuxer=mov \
        --enable-demuxer=smush \
        --enable-parser=aac \
        --enable-parser=h264 \
        --enable-parser=vp9 \
        --enable-w32threads \
    && make -j"$(nproc)" \
    && make install

FROM build-base AS winpix-windows

ARG WINPIX_VERSION=1.0.240308001
ARG WINPIX_NUPKG_SHA256=726acc93d6968e2146261a1e415521747d50ad69894c2b42b5d0d4c29fd66ec4

# SDL's D3D12 backend emits SDL_Push/PopGPUDebugGroup and
# SDL_InsertGPUDebugLabel through WinPixEventRuntime.dll, loaded at runtime.
# MIT-licensed since 1.0.240308001. Shipped with Debug packages only: Release
# builds compile the label calls out (AERON_GPU_DEBUG_LABELS), so the DLL is
# staged outside /opt/xwa-windows to keep it clear of the wildcard DLL copy.
ADD --checksum=sha256:${WINPIX_NUPKG_SHA256} \
    https://globalcdn.nuget.org/packages/winpixeventruntime.${WINPIX_VERSION}.nupkg \
    /tmp/winpix.nupkg

RUN mkdir -p /deps/winpix /opt/winpix/bin \
    && unzip -q /tmp/winpix.nupkg -d /deps/winpix \
    && install -m 644 /deps/winpix/bin/x64/WinPixEventRuntime.dll \
        /opt/winpix/bin/

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

# The mod importer's BC6H encoder is compiled by ISPC. ISPC's Linux release
# only writes Linux objects, so the Windows release runs under Wine.
FROM build-base AS ispc

RUN apt-get update && apt-get install -y --no-install-recommends \
        wine \
        wine64 \
    && rm -rf /var/lib/apt/lists/*

ARG ISPC_VERSION=1.30.0
ARG ISPC_WINDOWS_SHA256=e126a78fd15f12475bac6204f2536b3986e07973127e4f8e7336c3d304c4a69f

ADD --checksum=sha256:${ISPC_WINDOWS_SHA256} \
    https://github.com/ispc/ispc/releases/download/v${ISPC_VERSION}/ispc-v${ISPC_VERSION}-windows.zip \
    /tmp/ispc.zip

ENV WINEDEBUG=-all

# Wine reads the build's absolute paths as paths on its Z: drive, which is
# the root of the file system. The first run creates the Wine prefix.
RUN mkdir -p /tmp/ispc /opt/ispc \
    && unzip -q /tmp/ispc.zip -d /tmp/ispc \
    && cp -a /tmp/ispc/*/. /opt/ispc/ \
    && rm -rf /tmp/ispc /tmp/ispc.zip \
    && printf '#!/bin/sh\nexec wine /opt/ispc/bin/ispc.exe "$@"\n' > /usr/local/bin/ispc \
    && chmod +x /usr/local/bin/ispc \
    && ispc --version

# The ISPC texture compressor kernel takes minutes to compile, and it only
# changes with the two files copied here and the ISPC version. It has a stage
# of its own, so the build cache keeps it while the rest of the source
# changes.
FROM ispc AS ispc-kernel

COPY aeron/tools/imgbake/ispc_kernel.cmake /src/aeron/tools/imgbake/ispc_kernel.cmake
COPY aeron/third_party/ispc_texcomp/kernel.ispc /src/aeron/third_party/ispc_texcomp/kernel.ispc

RUN cmake -DISPC=/usr/local/bin/ispc -DSYSTEM=Windows -DPROCESSOR=x86_64 -DEXTENSION=.obj \
        -DOUT=/opt/ispc-kernel -P /src/aeron/tools/imgbake/ispc_kernel.cmake

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
COPY --from=winpix-windows /opt/winpix /opt/winpix
COPY --from=ispc-kernel /opt/ispc-kernel /opt/ispc-kernel
COPY --from=winpix-windows /deps/winpix/license.txt /opt/xwa-licenses/WinPixEventRuntime.txt
COPY --from=winpix-windows /deps/winpix/ThirdPartyNotices.txt /opt/xwa-licenses/WinPixEventRuntime-ThirdPartyNotices.txt
COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xwa-toolchain.cmake
COPY CMakeLists.txt /src/CMakeLists.txt
COPY README.md /src/README.md
COPY aeron /src/aeron
COPY cmake /src/cmake
COPY resources /src/resources
COPY shaders /src/shaders
COPY src /src/src
COPY tools /src/tools

ENV LD_LIBRARY_PATH=/opt/xwa-host/lib

RUN case "${XWA_BUILD_TYPE}" in \
        Release) xwa_debug_ui=OFF; xwa_release_flags="-O3 -DNDEBUG -g" ;; \
        Debug) xwa_debug_ui=ON; xwa_release_flags="-O3 -DNDEBUG" ;; \
        *) echo "XWA_BUILD_TYPE must be Release or Debug" >&2; exit 2 ;; \
    esac \
    && cmake -S /src -B /build/xwa -G Ninja \
        -DCMAKE_BUILD_TYPE="${XWA_BUILD_TYPE}" \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xwa-toolchain.cmake \
        -DCMAKE_C_FLAGS_RELEASE="${xwa_release_flags}" \
        -DCMAKE_CXX_FLAGS_RELEASE="${xwa_release_flags}" \
        -DAERON_FSR3_SHADERGEN_EXECUTABLE=/opt/xwa-host/bin/aeron_fsr3_shadergen \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xwa-host/bin/shadercross \
        -DAERON_ISPC_KERNEL_DIR=/opt/ispc-kernel \
        -DXWA_BUILD_TOOLS=ON \
        -DXWA_ENABLE_DEBUG_UI="${xwa_debug_ui}" \
        -DXWA_VERSION="${XWA_VERSION}" \
    && cmake --build /build/xwa --target xwa xwa_mod_importer --parallel

# Release builds compile with -g so that crash reports can be symbolized.
# The debug information goes into a .debug file for each program, which is
# published in a separate symbols archive. The shipped program only keeps a
# .gnu_debuglink to it. Stripping only removes sections at the end of the
# file, so the code addresses match the symbols exactly.
RUN mkdir -p /build/package/openxwa-windows-x86_64/licenses \
        /build/package/openxwa-windows-x86_64/shaders \
    && cp /build/xwa/OpenXWA.exe "/build/xwa/OpenXWA Mod Importer.exe" \
        /build/package/openxwa-windows-x86_64/ \
    && if [ "${XWA_BUILD_TYPE}" = Release ]; then \
        mkdir -p /build/symbols \
        && cd /build/package/openxwa-windows-x86_64 \
        && for program in OpenXWA.exe "OpenXWA Mod Importer.exe"; do \
            x86_64-w64-mingw32ucrt-objcopy --only-keep-debug "$program" "/build/symbols/${program}.debug" \
            && cp "/build/symbols/${program}.debug" . \
            && x86_64-w64-mingw32ucrt-objcopy --strip-debug --add-gnu-debuglink="${program}.debug" "$program" \
            && rm "${program}.debug" \
            && cp "$program" /build/symbols/ \
            || exit 1; \
        done; \
    fi \
    && cp /build/xwa/shaders/*.dxil /build/xwa/shaders/*.spv \
        /build/package/openxwa-windows-x86_64/shaders/ \
    && cp -a /build/xwa/resources /build/package/openxwa-windows-x86_64/ \
    && cp /src/README.md \
        /build/package/openxwa-windows-x86_64/README.md \
    && cp /opt/xwa-licenses/SDL3.txt \
        /build/package/openxwa-windows-x86_64/licenses/SDL3.txt \
    && cp /opt/xwa-licenses/zstd.txt \
        /build/package/openxwa-windows-x86_64/licenses/zstd.txt \
    && cp /opt/xwa-licenses/FFmpeg-LGPL-2.1.txt \
        /build/package/openxwa-windows-x86_64/licenses/FFmpeg-LGPL-2.1.txt \
    && cp /src/aeron/third_party/ispc_texcomp/LICENSE \
        /build/package/openxwa-windows-x86_64/licenses/ISPC-Texture-Compressor.txt \
    && cp /src/aeron/third_party/bcdec/LICENSE \
        /build/package/openxwa-windows-x86_64/licenses/bcdec.txt \
    && cp -L /usr/share/doc/gcc-mingw-w64-ucrt64-runtime/copyright \
        /build/package/openxwa-windows-x86_64/licenses/GCC-MinGW-w64.txt \
    && cp -L /usr/share/doc/mingw-w64-ucrt64-dev/copyright \
        /build/package/openxwa-windows-x86_64/licenses/MinGW-w64.txt \
    && cp /opt/xwa-windows/bin/*.dll /build/package/openxwa-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32ucrt-g++ -print-file-name=libgcc_s_seh-1.dll)" \
        /build/package/openxwa-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32ucrt-g++ -print-file-name=libstdc++-6.dll)" \
        /build/package/openxwa-windows-x86_64/ \
    && cp "$(x86_64-w64-mingw32ucrt-g++ -print-file-name=libwinpthread-1.dll)" \
        /build/package/openxwa-windows-x86_64/ \
    && if [ "${XWA_BUILD_TYPE}" = Debug ]; then \
        cp /opt/winpix/bin/WinPixEventRuntime.dll \
            /build/package/openxwa-windows-x86_64/ \
        && cp /opt/xwa-licenses/WinPixEventRuntime.txt \
            /build/package/openxwa-windows-x86_64/licenses/WinPixEventRuntime.txt \
        && cp /opt/xwa-licenses/WinPixEventRuntime-ThirdPartyNotices.txt \
            /build/package/openxwa-windows-x86_64/licenses/WinPixEventRuntime-ThirdPartyNotices.txt; \
    fi \
    # The bundled DLLs have debug information too, most of it in Debian's
    # libstdc++. Split it in the same way.
    && if [ "${XWA_BUILD_TYPE}" = Release ]; then \
        cd /build/package/openxwa-windows-x86_64 \
        && for library in *.dll; do \
            if x86_64-w64-mingw32ucrt-objdump -h "$library" | grep -q '\.debug_'; then \
                x86_64-w64-mingw32ucrt-objcopy --only-keep-debug "$library" "/build/symbols/${library}.debug" \
                && cp "/build/symbols/${library}.debug" . \
                && x86_64-w64-mingw32ucrt-objcopy --strip-debug --add-gnu-debuglink="${library}.debug" "$library" \
                && rm "${library}.debug" \
                && cp "$library" /build/symbols/ \
                || exit 1; \
            fi; \
        done; \
    fi

RUN test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.vert.dxil \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.frag.dxil \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.vert.spv \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_streak.frag.spv \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_tunnel.vert.dxil \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_tunnel.frag.dxil \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_tunnel.vert.spv \
    && test -f /build/package/openxwa-windows-x86_64/shaders/hyperspace_tunnel.frag.spv \
    && test -f /build/package/openxwa-windows-x86_64/resources/aeron/scene3d_defaults.yaml \
    && test -f /build/package/openxwa-windows-x86_64/resources/remaster/config.yaml \
    && test -f /build/package/openxwa-windows-x86_64/resources/assets/fonts/menu.fnt \
    && test -f /build/package/openxwa-windows-x86_64/resources/assets/fonts/menu.ktx2 \
    && x86_64-w64-mingw32ucrt-objdump -p \
        /build/package/openxwa-windows-x86_64/OpenXWA.exe \
        | grep -Eq 'Subsystem[[:space:]]+00000002' \
    && x86_64-w64-mingw32ucrt-objdump -p \
        "/build/package/openxwa-windows-x86_64/OpenXWA Mod Importer.exe" \
        | grep -Eq 'Subsystem[[:space:]]+00000002' \
    && if [ "${XWA_BUILD_TYPE}" = Debug ]; then \
        x86_64-w64-mingw32ucrt-objdump -h \
            /build/package/openxwa-windows-x86_64/OpenXWA.exe \
            | grep -q '\.debug_info' \
        && test -f /build/package/openxwa-windows-x86_64/WinPixEventRuntime.dll; \
    else \
        for program in OpenXWA.exe "OpenXWA Mod Importer.exe"; do \
            ! x86_64-w64-mingw32ucrt-objdump -h \
                "/build/package/openxwa-windows-x86_64/${program}" \
                | grep -q '\.debug_info' \
            && x86_64-w64-mingw32ucrt-objdump -h \
                "/build/package/openxwa-windows-x86_64/${program}" \
                | grep -q '\.gnu_debuglink' \
            && x86_64-w64-mingw32ucrt-objdump -h "/build/symbols/${program}.debug" \
                | grep -q '\.debug_info' \
            || exit 1; \
        done \
        && for library in /build/package/openxwa-windows-x86_64/*.dll; do \
            ! x86_64-w64-mingw32ucrt-objdump -h "$library" | grep -q '\.debug_' || exit 1; \
        done; \
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
    && unzip -tq "/build/artifacts/${package_name}.zip" \
    && if [ "${XWA_BUILD_TYPE}" = Release ]; then \
        symbols_name="${package_name}-symbols" \
        && mv /build/symbols "/build/${symbols_name}" \
        && find "/build/${symbols_name}" -exec touch -h -d "@315532800" {} + \
        && cd /build \
        && find "${symbols_name}" -print | LC_ALL=C sort \
            | TZ=UTC zip -X -9 -q "/build/artifacts/${symbols_name}.zip" -@ \
        && unzip -tq "/build/artifacts/${symbols_name}.zip"; \
    fi

# aeron_gltf_cook and opt2gltf for the Blender add-on's Windows package:
# zstd and the MinGW runtime linked statically, no SDL (aeron/tools/
# gltf_cook/CMakeLists.txt). The game's own zstd links the DLL; the static
# library is the tools'.
FROM build-base AS cooker-build

COPY --from=xwa-host-tools /opt/xwa-host /opt/xwa-host
COPY --from=sdl-windows /opt/xwa-windows /opt/xwa-windows
COPY --from=zstd-windows /opt/xwa-windows /opt/xwa-windows
COPY --from=zstd-windows /deps/zstd/LICENSE /build/artifacts/zstd.txt
COPY cmake/toolchains/mingw-w64-x86_64.cmake /opt/xwa-toolchain.cmake
COPY aeron /src/aeron

ENV LD_LIBRARY_PATH=/opt/xwa-host/lib

RUN cmake -S /src/aeron -B /build/cooker -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/opt/xwa-toolchain.cmake \
        -DAERON_FSR3_SHADERGEN_EXECUTABLE=/opt/xwa-host/bin/aeron_fsr3_shadergen \
        -DAERON_SHADERCROSS_EXECUTABLE=/opt/xwa-host/bin/shadercross \
        -DAERON_BUILD_TOOLS=ON \
        -DAERON_ENABLE_VIDEO=OFF \
        -DAERON_GLTF_COOK_STANDALONE=ON \
    && cmake --build /build/cooker --target aeron_gltf_cook opt2gltf --parallel \
    && cp /build/cooker/aeron_gltf_cook.exe /build/cooker/opt2gltf.exe /build/artifacts/

# Only Windows' own DLLs: no SDL, zstd or MinGW runtime.
RUN for tool in aeron_gltf_cook opt2gltf; do \
        x86_64-w64-mingw32ucrt-objdump -p /build/artifacts/$tool.exe | grep 'DLL Name' \
        && ! x86_64-w64-mingw32ucrt-objdump -p /build/artifacts/$tool.exe \
            | grep -iE 'DLL Name: (SDL|libzstd|libstdc|libgcc|libwinpthread)' \
        || exit 1; \
    done

FROM scratch AS cooker-artifact
COPY --from=cooker-build /build/artifacts/ /

FROM scratch AS artifact
COPY --from=xwa-build /build/artifacts/ /
