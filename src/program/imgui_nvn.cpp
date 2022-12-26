#include "imgui_nvn.h"
#include "imgui_backend/imgui_impl_nvn.hpp"
#include "init.h"
#include "lib.hpp"
#include "logger/Logger.hpp"
#include "helpers/InputHelper.h"

nvn::Device *nvnDevice;
nvn::Queue *nvnQueue;
nvn::CommandBuffer *nvnCmdBuf;

nvn::DeviceGetProcAddressFunc tempGetProcAddressFuncPtr;

nvn::CommandBufferInitializeFunc tempBufferInitFuncPtr;
nvn::DeviceInitializeFunc tempDeviceInitFuncPtr;
nvn::QueueInitializeFunc tempQueueInitFuncPtr;

NVNboolean deviceInit(nvn::Device *device, const nvn::DeviceBuilder *builder) {
    NVNboolean result = tempDeviceInitFuncPtr(device, builder);
    nvnDevice = device;
    nvn::nvnLoadCPPProcs(nvnDevice, tempGetProcAddressFuncPtr);
    return result;
}

NVNboolean queueInit(nvn::Queue *queue, const nvn::QueueBuilder *builder) {
    NVNboolean result = tempQueueInitFuncPtr(queue, builder);
    nvnQueue = queue;
    return result;
}

NVNboolean cmdBufInit(nvn::CommandBuffer *buffer, nvn::Device *device) {
    NVNboolean result = tempBufferInitFuncPtr(buffer, device);
    nvnCmdBuf = buffer;
    return result;
}

nvn::GenericFuncPtrFunc getProc(nvn::Device *device, const char *procName) {

    nvn::GenericFuncPtrFunc ptr = tempGetProcAddressFuncPtr(nvnDevice, procName);

    if (strcmp(procName, "nvnQueueInitialize") == 0) {
        tempQueueInitFuncPtr = (nvn::QueueInitializeFunc) ptr;
        return (nvn::GenericFuncPtrFunc) &queueInit;
    } else if (strcmp(procName, "nvnCommandBufferInitialize") == 0) {
        tempBufferInitFuncPtr = (nvn::CommandBufferInitializeFunc) ptr;
        return (nvn::GenericFuncPtrFunc) &cmdBufInit;
    }

    return ptr;
}

HOOK_DEFINE_TRAMPOLINE(NvnBootstrapHook) {
    static void *Callback(const char *funcName) {

        void *result = Orig(funcName);

        if (strcmp(funcName, "nvnDeviceInitialize") == 0) {
            tempDeviceInitFuncPtr = (nvn::DeviceInitializeFunc) result;
            return (void *) &deviceInit;
        }
        if (strcmp(funcName, "nvnDeviceGetProcAddress") == 0) {
            tempGetProcAddressFuncPtr = (nvn::DeviceGetProcAddressFunc) result;
            return (void *) &getProc;
        }

        return result;
    }
};

// TODO: move this hook into a game agnostic location
HOOK_DEFINE_TRAMPOLINE(ImGuiRenderFrameHook) {
    static void *Callback(void *thisPtr) {
        void *result = Orig(thisPtr);

        InputHelper::updatePadState(); // update input helper

        ImguiNvnBackend::updateInput(); // update backend inputs

        ImguiNvnBackend::newFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();
//        ImGui::ShowMetricsWindow();
//        ImGui::ShowDebugLogWindow();
//        ImGui::ShowStackToolWindow();
//        ImGui::ShowAboutWindow();
//        ImGui::ShowStyleEditor();
//        ImGui::ShowFontSelector("Font Selector");
//        ImGui::ShowUserGuide();

        ImGui::Render();
        ImguiNvnBackend::renderDrawData(ImGui::GetDrawData());

        return result;
    }
};

void nvnImGui::InstallHooks() {
    NvnBootstrapHook::InstallAtSymbol("nvnBootstrapLoader");
    ImGuiRenderFrameHook::InstallAtSymbol("_ZN2al15GameFrameworkNx9procDraw_Ev");
}

bool nvnImGui::InitImGui() {
    if (nvnDevice && nvnQueue) {

        Logger::log("Creating ImGui.\n");

        IMGUI_CHECKVERSION();

        ImGuiMemAllocFunc allocFunc = [](size_t size, void *user_data) {
            return nn::init::GetAllocator()->Allocate(size);
        };

        ImGuiMemFreeFunc freeFunc = [](void *ptr, void *user_data) {
            nn::init::GetAllocator()->Free(ptr);
        };

        ImGui::SetAllocatorFunctions(allocFunc, freeFunc, nullptr);

        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;

        ImGui::StyleColorsDark();

        ImguiNvnBackend::NvnBackendInitInfo initInfo = {
                .device = nvnDevice,
                .queue = nvnQueue,
                .cmdBuf = nvnCmdBuf
        };

        ImguiNvnBackend::InitBackend(initInfo);

        InputHelper::setPort(0); // set input helpers default port to zero

        return true;

    } else {
        Logger::log("Unable to create ImGui Renderer!\n");

        return false;
    }
}