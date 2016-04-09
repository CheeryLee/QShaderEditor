#QShaderEdit
###Introduction
QShaderEdit is a simple multiplatform shader editor inspired by Apple's OpenGL Shader Builder. It supports ARB programs, GLSL shaders and CgFx effects.

###Requirements
You will need at least:

* Qt 5.2
* GLEW 1.3
* cmake 2.4

Optionally, if the Cg (1.4 or above) runtime is found, support for CgFx effects will be 
available.

###Building

CMake is used as the build system. You need to create build directory and run cmake:
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```
The CMAKE_BUILD_TYPE parameter allows the following values:
* **Debug**: debug build.
* **Release**: release build.
* **RelWithDebInfo**: release build with debug information.

For more info about cmake, look at cmake's documentation.

After that run make:
```bash
make
```
