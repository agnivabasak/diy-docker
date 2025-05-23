# mini-docker
A simple and minimal replica of Docker - made for the purpose of learning.
<br>It is not a fully-fledged container runtime. 

## Prerequisites:
Make sure you have the following installed in your system:<br>
    <br>libcurl  - for handling HTTP requests
    <br>`sudo apt install libcurl4-openssl-dev`<br>
    <br>nlohmann:json -  for JSON parsing
    <br>`sudo apt install nlohmann-json3-dev`<br>
    <br>tar - for working with tar archives
    <br>`sudo apt install tar`<br>
    <br>Build tools – includes make and gcc
    <br>`sudo apt-get install build-essential.`<br>

## Steps to build an executable:
In the root directory run :<br>
    <br>`make clean` - to empty out the build directory first
    <br>`make` - to compile and get an executable
<br><br>This will store a mini-docker executable under the ./build directory

## Steps to run the code locally:
After the build is complete, you can execute <br>

<br>`sudo ./build/mini-docker run hello-world` <br>

<br>The sudo is required because the program performs privileged operations such as:
- Creating cgroups
- Setting CPU and memory limits
- Mapping user and group IDs (UID/GID)

<br>Other example images that you can run for testing:<br>
<br>`sudo ./build/mini-docker run ubuntu:latest`
<br>`sudo ./build/mini-docker run busybox:latest`
<br>`sudo ./build/mini-docker run python:latest`<br>
<br>**Note**: Since this tool implements only a minimal subset of Docker's functionality, some images may not run successfully.

### Commands
The available commands are listed below:
| Functionality | Command | Description |
| ------------ | ------------ | ------------ |
| Run Command | `sudo ./build/mini-docker run-command <command>` | Execute a single CLI command like 'ls','echo',etc in a minimal root filesystem (e.g., alpine-minirootfs) <br> Environment variable "MINIDOCKER_DEFAULT_FS" should be set to a valid path of a minimal root filesystem
| Pull Image | `sudo ./build/mini-docker pull <image name>[:<image_tag>]` | Pulls the image manifest, configuration and extracts the fs layers of the image into "/var/lib/minidocker/layers"<br>It uses "/tmp/minidocker" to store tarballs downloaded temporarily
| Run Container | `sudo ./build/mini-docker run <image name>[:<image_tag>]` | Pulls image if not available locally and then runs it in a container<br>Container fs is stored in "/var/lib/minidocker/containers" and destroyed at the end of the lifecycle

## Future Scope:

Since this is just a minimal replica of Docker, there is plenty of room for improvement and additional features.<br>
Some of the notable ones include:<br>
- Currently, the container FS is created by copying the image layers into a container directory (minidocker-\<hostname\>). Ideally, we should use a Copy-On-Write Filesystem like OverlayFS. This will help us containerize images which rely on symlinks. It will also help us save space.
- Allow containers to run in the background (detached mode), similar to Docker's -d option.
- Allow customization of memory and cpu allocated for containers (using environment variables or config file)
- Add support for features like port mapping (e.g., -p 8080:80), which are essential for exposing containerized services.
- More metadata can be stored about the images and containers, which can be further used to list images, remove images, list containers along with their statuses, start, stop, and remove containers, etc.
## Issues or bugs in the tool? Want to add a new functionality?
Contributions are always welcome. You could open up an issue if you feel like something is wrong with the tool or a PR if you just want to improve it.
