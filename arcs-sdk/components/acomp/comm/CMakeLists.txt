# Stream component sources
listenai_library_sources(
    stream/acomp_stream.c
    stream/acomp_stream_ipc.c
)

# Virtio component sources  
listenai_library_sources(
    virtio/virtqueue.c
    virtio/virtio_port.c
)

# Include directories
listenai_include_directories(
    ./stream
    ./virtio
)
