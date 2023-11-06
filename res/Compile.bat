@echo off
set vk=VULKAN_SDK
setlocal enableDelayedExpansion
!%vk%!/Bin/glslc.exe shader.vert -o vert.spv
!%vk%!/Bin/glslc.exe shader.frag -o frag.spv
pause