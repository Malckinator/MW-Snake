workspace "Snake"
    architecture "x64"
    startproject "Snake"

    configurations {
        "Debug",
        "Release"
    }

    include "vendor/mwengine"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    vulkanSDK = os.getenv("VULKAN_SDK")
    project "Snake"
        kind "ConsoleApp"
        language "C++"

        targetdir (_WORKING_DIR .. "/bin/" .. outputdir .. "/%{prj.name}")
        objdir (_WORKING_DIR .. "/bin/int/" .. outputdir .. "/%{prj.name}")

        files {
            "src/**.cpp",
            "src/**.h"
        }

        includedirs {
            "src/",
            "vendor/mwengine/",
            vulkanSDK .. "/include"
        }

        defines {
            
        }

        links {
            "mwengine"
        }

        filter "system:windows"
            cppdialect "c++17"
            staticruntime "On"
            systemversion "latest"
        
        filter "configurations:Debug"
            defines "DEBUG"
            symbols "On"
        
        filter "configurations:Release"
            defines "RELEASE"
            optimize "On"