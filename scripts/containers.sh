set -eu

docker buildx build -f scripts/Dockerfile.native-headless  -t radapter-headless:qt6 .
docker buildx build -f scripts/Dockerfile.native-gui       -t radapter-gui:qt6 .
docker buildx build -f scripts/Dockerfile.cross-headless   -t radapter-headless:qt6-arm64 .
docker buildx build -f scripts/Dockerfile.cross-gui        -t radapter-gui:qt6-arm64 .