#include <jni.h>
#include <string>

extern "C" {
#include "libavutil/avutil.h"
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_aspirin_liveproject_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++ 123";

    const char *av_info = av_version_info();

    return env->NewStringUTF(av_info);
}