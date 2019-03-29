
LIST(APPEND JORVIK_SOURCES
    jorvik/main.cpp
    jorvik/list.cmake
    jorvik/jorvik.cpp
    jorvik/jorvik.h
	jorvik/editor.cpp
	jorvik/editor.h
	jorvik/compile.cpp
	jorvik/compile.h
	jorvik/meta_tags.cpp
	jorvik/meta_tags.h
    jorvik/opus.h
    jorvik/opus.cpp
    jorvik/opus_asset_base.h
    jorvik/opus_asset_base.cpp
	jorvik/visual/scene.cpp
	jorvik/visual/scene.h
	jorvik/visual/render_node.cpp
	jorvik/visual/render_node.h
	jorvik/visual/render_graph.cpp
	jorvik/visual/render_graph.h
	jorvik/visual/pass_renderstate.cpp
	jorvik/visual/pass_renderstate.h
	jorvik/visual/scene_description_file_asset.cpp
	jorvik/visual/scene_description_file_asset.h
	jorvik/visual/geometry_file_asset.cpp
	jorvik/visual/geometry_file_asset.h
	jorvik/visual/shader_file_asset.cpp
	jorvik/visual/shader_file_asset.h
    jorvik/visual/IDevice.h
)

# DX 12 Device stuff
IF(PROJECT_DEVICE_DX12)

LIST(APPEND JORVIK_SOURCES
jorvik/visual/dx12/device_dx12.cpp
jorvik/visual/dx12/device_dx12.h
jorvik/visual/dx12/dx_device_resources.cpp
jorvik/visual/dx12/dx_device_resources.h
jorvik/visual/dx12/pipeline_state.h
jorvik/visual/dx12/pipeline_state.cpp
jorvik/visual/dx12/root_signature.h
jorvik/visual/dx12/root_signature.cpp
jorvik/visual/dx12/constants.cpp
jorvik/visual/dx12/constants.h
)

ENDIF()

IF(1)
LIST(APPEND JORVIK_SOURCES
jorvik/visual/vulkan/device_vulkan.cpp
jorvik/visual/vulkan/device_vulkan.h
jorvik/visual/vulkan/vk_device_resources.cpp
jorvik/visual/vulkan/vk_device_resources.h
jorvik/visual/vulkan/vk_debug_callback.cpp
jorvik/visual/vulkan/vk_debug_callback.h
jorvik/visual/vulkan/vk_window.cpp
jorvik/visual/vulkan/vk_window.h
)
ENDIF()

LIST(APPEND TEST_SOURCES
)

LIST(APPEND JORVIK_INCLUDES
    jorvik
    )
