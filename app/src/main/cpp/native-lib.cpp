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
    // 初始化网络
    avformat_network_init();
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
                              av_get_default_channel_layout(audioCodecContext->channels),
                              audioCodecContext->sample_fmt, audioCodecContext->sample_rate,
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
                av_frame_unref(frame);
            }
        }
    }
    // 关闭文件
    pcmOutFile.close();
    delete[] pcm;
    av_frame_free(&frame);
    // 关闭上下文
    avformat_close_input(&ic);
    avformat_free_context(ic);
    avcodec_close(audioCodecContext);
    avcodec_free_context(&audioCodecContext);
    swr_close(actx);
    swr_free(&actx);
    return env->NewStringUTF(hello.c_str());
}
