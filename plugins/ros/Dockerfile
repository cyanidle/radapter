FROM ros:jazzy

ARG UID=1000
ARG GID=1000

RUN usermod --uid ${UID} ubuntu
RUN groupmod --gid ${GID} ubuntu

# Add sudo support
RUN apt-get -y update
RUN apt-get install -y sudo
RUN echo ubuntu ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/ubuntu
RUN chmod 0440 /etc/sudoers.d/ubuntu
RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y \
    colcon \
    ccls \
    python3-colcon-common-extensions \
    python3-pip \
    clang \
    clang-format \
    clang-tidy \
    ros-jazzy-turtlesim \
    ros-jazzy-rqt* \
    ros-jazzy-example-interfaces ros-jazzy-demo-nodes-cpp \
    git cmake ninja-build build-essential \
    qt6-base-dev qt6-websockets-dev qt6-serialbus-dev qt6-serialport-dev \
    libqt6sql6-sqlite \
    curl libssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://raw.githubusercontent.com/dmtrKovalenko/fff.nvim/main/install-mcp.sh | bash
RUN echo "source /opt/ros/jazzy/setup.bash" >> /home/ubuntu/.bashrc
ENV SHELL /bin/bash

# Set the default user
USER ubuntu
CMD ["/bin/bash"]
