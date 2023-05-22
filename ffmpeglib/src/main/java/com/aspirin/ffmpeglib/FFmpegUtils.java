package com.aspirin.ffmpeglib;

public class FFmpegUtils {

    // Used to load the 'ffmpeglib' library on application startup.
    static {
        System.loadLibrary("ffmpeglib");
    }

    public static native int mp4ToMov(String srcPath, String destPath);

    /**
     * 解码成 pcm-->16000_1_s16le 格式
     *
     * @param srcPath
     * @param destPath
     * @return
     */
    public static native int decodeTOPcm(String srcPath, String destPath);

}