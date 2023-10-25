## Development Environment

This subsection covers the basic of developing with containers and building programs.
* [Clone the Repository](#Clone-the-Repository)
* [Container](#Container)
* [Build Program](#Build-Program)
* [Generate Config File](#Generate-Config-File)

### Clone the Repository

You have to explicitly state `--recursive` for cloning the repository and its submodules.
```bash
git clone --recursive git@github.com:iis-rlg-lab/game-solver.git
```

### Container

Before start, we should have an environment to build and run the framework. The framework is running in a container. No matter we want to develop some new functionality or train a network and solve a game, I recommend to do everything in the container.

To create a container, first install podman (or docker, but I'm not sure whether it can work in docker, probabily yes). The installation process can refer [here](https://podman.io/getting-started/installation).

Check the podman is installed successfully:
```bash
podman --version
```

Next create a solver container by our script:
```bash
./scripts/start-container.sh
```
By doing this, we will directly into a created container, you can do anything inside a container. The script will mount the working directory into the container path: "**/workspace**".

> **Hint**: Be careful that if you are not using the NV GPU server in CGILab, the script will try to mount a path "**/mnt/nfs/work/zero/solving-7x7-Killall-Go**" to the container path "**/7x7KillallGo**". The training data including the check point will be stored to this path by default.
Therefore, if you want to change the default path, you can follow the below hard fix:
```bash
# In start-container.sh
# original is
# ./minizero/scripts/start-container.sh $@ -v /mnt/nfs/work/zero/solving-7x7-Killall-Go:/7x7KillallGo --image kds285/solver
# Change the -v path as below
./minizero/scripts/start-container.sh $@ -v {your default path}:/7x7KillallGo --image kds285/solver
```
> However, it is a hard fix, so **don't push this hard fix to the repo**.

If you want to add some podman option when creating a container, you can just add the option parameter follow the command "**./sciprts/start-container.sh**". For example:
```bash
./scripts/start-container.sh -d --name "killallgo"
# -d option will create a container running in back ground
# --name option let you specify the container name
```

### Build Program

Build our framework is super easy, just doing this:
```bash
./scripts/build.sh {game type}
```
For example, to build the framework with game "7x7 Kill all Go":
```bash
./scripts/build.sh killallgo
```

### Generate Config File

To generate config file, you have to build the program first. In this section, I supposed that you have build the program and generate a binary executable file.

You can generate a default config file by following:
```bash
./build/{game type}/{game type}_solver -gen {file name}.cfg
```
For example:
```bash
./build/killallgo/killallgo_solver -gen test.cfg
```

To generate the config respect to the game type, you have to build the program with the specific game type first. And the binary executable file will generate the default config file according to which game you build.
