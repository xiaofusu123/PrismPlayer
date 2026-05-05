#include <iostream>
#include <libavutil/log.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

int main(int argc, char **argv) {
    // 处理命令行参数
    char* src = nullptr;
    char* dst = nullptr;

    av_log_set_level(AV_LOG_DEBUG);
    if (argc < 3) {
        av_log(nullptr, AV_LOG_INFO, "arguments must be more than 3!\n");
        exit(-1);
    }

    src = argv[1];
    dst = argv[2];

    // 打开多媒体文件
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, src, nullptr, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        exit(-1);
    }

    // 从多媒体文件中找到音频流
    int index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (index < 0) {
        av_log(fmt_ctx, AV_LOG_ERROR, "Could not find audio stream in file\n");

        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
        exit(-1);
    }

    // 打开目的文件的上下文
    AVFormatContext* out_fmt_ctx = avformat_alloc_context();
    if (!out_fmt_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "Could not allocate output format context\n");

        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
        exit(-1);
    }

    const AVOutputFormat* out_format = av_guess_format(nullptr, dst, nullptr);
    out_fmt_ctx->oformat = out_format;

    // 创建一个新的音频流
    AVStream* out_stream = avformat_new_stream(out_fmt_ctx, nullptr);

    // 设置输出音频参数
    AVStream* in_stream = fmt_ctx->streams[index];
    avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    out_stream->codecpar->codec_tag = 0;

    // 绑定输出文件的上下文和目的文件
    ret = avio_open2(&out_fmt_ctx->pb, dst, AVIO_FLAG_WRITE, nullptr, nullptr);
    if (ret < 0) {
        av_log(out_fmt_ctx, AV_LOG_ERROR, "%s\n", av_err2str(ret));

        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;

        avformat_free_context(out_fmt_ctx);
        out_fmt_ctx = nullptr;

        exit(-1);
    }

    // 写多媒体文件头部到目的文件
    ret = avformat_write_header(out_fmt_ctx, nullptr);
    if (ret < 0) {
        av_log(out_fmt_ctx, AV_LOG_ERROR, "%s\n", av_err2str(ret));

        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;

        avformat_free_context(out_fmt_ctx);
        out_fmt_ctx = nullptr;

        exit(-1);
    }

    // 从源多媒体文件读取音频数据到目的多媒体文件
    AVPacket pkt;
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == index) {
            pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt.dts = pkt.pts;
            pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
            pkt.stream_index = 0;
            pkt.pos = -1;

            ret = av_interleaved_write_frame(out_fmt_ctx, &pkt);
            av_packet_unref(&pkt);
        }
    }
    
    // 写多媒体文件尾部到目的文件
    av_interleaved_write_frame(out_fmt_ctx, nullptr);

    // 释放资源
    avformat_close_input(&fmt_ctx);
    fmt_ctx = nullptr;

    if (out_fmt_ctx->pb) {
        avio_closep(&out_fmt_ctx->pb);
    }

    avformat_free_context(out_fmt_ctx);
    out_fmt_ctx = nullptr;

    return 0;
}
