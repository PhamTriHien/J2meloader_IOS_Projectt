#!/bin/bash
set -e

echo "=== Building J2ME-Loader for iOS (.IPA) ==="

# 1. Clean and Archive
echo "[1/3] Archiving Xcode project..."
xcodebuild clean archive \
  -project J2MELoader-iOS.xcodeproj \
  -scheme J2MELoader-iOS \
  -configuration Release \
  -archivePath build/J2MELoader-iOS.xcarchive \
  -destination "generic/platform=iOS" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  AD_HOC_CODE_SIGNING_ALLOWED=YES \
  PROVISIONING_PROFILE=""

# 2. Package Payload directory
echo "[2/3] Packaging Payload folder..."
rm -rf build/Payload build/J2MELoader-iOS.ipa
mkdir -p build/Payload
cp -R build/J2MELoader-iOS.xcarchive/Products/Applications/*.app build/Payload/

# 3. Zip to .ipa
echo "[3/3] Creating .ipa package..."
cd build
zip -r -y J2MELoader-iOS.ipa Payload
cd ..

echo "=== SUCCESS: IPA built at ios/J2MELoader-iOS/build/J2MELoader-iOS.ipa ==="