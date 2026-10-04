# Sourced by the build scripts of the cores built from my forks: each fork is a
# sibling checkout, ../PS5_<Name> (github.com/mihawk-99/PS5_<Name>), whose main
# branch holds the port, and the script pins it by revision. Set core_name and
# source tools/core-stamp.sh first.
#
#   core_fork_setup                        the SDK, its compilers and the link recipe
#   core_fork_checkout FORK REVISION [submodules] [keep...]
#                                          a clean tree at the pin in source_dir
#   core_fork_info FILE SHA256             the core's libretro-core-info file, pinned
#   core_fork_stage BUILT INFO REVISION INPUT...
#                                          ABI check, stage, provenance, stamp
#
# A core links like every other one here: no libc or C++ runtime of its own, the
# console's libraries resolved at load time by the title's native loader
# (tooling/native/ps5-core.ld), and the core-local destructor registry
# (tooling/native/core_cxx_runtime.cpp) linked in.

core_info_revision=5a74858ab2f7a50cebb5a6330895bc38899531c0

core_fork_setup() {
    sdk="$root/.deps/native/ps5-payload-sdk"
    [[ -x $sdk/bin/prospero-clang ]] || { echo "error: bootstrap this project's SDK first" >&2; exit 2; }
    export PS5_PAYLOAD_SDK="$sdk" PS5_CLANG=/usr/bin/clang
    export CC="$sdk/bin/prospero-clang" CXX="$sdk/bin/prospero-clang++" AR="$sdk/bin/prospero-ar"
    core_work="$root/build/cores/$core_name"
    mkdir -p "$core_work/empty-libs" "$root/build/cores/stage/cores" "$root/build/cores/stage/info"
    "$CXX" -std=c++11 -fPIC -fno-exceptions -fno-rtti -c \
        "$root/tooling/native/core_cxx_runtime.cpp" -o "$core_work/core_cxx_runtime.o"
    # Upstream build files name libm, librt, libpthread, libdl and libutil; on
    # the console their functions are in libSceLibcInternal and libkernel_web (or
    # absent, as libutil's pseudo-terminals are), so empty archives satisfy the
    # names.
    local lib
    for lib in m rt pthread dl util; do
        [[ -f $core_work/empty-libs/lib$lib.a ]] || "$AR" rc "$core_work/empty-libs/lib$lib.a"
    done
    core_cc="${core_ccache:+$core_ccache }$CC"
    core_cxx="${core_ccache:+$core_ccache }$CXX"
    core_ldflags="-nostdlib -nodefaultlibs -Wl,-z,undefs -Wl,--build-id=sha1"
    core_ldflags+=" -Wl,-T,$root/tooling/native/ps5-core.ld -L$core_work/empty-libs"
    core_libs="$core_work/core_cxx_runtime.o -lkernel_web -lSceLibcInternal -lScePosixForWebKit"
}

# The fork beside this repository when it is there (no download), the published
# one otherwise; either way the tree is checked out at the pinned revision and
# cleaned, so nothing but the commit builds. KEEP names paths the clean spares,
# for a core whose build directory is inside its tree and too large to redo.
core_fork_checkout() {
    local fork_name=$1 revision=$2 submodules=${3:-} keep=() path origin got
    shift 2
    [[ $# == 0 ]] || shift
    for path in "$@"; do keep+=(-e "$path"); done
    local fork="$root/../$fork_name"
    source_dir="$root/.deps/$core_name-src"
    if [[ ! -d $source_dir/.git ]]; then
        origin=https://github.com/mihawk-99/$fork_name.git
        [[ -d $fork/.git ]] && origin=$(cd -- "$fork" && pwd)
        echo "==> [$core_name] fetching $revision from $origin"
        rm -rf -- "$source_dir"
        git clone --quiet --no-checkout "$origin" "$source_dir"
    fi
    git -C "$source_dir" cat-file -e "$revision^{commit}" 2>/dev/null ||
        git -C "$source_dir" fetch --quiet --no-recurse-submodules origin "$revision"
    git -C "$source_dir" checkout --force --quiet "$revision"
    git -C "$source_dir" clean -qfdx "${keep[@]}"
    if [[ $submodules == submodules ]]; then
        # A fork's own submodule forks are named by relative URL (../PS5_<Name>):
        # the sibling checkout here, which git fetches only when told to.
        git -C "$source_dir" submodule sync --recursive --quiet
        git -C "$source_dir" -c protocol.file.allow=always submodule update --init --recursive --quiet
        git -C "$source_dir" submodule foreach --quiet --recursive 'git clean -qfdx && git checkout --force --quiet HEAD'
    fi
    got=$(git -C "$source_dir" rev-parse HEAD)
    [[ $got == "$revision" ]] || { echo "error: the tree is at $got, wanted $revision" >&2; exit 2; }
    # Reproducibility: __DATE__ and __TIME__ are the pinned commit's own time.
    SOURCE_DATE_EPOCH=$(git -C "$source_dir" show -s --format=%ct "$revision")
    export SOURCE_DATE_EPOCH
}

core_fork_info() {
    local file=$1 digest=$2 cache="$root/.deps/downloads"
    mkdir -p "$cache"
    if [[ ! -f $cache/$file ]]; then
        curl --fail --location --retry 3 \
            "https://raw.githubusercontent.com/libretro/libretro-core-info/$core_info_revision/$file" \
            -o "$cache/$file.download"
        mv -- "$cache/$file.download" "$cache/$file"
    fi
    printf '%s  %s\n' "$digest" "$cache/$file" | sha256sum --check --status || {
        echo "error: cached input digest mismatch: $cache/$file" >&2; exit 1;
    }
    core_info="$cache/$file"
}

core_fork_stage() {
    local built=$1 info=$2 revision=$3
    shift 3
    local lib
    lib=$(basename -- "$built")
    cp -- "$built" "$core_work/$lib"
    python3 "$root/tools/check-core.py" "$core_work/$lib" --report "$core_work/abi.json"
    cp -- "$core_work/$lib" "$root/build/cores/stage/cores/$lib"
    cp -- "$info" "$root/build/cores/stage/info/${lib%.so}.info"
    python3 - "$core_work" "$revision" "$info" "$core_info_revision" "$@" <<'PY'
import hashlib, json, pathlib, sys
work, revision, info, info_revision = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
report = json.loads((work / 'abi.json').read_text())
report.update(source_revision=revision, info_revision=info_revision, info_sha256=sha(info))
report['port_inputs_sha256'] = {name: sha(name) for name in
    ['tools/core-fork.sh', 'tooling/native/ps5-core.ld', 'tooling/native/core_cxx_runtime.cpp', *sys.argv[5:]]}
(work / 'build.json').write_text(json.dumps(report, indent=2) + '\n')
PY
    core_stamp_write
    printf '==> [%s] built and ABI-checked revision %s; console loading is a separate gate\n' "$core_name" "$revision"
}
