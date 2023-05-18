#include <jni.h>
#include <string>
#include<fstream>
#include "LogUtils.h"

extern "C" {
#include <libavcodec/jni.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include "libavutil/avutil.h"
}

extern "C"
JNIEXPORT
jint JNI_OnLoad(JavaVM *vm, void *res) {
    av_jni_set_java_vm(vm, 0);
    return JNI_VERSION_1_4;
}

static double r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? 0 : (double) r.num / (double) r.den;
}

//当前时间戳 clock
long long GetNowMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int sec = tv.tv_sec % 360000;
    long long t = sec * 1000 + tv.tv_usec / 1000;
    return t;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_aspirin_liveproject_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++ ffmpeg 版本是---> ";

    const char *av_info = av_version_info();
    hello += avcodec_configuration();

    // 初始化解封装
    av_register_all();
    // 初始化编码器
    avcodec_register_all();
    // 打开文件
    AVFormatContext *ic = NULL;
    char path[] = "/sdcard/Android/dfda1d8f-d436-4c21-ae3f-1462ddf7547d.mp4";
    // char path[] = "/sdcard/video.flv";
    int re = avformat_open_input(&ic, path, 0, 0);
    if (re != 0) {
        LOGW("avformat_open_input failed!:%s", av_err2str(re));
        return env->NewStringUTF(hello.c_str());
    }
    //获取流信息
    re = avformat_find_stream_info(ic, 0);
    if (re != 0) {
        LOGW("avformat_find_stream_info failed!");
    }
    LOGW("duration = %lld nb_streams = %d", ic->duration, ic->nb_streams);
    // 音频索引
    int audioStreamIndex = 0;
    // 获取音频流信息
    audioStreamIndex = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    LOGW("av_find_best_stream audioStreamIndex = %d", audioStreamIndex);
    // 获取audio的AVStream
    AVStream *audioStream = ic->streams[audioStreamIndex];
    //////////////////////////////////////////////////////////
    AVCodec *audioCodec = avcodec_find_decoder(audioStream->codecpar->codec_id);
    if (!audioCodec) {
        LOGW("avcodec_find failed!");
        return env->NewStringUTF(hello.c_str());
    }
    // 音频解码器初始化
    AVCodecContext *audioCodecContext = avcodec_alloc_context3(audioCodec);
    avcodec_parameters_to_context(audioCodecContext, audioStream->codecpar);
    audioCodecContext->thread_count = 3;
    // 打开解码器
    re = avcodec_open2(audioCodecContext, 0, 0);
    if (re != 0) {
        LOGW("avcodec_open2  audio failed!");
        return env->NewStringUTF(hello.c_str());
    }
    // 读取帧数据
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    char *pcm = new char[16000 * 1];
    // 音频重采样上下文初始化
    SwrContext *actx = swr_alloc();
    actx = swr_alloc_set_opts(actx,
                              av_get_default_channel_layout(1), AV_SAMPLE_FMT_S16, 16000,
                              audioCodecContext->channel_layout, audioCodecContext->sample_fmt,
                              audioCodecContext->sample_rate,
                              0, 0);
    re = swr_init(actx);
    if (re != 0) {
        LOGW("swr_init failed!");
    }
    std::fstream pcmOutFile;
    // 以只写模式打开文件
    pcmOutFile.open("/sdcard/Android/16000_1_s16le_012.pcm", std::ios::out);
    // 解码
    for (;;) {
        int re = av_read_frame(ic, pkt);
        if (re != 0) {
            LOGW("读取到结尾处!");
            break;
        }
        // 仅仅处理音频
        if (pkt->stream_index == audioStreamIndex) {
            // 发送到线程中解码
            re = avcodec_send_packet(audioCodecContext, pkt);
            // 清理
            av_packet_unref(pkt);
            if (re != 0) {
                LOGW("avcodec_send_packet failed!");
                continue;
            }
            for (;;) {
                re = avcodec_receive_frame(audioCodecContext, frame);
                if (re != 0) {
                    // LOGW("avcodec_receive_frame failed!");
                    break;
                }
                uint8_t *out[2] = {0};
                out[0] = (uint8_t *) pcm;
                //音频重采样
                int len = swr_convert(actx,
                                      out, 16000,
                                      (const uint8_t **) frame->data, frame->nb_samples);
                // 写入文件
                pcmOutFile.write(pcm, len * 2);
                LOGW("swr_convert = %d", len);
                // 清理
                av_frame_unref(frame);
            }
        }
    }
    // 关闭文件
    pcmOutFile.close();
    delete[] pcm;
    // 释放资源
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avformat_close_input(&ic);
    avformat_free_context(ic);
    avcodec_close(audioCodecContext);
    avcodec_free_context(&audioCodecContext);
    swr_close(actx);
    swr_free(&actx);
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_aspirin_liveproject_MainActivity_pcmToMp3(JNIEnv *env, jobject thiz) {

    AVFormatContext *fmtCtx = avformat_alloc_context();

    AVCodecContext *codecCtx = NULL;
    const AVCodec *codec = NULL;

    AVFrame *frame = av_frame_alloc();
    AVPacket *pkt = av_packet_alloc();

    const char *inFileName = "/sdcard/Android/16000_1_s16le_012.pcm";
    const char *outFileName = "/sdcard/Android/16000_1_s16le_012.mp2";

    int ret = 0;

    // 初始化解封装
    av_register_all();
    // 初始化编码器
    avcodec_register_all();
    // 处理音频
    do {
        // avformat_alloc_output_context2
        if (avformat_alloc_output_context2(&fmtCtx, NULL, NULL, outFileName) < 0) {
            LOGE("Cannot alloc output file context.\n");
            return;
        }
        AVOutputFormat *outFmt = fmtCtx->oformat;
        if (avio_open(&fmtCtx->pb, outFileName, AVIO_FLAG_READ_WRITE) < 0) {
            LOGE("Cannot open output file.\n");
            return;
        }
        AVStream *outStream = avformat_new_stream(fmtCtx, NULL);
        if (!outStream) {
            LOGE("Cannot create a new stream to output file.\n");
            return;
        }
        // 设置参数
        AVCodecParameters *codecPara = fmtCtx->streams[outStream->index]->codecpar;
        codecPara->codec_type = AVMEDIA_TYPE_AUDIO;
        codecPara->codec_id = fmtCtx->oformat->audio_codec;
        codecPara->bit_rate = 128000;
        codecPara->channels = 1;
        codecPara->channel_layout = av_get_default_channel_layout(codecPara->channels);
        codecPara->sample_rate = 16000;
        codecPara->format = AV_SAMPLE_FMT_S16;
        // 查找编码器
        codec = avcodec_find_encoder(codecPara->codec_id);
        if (codec == NULL) {
            LOGE("Cannot find audio encoder.\n");
            return;
        }
        codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx, codecPara);
        if (codecCtx == NULL) {
            LOGE("Cannot alloc codec ctx from para.\n");
            return;
        }
        // 打开编码器
        if (avcodec_open2(codecCtx, NULL, NULL) < 0) {
            LOGE("Cannot open encoder.\n");
            return;
        }

        //===========
        frame->nb_samples = codecCtx->frame_size;
        frame->channels = codecCtx->channels;
        frame->channel_layout = codecCtx->channel_layout;
        frame->sample_rate = codecCtx->sample_rate;
        frame->format = codecCtx->sample_fmt;

        // PCM重采样
        struct SwrContext *swrCtx = swr_alloc_set_opts(NULL,
                                                       codecCtx->channel_layout,
                                                       codecCtx->sample_fmt,
                                                       codecCtx->sample_rate,
                                                       codecCtx->channel_layout,
                                                       codecCtx->sample_fmt,
                                                       codecCtx->sample_rate,
                                                       0, NULL);
        swr_init(swrCtx);

        /* 分配空间 */
        uint8_t **convert_data = (uint8_t **) calloc(codecCtx->channels, sizeof(*convert_data));
        av_samples_alloc(convert_data, NULL,
                         codecCtx->channels, codecCtx->frame_size, codecCtx->sample_fmt,
                         0);
        int size = av_samples_get_buffer_size(NULL,
                                              codecCtx->channels, codecCtx->frame_size,
                                              codecCtx->sample_fmt,
                                              0);
        uint8_t *frameBuf = (uint8_t *) av_malloc(size);
        avcodec_fill_audio_frame(frame, codecCtx->channels, codecCtx->sample_fmt,
                                 (const uint8_t *) frameBuf, size,
                                 0);
        // 写帧头
        ret = avformat_write_header(fmtCtx, NULL);
        // 打开输入文件
        FILE *inFile = fopen(inFileName, "rb");
        if (!inFile) {
            LOGE("Cannot open input file.\n");
            return;
        }
        for (int i = 0;; i++) {
            // 输入一帧数据的长度
            int length = frame->nb_samples * av_get_bytes_per_sample(codecCtx->sample_fmt) *
                         frame->channels;
            // 读PCM：特意注意读取的长度，否则可能出现转码之后声音变快或者变慢
            if (fread(frameBuf, 1, length, inFile) <= 0) {
                LOGE("Cannot read raw data from file.\n");
                return;
            } else if (feof(inFile)) {
                break;
            }
            swr_convert(swrCtx, convert_data, codecCtx->frame_size,
                        (const uint8_t **) frame->data,
                        frame->nb_samples);
            // 输出一帧数据的长度
            length = frame->nb_samples * av_get_bytes_per_sample(codecCtx->sample_fmt);
            // 双通道赋值（输出的AAC为双通道）
            memcpy(frame->data[0], convert_data[0], length);
            frame->pts = i * 100;
            if (avcodec_send_frame(codecCtx, frame) < 0) {
                while (avcodec_receive_packet(codecCtx, pkt) >= 0) {
                    pkt->stream_index = outStream->index;
                    LOGE("write %4d frame, size=%d, length=%d\n", i, size, length);
                    av_write_frame(fmtCtx, pkt);
                }
            }
            av_packet_unref(pkt);
        }
        // write trailer
        av_write_trailer(fmtCtx);
        fclose(inFile);
        av_free(frameBuf);
    } while (0);
    avformat_free_context(fmtCtx);
    avio_close(fmtCtx->pb);
    avcodec_close(codecCtx);
    // 释放资源
    av_packet_free(&pkt);
    av_frame_free(&frame);
}