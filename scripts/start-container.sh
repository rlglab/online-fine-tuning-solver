#!/bin/bash
set -e

usage()
{
    echo "Usage: ./start-container.sh [OPTION...]"
    echo ""
    echo "  -h, --help        Give this help list"
    echo "    , --image       Select the image name of the container"
    echo "  -v, --volume      Bind mount a volume into the container"
    echo "    , --name        Assign a name to the container"
	echo "  -d, --detach      Run container in background and print container ID"
    exit 1
}

if which podman >/dev/null; then
	container=podman
elif which docker >/dev/null; then
	container=docker
else
	echo "Neither podman nor docker is installed." >&2
	exit 1
fi

image_name=kds285/solver:latest
container_volume="-v .:/workspace"
container_argumenets=""
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		--image) shift; image_name=${1}
		;;
		-v|--volume) shift; container_volume="${container_volume} -v ${1}"
		;;
        --name) shift; container_argumenets="${container_argumenets} --name ${1}"
		;;
		-d|--detach) container_argumenets="${container_argumenets} -d"
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
		;;
	esac
	shift
done

container_argumenets=$(echo ${container_argumenets} | xargs)
echo "$container run ${container_argumenets} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name}"
$container run ${container_argumenets} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name}
