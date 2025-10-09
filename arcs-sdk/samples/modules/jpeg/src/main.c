/*
 * JPEG to RGB565 conversion sample from memory buffer
 * Uses IJG library to decompress JPEG and convert to RGB565 format
 */

#include "lisa_log.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* Include IJG JPEG library header */
#include "jpeglib.h"
#include "jerror.h"

/* Error handling structure */
struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */
    jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct my_error_mgr *my_error_ptr;

/* Custom error handler for JPEG decompression */
METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr) cinfo->err;

    /* Always display the message. */
    (*cinfo->err->output_message) (cinfo);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}

/* Memory source manager for JPEG decompression from memory */
typedef struct {
    struct jpeg_source_mgr pub;   /* public fields */
    const unsigned char *data;    /* source data */
    size_t data_size;             /* size of data */
    boolean start_of_data;        /* have we gotten any data yet? */
} mem_source_mgr;
typedef mem_source_mgr *mem_src_ptr;

/* Memory source manager initialization */
METHODDEF(void)
init_mem_source(j_decompress_ptr cinfo)
{
    mem_src_ptr src = (mem_src_ptr)cinfo->src;
    src->start_of_data = TRUE;
}

/* Fill input buffer from memory */
METHODDEF(boolean)
fill_mem_input_buffer(j_decompress_ptr cinfo)
{
    mem_src_ptr src = (mem_src_ptr)cinfo->src;

    if (src->data_size == 0) {
        /* Insert a fake EOI marker */
        static const JOCTET EOI_BUFFER[2] = {0xFF, JPEG_EOI};
        src->pub.next_input_byte = EOI_BUFFER;
        src->pub.bytes_in_buffer = 2;
        return FALSE;
    }

    src->pub.next_input_byte = src->data;
    src->pub.bytes_in_buffer = src->data_size;
    src->start_of_data = FALSE;
    src->data_size = 0;  /* We've consumed it all */

    return TRUE;
}

/* Skip data in memory buffer */
METHODDEF(void)
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    mem_src_ptr src = (mem_src_ptr)cinfo->src;

    if (num_bytes > 0) {
        while (num_bytes > (long)src->pub.bytes_in_buffer) {
            num_bytes -= (long)src->pub.bytes_in_buffer;
            (void)fill_mem_input_buffer(cinfo);
        }
        src->pub.next_input_byte += (size_t)num_bytes;
        src->pub.bytes_in_buffer -= (size_t)num_bytes;
    }
}

/* Term source - nothing to do */
METHODDEF(void)
term_source(j_decompress_ptr cinfo)
{
    /* Nothing to do */
}

/* We're using the jpeg_mem_src function from the IJG library (jdatasrc.c) */


/* Convert RGB888 (24-bit) to RGB565 (16-bit) */
static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    /* RGB565: 5 bits for red, 6 bits for green, 5 bits for blue */
    uint16_t rgb565 = ((r & 0xF8) << 8) | /* 5 bits for red */
                       ((g & 0xFC) << 3) | /* 6 bits for green */
                       ((b & 0xF8) >> 3);  /* 5 bits for blue */
    return rgb565;
}

/* JPEG to RGB565 conversion function from memory buffer */
int jpeg_memory_to_rgb565(const unsigned char *jpeg_data, size_t jpeg_size, 
                         uint16_t **rgb565_output, int *width, int *height)
{
    /* JPEG decompression variables */
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    JSAMPARRAY buffer;        /* Output row buffer */
    int row_stride;           /* Physical row width in output buffer */
    uint16_t *rgb565_buffer = NULL;  /* Buffer for all RGB565 pixels */
    uint16_t *current_line = NULL;   /* Pointer to current line in output buffer */
    
    /* Set up the error handler */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    
    /* Establish the setjmp return context for my_error_exit to use */
    if (setjmp(jerr.setjmp_buffer)) {
        /* If we get here, the JPEG code has signaled an error */
        jpeg_destroy_decompress(&cinfo);
        if (rgb565_buffer != NULL) {
            free(rgb565_buffer);
        }
        return -1;
    }
    
    /* Initialize the JPEG decompression object */
    jpeg_create_decompress(&cinfo);
    
    /* Specify data source from memory */
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);
    
    /* Read file header to get image information */
    (void) jpeg_read_header(&cinfo, TRUE);
    
    /* Set parameters for decompression */
    /* Output colorspace - we want RGB */
    cinfo.out_color_space = JCS_RGB;
    
    /* Start decompressor */
    (void) jpeg_start_decompress(&cinfo);
    
    /* Get image dimensions */
    *width = cinfo.output_width;
    *height = cinfo.output_height;
    
    /* Calculate row stride (bytes per row) */
    row_stride = cinfo.output_width * cinfo.output_components;
    
    /* Make a sample array to hold a row of JPEG pixels */
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
    
    /* Allocate memory for the full RGB565 image */
    rgb565_buffer = (uint16_t *)malloc(cinfo.output_width * cinfo.output_height * sizeof(uint16_t));
    if (!rgb565_buffer) {
        LOGI("Memory allocation failed\n");
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }
    
    /* Point to the start of the buffer */
    current_line = rgb565_buffer;
    
    /* Process each scanline */
    while (cinfo.output_scanline < cinfo.output_height) {
        /* Read a line of pixels */
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);
        
        /* Convert RGB to RGB565 */
        for (int i = 0; i < cinfo.output_width; i++) {
            uint8_t r = buffer[0][i * 3];     /* Red */
            uint8_t g = buffer[0][i * 3 + 1]; /* Green */
            uint8_t b = buffer[0][i * 3 + 2]; /* Blue */
            
            /* Convert RGB888 to RGB565 */
            current_line[i] = rgb888_to_rgb565(r, g, b);
        }
        
        /* Move to the next line in the output buffer */
        current_line += cinfo.output_width;
    }
    
    /* Finish decompression */
    (void) jpeg_finish_decompress(&cinfo);
    
    /* Release the JPEG decompression object */
    jpeg_destroy_decompress(&cinfo);
    
    /* Set the output buffer pointer */
    *rgb565_output = rgb565_buffer;
    
    return 0;
}

/* This is a minimal valid 1x1 pixel JPEG image in binary form */
static const unsigned char sample_jpeg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 
    0x01, 0x01, 0x00, 0x60, 0x00, 0x60, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 
    0x00, 0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08, 0x07, 0x07, 0x07, 0x09, 
    0x09, 0x08, 0x0a, 0x0c, 0x14, 0x0d, 0x0c, 0x0b, 0x0b, 0x0c, 0x19, 0x12, 
    0x13, 0x0f, 0x14, 0x1d, 0x1a, 0x1f, 0x1e, 0x1d, 0x1a, 0x1c, 0x1c, 0x20, 
    0x24, 0x2e, 0x27, 0x20, 0x22, 0x2c, 0x23, 0x1c, 0x1c, 0x28, 0x37, 0x29, 
    0x2c, 0x30, 0x31, 0x34, 0x34, 0x34, 0x1f, 0x27, 0x39, 0x3d, 0x38, 0x32, 
    0x3c, 0x2e, 0x33, 0x34, 0x32, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x09, 0x09, 
    0x09, 0x0c, 0x0b, 0x0c, 0x18, 0x0d, 0x0d, 0x18, 0x32, 0x21, 0x1c, 0x21, 
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 
    0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 
    0x32, 0x32, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x01, 0x00, 0x01, 0x03, 
    0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00, 
    0x15, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xff, 0xc4, 0x00, 0x14, 
    0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x01, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 
    0x11, 0x00, 0x3f, 0x00, 0xb2, 0xc0, 0x07, 0xff, 0xd9
};

int main(int argc, char **argv)
{
    LOGI("JPEG (from memory) to RGB565 converter sample\n");
    
    uint16_t *rgb565_data = NULL;
    int width, height;
    
    /* Convert JPEG to RGB565 */
    int result = jpeg_memory_to_rgb565(sample_jpeg, sizeof(sample_jpeg), 
                                      &rgb565_data, &width, &height);
    
    if (result == 0) {
        LOGI("Conversion completed successfully\n");
        LOGI("Image dimensions: %d x %d\n", width, height);
        
        /* You can now use the RGB565 data */
        /* For example, display it on an LCD screen */
        
        /* Don't forget to free the memory when done */
        free(rgb565_data);
    } else {
        LOGI("Conversion failed\n");
    }
    
    return 0;
}