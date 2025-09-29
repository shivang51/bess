#include "ui/ui.h"
#include "common/log.h"
#include "ui/icons/CodIcons.h"
#include "ui/icons/ComponentIcons.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/icons/MaterialIcons.h"

#include "application_state.h"
#include "assets.h"
#include "settings/settings.h"
#include "ui/ui_main/ui_main.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include "scene/renderer/vulkan/vulkan_renderer.h"

namespace Bess::UI {
    void init(GLFWwindow *window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO &io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-viewport

        io.IniFilename = "bess.ini";

        ImGui::StyleColorsDark();

        loadFontAndSetScale(Config::Settings::getFontSize(), Config::Settings::getScale());
        Config::Settings::loadCurrentTheme();

        ImGui_ImplGlfw_InitForVulkan(window, true);

        initVulkanImGui();
    }

    void initVulkanImGui() {
        if (!Renderer2D::VulkanRenderer::isInitialized) {
            BESS_ERROR("VulkanRenderer not initialized! Call VulkanRenderer::init() first.");
            return;
        }

        const auto &vulkanRenderer = Bess::Renderer2D::VulkanRenderer::instance();

        const auto device = vulkanRenderer.getDevice();
        if (!device) {
            BESS_ERROR("[UI] Vulkan device not available!");
            return;
        }

        constexpr std::array<VkDescriptorPoolSize, 1> poolSizes = {{
                                                           {.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount=1000}
                                                           }};

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device->device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            BESS_ERROR("Failed to create descriptor pool for ImGui!");
            return;
        }

        // Setup ImGui Vulkan backend
        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.ApiVersion = VK_API_VERSION_1_0;
        initInfo.Instance = vulkanRenderer.getVkInstance();
        initInfo.PhysicalDevice = device->physicalDevice();
        initInfo.Device = device->device();
        initInfo.QueueFamily = device->queueFamilyIndices().graphicsFamily.value();
        initInfo.Queue = device->graphicsQueue();
        initInfo.DescriptorPool = descriptorPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = 2;
        initInfo.UseDynamicRendering = false;

        const auto pipeline = vulkanRenderer.getPipeline();
        initInfo.PipelineInfoMain.RenderPass = pipeline->renderPass();
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        initInfo.PipelineInfoForViewports.RenderPass = pipeline->renderPass();
        initInfo.PipelineInfoForViewports.Subpass = 0;
        initInfo.PipelineInfoForViewports.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        if (!ImGui_ImplVulkan_Init(&initInfo)) {
            BESS_ERROR("Failed to initialize ImGui Vulkan backend!");
            vkDestroyDescriptorPool(device->device(), descriptorPool, nullptr);
            return;
        }

        BESS_INFO("ImGui Vulkan backend initialized successfully!");
    }

    void shutdown() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    void begin() {
        if (Config::Settings::shouldFontRebuild()) {
            loadFontAndSetScale(Config::Settings::getFontSize(),
                                Config::Settings::getScale());
            Config::Settings::setFontRebuild(true);
        }

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
        const ImGuiViewport *viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |=
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        static bool pOpen = true;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const auto mainDockspaceId = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(mainDockspaceId);
    }

    void end() {
        ImGui::End();
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::Render();

        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    ImFont *Fonts::largeFont = nullptr;
    ImFont *Fonts::mediumFont = nullptr;
    void loadFontAndSetScale(float fontSize, float scale) {
        ImGuiIO &io = ImGui::GetIO();
        ImGuiStyle &style = ImGui::GetStyle();

        constexpr auto robotoPath = Assets::Fonts::Paths::roboto.paths[0].data();

        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF(robotoPath, fontSize);
        Fonts::largeFont = io.Fonts->AddFontFromFileTTF(robotoPath, fontSize * 2.0F);
        Fonts::mediumFont = io.Fonts->AddFontFromFileTTF(robotoPath, fontSize * 1.5F);
        io.FontDefault = io.Fonts->AddFontFromFileTTF(robotoPath, fontSize);

        ImFontConfig config;
        float r = 2.2F / 3.0F;
        config.MergeMode = true;
        config.PixelSnapH = true;

        constexpr auto compIconsPath = Assets::Fonts::Paths::componentIcons.paths[0].data();
        constexpr auto codeIconsPath = Assets::Fonts::Paths::codeIcons.paths[0].data();
        constexpr auto materialIconsPath = Assets::Fonts::Paths::materialIcons.paths[0].data();
        constexpr auto fontAwesomeIconsPath = Assets::Fonts::Paths::fontAwesomeIcons.paths[0].data();

        static const std::array<ImWchar, 3> compIconRanges = {Icons::ComponentIcons::SIZE_MIN_CI, Icons::ComponentIcons::SIZE_MAX_CI, 0};
        io.Fonts->AddFontFromFileTTF(compIconsPath, fontSize * r, &config, compIconRanges.data());

        static const std::array<ImWchar, 3> codiconIconRanges = {Icons::CodIcons::ICON_MIN_CI, Icons::CodIcons::ICON_MAX_CI, 0};
        config.GlyphOffset.y = fontSize / 5.0F;
        io.Fonts->AddFontFromFileTTF(codeIconsPath, fontSize, &config, codiconIconRanges.data());

        config.GlyphOffset.y = r;
        static const std::array<ImWchar, 3> matIconRanges = {Icons::MaterialIcons::ICON_MIN_MD, Icons::MaterialIcons::ICON_MAX_MD, 0};
        io.Fonts->AddFontFromFileTTF(materialIconsPath, fontSize * r, &config, matIconRanges.data());

        static const std::array<ImWchar, 3> faIconRangesR = {Icons::FontAwesomeIcons::SIZE_MIN_FA, Icons::FontAwesomeIcons::SIZE_MAX_FA, 0};
        config.GlyphOffset.y = -r;
        io.Fonts->AddFontFromFileTTF(fontAwesomeIconsPath, fontSize * r, &config, faIconRangesR.data());

        io.FontGlobalScale = scale;
        // io.Fonts->Build(); // Vulkan backend handles font atlas building

        if (Config::Settings::shouldFontRebuild()) {
            // Vulkan font rebuilding will be handled by VulkanRenderer
            // ImGui_ImplVulkan_DestroyFontUploadObjects();
            // ImGui_ImplVulkan_CreateFontsTexture();
        }
    }

    void setCursorPointer() {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    void setCursorMove() {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    void setCursorNormal() { ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow); }

    void drawStats(int fps) {
        ImGui::Begin("Stats");
        ImGui::Text("FPS: %d", fps);
        switch (ApplicationState::getCurrentPage()->getIdentifier()) {
        case Pages::PageIdentifier::MainPage:
            UI::UIMain::drawStats(fps);
            break;
        case Pages::PageIdentifier::StartPage:
        default:
            break;
        }

        ImGui::End();
    }

} // namespace Bess::UI
