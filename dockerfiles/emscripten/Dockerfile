FROM trzeci/emscripten-slim:sdk-tag-1.37.21-64bit
ADD files/ /src/files/
ADD https://github.com/akheron/jansson/archive/v2.10.tar.gz /src/
ADD https://github.com/nih-at/libzip/archive/rel-1-3-0.tar.gz /src/
ADD https://github.com/xiph/speexdsp/archive/SpeexDSP-1.2rc3.tar.gz /src/

RUN \
  cd /src/ && \
  tar xf v2.10.tar.gz && rm v2.10.tar.gz && \
  tar xf rel-1-3-0.tar.gz && rm rel-1-3-0.tar.gz && \
  tar xf SpeexDSP-1.2rc3.tar.gz && rm SpeexDSP-1.2rc3.tar.gz

RUN \
  apt -y update && \
  apt install -y build-essential cmake tree

RUN \
  tree /src/

# Build jansson
RUN \
  mkdir -p /src/jansson-2.10/build && \
  cd       /src/jansson-2.10/build && \
  cmake ../ -DCMAKE_TOOLCHAIN_FILE=/src/files/Emscripten-wasm.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo && \
  make
