workspace "Test"
	configurations {"Debug", "Release"}
	platforms {"x32", "x64"}
	location "build"

libdirs {
os.findlib("d3d12.lib"),
os.findlib("dxgi.lib"),
os.findlib("d3dcompiler.lib"),
os.findlib("dxguid.lib"),
os.findlib("dxcore.lib"),
}

includedirs {"**include**", "**include//directx"}

cppdialect "C++20"
cdialect "C17"

links {"d3d12", "dxgi", "d3dcompiler", "dxguid", "dxcore"}

filter "system:Windows"
	system "windows"
	systemversion "10.0.22621.0"
	defines {"_WINDOWS"}
	libdirs {
	os.findlib("windowscodecs.lib")
	}
	links {"windowscodecs"}
	externalincludedirs {
	"C:\\Program Files %28x86%29\\Windows Kits\\10\\Include\\10.0.22621.0\\shared",
	"C:\\Program Files %28x86%29\\Windows Kits\\10\\Include\\10.0.22621.0\\um"
	}

filter "configurations:Debug"
	defines {"_DEBUG"}
	optimize("Off")

filter {"platforms:x32"} 
	architecture "x32"
filter {"platforms:x64"}
	architecture "x64"

filter "configurations:Release"
	defines {"NDEBUG"}
	optimize "On"

filter {"files:**.hlsl"}
	flags "ExcludeFromBuild"
	buildaction "FxCompile"
	shadermodel "5.1"

project "Executable"
	kind "WindowedApp"
	language "C++"
	location "build"
	files {"**.cpp", "**.h", "**.hlsl"}
	