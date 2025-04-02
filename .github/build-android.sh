#!/bin/bash

# Set the architecture passed as an argument
ARCHITECTURE=$1

# Define the architecture mapping
case $ARCHITECTURE in
  "i686")
    TARGET_ANDROID_ARCH="x86"
    ;;
  "x86_64")
    TARGET_ANDROID_ARCH="x86_64"
    ;;
  "arm")
    TARGET_ANDROID_ARCH="armeabi-v7a"
    ;;
  "aarch64")
    TARGET_ANDROID_ARCH="arm64-v8a"
    ;;
  *)
    echo "Unsupported architecture: $ARCHITECTURE"
    exit 1
    ;;
esac

echo "Using TARGET_ANDROID_ARCH: $TARGET_ANDROID_ARCH"

apt-mark hold termux-exec
apt-get upgrade -yq -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold"
pkg install -y git wget make python zip clang binutils apksigner aapt libglvnd-dev sdl2 ndk-sysroot

chmod 777 -R .
make -C lib/src/lua clean || true
make -C lib/src/lua -j$(nproc) linux || exit 1

# Set the number of retries for 'armeabi-v7a' architecture
if [ "$TARGET_ANDROID_ARCH" == "armeabi-v7a" ]; then
  attempt=0
  max_attempts=3
  until [ $attempt -ge $max_attempts ]
  do
    echo "Attempt $((attempt+1)) of $max_attempts..."
    make TARGET_ARCH=$ARCHITECTURE TARGET_BITS=32 ARCH_APK=$TARGET_ANDROID_ARCH TARGET_ANDROID=1 NOEXTRACT=1 -j32 && break
    attempt=$((attempt+1))
    echo "Retrying in 5 seconds..."
    sleep 5
  done
else
  # For other architectures, run the make command once
  make TARGET_ARCH=$ARCHITECTURE ARCH_APK=$TARGET_ANDROID_ARCH TARGET_ANDROID=1 NOEXTRACT=1 -j32
fi