LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := e3.c 

LOCAL_CFLAGS := \
    -std=gnu99 \
    -DLIBDIR=\"/system/usr/share\" \
    -DLINUX=1 -DARMCPU=1

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES += libc

LOCAL_MODULE := e3c
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

