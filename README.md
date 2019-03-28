Vookoo 2.0
==========

Vookoo is a set of dependency-free utilities to assist in the construction and updating of
Vulkan graphics data structres.

Documentation:

[classes](https://andy-thomason.github.io/Vookoo/doc/html/classes.html)

Shaders modules are easy to construct:

    vku::ShaderModule vert_{device, BINARY_DIR "helloTriangle.vert.spv"};
    vku::ShaderModule frag_{device, BINARY_DIR "helloTriangle.frag.spv"};

Pipelines can be built with a few lines of code compared to many hundreds
in the C and C++ libraries

    vku::PipelineMaker pm{(uint32_t)width, (uint32_t)height};
    pm.shader(vk::ShaderStageFlagBits::eVertex, vert_);
    pm.shader(vk::ShaderStageFlagBits::eFragment, frag_);
    pm.vertexBinding(0, (uint32_t)sizeof(Vertex));
    pm.vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(Vertex, pos));
    pm.vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, (uint32_t)offsetof(Vertex, colour));
  
Tetxures are easy to construct and upload:

    // Create an image, memory and view for the texture on the GPU.
    vku::TextureImage2D texture{device, fw.memprops(), 2, 2, vk::Format::eR8G8B8A8Unorm};

    // Create an image and memory for the texture on the CPU.
    vku::TextureImage2D stagingBuffer{device, fw.memprops(), 2, 2, vk::Format::eR8G8B8A8Unorm, true};

    // Copy pixels into the staging buffer
    static const uint8_t pixels[] = { 0xff, 0xff, 0xff, 0xff,  0x00, 0xff, 0xff, 0xff,  0xff, 0x00, 0xff, 0xff,  0xff, 0xff, 0x00, 0xff, };
    stagingBuffer.update(device, (const void*)pixels, 4);

    // Copy the staging buffer to the GPU texture and set the layout.
    vku::executeImmediately(device, window.commandPool(), fw.graphicsQueue(), [&](vk::CommandBuffer cb) {
      texture.copy(cb, stagingBuffer);
      texture.setLayout(cb, vk::ImageLayout::eShaderReadOnlyOptimal);
    });

    // Free the staging buffer.
    stagingBuffer = vku::TextureImage2D{};

It is derived from the excellent projects of Sascha Willems,
Alexander Overvoorde and Khronos for Vulkan-Hpp project.

[https://github.com/SaschaWillems/Vulkan]

[https://github.com/Overv/VulkanTutorial]

[https://github.com/KhronosGroup/Vulkan-Hpp]

Vookoo adds a "vku" namespace to the "vk" namespace of the C++ interface
and provides user friendly interfacs for building pipelines and other
Vulkan data structures.

The aim of this project is to make Vulkan programming as easy as OpenGL.
Vulkan is known for immensely verbose data structures and exacting rules
but this can be mitigated but providing classes to help the construction
of Vulkan resources.

If you want to contribute to Vookoo, please send my some pull requests.
I will post some work areas that could do with improvement.

History
=======

Vookoo1.0 was an earlier project that did much the same thing but acted
as a "Layer" on top of the C interface. Because it was duplicating much
of the work of Vulkan-Hpp, we decided to replace it with the new Vookoo interface.
The old one is still around if you want to use it.

Library
=======

Currently the library consists of two header files:

    vku.hpp            The library itself
    vku_framework.hpp  An easy framework for running the examples

If you have an existing game engine then vku can be used with no dependencies.

If you are learning Vulkan, the framework library provides window services
by way of glfw3.

The repository contains all the files needed to build the examples, but
you may also use the headers stand-alone, for example with Android builds.

Examples
========

There are currently these examples (in order of complexity)

    Simple examples
    helloTriangle   Draw a triangle using vertex buffers
    pushConstants   Draw a rotating triangle
    texture         Draw a textured triangle

    Complex examples    
    teapot          Draw the Utah teapot with cube map reflections
    molvoo          A molecular viewer for very large complexes

To build, you will need the Vulkan SDK from LunarG:

    https://www.lunarg.com/vulkan-sdk/

Once installed, check that the GLSL compiler works:

    $ glslangValidator

Building the examples on Windows:

    mkdir build
    cd build
    cmake -G "Visual Studio 14 2015 Win64" ..\examples
    VookooExamples.sln

Building the examples on Linux:

    sudo apt install libxinerama-dev libxcursor-dev libxrandr-dev
    mkdir build
    cd build
    cmake ../examples
    make


