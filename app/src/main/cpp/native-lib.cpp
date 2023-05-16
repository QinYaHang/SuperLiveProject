#include <jni.h>
#include <string>

extern "C" {
#include "libavutil/avutil.h"
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_aspirin_liveproject_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++ ffmpeg 版本是---> ";

    const char *av_info = av_version_info();
    hello.append(av_info);

    return env->NewStringUTF(hello.c_str());
}