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

# RESSOURCES

include $(CLEAR_VARS)

LOCAL_MODULE := e3ws.hlp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/share

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE := e3.res
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/share

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)

