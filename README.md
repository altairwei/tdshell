# TDShell

An interactive Telegram command line interface utilizing [TDLib](https://github.com/tdlib/td).

## Usage

### Running Modes

TDShell can be run in two different ways:

* **Interactive mode**: Simply type `tdshell` with some options.
* **One-shot command mode**: Run commands as if they are sub-commands of `tdshell`.

A Telegram database will be generated in the current working directory. However, you can change its location using the `tdshell -d path/to/folder` option. To encrypt this database, you will be prompted to enter an encryption key. If you prefer not to set an encryption key, use `tdshell -N` to set an empty key.

Type `help` to view available commands, and `exit` to close TDShell.

### Downloading Media Files

This is the primary goal of my development of TDShell:

```shell
# Download files in messages with given message IDs
download --chat-id -1001472283207 --ids 6887137168 6887184146

# Download files in messages given post links
download --output-folder ./videos/news/ --links https://t.me/AChannel/6560 https://t.me/BChannel/123

# Download files from message XX to message YY
download --chat-id AChannel --range XX YY
download --chat-id AChannel --range XX,YY
download --chat-id AChannel --range XX --range YY
```

### Viewing Chats or Messages

Use `--help` to view options for the following commands:

* `chats`: A command to list all chats in your account.
* `history`: View the history of a chat.
* `chatinfo`: Retrieve information about a chat.
* `messagelink`: Read post links and print messages. 

## How to Develop

### Windows

Use `vcpkg` to install dependencies:

```shell
./vcpkg.exe install gperf:x64-windows openssl:x64-windows zlib:x64-windows
```

Place the toolchain file `CMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake` in the CMake configuration settings of your preferred IDE. Please refer to [Using vcpkg with CMake](https://github.com/Microsoft/vcpkg#using-vcpkg-with-cmake).

Example for VSCode:

```json
{
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake"
    }
}
```

### Linux

Ensure that `zlib`, `OpenSSL`, and `gperf` are installed:

```shell
sudo apt-get install zlib1g-dev libssl-dev gperf
```

Then, use CMake to build the project.
