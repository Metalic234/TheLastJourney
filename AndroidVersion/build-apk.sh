#!/usr/bin/env bash
# Build and sign The Last Journey APK from the current game sources.
#
#   ./build-apk.sh                  keep version name, versionCode + 1
#   ./build-apk.sh -n 1.5 -c 7      set version name and code
#   ./build-apk.sh -o /tmp/x.apk    write the result somewhere else
#   ./build-apk.sh -t base.apk      build from another base APK
#
# Needs JDK 17+, apktool, Android build-tools (apksigner + zipalign) and
# esbuild. If they are not on PATH, point at them with:
#   APKTOOL_JAR=/path/to/apktool.jar
#   ANDROID_HOME=/path/to/android-sdk   (or ANDROID_BUILD_TOOLS=/path/to/build-tools/37.0.0)
#   ESBUILD=/path/to/esbuild
#
# The script decodes the current APK as its base (keeping the activity and
# resources untouched), swaps in a freshly bundled game, signs with the
# release keystore and refuses to finish unless the certificate matches the
# one the installed app already has.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SIGNING="$HERE/signing"
PROPS="$SIGNING/keystore.properties"
TEMPLATE="$HERE/TheLastJourney.apk"
OUT="$TEMPLATE"

# Certificate of the release key. Every build must match this, otherwise the
# APK could not install over the copy already on people's phones.
EXPECTED_CERT="327f312be0dea7cb80509917312442bd26debfae9913023273c0dc72ebee6ef6"

VERSION_NAME=""
VERSION_CODE=""

usage() {
  cat <<'USAGE'
usage: build-apk.sh [-n VERSION_NAME] [-c VERSION_CODE] [-o OUTPUT_APK] [-t BASE_APK]

  -n, --version-name   versionName for the new build (default: keep current)
  -c, --version-code   versionCode for the new build (default: current + 1)
  -o, --out            where to write the signed APK (default: this folder)
  -t, --template       base APK to rebuild from (default: TheLastJourney.apk)
  -h, --help           show this help
USAGE
}

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

while [ $# -gt 0 ]; do
  case "$1" in
    -n|--version-name) VERSION_NAME="${2:-}"; [ -n "$VERSION_NAME" ] || die "$1 needs a value"; shift 2 ;;
    -c|--version-code) VERSION_CODE="${2:-}"; [ -n "$VERSION_CODE" ] || die "$1 needs a value"; shift 2 ;;
    -o|--out)          OUT="${2:-}";          [ -n "$OUT" ] || die "$1 needs a value"; shift 2 ;;
    -t|--template)     TEMPLATE="${2:-}";     [ -n "$TEMPLATE" ] || die "$1 needs a value"; shift 2 ;;
    -h|--help)         usage; exit 0 ;;
    *)                 die "unknown option: $1 (try --help)" ;;
  esac
done

# Windows Git Bash: accept C:\... style paths from the environment.
posix_path() {
  case "$1" in
    "") printf '%s' "" ;;
    *) if command -v cygpath >/dev/null 2>&1; then cygpath -u "$1"; else printf '%s' "$1"; fi ;;
  esac
}
APKTOOL_JAR="$(posix_path "${APKTOOL_JAR:-}")"
ESBUILD="$(posix_path "${ESBUILD:-}")"
ANDROID_HOME="$(posix_path "${ANDROID_HOME:-}")"
ANDROID_SDK_ROOT="$(posix_path "${ANDROID_SDK_ROOT:-}")"
ANDROID_BUILD_TOOLS="$(posix_path "${ANDROID_BUILD_TOOLS:-}")"

[ -f "$TEMPLATE" ]          || die "base APK not found: $TEMPLATE"
[ -f "$PROPS" ]             || die "missing $PROPS"
[ -f "$ROOT/src/main.js" ]  || die "missing game sources: $ROOT/src/main.js"
[ -f "$ROOT/styles.css" ]   || die "missing $ROOT/styles.css"
[ -f "$HERE/touch.js" ]     || die "missing touch overlay: $HERE/touch.js"

# --- tools -----------------------------------------------------------------
if [ -n "${APKTOOL_JAR:-}" ]; then
  [ -f "$APKTOOL_JAR" ] || die "APKTOOL_JAR not found: $APKTOOL_JAR"
  have java || die "java not found; install a JDK or set JAVA_HOME/PATH"
  APKTOOL=(java -jar "$APKTOOL_JAR")
elif have apktool; then
  APKTOOL=(apktool)
else
  die "apktool not found; set APKTOOL_JAR=/path/to/apktool.jar or put apktool on PATH"
fi

if [ -n "${ESBUILD:-}" ]; then
  [ -x "$ESBUILD" ] || die "ESBUILD is not executable: $ESBUILD"
  ESBUILD_CMD=("$ESBUILD")
elif have esbuild; then
  ESBUILD_CMD=("$(command -v esbuild)")
elif have npx; then
  ESBUILD_CMD=(npx --yes esbuild)
else
  die "esbuild not found; npm i -g esbuild, or set ESBUILD=/path/to/esbuild"
fi

find_build_tools() {
  local dir
  for base in "${ANDROID_BUILD_TOOLS:-}" "${ANDROID_HOME:-}" "${ANDROID_SDK_ROOT:-}"; do
    [ -n "$base" ] || continue
    if [ -x "$base/apksigner" ] || [ -x "$base/apksigner.bat" ]; then
      printf '%s\n' "$base"; return 0
    fi
    if [ -d "$base/build-tools" ]; then
      dir="$(ls -d "$base"/build-tools/*/ 2>/dev/null | sort -V | tail -n 1)"
      if [ -n "$dir" ] && { [ -x "${dir%/}/apksigner" ] || [ -x "${dir%/}/apksigner.bat" ]; }; then
        printf '%s\n' "${dir%/}"; return 0
      fi
    fi
  done
  if have apksigner && have zipalign; then
    dirname "$(command -v apksigner)"; return 0
  fi
  return 1
}

BT="$(find_build_tools)" || die "Android build-tools not found; set ANDROID_HOME or ANDROID_BUILD_TOOLS"
APKSIGNER="$BT/apksigner"; [ -x "$APKSIGNER" ] || APKSIGNER="$BT/apksigner.bat"
ZIPALIGN="$BT/zipalign";   [ -x "$ZIPALIGN" ]   || ZIPALIGN="$BT/zipalign.exe"
[ -x "$APKSIGNER" ] || die "apksigner not found in $BT"
[ -x "$ZIPALIGN" ]  || die "zipalign not found in $BT"

# --- keystore --------------------------------------------------------------
prop() { sed -n "s/^$1=//p" "$PROPS" | head -n 1 | tr -d '\r'; }
STORE_FILE="$(prop storeFile)"
STORE_PASS="$(prop storePassword)"
KEY_ALIAS="$(prop keyAlias)"
KEY_PASS="$(prop keyPassword)"
[ -n "$STORE_FILE" ] && [ -n "$STORE_PASS" ] && [ -n "$KEY_ALIAS" ] && [ -n "$KEY_PASS" ] \
  || die "keystore.properties needs storeFile, storePassword, keyAlias and keyPassword"
KEYSTORE="$SIGNING/$STORE_FILE"
[ -f "$KEYSTORE" ] || die "keystore not found: $KEYSTORE"

WORK="$(mktemp -d)"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

# --- decode the base APK ---------------------------------------------------
printf '==> Decoding base APK %s\n' "$TEMPLATE"
"${APKTOOL[@]}" d -f -s -o "$WORK/app" "$TEMPLATE" >/dev/null
APP="$WORK/app"
[ -d "$APP/assets/www" ] || die "base APK has no assets/www folder"

# --- rebuild the game bundle ----------------------------------------------
printf '==> Bundling game sources\n'
( cd "$ROOT" && "${ESBUILD_CMD[@]}" src/main.js --bundle --format=iife \
    --target=es2020 --charset=utf8 --log-level=warning \
    --outfile="$APP/assets/www/game.bundle.js" >/dev/null )
cp "$ROOT/styles.css" "$APP/assets/www/styles.css"
# Touch overlay lives in this folder so button fixes ship in a rebuild.
cp "$HERE/touch.js" "$APP/assets/www/touch.js"

# --- version ---------------------------------------------------------------
CUR_CODE="$(awk '/^  versionCode:/{print $2; exit}' "$APP/apktool.yml")"
CUR_NAME="$(awk '/^  versionName:/{print $2; exit}' "$APP/apktool.yml")"
[ -n "$CUR_CODE" ] || die "could not read versionCode from apktool.yml"
[ -n "$CUR_NAME" ] || die "could not read versionName from apktool.yml"

[ -n "$VERSION_CODE" ] || VERSION_CODE=$((CUR_CODE + 1))
[ -n "$VERSION_NAME" ] || VERSION_NAME="$CUR_NAME"
case "$VERSION_CODE" in
  *[!0-9]*) die "versionCode must be a number, got: $VERSION_CODE" ;;
esac

printf '==> Version %s (code %s)\n' "$VERSION_NAME" "$VERSION_CODE"
sed -e "s/^  versionCode: .*/  versionCode: $VERSION_CODE/" \
    -e "s/^  versionName: .*/  versionName: $VERSION_NAME/" \
    "$APP/apktool.yml" > "$APP/apktool.yml.tmp"
mv "$APP/apktool.yml.tmp" "$APP/apktool.yml"

# --- build, align, sign ----------------------------------------------------
printf '==> Building APK\n'
"${APKTOOL[@]}" b -o "$WORK/unsigned.apk" "$APP" >/dev/null

printf '==> Aligning APK\n'
"$ZIPALIGN" -p -f 4 "$WORK/unsigned.apk" "$WORK/aligned.apk"

printf '==> Signing APK\n'
PW_FILE="$WORK/passwords"
printf '%s\n%s\n' "$STORE_PASS" "$KEY_PASS" > "$PW_FILE"
chmod 600 "$PW_FILE"
"$APKSIGNER" sign \
  --ks "$KEYSTORE" \
  --ks-key-alias "$KEY_ALIAS" \
  --ks-pass "file:$PW_FILE" \
  --key-pass "file:$PW_FILE" \
  --v1-signing-enabled true \
  --v2-signing-enabled true \
  --v3-signing-enabled true \
  --out "$WORK/signed.apk" \
  "$WORK/aligned.apk"

# --- verify ----------------------------------------------------------------
printf '==> Verifying signature\n'
"$APKSIGNER" verify --print-certs "$WORK/signed.apk" > "$WORK/verify.txt" \
  || die "signature verification failed"
CERT="$(awk -F': ' '/certificate SHA-256 digest/{print tolower($NF); exit}' "$WORK/verify.txt")"
if [ "$CERT" != "$EXPECTED_CERT" ]; then
  die "signed with the wrong key (got $CERT, expected $EXPECTED_CERT); the build would not install over the current app"
fi

mkdir -p "$(dirname "$OUT")"
mv "$WORK/signed.apk" "$OUT"

printf '\nSigned %s\n' "$OUT"
printf '  package   com.zai.lastjourney\n'
printf '  version   %s (code %s)\n' "$VERSION_NAME" "$VERSION_CODE"
printf '  cert      %s\n' "$CERT"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$OUT" | sed 's/^/  sha256    /'
elif command -v shasum >/dev/null 2>&1; then
  shasum -a 256 "$OUT" | sed 's/^/  sha256    /'
fi
