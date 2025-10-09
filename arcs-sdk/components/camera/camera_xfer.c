#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "camera_xfer.h"

__attribute__((weak)) struct cam_xfer *spi_xfer_get(void)
{
    return NULL;
}

struct cam_xfer *cam_xfer_init(const xfer_hw_config_t *config, struct cam_xfer_queue *queue)
{
    struct cam_xfer *xfer = NULL;

    xfer = spi_xfer_get();
    assert(xfer);

    xfer->queue.queue_in = queue->queue_in;
    xfer->queue.queue_out = queue->queue_out;
    xfer->queue.width = queue->width;
    xfer->queue.height = queue->height;

    xfer->ops->cam_xfer_init(config, &xfer->queue);

    return xfer;
}

int spi_xfer_start(struct cam_xfer *xfer)
{
    return xfer->ops->cam_xfer_start();
}

int spi_xfer_stop(struct cam_xfer *xfer)
{
    return xfer->ops->cam_xfer_stop();
}

int spi_xfer_recv(struct cam_xfer *xfer, uint8_t *buf, size_t len)
{
    return xfer->ops->cam_xfer_recv(buf, len);
}

int cam_xfer_deinit(struct cam_xfer *xfer)
{
    return xfer->ops->cam_xfer_deinit();
}


