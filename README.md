# brainCloud S2S C++ api

This is the brainCloud Server-To-Server C++ library.

Requirements:

- git or downloaded zip archive of brainCloudS2S library 
- Visual Studio 17 2022
- CMake https://cmake.org/
- vcpkg https://vcpkg.io/en/

### Important note: 
Once you have installed vcpkg, make sure you have an environment variable with the name of `VCPKG` set to the root path of your vcpkg installation. Without this CMake will not find the correct toolchain file.

In Powershell you can set the environment variable like so:
```bash
$env:VCPKG="C:/path/to/vcpkg"
```
In Windows Command Prompt:
```bash
set VCPKG=C:/path/to/vcpkg
```
In Unix terminal:
```bash
export VCPKG=/path/to/vcpkg
```

By default, this project uses Visual Studio to generate the project solution. You may choose other generators such as Ninja for faster builds by changing the generator value in the appropriate preset that you are using in the `CMakePresets.json` file.
Example:
```bash 
	"generator": "Visual Studio 17 2022"
```
to 
```bash 
	"generator": "Ninja"
```
You could also change this to another version of Visual Studio if needed.

## Building static library on Windows

Steps (Command lines are done in Powershell or Windows command prompt):

1.	Clone the repository somewhere on your machine, then cd into it
	```bash
	git clone https://github.com/getbraincloud/brainclouds2s-cpp
	cd brainclouds2s-cpp
	```
2.	Update submodules so thirdparty libraries are present when building.
	```bash
	git submodule update --init --recursive
	```
3.	Generate the solution using cmake and the appropriate preset, for windows we will use the `windows-default` preset. If you want to include the unit tests in the build, add the `-DBUILD_TESTS=ON` flag like so
	```bash
	1cmake --preset=windows-default . -DBUILD_TESTS=ON
	```
4.	Once the solution is generated, cd into the build folder and build the library with cmake. Or you can build it by opening the generated solution in Visual Studio and directly building it from there. 
	```bash
	cd build
	cmake --build .
	```
	
This will build the static library into this output folder: `build/Debug`
 
## Adding the library source to your project:
```
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/brainclouds2s-cpp/cmake/")

add_subdirectory(brainclouds2s-cpp)

target_include_directories(${PROJECT_NAME} PRIVATE brainclouds2s-cpp/include)
target_link_libraries(${PROJECT_NAME} PRIVATE brainclouds2s)
```

## How to use in your code:
```
#include <brainclouds2s.h>
#include <json/json.h>

using namespace BrainCloud;

```

## Running Unit Tests 

Ensure you have generated the solution with this option: `-DBUILD_TESTS=ON`

Before running the Unit Tests, you must have a valid brainCloud app setup in the brainCloud portal, and create an ids.h file

The ids.h file should look like this:
```
serverUrl=https://api.braincloudservers.com/dispatcherv2
appId=[your-AppId]
secret=[your-App-Secret]
version=[your-version]
serverName=[your-server-name]
serverSecret=[your-server-secret]
s2sUrl=https://api.braincloudservers.com/s2sdispatcher
```
Make sure to change these values to your appropriate app values and that there are no trailing spaces. And place this file where you want to run your tests.

After your solution is built you can either run the Unit tests directly within Visual Studio, or you can run the tests build output file here `build/tests/Debug/testbcs2s.exe` - If you are running the .exe in this location, this is where you would put your ids.h file. If you are running the tests from Visual Studio, you would put your ids.h file in the `build/tests` folder instead of the `build/tests/Debug` folder. 
