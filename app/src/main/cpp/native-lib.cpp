#include <jni.h>
#include <string>
#include<fstream>

extern "C" JNIEXPORT jstring JNICALL
Java_com_aspirin_liveproject_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++ ffmpeg 版本是---> ";
    return env->NewStringUTF(hello.c_str());
}
