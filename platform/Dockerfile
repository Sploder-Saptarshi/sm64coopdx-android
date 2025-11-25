FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y \
        android-sdk \
        sdkmanager \
        build-essential \
        bsdmainutils \
        libreadline-dev \
        libglew-dev \
        libsdl2-dev \
        libcurl4-openssl-dev

ENV ANDROID_SDK_ROOT=/usr/lib/android-sdk
ENV PATH=${ANDROID_SDK_ROOT}/tools/bin:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:${PATH}

RUN yes | sdkmanager --licenses

WORKDIR /sm64
