LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wno-address-of-packed-member -DJUICE_EXPORTS
LOCAL_MODULE := libjuice
LIBJUICE_SRC_FILES := src/addr.c \
					src/agent.c \
					src/base64.c \
					src/conn.c \
					src/conn_mux.c \
					src/conn_poll.c \
					src/conn_thread.c \
					src/const_time.c \
					src/crc32.c \
					src/hash.c \
					src/hmac.c \
					src/ice.c \
					src/juice.c \
					src/log.c \
					src/random.c \
					src/server.c \
					src/stun.c \
					src/timestamp.c \
					src/turn.c \
					src/udp.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include/juice
LOCAL_SRC_FILES := $(addprefix $(LOCAL_PATH)/,$(LIBJUICE_SRC_FILES))
include $(BUILD_STATIC_LIBRARY)