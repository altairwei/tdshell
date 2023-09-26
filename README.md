# TDShell

A Telegram command line interface around TDLib.

## How to develop

### Windows

Use `vcpkg` to install dependecies:

```shell
./vcpkg.exe install gperf:x64-windows openssl:x64-windows zlib:x64-windows
```

Put the toolchain file `CMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake` to the CMake configuration settings of your favor IDE, please refere to [Using vcpkg with CMake](https://github.com/Microsoft/vcpkg#using-vcpkg-with-cmake).

Take VSCode for example:

```json
{
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake"
    }
}
```

