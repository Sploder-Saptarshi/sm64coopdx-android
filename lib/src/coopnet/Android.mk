LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_CFLAGS := -DJUICE_STATIC -DTARGET_ANDROID
LOCAL_MODULE := coopnet
COOPNET_SRC_FILES := common/client.cpp \
					common/connection.cpp \
					common/libcoopnet.cpp \
					common/lobby.cpp \
					common/mpacket.cpp \
					common/peer.cpp \
					common/server.cpp \
					common/socket.cpp \
					common/utils.cpp
LOCAL_STATIC_LIBRARIES := libjuice
LOCAL_C_INCLUDES := $(LOCAL_PATH)/common $(LOCAL_PATH)/lib/include
LOCAL_SRC_FILES := $(addprefix $(LOCAL_PATH)/,$(COOPNET_SRC_FILES))
include $(BUILD_STATIC_LIBRARY)