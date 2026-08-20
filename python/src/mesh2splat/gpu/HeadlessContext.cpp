///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Headless OpenGL Context Impl        //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "HeadlessContext.hpp"
#include <cstring>

// Platform detection
#if defined(__APPLE__)
    #define M2S_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define M2S_PLATFORM_LINUX 1
#elif defined(_WIN32)
    #define M2S_PLATFORM_WINDOWS 1
#else
    #define M2S_PLATFORM_UNSUPPORTED 1
#endif

//------------------------------------------------------------------------------
// macOS CGL Implementation
//------------------------------------------------------------------------------
#if M2S_PLATFORM_MACOS

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

namespace mesh2splat {

class HeadlessContext::Impl {
public:
    CGLContextObj context_ = nullptr;
    CGLPixelFormatObj pixelFormat_ = nullptr;
    std::string errorMessage_;
    int majorVersion_ = 4;
    int minorVersion_ = 1;
    
    Impl(int majorVersion, int minorVersion)
        : majorVersion_(majorVersion), minorVersion_(minorVersion) {
        
        // macOS only supports up to OpenGL 4.1 (Legacy profile)
        // Request OpenGL 3.2+ Core Profile which gives us 4.1 on modern Macs
        CGLPixelFormatAttribute attributes[] = {
            kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
            kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
            kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
            kCGLPFAAccelerated,
            kCGLPFAAllowOfflineRenderers, // Allow headless GPU
            (CGLPixelFormatAttribute)0
        };
        
        GLint numPixelFormats = 0;
        CGLError error = CGLChoosePixelFormat(attributes, &pixelFormat_, &numPixelFormats);
        
        if (error != kCGLNoError || numPixelFormats == 0) {
            errorMessage_ = "Failed to find suitable pixel format: ";
            errorMessage_ += CGLErrorString(error);
            return;
        }
        
        error = CGLCreateContext(pixelFormat_, nullptr, &context_);
        if (error != kCGLNoError) {
            errorMessage_ = "Failed to create CGL context: ";
            errorMessage_ += CGLErrorString(error);
            CGLDestroyPixelFormat(pixelFormat_);
            pixelFormat_ = nullptr;
            return;
        }
    }
    
    ~Impl() {
        if (context_) {
            CGLSetCurrentContext(nullptr);
            CGLDestroyContext(context_);
        }
        if (pixelFormat_) {
            CGLDestroyPixelFormat(pixelFormat_);
        }
    }
    
    bool makeCurrent() {
        if (!context_) return false;
        CGLError error = CGLSetCurrentContext(context_);
        return error == kCGLNoError;
    }
    
    void release() {
        CGLSetCurrentContext(nullptr);
    }
    
    bool isValid() const {
        return context_ != nullptr;
    }
    
    std::string getGLVersion() const {
        if (!context_) return "N/A";
        CGLSetCurrentContext(context_);
        const char* version = (const char*)glGetString(GL_VERSION);
        return version ? version : "Unknown";
    }
    
    std::string getGLRenderer() const {
        if (!context_) return "N/A";
        CGLSetCurrentContext(context_);
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        return renderer ? renderer : "Unknown";
    }
};

bool HeadlessContext::isAvailable() {
    HeadlessContext test(4, 1);
    return test.isValid();
}

} // namespace mesh2splat

//------------------------------------------------------------------------------
// Linux EGL Implementation
//------------------------------------------------------------------------------
#elif M2S_PLATFORM_LINUX

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include "GLLoader.hpp"

namespace mesh2splat {

class HeadlessContext::Impl {
public:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLConfig config_ = nullptr;
    std::string errorMessage_;
    int majorVersion_ = 4;
    int minorVersion_ = 1;
    bool glFunctionsLoaded_ = false;
    
    Impl(int majorVersion, int minorVersion)
        : majorVersion_(majorVersion), minorVersion_(minorVersion) {
        
        // Try to get EGL display
        // First try the default display (works with Mesa)
        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        
        if (display_ == EGL_NO_DISPLAY) {
            // Try platform-specific device enumeration
            PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT = 
                (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
            PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = 
                (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
            
            if (eglQueryDevicesEXT && eglGetPlatformDisplayEXT) {
                EGLDeviceEXT devices[8];
                EGLint numDevices = 0;
                
                if (eglQueryDevicesEXT(8, devices, &numDevices) && numDevices > 0) {
                    // Use first available device
                    display_ = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, devices[0], nullptr);
                }
            }
        }
        
        if (display_ == EGL_NO_DISPLAY) {
            errorMessage_ = "Failed to get EGL display";
            return;
        }
        
        EGLint major, minor;
        if (!eglInitialize(display_, &major, &minor)) {
            errorMessage_ = "Failed to initialize EGL";
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Choose EGL config
        EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
        };
        
        EGLint numConfigs;
        if (!eglChooseConfig(display_, configAttribs, &config_, 1, &numConfigs) || numConfigs == 0) {
            errorMessage_ = "Failed to choose EGL config";
            eglTerminate(display_);
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Bind OpenGL API (not OpenGL ES)
        if (!eglBindAPI(EGL_OPENGL_API)) {
            errorMessage_ = "Failed to bind OpenGL API";
            eglTerminate(display_);
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Create context with requested version
        EGLint contextAttribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, majorVersion_,
            EGL_CONTEXT_MINOR_VERSION, minorVersion_,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE
        };
        
        context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs);
        if (context_ == EGL_NO_CONTEXT) {
            errorMessage_ = "Failed to create EGL context";
            eglTerminate(display_);
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Create a 1x1 pbuffer surface (required for some operations)
        EGLint pbufferAttribs[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };
        
        surface_ = eglCreatePbufferSurface(display_, config_, pbufferAttribs);
        // Surface is optional for compute, so don't fail if it's not created
    }
    
    ~Impl() {
        if (display_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            
            if (context_ != EGL_NO_CONTEXT) {
                eglDestroyContext(display_, context_);
            }
            if (surface_ != EGL_NO_SURFACE) {
                eglDestroySurface(display_, surface_);
            }
            eglTerminate(display_);
        }
    }
    
    bool makeCurrent() {
        if (display_ == EGL_NO_DISPLAY || context_ == EGL_NO_CONTEXT) {
            return false;
        }
        // Try with surface first, then without
        bool success = false;
        if (surface_ != EGL_NO_SURFACE) {
            success = eglMakeCurrent(display_, surface_, surface_, context_) == EGL_TRUE;
        } else {
            success = eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_) == EGL_TRUE;
        }
        
        // Load GL functions on first successful makeCurrent
        if (success && !glFunctionsLoaded_) {
            if (!gl::loadGLFunctions()) {
                errorMessage_ = "Failed to load OpenGL functions";
                return false;
            }
            glFunctionsLoaded_ = true;
        }
        
        return success;
    }
    
    void release() {
        if (display_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
    }
    
    bool isValid() const {
        return display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT;
    }
    
    std::string getGLVersion() const {
        if (!isValid()) return "N/A";
        const char* version = (const char*)glGetString(GL_VERSION);
        return version ? version : "Unknown";
    }
    
    std::string getGLRenderer() const {
        if (!isValid()) return "N/A";
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        return renderer ? renderer : "Unknown";
    }
};

bool HeadlessContext::isAvailable() {
    HeadlessContext test(4, 1);
    return test.isValid();
}

} // namespace mesh2splat

//------------------------------------------------------------------------------
// Windows WGL Implementation
//------------------------------------------------------------------------------
#elif M2S_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include "GLLoader.hpp"

namespace mesh2splat {

class HeadlessContext::Impl {
public:
    HINSTANCE instance_ = GetModuleHandleA(nullptr);
    HWND window_ = nullptr;
    HDC dc_ = nullptr;
    HGLRC context_ = nullptr;
    std::string errorMessage_;
    int majorVersion_ = 4;
    int minorVersion_ = 1;
    bool glFunctionsLoaded_ = false;

    Impl(int majorVersion, int minorVersion)
        : majorVersion_(majorVersion), minorVersion_(minorVersion) {
        const char* className = "Mesh2SplatHiddenGLWindow";

        WNDCLASSA wc = {};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = instance_;
        wc.lpszClassName = className;
        RegisterClassA(&wc);

        window_ = CreateWindowExA(
            0, className, "Mesh2Splat OpenGL Context",
            WS_OVERLAPPEDWINDOW, 0, 0, 1, 1,
            nullptr, nullptr, instance_, nullptr);
        if (!window_) {
            errorMessage_ = "Failed to create hidden WGL window";
            return;
        }

        dc_ = GetDC(window_);
        if (!dc_) {
            errorMessage_ = "Failed to get hidden WGL device context";
            return;
        }

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pixelFormat = ChoosePixelFormat(dc_, &pfd);
        if (pixelFormat == 0 || !SetPixelFormat(dc_, pixelFormat, &pfd)) {
            errorMessage_ = "Failed to set WGL pixel format";
            return;
        }

        HGLRC legacyContext = wglCreateContext(dc_);
        if (!legacyContext || !wglMakeCurrent(dc_, legacyContext)) {
            errorMessage_ = "Failed to create bootstrap WGL context";
            if (legacyContext) {
                wglDeleteContext(legacyContext);
            }
            return;
        }

        glewExperimental = GL_TRUE;
        GLenum glewResult = glewInit();
        while (glGetError() != GL_NO_ERROR) {}
        if (glewResult != GLEW_OK) {
            errorMessage_ = "Failed to initialize GLEW for WGL bootstrap";
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(legacyContext);
            return;
        }

        if (wglewIsSupported("WGL_ARB_create_context")) {
            int attribs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, majorVersion_,
                WGL_CONTEXT_MINOR_VERSION_ARB, minorVersion_,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            context_ = wglCreateContextAttribsARB(dc_, nullptr, attribs);
        }

        if (!context_) {
            context_ = legacyContext;
            legacyContext = nullptr;
        }

        if (!wglMakeCurrent(dc_, context_)) {
            errorMessage_ = "Failed to make WGL context current";
            if (legacyContext) {
                wglDeleteContext(legacyContext);
            }
            return;
        }

        if (legacyContext) {
            wglDeleteContext(legacyContext);
        }

        if (!gl::loadGLFunctions()) {
            errorMessage_ = "Failed to load OpenGL functions with GLEW";
            return;
        }
        glFunctionsLoaded_ = true;
    }

    ~Impl() {
        if (context_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(context_);
        }
        if (dc_ && window_) {
            ReleaseDC(window_, dc_);
        }
        if (window_) {
            DestroyWindow(window_);
        }
    }

    bool makeCurrent() {
        if (!dc_ || !context_) {
            return false;
        }
        bool success = wglMakeCurrent(dc_, context_) == TRUE;
        if (success && !glFunctionsLoaded_) {
            success = gl::loadGLFunctions();
            glFunctionsLoaded_ = success;
        }
        return success;
    }

    void release() {
        wglMakeCurrent(nullptr, nullptr);
    }

    bool isValid() const {
        return dc_ != nullptr && context_ != nullptr;
    }

    std::string getGLVersion() const {
        if (!isValid()) return "N/A";
        wglMakeCurrent(dc_, context_);
        const char* version = (const char*)glGetString(GL_VERSION);
        return version ? version : "Unknown";
    }

    std::string getGLRenderer() const {
        if (!isValid()) return "N/A";
        wglMakeCurrent(dc_, context_);
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        return renderer ? renderer : "Unknown";
    }
};

bool HeadlessContext::isAvailable() {
    HeadlessContext test(4, 1);
    return test.isValid();
}

} // namespace mesh2splat

//------------------------------------------------------------------------------
// Unsupported Platform
//------------------------------------------------------------------------------
#else

namespace mesh2splat {

class HeadlessContext::Impl {
public:
    std::string errorMessage_ = "Headless GPU not supported on this platform";
    
    Impl(int, int) {}
    bool makeCurrent() { return false; }
    void release() {}
    bool isValid() const { return false; }
    std::string getGLVersion() const { return "N/A"; }
    std::string getGLRenderer() const { return "N/A"; }
};

bool HeadlessContext::isAvailable() {
    return false;
}

} // namespace mesh2splat

#endif

//------------------------------------------------------------------------------
// Common Implementation
//------------------------------------------------------------------------------

namespace mesh2splat {

HeadlessContext::HeadlessContext(int majorVersion, int minorVersion)
    : impl_(std::make_unique<Impl>(majorVersion, minorVersion)) {
}

HeadlessContext::~HeadlessContext() = default;

HeadlessContext::HeadlessContext(HeadlessContext&& other) noexcept = default;
HeadlessContext& HeadlessContext::operator=(HeadlessContext&& other) noexcept = default;

bool HeadlessContext::makeCurrent() {
    return impl_->makeCurrent();
}

void HeadlessContext::release() {
    impl_->release();
}

bool HeadlessContext::isValid() const {
    return impl_->isValid();
}

std::string HeadlessContext::getGLVersion() const {
    return impl_->getGLVersion();
}

std::string HeadlessContext::getGLRenderer() const {
    return impl_->getGLRenderer();
}

std::string HeadlessContext::getErrorMessage() const {
    return impl_->errorMessage_;
}

} // namespace mesh2splat
