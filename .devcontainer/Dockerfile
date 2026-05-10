FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

# Install versioned system packages from manifest
COPY system-packages.txt /tmp/system-packages.txt
RUN apt-get update && \
    grep -v '^\s*#' /tmp/system-packages.txt | grep -v '^\s*$' | \
    xargs apt-get install -y --no-install-recommends && \
    rm -rf /var/lib/apt/lists/* /tmp/system-packages.txt

# Add video group (GID 44 matches host)
RUN groupadd -g 44 video 2>/dev/null || true

# Create dev user in video group
ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid ${USER_GID} ${USERNAME} 2>/dev/null || true \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
    && usermod -aG video ${USERNAME}

USER ${USERNAME}

WORKDIR /workspace
