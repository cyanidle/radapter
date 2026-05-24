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
    git cmake ninja-build build-essential \
    libqt5websockets5-dev libqt5serialbus5-dev libqt5serialport5-dev \
    libqt5sql5-mysql libqt5sql5-odbc libqt5sql5-psql libqt5sql5-sqlite \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN echo "source /opt/ros/jazzy/setup.bash" >> /home/ubuntu/.bashrc
ENV SHELL /bin/bash

# Set the default user
USER ubuntu
CMD ["/bin/bash"]
