#!/bin/bash

echo "=== Building J2ME-Loader for iOS (.IPA) ==="

rm -rf build
mkdir -p build/Release-iphoneos build/Payload

echo "[1/3] Compiling and Building Xcode project..."
xcodebuild build \
  -project J2MELoader-iOS.xcodeproj \
  -scheme J2MELoader-iOS \
  -configuration Release \
  -sdk iphoneos \
  -destination "generic/platform=iOS" \
  CONFIGURATION_BUILD_DIR="$(pwd)/build/Release-iphoneos" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  AD_HOC_CODE_SIGNING_ALLOWED=YES > build/xcodebuild.log 2>&1

BUILD_STATUS=$?

if [ $BUILD_STATUS -ne 0 ]; then
  echo "=== XCODEBUILD FAILED (Exit Code: $BUILD_STATUS) ==="
  cat build/xcodebuild.log
  
  if [ -n "$GITHUB_STEP_SUMMARY" ]; then
    echo "### Xcodebuild Build Errors" >> "$GITHUB_STEP_SUMMARY"
    echo '```' >> "$GITHUB_STEP_SUMMARY"
    tail -n 100 build/xcodebuild.log >> "$GITHUB_STEP_SUMMARY"
    echo '```' >> "$GITHUB_STEP_SUMMARY"
  fi
  
  # Try to commit error log to branch ci-debug
  git config --global user.name "github-actions[bot]"
  git config --global user.email "github-actions[bot]@users.noreply.github.com"
  git checkout -B ci-debug || true
  cp build/xcodebuild.log xcodebuild_error.log
  git add -f xcodebuild_error.log
  git commit -m "ci: capture xcodebuild error log [skip ci]" || true
  git push -f origin ci-debug || true
  
  exit $BUILD_STATUS
fi

echo "=== Xcodebuild Succeeded! ==="

echo "[2/3] Packaging Payload folder..."
cp -R build/Release-iphoneos/*.app build/Payload/

echo "[3/3] Creating .ipa package..."
cd build
zip -r -y J2MELoader-iOS.ipa Payload
cd ..

echo "=== SUCCESS: IPA built at $(pwd)/build/J2MELoader-iOS.ipa ==="