FROM emrevoid/ndnsim-os:ubuntu-20.04

LABEL mantainer="Casadei Andrea <andrea.casadei22@studio.unibo.it>, Margotta Fabrizio <fabrizio.margotta@studio.unibo.it>"

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
# (Optional) Change visualizer default speed:
# RUN sed -e 's/speed_adj = gtk.Adjustment(1.0/speed_adj = gtk.Adjustment(0.4/g' -i ./src/visualizer/visualizer/core.py
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

# Copy the new header and source files into the container
COPY ob-map.hpp ob-queue.hpp ob-sim.cpp /app/

# Install build tools and OpenSSL development libraries
RUN apt-get update && apt-get install -y build-essential libssl-dev

# Compile ob-sim.cpp, linking against OpenSSL libraries
RUN g++ ob-sim.cpp -o ob-sim -lssl -lcrypto

# Switch back to the non-root user
USER ndn

# Set default command to run the simulation
CMD ["/app/ob-sim"]
