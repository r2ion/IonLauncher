# Build Instructions
The following steps will allow you to compile your own NorthstarLauncher executable from the code in this repository.

(Images aren't changed, use your own intelligence to figure out how it should be in the latest Visual Studio, I couldn't be bothered fully rewriting this)

*This guide assumes you have already installed Northstar as shown in [this page](https://r2northstar.gitbook.io/r2northstar-wiki/installing-northstar/basic-setup)*

## Windows
### Steps
1. **Install Git** from [this link](https://git-scm.com)
2. **Clone** the [R2Ion/IonLauncher](https://github.com/R2Ion/IonLauncher) repo using this command `git clone https://github.com/R2Ion/IonLauncher.git`
3. **Install Visual Studio 2026** from [this link](https://visualstudio.microsoft.com/downloads/). Northstar uses the vc2026 compiler, which is provided with Visual Studio. *You only need to download the Community edition.* It is highly likely that older versions (2022 etc) will not work, but this hasn't been checked.
4. If you are prompted to download Workloads, check "Desktop Development with C++" and ".NET Desktop Development". If you are not prompted, don't worry, you'll be able to install this later on as well.

![Desktop Development Workload](https://user-images.githubusercontent.com/40443620/147722260-b6ec90e9-7b74-4fb7-b512-680c039afaef.png)

6. **Open the NorthstarLauncher folder** you unzipped with Visual Studio.

7. You may be prompted by visual studio to generate the cmake cache. To do this open the root `CMakeLists.txt` and click **Generate**. Once you do this you should be able to build the project. Note that some editors might not use CMake Presets out of the box, you only need to check this if you're getting errors while configuring. If so, double check you are using one (might be something like `x64 Release` consult your nearest LLM if this doesn't make sense)

![Generate CMake Cache Prompt](https://github.com/R2Northstar/NorthstarLauncher/assets/64418963/2d825acb-3118-4cf0-84d2-cbc9174dece5)

8. In the top ribbon, press on **Build,** then **Build all.**

![Build Ribbon Button](https://github.com/R2Northstar/NorthstarLauncher/assets/64418963/cd8e87b6-7b0f-462c-88bf-639777396501)

9. Wait for your build to finish. You can check on its status from the Output tab at the bottom
10. Once your build is finished, **Open the directory in File Explorer.** Then, go to `build/game`. You should see NorthstarLauncher.exe and Northstar.dll, as well as a couple other files.
11. **_In your Titanfall2 directory_**, move the preexisting NorthstarLauncher.exe and Northstar.dll into a new folder. You'll want to keep the default launcher backed up before testing any changes.
12. Back in the build debug directory, **Move NorthstarLauncher.* (all files are needed if you aren't publishing the launcher to a single file) and Northstar.dll to your Titanfall2 folder.**

If everything is correct, you should now be able to launch the Northstar client with your changes applied.

You can tweak how Ion is built with `config.cmake` which will be created after first generating the cmake cache.

* `NS_BINARY_DIR` sets the path Northstar will output binaries to, useful for testing e.g you might use `"C:/Program Files/EA Games/Titanfall2"`
* `NS_PUBLISH_LAUNCHER` when `ON` (which it is by default), publishes the C# launcher WPF binary into a single executable, you might want to disable (set value to `OFF`) if you need to do proper testing.
* `NS_BUILD_LEGACY_CPP_LAUNCHER` when `ON` will build the old C++ launcher for Northstar, useful if you're having issues with the C# binary.

### VS Build Tools

Developers who can work a command line may be interested in using [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2026) to compile the project, as an alternative to installing the full Visual Studio IDE.

Note that you cannot use Ninja for Ion, this is due to the haunted technology we use to build C# in cmake.

- Follow the same steps as above for Visual Studio Build Tools, but instead of opening in Visual Studio, run the Command Prompt for VS 2022 and navigate to the NorthstarLauncher.

- Run `cmake . -G "Visual Studio 18 2026" -B build` to generate build files.

- Run `cmake --build build/` to build the project.

## Linux

**Note these probably DO NOT work for Ion, if you are a Linux developer who would like to fix this please get in touch.**

### Steps
1. Clone the GitHub repo
2. Use `cd` to navigate to the cloned repo's directory
3. Then, run the following commands in order:
* `docker build --rm -t northstar-build-fedora .`
* `docker run --rm -it -e CC=cl -e CXX=cl --mount type=bind,source="$(pwd)",destination=/build northstar-build-fedora cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Windows -G "Ninja" -B build`
* `docker run --rm -it -e CC=cl -e CXX=cl --mount type=bind,source="$(pwd)",destination=/build northstar-build-fedora cmake --build build/`

#### Podman

When using [`podman`](https://podman.io/) instead of Docker on an SELinux enabled distro, make sure to add the `z` flag when mounting the directory to correctly label it to avoid SELinux denying access.

As such the corresponding commands are

* `podman build --rm -t northstar-build-fedora .`
* `podman run --rm -it -e CC=cl -e CXX=cl --mount type=bind,source="$(pwd)",destination=/build,z northstar-build-fedora cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Windows -G "Ninja" -B build`
* `podman run --rm -it -e CC=cl -e CXX=cl --mount type=bind,source="$(pwd)",destination=/build,z northstar-build-fedora cmake --build build/`
