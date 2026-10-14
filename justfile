# Debug and release builds live side by side under build/. Both copy their
# plugins to build/plugins.

debug_dir := "build/debug"
release_dir := "build/release"
plugin_dir := "build/plugins"

default:
    @just --list

build:
    cmake -S . -B {{debug_dir}} -DCMAKE_BUILD_TYPE=Debug
    cmake --build {{debug_dir}} -j
    @just _publish {{debug_dir}}
    ln -sf {{debug_dir}}/compile_commands.json compile_commands.json

release:
    cmake -S . -B {{release_dir}} -DCMAKE_BUILD_TYPE=Release
    cmake --build {{release_dir}} -j
    @just _publish {{release_dir}}

install: release
    #!/usr/bin/env bash
    set -euo pipefail
    dir=$(cmake -L -N {{release_dir}} | sed -n 's/^DLT_PLUGIN_INSTALL_DIR:PATH=//p')
    parent=$dir
    while [ ! -e "$parent" ]; do parent=$(dirname "$parent"); done
    if [ -w "$parent" ]; then
        cmake --install {{release_dir}}
    else
        sudo cmake --install {{release_dir}}
    fi

lint: build
    run-clang-tidy -p {{debug_dir}} -quiet \
        -source-filter '^{{justfile_directory()}}/(?!build/)' \
        -header-filter '^{{justfile_directory()}}/' \
        -exclude-header-filter '^{{justfile_directory()}}/build/' \
        -removed-arg=-mno-direct-extern-access

_publish dir:
    mkdir -p {{plugin_dir}}
    cp --remove-destination {{dir}}/out/plugins/* {{plugin_dir}}/

format:
    git ls-files --cached --others --exclude-standard '*.cpp' '*.h' | xargs clang-format -i

clean:
    rm -rf build
