# brainclouds2s-cpp
brainCloud S2S C++ api

Add to project:
```
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/brainclouds2s-cpp/cmake/")

add_subdirectory(brainclouds2s-cpp)

target_include_directories(${PROJECT_NAME} PRIVATE brainclouds2s-cpp/include)
target_link_libraries(${PROJECT_NAME} PRIVATE brainclouds2s)
```

Add to code:
```
#include <brainclouds2s.h>
#include <json/json.h>

using namespace BrainCloud;


```

To run test:
```
mkdir build 
cd build
cmake -DBUILD_TEST=ON -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target testbcs2s --config Debug -- -j 4
# make sure to copy app/server info into test/ids.txt
./testbcs2s
```
To filter test:
```
./testbcs2s [RTT]
```
