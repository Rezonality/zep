////////////////////////////////////////////////////////////////////////////////
//
// Vookoo compute example (C) 2018 Andy Thomason
//
// This is a simple introduction to the vulkan C++ interface by way of Vookoo
// which is a layer to make creating Vulkan resources easy.
//

#define VKU_NO_GLFW
#include <vku/vku.hpp>
#include <vku/vku_framework.hpp>

int main() {
  vku::Framework fw{"Hello compute"};
  if (!fw.ok()) {
    std::cout << "Framework creation failed" << std::endl;
    exit(1);
  }

  // Get a device from the demo framework.
  auto device = fw.device();
  auto cache = fw.pipelineCache();
  auto descriptorPool = fw.descriptorPool();
  auto memprops = fw.memprops();

  typedef vk::CommandPoolCreateFlagBits ccbits;
  vk::CommandPoolCreateInfo cpci{ ccbits::eTransient|ccbits::eResetCommandBuffer, fw.computeQueueFamilyIndex() };
  auto commandPool = device.createCommandPoolUnique(cpci);

  static constexpr int N = 128;

  // Up to 256 bytes of immediate data.
  struct PushConstants {
    float value;   // The shader just adds this to the buffer.
    float pad[3];  // Buffers are usually 16 byte aligned.
  };

  // Descriptor set layout.
  // Shader has access to a single storage buffer.
  vku::DescriptorSetLayoutMaker dsetlm{};
  dsetlm.buffer(0U, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1);
  auto dsetLayout = dsetlm.createUnique(device);

  // The descriptor set itself.
  vku::DescriptorSetMaker dsm{};
  dsm.layout(*dsetLayout);
  auto dsets = dsm.create(device, descriptorPool);
  auto descriptorSet = dsets[0];

  // Pipeline layout.
  // Shader has one descriptor set and some push constants.
  vku::PipelineLayoutMaker plm{};
  plm.descriptorSetLayout(*dsetLayout);
  plm.pushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants));
  auto pipelineLayout = plm.createUnique(device);
 
  // The pipeline itself. 
  auto shader = vku::ShaderModule{device, BINARY_DIR "helloCompute.comp.spv"};
  vku::ComputePipelineMaker cpm{};
  cpm.shader(vk::ShaderStageFlagBits::eCompute, shader);
  auto pipeline = cpm.createUnique(device, cache, *pipelineLayout);

  // A buffer to store the results in.
  // Note: this won't work for everyone. With some devices you
  // may need to explictly upload and download data.
  using bflags = vk::BufferUsageFlagBits;
  using mflags = vk::MemoryPropertyFlagBits;
  auto mybuf = vku::GenericBuffer(device, memprops, bflags::eStorageBuffer, N * sizeof(float), mflags::eHostVisible);

  vku::DescriptorSetUpdater update;
  update.beginDescriptorSet(descriptorSet);
  update.beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer);
  update.buffer(mybuf.buffer(), 0, N * sizeof(float));
  update.update(device); // this only copies the pointer, not any data.

  // Run some code on the GPU.
  vku::executeImmediately(device, *commandPool, fw.computeQueue(), [&](vk::CommandBuffer cb) {
    PushConstants cu = {2.0f};
    cb.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants), &cu);

    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSet, nullptr);
    cb.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
    cb.dispatch(N, 1, 1);
  });

  device.waitIdle();

  // Print the result (2.0f + 0..127)
  float * p = (float*)mybuf.map(device);
  for (int i = 0; i != N; ++i) {
    printf("%f ", p[i]);
  }
  printf("\n");
  mybuf.unmap(device);
}


