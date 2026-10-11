#!/bin/zsh
# Packages a macOS Release build (./scripts/build_release.sh) into Bess.app.
#
#   Bess.app/Contents/MacOS      Bess, libBessRuntime.dylib, libwebgpu_dawn.dylib
#   Bess.app/Contents/Resources  assets/, plugins/, bindings/bessplug/, Bess.icns
#
# The app still needs the Homebrew Python it was built against, and yosys is
# only found if it is on the PATH of the launching process.
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
echo "-- Signing (ad-hoc)"
codesign --force --deep --sign - "$APP_DIR"
codesign --verify --deep --strict "$APP_DIR"

echo "-- Done: $APP_DIR"
