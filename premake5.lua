include "./vendor/premake/premake_customization/solution_items.lua"

workspace "LoveInjector"
	architecture "x64"
	startproject "LoveInjector"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["xorstr"] = "%{wks.location}/LoveInjector/vendor/xorstr/include"

group "Core"
	include "LoveInjector"
group ""

group "Dependencies"
	include "vendor/premake"
group ""