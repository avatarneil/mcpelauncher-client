#include "gles_arm64_bridge.h"
#include <cstring>

static GLESCopyImage copyImage;
static GLESTexSubImage3D upload;
static GLESCompressedTexSubImage3D compressedUpload;
static GLESBlitFramebuffer blit;

// This shim lets the headless test send AAPCS64-shaped arguments through the
// exact production template, then verify the native driver's resulting pixels.
extern "C" void* bridge(const char* name, void* native) {
    if(std::strcmp(name, "copy") == 0) {
        copyImage = reinterpret_cast<GLESCopyImage>(native);
        return reinterpret_cast<void*>(&AndroidGLESBridge<copyImage>::invoke);
    }
    if(std::strcmp(name, "upload") == 0) {
        upload = reinterpret_cast<GLESTexSubImage3D>(native);
        return reinterpret_cast<void*>(&AndroidGLESBridge<upload>::invoke);
    }
    if(std::strcmp(name, "compressed") == 0) {
        compressedUpload = reinterpret_cast<GLESCompressedTexSubImage3D>(native);
        return reinterpret_cast<void*>(&AndroidGLESBridge<compressedUpload>::invoke);
    }
    if(std::strcmp(name, "blit") == 0) {
        blit = reinterpret_cast<GLESBlitFramebuffer>(native);
        return reinterpret_cast<void*>(&AndroidGLESBridge<blit>::invoke);
    }
    return nullptr;
}
