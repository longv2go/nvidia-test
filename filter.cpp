#include <stdio.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include "utils/FFmpegDemuxer.h"
#include "utils/FFmpegStreamer.h"

#define INPUT_FILE1 "a.mp4"
#define INPUT_FILE2 "b.mp4"

#define FILE1_SIZE "1920x1080"
#define FILE2_SIZE "720x1270"
#define INPUT_PIX_FMT AV_PIX_FMT_YUV420P

AVFilterContext* input_filt_buf;
AVFilterContext* input_filt_buf2;
AVFilterContext* bufferSink_ctx;

int init_filters(AVFilterGraph* filter_graph, int in_width_1, int in_height_1, int in_widht_2, int in_height_2) {
    int ret = 0;

    // input video 2
    char args[512];
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        in_width_1, in_height_1, INPUT_PIX_FMT,
        1, 25, 1, 1);
    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");
    ret = avfilter_graph_create_filter(&input_filt_buf, bufferSrc, "in", args, NULL, filter_graph);
    if (ret < 0) {
        printf("Fail to create filter bufferSrc\n");
        return -1;
    }

    // input video 2
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        in_widht_2, in_height_2, INPUT_PIX_FMT,
        1, 25, 1, 1);
    ret = avfilter_graph_create_filter(&input_filt_buf2, bufferSrc, "in2", args, NULL, filter_graph);
    if (ret < 0) {
        printf("Fail to create filter bufferSrc\n");
        return -1;
    }

    // overlay
    const AVFilter *overlayFilter = avfilter_get_by_name("overlay");
    AVFilterContext *overlayFilter_ctx;
    ret = avfilter_graph_create_filter(&overlayFilter_ctx, overlayFilter, "overlay", "W-w:H-h", NULL, filter_graph);
    if (ret < 0) {
        printf("Fail to create overlay filter\n");
        return -1;
    }

    // sink filter
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    ret = avfilter_graph_create_filter(&bufferSink_ctx, bufferSink, "out", NULL, NULL, filter_graph);
    if (ret < 0) {
        printf("Fail to create filter sink filter\n");
        return -1;
    }

    // link filters
    ret = avfilter_link(input_filt_buf, 0, overlayFilter_ctx, 0);
    if (ret != 0) {
        printf("Fail to link split filter's second pad and crop filter\n");
        return -1;
    }

    ret = avfilter_link(input_filt_buf2, 0, overlayFilter_ctx, 1);
    if (ret != 0) {
        printf("Fail to link split filter's second pad and crop filter\n");
        return -1;
    }

    ret = avfilter_link(overlayFilter_ctx, 0, bufferSink_ctx, 0);
    if (ret != 0) {
        printf("Fail to link split filter's second pad and crop filter\n");
        return -1;
    }


    // config filters
    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0) {
        printf("Fail in filter graph\n");
        return -1;
    }

    // dump 
    char *graph_str = avfilter_graph_dump(filter_graph, NULL);
    printf("Graph:\n%s\n", graph_str);

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: filter file1.yuv file2.yuv\n");
        exit(1);
    }

    int ret = 0;

    AVFilterGraph* filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        printf("Fail to create filter graph!\n");
        return -1;
    }

    FFmpegDemuxer *file1 = new FFmpegDemuxer(argv[1], FILE1_SIZE);
    FFmpegDemuxer *file2 = new FFmpegDemuxer(argv[2], FILE2_SIZE);
    FILE* outFile = fopen("out.yuv", "wb");
    if (!outFile) {
        printf("Fail to create file for output\n");
        return -1;
    }

    init_filters(filter_graph, file1->GetWidth(), file1->GetHeight(), file2->GetWidth(), file2->GetHeight());

    // input frame 1
    AVFrame *frame_in1 = av_frame_alloc();
    unsigned char *frame_buffer_in = (unsigned char *)av_malloc(av_image_get_buffer_size(INPUT_PIX_FMT, file1->GetWidth(), file1->GetHeight(), 1));
    av_image_fill_arrays(frame_in1->data, frame_in1->linesize, frame_buffer_in,
        INPUT_PIX_FMT, file1->GetWidth(), file1->GetHeight(), 1);

    // input frame 2
    AVFrame *frame_in2 = av_frame_alloc();
    unsigned char *frame_buffer_in2 = (unsigned char *)av_malloc(av_image_get_buffer_size(INPUT_PIX_FMT, file2->GetWidth(), file2->GetHeight(), 1));
    av_image_fill_arrays(frame_in2->data, frame_in2->linesize, frame_buffer_in,
        INPUT_PIX_FMT, file2->GetWidth(), file2->GetHeight(), 1);

    // output 
    AVFrame *frame_out = av_frame_alloc();
    unsigned char *frame_buffer_out = (unsigned char *)av_malloc(av_image_get_buffer_size(INPUT_PIX_FMT, file1->GetWidth(), file1->GetHeight(), 1));
    av_image_fill_arrays(frame_out->data, frame_out->linesize, frame_buffer_out,
        INPUT_PIX_FMT, file1->GetWidth(), file1->GetHeight(), 1);


    int nVideoBytes = 0, nFrame = 0;
    uint8_t *pVideo = NULL;
    do {
        bool result = file1->Demux(&pVideo, &nVideoBytes);
        // printf("Get video bytes: %d\n", nVideoBytes);

        if (nVideoBytes && result) {
            frame_in1->data[0] = pVideo;
            frame_in1->data[1] = pVideo + file1->GetWidth() * file1->GetHeight();
            frame_in1->data[2] = pVideo + file1->GetWidth() * file1->GetHeight() * 5 / 4;
            frame_in1->width = file1->GetWidth();
            frame_in1->height = file1->GetHeight();
            frame_in1->format = INPUT_PIX_FMT;

            if (av_buffersrc_add_frame(input_filt_buf, frame_in1) < 0) {
                printf("Error while add frame.\n");
                break;
            }
        }

        int nVideoBytes2 = 0;
        uint8_t *pVideo2 = NULL;
        result = file2->Demux(&pVideo2, &nVideoBytes2);
        // printf("Get video bytes2: %d\n", nVideoBytes2);

        if (nVideoBytes2 && result) {
            frame_in2->data[0] = pVideo2;
            frame_in2->data[1] = pVideo2 + file2->GetWidth() * file2->GetHeight();
            frame_in2->data[2] = pVideo2 + file2->GetWidth() * file2->GetHeight() * 5 / 4;
            frame_in2->width = file2->GetWidth();
            frame_in2->height = file2->GetHeight();
            frame_in2->format = INPUT_PIX_FMT;

            if (av_buffersrc_add_frame(input_filt_buf2, frame_in2) < 0) {
                printf("Error while add frame.\n");
                break;
            }
        }

        // pull frame 
        ret = av_buffersink_get_frame(bufferSink_ctx, frame_out);
        if (ret < 0) {
            printf("Error for av_buffersink_get_frame\n");
            break;
        } else {
            // copy Y
            for (int i = 0; i < frame_out->height; i++) {
                fwrite(frame_out->data[0] + frame_out->linesize[0] * i, 1, frame_out->width, outFile);
            }
            // copy U
            for (int i = 0; i < frame_out->height / 2; i++) {
                fwrite(frame_out->data[1] + frame_out->linesize[1] * i, 1, frame_out->width / 2, outFile);
            }
            // copy V
            for (int i = 0; i < frame_out->height / 2; i++) {
                fwrite(frame_out->data[2] + frame_out->linesize[2] * i, 1, frame_out->width / 2, outFile);
            }
        }
        av_frame_unref(frame_out);

    } while (nVideoBytes);

end:
    avfilter_graph_free(&filter_graph);
    av_frame_free(&frame_in1);
    av_frame_free(&frame_in2);
    av_frame_free(&frame_out);

    delete file1;
    delete file2;

    return 0; 
}
