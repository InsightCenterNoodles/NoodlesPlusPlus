# NOODLES C++ Implementation

Requirements

| Compiler | Version |
|----------| --------|
| XCode | >= 12.4 |

Dependences

|Package | Version |
|--------|---------|
|Qt      | >= 5.15.2    |
|CMake   | >= 3.16    |

Note that the Qt package must include the 3DCore, 3DRender, 3DExtras for the example client.

To build, we use CMake. If you are not using QtCreator as your IDE, you should specify your Qt installation either by

* Set `CMAKE_PREFIX_PATH` environment variable to the Qt installation prefix
* Set `Qt5_DIR` CMake variable (using `ccmake`, etc) to the location of your `Qt5Config.cmake` file.

You can then build (`make`).

You should have the following binaries built:

| Binary | Description |
|--------| ------------|
|noodlesServer| Server library|
|noodlesClient| Client library|
|plottyN | Example plotting server|
|exampleClient| Example 3D graphical client|
