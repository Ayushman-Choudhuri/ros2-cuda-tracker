# 24.03 is the first TensorRT tag on Ubuntu 22.04, which is what ROS Humble has
# apt binaries for. 23.03 was 20.04 and cannot install Humble at all.
FROM nvcr.io/nvidia/tensorrt:24.03-py3

ARG DEBIAN_FRONTEND=noninteractive

# Install versioned system packages from manifest
COPY system-packages.txt /tmp/system-packages.txt
RUN apt-get update && \
    grep -v '^\s*#' /tmp/system-packages.txt | grep -v '^\s*$' | \
    xargs apt-get install -y --no-install-recommends && \
    rm -rf /var/lib/apt/lists/* /tmp/system-packages.txt

# ROS 2 Humble. Kept as its own layer so a TensorRT base bump does not force a
# full ROS reinstall.
RUN apt-get update && apt-get install -y --no-install-recommends \
        curl gnupg lsb-release software-properties-common && \
    curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
        -o /usr/share/keyrings/ros-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(lsb_release -cs) main" \
        > /etc/apt/sources.list.d/ros2.list && \
    apt-get update && apt-get install -y --no-install-recommends \
        ros-humble-ros-base \
        ros-humble-cv-bridge \
        ros-humble-rclcpp-lifecycle \
        ros-humble-rclcpp-components \
        python3-colcon-common-extensions \
        python3-rosdep && \
    rm -rf /var/lib/apt/lists/*

# Add video group (GID 44 matches host)
RUN groupadd -g 44 video 2>/dev/null || true

# Create dev user in video group
ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid ${USER_GID} ${USERNAME} 2>/dev/null || true \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
    && usermod -aG video ${USERNAME} \
    && mkdir -p /home/${USERNAME}/.ssh \
    && chmod 700 /home/${USERNAME}/.ssh \
    && chown ${USER_UID}:${USER_GID} /home/${USERNAME}/.ssh \
    && chsh -s $(which bash) ${USERNAME}

COPY requirements.txt /tmp/requirements.txt
RUN python3 -m pip install --no-cache-dir -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt

USER ${USERNAME}

WORKDIR /workspace
