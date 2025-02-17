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
RUN ./waf install && ldconfig
# Fix for potential issues with library paths
RUN ln -s /usr/lib/x86_64-linux-gnu/ /usr/lib64
USER ndn

# --- End ndnSIM Setup ---

# --- Add and Build Simulation Code ---

# Switch to root to set up the simulation environment
USER root

# Set a new working directory
WORKDIR /app

# Copy  baseline simulation code from the build context to /app inside the container
COPY baseline-ndn-simulation.cpp .

# (Optional) If a C++ compiler isn't already available, uncomment the following line:
 RUN apt-get update && apt-get install -y build-essential

# Compile 
RUN g++ baseline-ndn-simulation.cpp -o baseline-ndn-simulation

# Switch back to the non-root user
USER ndn

# Set the default command to run  simulation code
CMD ["/app/baseline-ndn-simulation"]
