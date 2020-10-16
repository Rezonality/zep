-- The _ACTION variable can be null, which will be annoying.
-- Let's make a action that won't be null
local action = _ACTION or ""

-- The test solution
solution "nod_tests"
	location             ( "build/" .. action )
	configurations       { "debug", "release" }
	includedirs          { ".", "../include" }

	-- Add some flags for gmake builds
	if _ACTION == "gmake" then
		buildoptions     { "-Wall" }
		buildoptions     { "-std=c++11" }
	end

	-- Since premake doesn't implement the clean command
	-- on all platforms, we define our own
	if action == "clean" then
		os.rmdir("build")
		os.rmdir("bin")
		os.rmdir("lib")
	end

	-- Debug configuration
	configuration { "debug" }
		targetdir        ( "bin/" .. action .. "/debug" )
		defines          { "_DEBUG", "DEBUG" }
		symbols          ( "On" )
		characterset     ( "Unicode" )
		libdirs          { "lib/" .. action .. "/debug" }

	-- Release configuration
	configuration { "release" }
		targetdir        ( "bin/" .. action .. "/release" )
		optimize         ( "Full" )
		defines          { "NDEBUG" }
		characterset     ( "Unicode" )
		libdirs          { "lib/" .. action .. "/release" }

-- The test project definition
project "nod_tests"
	language    "C++"
	kind        "ConsoleApp"
	uuid        "a66d28ec-a395-42c6-8982-550c956bce57"
	files {
		"**.hpp",
		"**.cpp" 
	}
