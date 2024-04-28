# syntax=docker/dockerfile:1

FROM --platform=linux/amd64 ubuntu:20.04
ENV TZ=Europe/Minsk
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
ENV DEBIAN_FRONTEND=noninteractive

# Update the package lists and install essential packages
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python-is-python3 \
    #software-properties-common \
    curl \
    wget \
    gcc \
    g++ \
    make \
    #libgtest-dev \
    git-all \
    && apt-get update && apt-get install -y cmake \
    #libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

# Go
RUN wget -O /tmp/go1.20.1.linux-amd64.tar.gz https://go.dev/dl/go1.20.1.linux-amd64.tar.gz && \
    tar -xzf /tmp/go1.20.1.linux-amd64.tar.gz -C /usr/local && \
    echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc && rm -rf /tmp/go1.20.1.linux-amd64.tar.gz


# # Rust
# RUN wget -q -O - https://sh.rustup.rs | sh -s -- -y --profile minimal

# # Scala
# RUN echo "deb https://repo.scala-sbt.org/scalasbt/debian all main" | tee /etc/apt/sources.list.d/sbt.list && \
#     echo "deb https://repo.scala-sbt.org/scalasbt/debian /" | tee /etc/apt/sources.list.d/sbt_old.list && \
#     wget -q -O - "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x2EE0EA64E40A89B84B2DF73499E82A75642AC823" | apt-key add && \
#     apt update && apt install sbt --no-install-recommends -y && echo "alias sbt='sbt -Dsbt.rootdir=true'" >> ~/.bashrc

# # Node and NPM
# RUN wget -q -O - https://deb.nodesource.com/setup_18.x | bash - && \
#     apt install -y nodejs

ARG CACHEBUST=1

# setup bashrc
#RUN echo "source /Users/poseidon/Desktop/EPFL/MA1/IntroOS/devel/setup.bash" >> /root/.bashrc

WORKDIR /home/root/cs470
CMD /bin/bash