////////////////////////////////////////////////////////////////////////////////
//
// Vookoo triangle example (C) 2017 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//
// In this sample we demonstrate push constants which are an easy way to
// pass small shader parameters without synchonisation issues.
//
// Compare this file with helloTriangle.cpp to see what we have done.

// Include the demo framework, vookoo (vku) for building objects and glm for maths.
// The demo framework uses GLFW to create windows.
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#include <glm/ext.hpp> // for rotate()

int main() {
  // Initialise the GLFW framework.
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  // Make a window
  const char *title = "pushConstants";
  bool fullScreen = false;
  int width = 800;
  int height = 800;
  GLFWmonitor *monitor = nullptr;
  auto glfwwindow = glfwCreateWindow(width, height, title, monitor, nullptr);

  {
    // Initialise the Vookoo demo framework.
    vku::Framework fw{title};
    if (!fw.ok()) {
      std::cout << "Framework creation failed" << std::endl;
      exit(1);
    }

    // Get a device from the demo framework.
    vk::Device device = fw.device();

    // Create a window to draw into
    vku::Window window{fw.instance(), device, fw.physicalDevice(), fw.graphicsQueueFamilyIndex(), glfwwindow};
    if (!window.ok()) {
      std::cout << "Window creation failed" << std::endl;
      exit(1);
    }

    // Create two shaders, vertex and fragment. See the files pushConstants.vert
    // and pushConstants.frag for details.
    vku::ShaderModule vert_{device, BINARY_DIR "pushConstants.vert.spv"};
    vku::ShaderModule frag_{device, BINARY_DIR "pushConstants.frag.spv"};

    // These are the parameters we are passing to the shaders
    // Note! be very careful when using vec3, vec2, float and vec4 together
    // as there are alignment rules you must follow.
    struct Uniform {
      glm::vec4 colour;
      glm::mat4 rotation;
    };

    // Make a default pipeline layout. This shows how pointers
    // to resources are layed out.
    // 
    vku::PipelineLayoutMaker plm{};
    plm.pushConstantRange(vk::ShaderStageFlagBits::eAll, 0, sizeof(Uniform));
    auto pipelineLayout_ = plm.createUnique(device);

    // We will use this simple vertex description.
    // It has a 2D location (x, y) and a colour (r, g, b)
    struct Vertex { glm::vec2 pos; glm::vec3 colour; };

    // This is our triangle.
    const std::vector<Vertex> vertices = {
      {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };
    vku::HostVertexBuffer buffer(device, fw.memprops(), vertices);

    // Make a pipeline to use the vertex format and shaders.
    vku::PipelineMaker pm{(uint32_t)width, (uint32_t)height};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, pos));
    pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, colour));

    // Create a pipeline using a renderPass built for our window.
    auto renderPass = window.renderPass();
    auto &cache = fw.pipelineCache();
    auto pipeline = pm.createUnique(device, cache, *pipelineLayout_, renderPass);

    Uniform u;
    u.colour = glm::vec4{1, 1, 1, 1};
    u.rotation = glm::mat4{1};
    int frame = 0;

    // Loop waiting for the window to close.
    while (!glfwWindowShouldClose(glfwwindow)) {
      glfwPollEvents();

      u.rotation = glm::rotate(u.rotation, glm::radians(1.0f), glm::vec3(0, 0, 1));
      u.colour.r = std::sin(frame * 0.01f);
      u.colour.g = std::cos(frame * 0.01f);
      frame++;

      // draw one triangle.
      // Unlike helloTriangle, we generate the command buffer dynamicly
      // because it will contain different values on each frame.
      window.draw(
        device, fw.graphicsQueue(),
        [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
          vk::CommandBufferBeginInfo bi{};
          cb.begin(bi);
          cb.pushConstants(
            *pipelineLayout_, vk::ShaderStageFlagBits::eAll, 0, sizeof(u), (const void*)&u
          );
          cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
          cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
          cb.bindVertexBuffers(0, buffer.buffer(), vk::DeviceSize(0));
          cb.draw(3, 1, 0, 0);
          cb.endRenderPass();
          cb.end();
        }
      );

      // Very crude method to prevent your GPU from overheating.
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Wait until all drawing is done and then kill the window.

    device.waitIdle();
    // The Framework and Window objects will be destroyed here.
  }

  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
