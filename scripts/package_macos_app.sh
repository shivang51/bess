#!/bin/zsh
# Packages a macOS Release build (./scripts/build_release.sh) into Bess.app.
#
#   Bess.app/Contents/MacOS       Bess, libBessRuntime.dylib, libwebgpu_dawn.dylib
#   Bess.app/Contents/Frameworks  Python.framework and the non-system dylibs
#                                 (freetype, openssl, ...) the binaries link
#   Bess.app/Contents/Resources   assets/, plugins/, bindings/bessplug/, Bess.icns
#
# The app is self-contained apart from yosys, which is only found if it is on
# the PATH of the launching process.
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN_DIR="$REPO_ROOT/bin/Release/$(uname -m)"
APP_DIR="$BIN_DIR/Bess.app"
CONTENTS="$APP_DIR/Contents"
VERSION="${BESS_VERSION:-0.1.0}"

for required in Bess libBessRuntime.dylib libwebgpu_dawn.dylib assets plugins bindings/bessplug; do
    if [ ! -e "$BIN_DIR/$required" ]; then
        echo "[-] Missing $BIN_DIR/$required. Run ./scripts/build_release.sh first."
        exit 1
    fi
done

echo "-- Creating $APP_DIR"
rm -rf "$APP_DIR"
mkdir -p "$CONTENTS/MacOS" "$CONTENTS/Resources/bindings"

cp "$BIN_DIR/Bess" "$BIN_DIR/libBessRuntime.dylib" "$BIN_DIR/libwebgpu_dawn.dylib" \
    "$CONTENTS/MacOS/"
cp -R "$BIN_DIR/assets" "$BIN_DIR/plugins" "$CONTENTS/Resources/"
cp -R "$BIN_DIR/bindings/bessplug" "$CONTENTS/Resources/bindings/"
find "$CONTENTS/Resources" \( -name '__pycache__' -o -name '*.pyc' \) -prune -exec rm -rf {} +

# The Python module keeps the absolute build-tree rpath; point it at
# Contents/MacOS instead (Resources/bindings/bessplug -> ../../../MacOS).
for module in "$CONTENTS"/Resources/bindings/bessplug/bessplug*.so(N); do
    for rpath in $(otool -l "$module" | awk '/LC_RPATH/ {getline; getline; print $2}'); do
        install_name_tool -delete_rpath "$rpath" "$module"
    done
    install_name_tool -add_rpath "@loader_path/../../../MacOS" "$module"
done

FRAMEWORKS="$CONTENTS/Frameworks"
FRAMEWORKS_REF="@executable_path/../Frameworks"

# Embed the Python framework the binaries were linked against. Only the
# interpreter, its Info.plist, and the stdlib (minus tests, IDLE, tkinter and
# site-packages) are copied.
PY_LIB="$(otool -L "$CONTENTS/MacOS/Bess" | awk '/Python\.framework/ {print $1; exit}')"
if [ -z "$PY_LIB" ]; then
    echo "[-] Bess does not link a Python framework; cannot embed Python."
    exit 1
fi
PY_VERSION_DIR="${PY_LIB:h}"
PY_VERSION="${PY_VERSION_DIR:t}"
PY_SRC="$(cd "$PY_VERSION_DIR" && pwd -P)"
PY_FRAMEWORK="$FRAMEWORKS/Python.framework"
PY_DST="$PY_FRAMEWORK/Versions/$PY_VERSION"
PY_DST_REF="$FRAMEWORKS_REF/Python.framework/Versions/$PY_VERSION"

echo "-- Embedding Python $PY_VERSION from $PY_SRC"
mkdir -p "$PY_DST/Resources" "$PY_DST/lib/python$PY_VERSION/site-packages"
cp "$PY_SRC/Python" "$PY_DST/"
cp "$PY_SRC/Resources/Info.plist" "$PY_DST/Resources/"
rsync -a \
    --exclude='/test/' --exclude='/idlelib/' --exclude='/tkinter/' \
    --exclude='/turtledemo/' --exclude='/ensurepip/' --exclude='/site-packages' \
    --exclude='/config-*/' --exclude='/lib-dynload/_tkinter*' \
    --exclude='/lib-dynload/_test*' --exclude='/lib-dynload/xx*' \
    "$PY_SRC/lib/python$PY_VERSION/" "$PY_DST/lib/python$PY_VERSION/"
ln -s "$PY_VERSION" "$PY_FRAMEWORK/Versions/Current"
ln -s Versions/Current/Python "$PY_FRAMEWORK/Python"
ln -s Versions/Current/Resources "$PY_FRAMEWORK/Resources"
chmod -R u+w "$FRAMEWORKS"

# install_name_tool warns on every signed file; the bundle is re-signed at the end.
retarget() {
    install_name_tool "$@" 2>&1 | { grep -v 'will invalidate the code signature' >&2 || true; }
    return ${pipestatus[1]}
}

# Load commands of a Mach-O file. Dependency lines are tab-indented; the
# others are "<file>:" headers, one per architecture in a universal binary.
dependencies() {
    otool -L "$1" | awk '/^\t/ {print $1}'
}

is_macho() {
    file -b "$1" | grep -q '^Mach-O'
}

# Rewrites every non-system dependency of a Mach-O file to point inside the
# bundle, copying the dependency in first if needed. Files inside the Python
# framework keep their relative layout; everything else goes flat into
# Contents/Frameworks. Newly copied files are queued for the same treatment.
typeset -a QUEUE
relink() {
    local file="$1" dep target ref
    for dep in $(dependencies "$file"); do
        case "$dep" in
            /usr/lib/*|/System/*|@*) continue ;;
        esac
        if [[ "$dep" == "$PY_VERSION_DIR"/* || "$dep" == "$PY_SRC"/* ]]; then
            local rel="${dep#$PY_VERSION_DIR/}"
            rel="${rel#$PY_SRC/}"
            target="$PY_DST/$rel"
            ref="$PY_DST_REF/$rel"
        else
            target="$FRAMEWORKS/${dep:t}"
            ref="$FRAMEWORKS_REF/${dep:t}"
        fi
        if [ ! -e "$target" ]; then
            mkdir -p "${target:h}"
            cp -L "$dep" "$target"
            chmod u+w "$target"
            QUEUE+=("$target")
        fi
        if [ "$file" = "$target" ]; then
            continue
        fi
        retarget -change "$dep" "$ref" "$file"
    done
}

echo "-- Bundling dependencies"
QUEUE=()
for file in $(find "$CONTENTS" -type f \( -perm -u+x -o -name '*.so' -o -name '*.dylib' \)); do
    is_macho "$file" && QUEUE+=("$file")
done
QUEUE+=("$PY_DST/Python")
typeset -A SEEN
typeset -a MACHO_FILES
while (( ${#QUEUE} )); do
    file="${QUEUE[1]}"
    QUEUE=("${(@)QUEUE[2,-1]}")
    [ -n "${SEEN[$file]}" ] && continue
    SEEN[$file]=1
    MACHO_FILES+=("$file")
    # Python.org frameworks are universal; keep only the slice the app runs.
    if [ "$(lipo -archs "$file" | wc -w)" -gt 1 ]; then
        lipo "$file" -thin "$(uname -m)" -output "$file"
    fi
    case "$file" in
        "$PY_DST"/Python) retarget -id "$PY_DST_REF/Python" "$file" ;;
        "$FRAMEWORKS"/*.dylib) retarget -id "$FRAMEWORKS_REF/${file:t}" "$file" ;;
    esac
    relink "$file"
done

leftover="$(for file in $MACHO_FILES; do dependencies "$file"; done \
    | grep -vE '^(/usr/lib/|/System/|@)' | sort -u)"
if [ -n "$leftover" ]; then
    echo "[-] Unbundled dependencies remain:"
    echo "$leftover"
    exit 1
fi

echo "-- Generating icon"
ICONSET="$(mktemp -d)/Bess.iconset"
mkdir -p "$ICONSET"
LOGO="$REPO_ROOT/assets/images/logo/BessLogo4x.png"
for size in 16 32 128 256 512; do
    sips -z $size $size "$LOGO" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
    sips -z $((size * 2)) $((size * 2)) "$LOGO" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$CONTENTS/Resources/Bess.icns"
rm -rf "$(dirname "$ICONSET")"

cat > "$CONTENTS/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Bess</string>
    <key>CFBundleDisplayName</key>
    <string>BESS</string>
    <key>CFBundleIdentifier</key>
    <string>io.github.shivang51.bess</string>
    <key>CFBundleExecutable</key>
    <string>Bess</string>
    <key>CFBundleIconFile</key>
    <string>Bess</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>LSMinimumSystemVersion</key>
    <string>26.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

# install_name_tool invalidates signatures; arm64 refuses to load unsigned code.
# Sign every binary individually (codesign --deep skips the loose .so files in
# the Python stdlib), then the framework and the app, inside out.
echo "-- Signing (ad-hoc)"
sign() {
    codesign --force --sign - "$1" 2>&1 | { grep -v 'replacing existing signature' >&2 || true; }
    return ${pipestatus[1]}
}
for file in $MACHO_FILES; do
    sign "$file"
done
sign "$PY_FRAMEWORK"
sign "$APP_DIR"
codesign --verify --deep --strict "$APP_DIR"

echo "-- Done: $APP_DIR"
