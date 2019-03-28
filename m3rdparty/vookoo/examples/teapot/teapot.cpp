
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/gtx/io.hpp>

#include <gilgamesh/mesh.hpp>
#include <gilgamesh/scene.hpp>
#include <gilgamesh/shapes/teapot.hpp>
#include <gilgamesh/decoders/fbx_decoder.hpp>
#include <gilgamesh/encoders/fbx_encoder.hpp>

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  const char *title = "teapot";
  auto glfwwindow = glfwCreateWindow(800, 600, title, nullptr, nullptr);

  {
    vku::Framework fw{title};
    if (!fw.ok()) {
      std::cout << "Framework creation failed" << std::endl;
      exit(1);
    }

    vk::Device device = fw.device();

    vku::Window window{fw.instance(), fw.device(), fw.physicalDevice(), fw.graphicsQueueFamilyIndex(), glfwwindow};
    if (!window.ok()) {
      std::cout << "Window creation failed" << std::endl;
      exit(1);
    }
    ////////////////////////////////////////
    //
    // Build the descriptor sets

    // This pipeline layout is going to be shared amongst several pipelines.
    vku::DescriptorSetLayoutMaker dslm{};
    dslm.buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 1);
    dslm.image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    dslm.image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1);
    auto layout = dslm.createUnique(device);

    vku::DescriptorSetMaker dsm{};
    dsm.layout(*layout);
    auto descriptorSets = dsm.create(device, fw.descriptorPool());

    vku::PipelineLayoutMaker plm{};
    plm.descriptorSetLayout(*layout);
    auto pipelineLayout = plm.createUnique(device);


    // Use Gilgamesh to build the Teapot.
    gilgamesh::simple_mesh mesh;
    gilgamesh::teapot shape;
    shape.build(mesh);
    mesh.reindex(true);

    struct Vertex { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };
    std::vector<Vertex> vertices;

    auto meshpos = mesh.pos();
    auto meshnormal = mesh.normal();
    auto meshuv = mesh.uv(0);
    for (size_t i = 0; i != meshpos.size(); ++i) {
      glm::vec3 pos = meshpos[i];
      glm::vec3 normal = meshnormal[i];
      glm::vec2 uv = meshuv[i];
      vertices.emplace_back(Vertex{pos, normal, uv});
    }
    std::vector<uint32_t> indices = mesh.indices32();

    vku::HostVertexBuffer vbo(fw.device(), fw.memprops(), vertices);
    vku::HostIndexBuffer ibo(fw.device(), fw.memprops(), indices);
    uint32_t indexCount = (uint32_t)indices.size();

    struct Uniform {
      glm::mat4 modelToPerspective;
      glm::mat4 modelToWorld;
      glm::mat4 normalToWorld;
      glm::mat4 modelToLight;
      glm::vec4 cameraPos;
      glm::vec4 lightPos;
    };

    // World matrices of model, camera and light
    glm::mat4 modelToWorld = glm::rotate(glm::mat4{1}, glm::radians(-90.0f), glm::vec3(1, 0, 0));
    glm::mat4 cameraToWorld = glm::translate(glm::mat4{1}, glm::vec3(0, 2, 8));
    glm::mat4 lightToWorld = glm::translate(glm::mat4{1}, glm::vec3(8, 6, 0));
    lightToWorld = glm::rotate(lightToWorld, glm::radians(90.0f), glm::vec3(0, 1, 0));
    lightToWorld = glm::rotate(lightToWorld, glm::radians(-30.0f), glm::vec3(1, 0, 0));

    // This matrix converts between OpenGL perspective and Vulkan perspective.
    // It flips the Y axis and shrinks the Z value to [0,1]
    glm::mat4 leftHandCorrection(
      1.0f,  0.0f, 0.0f, 0.0f,
      0.0f, -1.0f, 0.0f, 0.0f,
      0.0f,  0.0f, 0.5f, 0.0f,
      0.0f,  0.0f, 0.5f, 1.0f
    );

    uint32_t shadowSize = 512;

    glm::mat4 cameraToPerspective = leftHandCorrection * glm::perspective(glm::radians(45.0f), (float)window.width()/window.height(), 1.0f, 100.0f);
    glm::mat4 lightToPerspective = leftHandCorrection * glm::perspective(glm::radians(30.0f), (float)shadowSize/shadowSize, 1.0f, 100.0f);

    bool lookFromLight = false;
    if (lookFromLight) {
      cameraToWorld = lightToWorld;
      cameraToPerspective = lightToPerspective;
    }

    glm::mat4 worldToCamera = glm::inverse(cameraToWorld);
    glm::mat4 worldToLight = glm::inverse(lightToWorld);

    // Create, but do not upload the uniform buffer as a device local buffer.
    vku::UniformBuffer ubo(fw.device(), fw.memprops(), sizeof(Uniform));

    ////////////////////////////////////////
    //
    // Build the final pipeline including enabling the depth test

    vku::ShaderModule final_vert{device, BINARY_DIR "teapot.vert.spv"};
    vku::ShaderModule final_frag{device, BINARY_DIR "teapot.frag.spv"};

    vku::PipelineMaker pm{window.width(), window.height()};
    pm.shader(vk::ShaderStageFlagBits::eVertex, final_vert);
    pm.shader(vk::ShaderStageFlagBits::eFragment, final_frag);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, pos));
    pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
    pm.vertexAttribute(2, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));
    pm.depthTestEnable(VK_TRUE);
    pm.cullMode(vk::CullModeFlagBits::eBack);
    pm.frontFace(vk::FrontFace::eCounterClockwise);

    auto renderPass = window.renderPass();
    auto &cache = fw.pipelineCache();
    auto finalPipeline = pm.createUnique(device, cache, *pipelineLayout, renderPass);

    ////////////////////////////////////////
    //
    // Build a pipeline for shadows

    vku::ShaderModule shadow_vert{device, BINARY_DIR "teapot.shadow.vert.spv"};
    vku::ShaderModule shadow_frag{device, BINARY_DIR "teapot.shadow.frag.spv"};

    vku::PipelineMaker spm{shadowSize, shadowSize};
    spm.shader(vk::ShaderStageFlagBits::eVertex, shadow_vert);
    spm.shader(vk::ShaderStageFlagBits::eFragment, shadow_frag);
    spm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    spm.vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, pos));
    spm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, normal));
    spm.vertexAttribute(2, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, uv));

    // Shadows render only to the depth buffer
    // Depth test is important.
    spm.depthTestEnable(VK_TRUE);
    spm.cullMode(vk::CullModeFlagBits::eBack);
    spm.frontFace(vk::FrontFace::eClockwise);

    // We will be using the depth bias dynamic state: cb.setDepthBias()
    spm.dynamicState(vk::DynamicState::eDepthBias);

    // This image is the depth buffer for the first pass and becomes the texture for the second pass.
    vku::DepthStencilImage shadowImage(device, fw.memprops(), shadowSize, shadowSize);

    // Build the renderpass using only depth/stencil.
    vku::RenderpassMaker rpm;

    // The depth/stencil attachment.
    // Clear to 1.0f at the start (eClear)
    // Save the depth buffer at the end. (eStore)
    rpm.attachmentBegin(shadowImage.format());
    rpm.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rpm.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rpm.attachmentInitialLayout(vk::ImageLayout::eUndefined);
    rpm.attachmentFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    // A subpass to render using the above attachment.
    rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rpm.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 0);

    // A dependency to reset the layout of the depth buffer to eUndefined.
    // eLateFragmentTests is the stage where the Z buffer is written.
    rpm.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rpm.dependencySrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    rpm.dependencyDstStageMask(vk::PipelineStageFlagBits::eLateFragmentTests);

    // A dependency to transition to eShaderReadOnlyOptimal.
    rpm.dependencyBegin(0, VK_SUBPASS_EXTERNAL);
    rpm.dependencySrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests);
    rpm.dependencyDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    rpm.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);

    // Use the maker object to construct the renderpass
    vk::UniqueRenderPass shadowRenderPass = rpm.createUnique(device);

    // Build the framebuffer.
    vk::ImageView attachments[1] = {shadowImage.imageView()};
    vk::FramebufferCreateInfo fbci{{}, *shadowRenderPass, 1, attachments, shadowSize, shadowSize, 1 };
    vk::UniqueFramebuffer shadowFrameBuffer = device.createFramebufferUnique(fbci);

    // Build the pipeline for this renderpass.
    auto shadowPipeline = spm.createUnique(device, cache, *pipelineLayout, *shadowRenderPass, false);

    // Only one attachment to clear, the depth buffer.
    vk::ClearDepthStencilValue clearDepthValue{ 1.0f, 0 };
    std::array<vk::ClearValue, 1> clearColours{clearDepthValue};

    // Begin rendering using the framebuffer and renderpass
    vk::RenderPassBeginInfo shadowRpbi{};
    shadowRpbi.renderPass = *shadowRenderPass;
    shadowRpbi.framebuffer = *shadowFrameBuffer;
    shadowRpbi.renderArea = vk::Rect2D{{0, 0}, {shadowSize, shadowSize}};
    shadowRpbi.clearValueCount = (uint32_t)clearColours.size();
    shadowRpbi.pClearValues = clearColours.data();

    ////////////////////////////////////////
    //
    // Create a cubemap

    // see: https://github.com/dariomanesku/cmft
    auto cubeBytes = vku::loadFile(SOURCE_DIR "okretnica.ktx");
    vku::KTXFileLayout ktx(cubeBytes.data(), cubeBytes.data()+cubeBytes.size());
    if (!ktx.ok()) {
      std::cout << "Could not load KTX file" << std::endl;
      exit(1);
    }

    vku::TextureImageCube cubeMap{device, fw.memprops(), ktx.width(0), ktx.height(0), ktx.mipLevels(), vk::Format::eR8G8B8A8Unorm};

    vku::GenericBuffer stagingBuffer(device, fw.memprops(), vk::BufferUsageFlagBits::eTransferSrc, cubeBytes.size(), vk::MemoryPropertyFlagBits::eHostVisible);
    stagingBuffer.updateLocal(device, (const void*)cubeBytes.data(), cubeBytes.size());
    cubeBytes = std::vector<uint8_t>{};

    // Copy the staging buffer to the GPU texture and set the layout.
    vku::executeImmediately(device, window.commandPool(), fw.graphicsQueue(), [&](vk::CommandBuffer cb) {
      vk::Buffer buf = stagingBuffer.buffer();
      for (uint32_t mipLevel = 0; mipLevel != ktx.mipLevels(); ++mipLevel) {
        auto width = ktx.width(mipLevel); 
        auto height = ktx.height(mipLevel); 
        auto depth = ktx.depth(mipLevel); 
        for (uint32_t face = 0; face != ktx.faces(); ++face) {
          cubeMap.copy(cb, buf, mipLevel, face, width, height, depth, ktx.offset(mipLevel, 0, face));
        }
      }
      cubeMap.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
    });

    // Free the staging buffer.
    stagingBuffer = vku::GenericBuffer{};

    ////////////////////////////////////////
    //
    // Update the descriptor sets for the shader uniforms.

    vku::SamplerMaker sm{};
    sm.magFilter(vk::Filter::eLinear);
    sm.minFilter(vk::Filter::eLinear);
    sm.mipmapMode(vk::SamplerMipmapMode::eNearest);
    vk::UniqueSampler finalSampler = sm.createUnique(device);

    vku::SamplerMaker ssm{};
    ssm.magFilter(vk::Filter::eNearest);
    ssm.minFilter(vk::Filter::eNearest);
    ssm.mipmapMode(vk::SamplerMipmapMode::eNearest);
    vk::UniqueSampler shadowSampler = ssm.createUnique(device);

    vku::DescriptorSetUpdater update;
    update.beginDescriptorSet(descriptorSets[0]);

    // Set initial uniform buffer value
    update.beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer);
    update.buffer(ubo.buffer(), 0, sizeof(Uniform));

    // Set initial finalSampler value
    update.beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(*finalSampler, cubeMap.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    update.beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler);
    update.image(*shadowSampler, shadowImage.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

    update.update(device);

    // Depth bias pulls the depth buffer value away from the surface to prevent artifacts.
    // The bias changes as the triangle slopes away from the camera.
    // Values taken from Sasha Willems' shadowmapping example.
    const float depthBiasConstantFactor = 1.25f;
    const float depthBiasSlopeFactor = 1.75f;

    if (!window.ok()) {
      std::cout << "Window creation failed" << std::endl;
      exit(1);
    }

    while (!glfwWindowShouldClose(glfwwindow)) {
      glfwPollEvents();

      window.draw(fw.device(), fw.graphicsQueue(),
        [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo &rpbi) {
          Uniform uniform{};
          modelToWorld = glm::rotate(modelToWorld, glm::radians(1.0f), glm::vec3(0, 0, 1));
          uniform.modelToPerspective = cameraToPerspective * worldToCamera * modelToWorld;
          uniform.normalToWorld = modelToWorld;
          uniform.modelToWorld = modelToWorld;
          uniform.modelToLight = lightToPerspective * worldToLight * modelToWorld;
          uniform.lightPos = lightToWorld[3];
          uniform.cameraPos = cameraToWorld[3];

          // Record the dynamic buffer.
          vk::CommandBufferBeginInfo bi{};
          cb.begin(bi);

          // Copy the uniform data to the buffer. (note this is done
          // inline and so we can discard "unform" afterwards)
          cb.updateBuffer(ubo.buffer(), 0, sizeof(Uniform), (void*)&uniform);

          // First renderpass. Draw the shadow.
          cb.beginRenderPass(shadowRpbi, vk::SubpassContents::eInline);
          cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowPipeline);
          cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
          cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
          cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
          cb.setDepthBias(depthBiasConstantFactor, 0.0f, depthBiasSlopeFactor);
          cb.drawIndexed(indexCount, 1, 0, 0, 0);
          cb.endRenderPass();

          // Second renderpass. Draw the final image.
          cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
          cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *finalPipeline);
          cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
          cb.bindIndexBuffer(ibo.buffer(), vk::DeviceSize(0), vk::IndexType::eUint32);
          cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSets, nullptr);
          cb.drawIndexed(indexCount, 1, 0, 0, 0);
          cb.endRenderPass();

          cb.end();
        }
      );

      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    device.waitIdle();
  }

  glfwDestroyWindow(glfwwindow);
  glfwTerminate();

  return 0;
}
