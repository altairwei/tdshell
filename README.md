# TDShell

An interactive Telegram command line interface around [TDLib](https://github.com/tdlib/td).

## Usage

### Running mode

There are two ways to run TDShell:

* interactive mode: 
* one-shot command mode: run a commands like the sub-command of `tdshell`

A Telegram database will be produced in current working directory, but this can be changed using `-D path/to/folder` option. You can encrypt this database using a password, by specify `-K`, a prompt will be shown for entering the encryption key.

### Download media

This is the primary goal of my developing of TDShell

### View chats

* `chats`: a command to list all chats in your account.
* `history`: view the history of a chat.

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

### Linux

Make sure `zlib`, `OpenSSL` and `gperf` have been installed:

```shell
sudo apt-get install zlib1g-dev libssl-dev gperf
```

Then use CMake to build this project.
