# Save the local path
my_LOCAL_PATH := $(call my-dir)

FREETYPE_LIBRARY_PATH := external/freetype-2.4.12

# Build freetype library
ifneq ($(FREETYPE_LIBRARY_PATH),)
    include $(my_LOCAL_PATH)/$(FREETYPE_LIBRARY_PATH)/Android.mk
endif

# Restore local path
LOCAL_PATH := $(my_LOCAL_PATH)
include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_ttf

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_SRC_FILES := SDL_ttf.c

ifneq ($(FREETYPE_LIBRARY_PATH),)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(FREETYPE_LIBRARY_PATH)/include
    LOCAL_STATIC_LIBRARIES += freetype
endif

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_EXPORT_C_INCLUDES += $(LOCAL_C_INCLUDES)

include $(BUILD_SHARED_LIBRARY)
