Installation on Ubuntu
======================

Download SDK from [https://vulkan.lunarg.com/sdk/home#linux]

This example is 1.0.65.0:

    mkdir ~/vulkan
    cd ~/vulkan
    cp ~/Downloads/vulkansdk-linux-x86_64-1.0.65.0.run .
    chmod ugo+x vulkansdk-linux-x86_64-1.0.65.0.run
    ./vulkansdk-linux-x86_64-1.0.65.0.run
    sudo apt install libvulkan-dev libxinerama-dev libxcursor-dev libxrandr-dev x11-xserver-utils

Hack your ~/.bashrc

    export VULKAN_SDK=/home/<you>/vulkan/VulkanSDK/1.0.65.0/
    export PATH=$PATH:$VULKAN_SDK/x86_64/bin
    export LD_LIBRARY_PATH=$VULKAN_SDK/x86_64/
    export VK_LAYER_PATH=$VULKAN_SDK/x86_64/etc/explicit_layer.d

Run a new shell (ctrl-alt-T)

