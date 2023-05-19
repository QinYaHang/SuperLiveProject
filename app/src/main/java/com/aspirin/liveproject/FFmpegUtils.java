package com.aspirin.liveproject;

/**
 * @Author 三元之
 * @Date 2023-05-19-9:31
 */
public class FFmpegUtils {

    static {
        System.loadLibrary("life");
    }

    /**
     * 解码成 pcm-->16000_1_s16le 格式
     *
     * @param srcPath
     * @param destPath
     * @return
     */
    public static native int decodeTOPcm(String srcPath, String destPath);

}
