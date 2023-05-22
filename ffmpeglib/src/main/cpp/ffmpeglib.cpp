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

extern "C"
JNIEXPORT jint JNICALL
Java_com_aspirin_ffmpeglib_FFmpegUtils_mp4ToMov(JNIEnv *env, jclass clazz, jstring src_path,
                                                jstring dest_path) {
    // 获取源文件路径
    const char *file_src_path = env->GetStringUTFChars(src_path, NULL);
    const char *file_dest_path = env->GetStringUTFChars(dest_path, NULL);
    LOGD("file_src_path is %s , file_dest_path is %s  ", file_src_path, file_dest_path);

    // 初始化解封装和编码器
    av_register_all();
    avcodec_register_all();

    // 1 open input file
    AVFormatContext *ic = NULL;
    avformat_open_input(&ic, file_src_path, 0, 0);
    if (!ic) {
        LOGE("avformat_open_input failed!");
        return -1;
    }

    // 2 create output context
    AVFormatContext *oc = NULL;
    avformat_alloc_output_context2(&oc, NULL, NULL, file_dest_path);
    if (!oc) {
        LOGE("avformat_alloc_output_context2 failed!");
        return -1;
    }

    // 3 add the stream
    AVStream *videoStream = avformat_new_stream(oc, NULL);
    AVStream *audioStream = avformat_new_stream(oc, NULL);

    // 4 copy para
    avcodec_parameters_copy(videoStream->codecpar, ic->streams[0]->codecpar);
    avcodec_parameters_copy(audioStream->codecpar, ic->streams[1]->codecpar);
    videoStream->time_base = ic->streams[0]->time_base;
    audioStream->time_base = ic->streams[1]->time_base;
    videoStream->codecpar->codec_tag = 0;
    audioStream->codecpar->codec_tag = 0;

    LOGD("==================================================================================");

    // 5 open out file io,write head
    int ret = avio_open(&oc->pb, file_dest_path, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("avio open failed! : %s", av_err2str(ret));
        return -1;
    }
    ret = avformat_write_header(oc, NULL);
    if (ret < 0) {
        LOGE("avformat_write_header failed! : %s", av_err2str(ret));
        return -1;
    }
    AVPacket pkt;
    for (;;) {
        int re = av_read_frame(ic, &pkt);
        if (re < 0) {
            break;
        }
//        pkt.pts = av_rescale_q_rnd(pkt.pts,
//                                   ic->streams[pkt.stream_index]->time_base,
//                                   oc->streams[pkt.stream_index]->time_base,
//                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
//        );
//        pkt.dts = av_rescale_q_rnd(pkt.dts,
//                                   ic->streams[pkt.stream_index]->time_base,
//                                   oc->streams[pkt.stream_index]->time_base,
//                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
//        );
//        pkt.pos = -1;
//        pkt.duration = av_rescale_q_rnd(pkt.duration,
//                                        ic->streams[pkt.stream_index]->time_base,
//                                        oc->streams[pkt.stream_index]->time_base,
//                                        (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
//        );
        av_write_frame(oc, &pkt);
        av_packet_unref(&pkt);
    }
    av_write_trailer(oc);
    avio_close(oc->pb);
    return JNI_OK;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_aspirin_ffmpeglib_FFmpegUtils_decodeTOPcm(JNIEnv *env, jclass clazz, jstring src_path,
                                                   jstring dest_path) {
    // 获取源文件路径
    const char *file_src_path = env->GetStringUTFChars(src_path, NULL);
    const char *file_dest_path = env->GetStringUTFChars(dest_path, NULL);
    LOGD("file_src_path is %s , file_dest_path is %s  ", file_src_path, file_dest_path);

    // 初始化解封装和编码器
    av_register_all();
    avcodec_register_all();

    // 上下文Context
    AVFormatContext *pcmFormatContext = NULL;
    AVCodecContext *audioCodecContext = NULL;

    // 音频重采样上下文初始化
    SwrContext *swrContext = NULL;

    // 输出文件流
    std::fstream pcmOutFile;

    // 读取帧数据
    AVPacket *pcmPkt = NULL;
    AVFrame *pcmFrame = NULL;
    char *pcmData = NULL;

    // 输出音频相关格式配置
    const int outChannels = 1;
    const int outSampleRate = 16000;
    const AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_S16;

    // 流程处理
    int re = JNI_OK;
    do {
        // 打开文件
        re = avformat_open_input(&pcmFormatContext, file_src_path, 0, 0);
        if (re != 0) {
            LOGE("avformat_open_input failed! : %s", av_err2str(re));
            break;
        }
        // 获取音频流信息
        re = avformat_find_stream_info(pcmFormatContext, 0);
        if (re < 0) {
            LOGE("avformat_find_stream_info failed! : %s", av_err2str(re));
            break;
        }
        // 获取音频流音频索引
        int audioStreamIndex = av_find_best_stream(pcmFormatContext, AVMEDIA_TYPE_AUDIO,
                                                   -1, -1, NULL, 0);
        LOGD("av_find_best_stream audioStreamIndex = %d", audioStreamIndex);
        // 获取audio的AVStream
        AVStream *audioStream = pcmFormatContext->streams[audioStreamIndex];
        //////////////////////////////////////////////////////////
        AVCodec *audioCodec = avcodec_find_decoder(audioStream->codecpar->codec_id);
        if (!audioCodec) {
            LOGE("avcodec_find failed!");
            break;
        }
        // 音频解码器初始化
        audioCodecContext = avcodec_alloc_context3(audioCodec);
        avcodec_parameters_to_context(audioCodecContext, audioStream->codecpar);
        audioCodecContext->thread_count = 3;
        // 打开解码器
        re = avcodec_open2(audioCodecContext, audioCodec, 0);
        if (re != 0) {
            LOGE("avcodec_open2 audio failed! : %s", av_err2str(re));
            break;
        }
        // 判断是否需要重采样
        bool isNeedAudioSwr = outChannels != audioCodecContext->channels ||
                              outSampleRate != audioCodecContext->sample_rate ||
                              outSampleFmt != audioCodecContext->sample_fmt;
        if (isNeedAudioSwr) {
            // 音频重采样上下文初始化
            swrContext = swr_alloc();
            swrContext = swr_alloc_set_opts(swrContext,
                                            av_get_default_channel_layout(outChannels),
                                            outSampleFmt, outSampleRate,
                                            av_get_default_channel_layout(
                                                    audioCodecContext->channels),
                                            audioCodecContext->sample_fmt,
                                            audioCodecContext->sample_rate,
                                            0, 0);
            re = swr_init(swrContext);
            if (re != 0) {
                LOGE("swr_init failed! : %s", av_err2str(re));
                break;
            }
        }
        // 以只写模式打开文件
        pcmOutFile.open(file_dest_path, std::ios::out);
        // 读取帧数据
        pcmPkt = av_packet_alloc();
        pcmFrame = av_frame_alloc();
        pcmData = new char[16000 * 1];
        for (;;) {
            int re = av_read_frame(pcmFormatContext, pcmPkt);
            if (re != 0) {
                LOGD("读取到结尾处!");
                break;
            }
            // 仅仅处理音频
            if (pcmPkt->stream_index == audioStreamIndex) {
                // 发送到线程中解码
                re = avcodec_send_packet(audioCodecContext, pcmPkt);
                if (re != 0) {
                    LOGE("avcodec_send_packet failed! : %s", av_err2str(re));
                    continue;
                }
                for (;;) {
                    re = avcodec_receive_frame(audioCodecContext, pcmFrame);
                    if (re != 0) {
                        break;
                    }
                    // 判断是否需要重采样
                    if (isNeedAudioSwr) {
                        uint8_t *out[2] = {0};
                        out[0] = (uint8_t *) pcmData;
                        // 音频重采样
                        int len = swr_convert(swrContext,
                                              out, outSampleRate,
                                              (const uint8_t **) pcmFrame->data,
                                              pcmFrame->nb_samples);
                        if (len < 0) {
                            LOGE("swr_convert failed! :%s ", av_err2str(len));
                            continue;
                        }
                        // 写入文件
                        pcmOutFile.write(pcmData, len * av_get_bytes_per_sample(outSampleFmt));
                    } else {
                        // 输出一帧数据的长度
                        int length = pcmFrame->nb_samples * av_get_bytes_per_sample(outSampleFmt);
                        // 写入文件
                        pcmOutFile.write((char *) pcmFrame->data[0], length);
                    }
                    // 清理
                    av_frame_unref(pcmFrame);
                }
            }
            // 清理pkt
            av_packet_unref(pcmPkt);
        }
    } while (0);
    // 释放资源
    if (pcmFormatContext) {
        avformat_close_input(&pcmFormatContext);
        avformat_free_context(pcmFormatContext);
    }
    if (audioCodecContext) {
        avcodec_close(audioCodecContext);
        avcodec_free_context(&audioCodecContext);
    }
    if (swrContext) {
        swr_close(swrContext);
        swr_free(&swrContext);
    }
    // 释放帧数据
    if (pcmData) {
        delete[] pcmData;
    }
    if (pcmPkt) {
        av_packet_free(&pcmPkt);
    }
    if (pcmFrame) {
        av_frame_free(&pcmFrame);
    }
    // 关闭输出文件
    if (pcmOutFile && pcmOutFile.is_open()) {
        pcmOutFile.close();
    }
    // 释放
    env->ReleaseStringUTFChars(src_path, file_src_path);
    env->ReleaseStringUTFChars(dest_path, file_dest_path);
    // 返回结果
    return re;
}
