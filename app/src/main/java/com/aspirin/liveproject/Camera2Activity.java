package com.aspirin.liveproject;

import android.Manifest;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.util.Size;
import android.view.TextureView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import com.aspirin.liveproject.camera.Camera2Helper;
import com.aspirin.liveproject.camera.ImageUtil;

import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.ByteBuffer;

public class Camera2Activity extends AppCompatActivity implements TextureView.SurfaceTextureListener, Camera2Helper.Camera2Listener {

    private static final String TAG = "Camera2Activity";
    private TextureView textureView;

    Camera2Helper camera2Helper;

    private MediaCodec mediaCodec;

    //   先转成nv21   再转成  yuv420    n21 横着   1   竖着
    private byte[] nv21;//width  height
    byte[] nv21_rotated;
    byte[] nv12;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_camera2);
        checkPermissions();
        initView();
    }

    private void initView() {
        textureView = findViewById(R.id.texture_preview);
        textureView.setSurfaceTextureListener(this);
    }

    @Override
    public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surface, int width, int height) {
        // 打开摄像头  先后顺序
        initCamera();
    }

    void initCamera() {
        camera2Helper = new Camera2Helper(this);
        camera2Helper.start(textureView);
    }

    @Override
    public void onPreview(byte[] y, byte[] u, byte[] v, Size previewSize, int stride) {
        if (nv21 == null) {
            nv21 = new byte[stride * previewSize.getHeight() * 3 / 2];
            nv21_rotated = new byte[stride * previewSize.getHeight() * 3 / 2];
        }
        if (mediaCodec == null) {
            initCodec(previewSize);
        }
        byte[] temp = ImageUtil.yuvToNv210(y, u, v, stride, previewSize.getHeight());
//        // 输出成H264的码流
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        int inIndex = mediaCodec.dequeueInputBuffer(0);
        if (inIndex >= 0) {
            ByteBuffer byteBuffer = mediaCodec.getInputBuffer(inIndex);
            byteBuffer.clear();
            byteBuffer.put(temp, 0, temp.length);
            mediaCodec.queueInputBuffer(inIndex, 0, temp.length, 0, 0);
        }
        int outIndex = mediaCodec.dequeueOutputBuffer(info, 1000);
        if (outIndex >= 0) {
            ByteBuffer byteBuffer = mediaCodec.getOutputBuffer(outIndex);
            byte[] ba = new byte[byteBuffer.remaining()];
            byteBuffer.get(ba);
            Log.e("rtmp", "ba = " + ba.length + "");
            writeContent(ba);
            writeBytes(ba);
            mediaCodec.releaseOutputBuffer(outIndex, false);
        }
    }

    public String writeContent(byte[] array) {
        char[] HEX_CHAR_TABLE = {
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
        };
        StringBuilder sb = new StringBuilder();
        for (byte b : array) {
            sb.append(HEX_CHAR_TABLE[(b & 0xf0) >> 4]);
            sb.append(HEX_CHAR_TABLE[b & 0x0f]);
        }
        Log.i(TAG, "writeContent: " + sb.toString());
        FileWriter writer = null;
        try {
            // 打开一个写文件器，构造函数中的第二个参数true表示以追加形式写文件
            writer = new FileWriter(Environment.getExternalStorageDirectory() + "/codec.txt", true);
            writer.write(sb.toString());
            writer.write("\n");
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (writer != null) {
                    writer.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        return sb.toString();
    }

    public void writeBytes(byte[] array) {
        FileOutputStream writer = null;
        try {
            // 打开一个写文件器，构造函数中的第二个参数true表示以追加形式写文件
            writer = new FileOutputStream(Environment.getExternalStorageDirectory() + "/codec.h264", true);
            writer.write(array);
            writer.write('\n');
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (writer != null) {
                    writer.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * 初始化编码器
     *
     * @param size
     */
    private void initCodec(Size size) {
        try {
            // H.264/AVC video
            mediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            final MediaFormat format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, size.getHeight(), size.getWidth());
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, 15);//15*2 =30帧
            format.setInteger(MediaFormat.KEY_BIT_RATE, 4000_000);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 2);//2s一个I帧
            // 配置
            mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            mediaCodec.start();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surface, int width,
                                            int height) {

    }

    @Override
    public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surface) {

    }

    @Override
    public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surface) {
        return false;
    }

    /**
     * 检查权限
     */
    private boolean checkPermissions() {
        boolean allGranted = true;
        allGranted &= ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            // 请求权限
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.CAMERA}, 1);
        }
        return allGranted;
    }
}