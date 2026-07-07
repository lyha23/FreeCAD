#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/build_freecad_macos_homebrew.sh [options]

Configure and build FreeCAD on macOS/Homebrew using the dependency paths that
avoid Conda/SDK ICU mixing and PySide6 package-layout issues.

Options:
  --configure-only   Run CMake configure/generate only.
  --build-only       Skip configure and only build the existing build dir.
  --clean-cache      Remove CMakeCache.txt and CMakeFiles before configuring.
  --target NAME      Build one CMake target instead of the default all target.
  -h, --help         Show this help.

Environment overrides:
  BUILD_DIR          Default: <repo>/build/relwithdebinfo
  BUILD_TYPE         Default: RelWithDebInfo
  JOBS               Default: number of local CPUs
  PYTHON_EXECUTABLE  Default: <brew-prefix>/bin/python3
  ICU_ROOT           Default: brew --prefix icu4c@78, then icu4c
  MEDFILE_ROOT_DIR   Default: brew --prefix med-file@4.1.1_py312
  BUILD_BIM          Default: OFF. Set ON after installing lark for Python.
EOF
}

die() {
    echo "error: $*" >&2
    exit 1
}

info() {
    echo "==> $*"
}

brew_prefix_for() {
    local formula="$1"
    brew --prefix "${formula}" 2>/dev/null || true
}

ensure_symlink() {
    local target="$1"
    local link="$2"

    [[ -e "${target}" ]] || return 0

    if [[ -e "${link}" && ! -L "${link}" ]]; then
        die "${link} exists and is not a symlink; move it aside or set FIX_PYSIDE_LINKS=0"
    fi

    ln -sfn "${target}" "${link}"
}

configure_only=0
build_only=0
clean_cache=0
target=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --configure-only)
            configure_only=1
            shift
            ;;
        --build-only)
            build_only=1
            shift
            ;;
        --clean-cache)
            clean_cache=1
            shift
            ;;
        --target)
            [[ $# -ge 2 ]] || die "--target requires a value"
            target="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown argument: $1"
            ;;
    esac
done

[[ "${configure_only}" == "1" && "${build_only}" == "1" ]] && \
    die "--configure-only and --build-only cannot be used together"

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

brew_prefix="${BREW_PREFIX:-$(brew --prefix 2>/dev/null || true)}"
[[ -n "${brew_prefix}" ]] || brew_prefix="/opt/homebrew"
[[ -d "${brew_prefix}" ]] || die "Homebrew prefix not found: ${brew_prefix}"

build_type="${BUILD_TYPE:-RelWithDebInfo}"
build_dir="${BUILD_DIR:-${repo_root}/build/relwithdebinfo}"
python_executable="${PYTHON_EXECUTABLE:-${brew_prefix}/bin/python3}"
jobs="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}"
build_bim="${BUILD_BIM:-OFF}"

icu_root="${ICU_ROOT:-$(brew_prefix_for icu4c@78)}"
[[ -n "${icu_root}" ]] || icu_root="$(brew_prefix_for icu4c)"
[[ -n "${icu_root}" ]] || die "ICU not found. Try: brew install icu4c@78"

medfile_root="${MEDFILE_ROOT_DIR:-$(brew_prefix_for med-file@4.1.1_py312)}"
[[ -n "${medfile_root}" ]] || medfile_root="$(brew_prefix_for med-file)"
[[ -n "${medfile_root}" ]] || die "MEDFile not found. Install a med-file Homebrew formula or configure with -DBUILD_SMESH=OFF"

[[ -x "${python_executable}" ]] || die "Python executable not found: ${python_executable}"
[[ -f "${icu_root}/lib/libicuuc.dylib" ]] || die "ICU uc library not found under ${icu_root}"
[[ -f "${icu_root}/lib/libicui18n.dylib" ]] || die "ICU i18n library not found under ${icu_root}"
[[ -f "${medfile_root}/include/med.h" ]] || die "med.h not found under ${medfile_root}/include"

export PATH="${brew_prefix}/bin:${brew_prefix}/sbin:/usr/bin:/bin:/usr/sbin:/sbin:${PATH}"

if [[ "${FIX_PYSIDE_LINKS:-1}" == "1" ]]; then
    pyside_root="$(brew_prefix_for pyside)"
    if [[ -n "${pyside_root}" && -d "${pyside_root}" ]]; then
        info "Repairing Homebrew PySide/Shiboken compatibility symlinks"
        ensure_symlink "${brew_prefix}/share/PySide6/typesystems" "${brew_prefix}/typesystems"
        ensure_symlink "${brew_prefix}/share/PySide6/glue" "${brew_prefix}/glue"
        ensure_symlink "${pyside_root}/PySide6" "${brew_prefix}/PySide6"
        ensure_symlink "${pyside_root}/shiboken6" "${brew_prefix}/shiboken6"
    fi
fi

cmake_configure_args=(
    -S "${repo_root}"
    -B "${build_dir}"
    -U "ICU_*"
    -U "MEDFILE_*"
    -DCMAKE_BUILD_TYPE="${build_type}"
    -DPython3_EXECUTABLE="${python_executable}"
    -DICU_ROOT="${icu_root}"
    -DICU_INCLUDE_DIR="${icu_root}/include"
    -DICU_UC_LIBRARY_RELEASE="${icu_root}/lib/libicuuc.dylib"
    -DICU_I18N_LIBRARY_RELEASE="${icu_root}/lib/libicui18n.dylib"
    -DMEDFILE_ROOT_DIR="${medfile_root}"
    -DBUILD_BIM="${build_bim}"
)

cmake_build_args=(
    --build "${build_dir}"
    --parallel "${jobs}"
)

if [[ -n "${target}" ]]; then
    cmake_build_args+=(--target "${target}")
fi

cd "${repo_root}"

if [[ "${build_only}" != "1" ]]; then
    if [[ "${clean_cache}" == "1" ]]; then
        info "Removing CMake cache from ${build_dir}"
        rm -f "${build_dir}/CMakeCache.txt"
        rm -rf "${build_dir}/CMakeFiles"
    fi

    info "Configuring ${build_dir}"
    cmake "${cmake_configure_args[@]}"
fi

if [[ "${configure_only}" != "1" ]]; then
    info "Building ${build_dir} with ${jobs} job(s)"
    cmake "${cmake_build_args[@]}"
fi
