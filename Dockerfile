FROM emrevoid/ndnsim-os:ubuntu-20.04

LABEL maintainer="Casadei Andrea <andrea.casadei22@studio.unibo.it>, Margotta Fabrizio <fabrizio.margotta@studio.unibo.it>"

# Utility environment variables
ENV NDNSIM_PATH=${HOME}/ndnSIM
ENV NS3_PATH=${NDNSIM_PATH}/ns-3

# --- ndnSIM Setup ---

# Check out to the used build of ns-3
WORKDIR ${NS3_PATH}
RUN git pull --all \
    && git checkout 173aec9e080c71e75cca67fb3088834a52f4956a

# Configure pybindgen
WORKDIR ${NDNSIM_PATH}/pybindgen
RUN git config --global user.email "gmaclean@ucsc.edu" && \
    git config --global user.name "graeme" && \
    git pull --all && \
    git checkout 572e1d92d9d0388de86b73d1ee37a10d0b0b633a

# Checkout specific versions for NFD and ndn-cxx
WORKDIR ${NS3_PATH}/src/ndnSIM/NFD
RUN git checkout NFD-0.7.0-ndnSIM

WORKDIR ${NS3_PATH}/src/ndnSIM/ndn-cxx
RUN git checkout ndn-cxx-0.7.0-ndnSIM

# Prepare environment for ndnSIM
ENV PKG_CONFIG=/usr/local/lib/pkgconfig:${NS3_PATH}/build/src/core
ENV LD_LIBRARY_PATH=/usr/local/lib

# Configure, compile, and install ndnSIM
WORKDIR ${NS3_PATH}
RUN ./waf configure -d optimized && ./waf
USER root

# --- (optional for setting up visualization) ---
# GTK and X11 support for visualization
RUN sed -i '/cloud.r-project.org/d' /etc/apt/sources.list && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E298A3A825C0D65DFD57CBB651716619E084DAB9

# Install Python 2.7 and required packages
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    python2.7 \
    python2.7-dev \
    python-is-python2 \
    graphviz \
    libgraphviz-dev \
    pkg-config \
    libcairo2-dev \
    libgirepository1.0-dev \
    gir1.2-gtk-3.0 \
    gir1.2-goocanvas-2.0 \
    python3-pip \
    curl \
    x11-apps \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install pip for Python 2
RUN curl https://bootstrap.pypa.io/pip/2.7/get-pip.py --output get-pip.py && \
    python2.7 get-pip.py && \
    rm get-pip.py

# Install Python 2 packages via pip
RUN pip2 install \
    pygraphviz \
    ipython==5.10.0 \
    pygobject

RUN ./waf install && ldconfig
# Fix for potential issues with library paths
RUN ln -s /usr/lib/x86_64-linux-gnu/ /usr/lib64
USER ndn

# --- End Setup ---

# --- Add simulation code ---

# Switch to root
USER root

WORKDIR /app

# Install build tools, OpenSSL development libraries, gprof, and other useful utilities
RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    binutils \
    vim \
    nano \
    python3 \
    python3-pip \
    python3-matplotlib \
    python3-numpy \
    iputils-ping \
    iproute2 \
    htop \
    tmux \
    screen \
    wget \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# (Optional) Install Google Test libraries, kept from original setup.
RUN apt-get update && apt-get install -y libgtest-dev cmake && \
    cd /usr/src/gtest && \
    cmake . && \
    make && \
    cp ./lib/*.a /usr/lib && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Create directories for results and configurations
RUN mkdir -p /app/results /app/configs

# Create a script to compile the code with up-to-date headers
RUN echo '#!/bin/bash\n\
cd /app\n\
g++ -o tree-test tree-test.cpp -lcrypto -pthread -std=c++17 "$@"\n\
echo "Compilation complete. Run with: ./tree-test [test_mode] [options]"\n\
' > /app/compile.sh && chmod +x /app/compile.sh

# Expose port 12345 for the integration test (UDP-based network simulation).
EXPOSE 12345

# Copy files as the last step to leverage Docker caching
# Note: when rebuilding, only this step will be executed if files changed
COPY crypto.hpp tree-map.hpp tree-queue.hpp tree-test.cpp /app/

# Set working directory
WORKDIR /app

# Compile the code during build
RUN /app/compile.sh

# Switch back to the ndn user for better security
USER ndn
RUN mkdir -p ~/results

# Set appropriate permissions
USER root
RUN chown -R ndn:ndn /app
USER ndn

# Use bash as the entrypoint and run the initialization script
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/app/init.sh && /bin/bash"]