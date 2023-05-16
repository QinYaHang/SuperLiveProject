package com.aspirin.liveproject.camera;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.TextureView;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

/**
 * @Author 三元之
 * @Date 2023-04-24-16:43
 */
public class Camera2Helper {

    private Context context;
    private Size mPreviewSize;
    private Point previewViewSize;
    private ImageReader mImageReader;
    private Handler mBackgroundHandler;
    private HandlerThread mBackgroundThread;
    private CameraDevice mCameraDevice;
    private TextureView mTextureView;
    CaptureRequest.Builder mPreviewRequestBuilder;
    private CameraCaptureSession mCaptureSession;
    private Camera2Listener camera2Listener;

    /**
     * 构造
     *
     * @param context
     */
    public Camera2Helper(Context context) {
        this.context = context;
        this.camera2Listener = (Camera2Listener) context;
        // 子线程
        mBackgroundThread = new HandlerThread("CameraBackground");
        mBackgroundThread.start();
        mBackgroundHandler = new Handler(mBackgroundThread.getLooper());
    }

    /**
     * 开启摄像头
     *
     * @param textureView
     */
    public synchronized void start(TextureView textureView) {
        this.mTextureView = textureView;
        // 摄像头的管理类
        CameraManager cameraManager =
                (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        try {
            String[] cameraIdList = cameraManager.getCameraIdList();
            Log.d("Camera2Helper-->cameraIdList", Arrays.toString(cameraIdList));
            // 这个摄像头的配置信息
            CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics("0");
            //  以及获取图片输出的尺寸和预览画面输出的尺寸
            // 支持哪些格式               获取到的  摄像预览尺寸    textView
            StreamConfigurationMap map =
                    characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            //寻找一个 最合适的尺寸     ---》 一模一样
            mPreviewSize =
                    getBestSupportedSize(Arrays.asList(map.getOutputSizes(SurfaceTexture.class)));
            //nv21      420   保存到文件
            mImageReader = ImageReader.newInstance(mPreviewSize.getWidth(),
                    mPreviewSize.getHeight(), ImageFormat.YUV_420_888, 2);
            // setOnImageAvailableListener
            mImageReader.setOnImageAvailableListener(new OnImageAvailableListenerImpl(),
                    mBackgroundHandler);
            if (ActivityCompat.checkSelfPermission(context, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
                return;
            }
            // 打开相机
            cameraManager.openCamera("0", mDeviceStateCallback, mBackgroundHandler);
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

    private void createCameraPreviewSession() {
        try {
            SurfaceTexture texture = mTextureView.getSurfaceTexture();
            // 设置预览宽高
            texture.setDefaultBufferSize(mPreviewSize.getWidth(), mPreviewSize.getHeight());
            //            创建有一个Surface   画面  ---》1
            Surface surface = new Surface(texture);
            //            预览 还不够
            mPreviewRequestBuilder =
                    mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            //
            mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE,
                    CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
            // 预览的TexetureView
            mPreviewRequestBuilder.addTarget(surface);
            // 想要获取每一帧的数据，需要给ImageReader设置Target
            mPreviewRequestBuilder.addTarget(mImageReader.getSurface());
            // 建立连接，开启会话
            mCameraDevice.createCaptureSession(Arrays.asList(surface, mImageReader.getSurface()),
                    mCaptureStateCallback, mBackgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private CameraCaptureSession.StateCallback mCaptureStateCallback =
            new CameraCaptureSession.StateCallback() {

        @Override
        public void onConfigured(@NonNull CameraCaptureSession session) {
            //            系统的相机
            // The camera is already closed
            if (null == mCameraDevice) {
                return;
            }
            mCaptureSession = session;
            try {
                // 发起重复请求,将实时的图像帧发送到手机页面上进行渲染
                mCaptureSession.setRepeatingRequest(mPreviewRequestBuilder.build(),
                        new CameraCaptureSession.CaptureCallback() {
                }, mBackgroundHandler);
            } catch (CameraAccessException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void onConfigureFailed(@NonNull CameraCaptureSession session) {

        }
    };

    /**
     * 设备状态
     */
    private CameraDevice.StateCallback mDeviceStateCallback = new CameraDevice.StateCallback() {

        @Override
        public void onOpened(@NonNull CameraDevice cameraDevice) {
            // 成功     前提       绝对
            mCameraDevice = cameraDevice;
            // 建立绘画
            createCameraPreviewSession();
        }

        @Override
        public void onDisconnected(@NonNull CameraDevice cameraDevice) {
            cameraDevice.close();
            mCameraDevice = null;
        }

        @Override
        public void onError(@NonNull CameraDevice cameraDevice, int error) {
            cameraDevice.close();
            mCameraDevice = null;
            Log.d("Camera2Helper-->onError", error + "-----------------");
        }
    };

    /**
     * OnImageAvailableListenerImpl
     */
    private class OnImageAvailableListenerImpl implements ImageReader.OnImageAvailableListener {

        private byte[] y;
        private byte[] u;
        private byte[] v;

        @Override
        public void onImageAvailable(ImageReader reader) {
            // 不是设置回调了
            Image image = reader.acquireLatestImage();
            // 判空
            if (image == null) {
                return;
            }
            try {
                // yuv  H264
                Image.Plane[] planes = image.getPlanes();
                // 重复使用同一批byte数组，减少gc频率
                if (y == null) {
                    // limit  是 缓冲区 所有的大小     position 起始大小
                    y = new byte[planes[0].getBuffer()
                            .limit() - planes[0].getBuffer()
                            .position()];
                    u = new byte[planes[1].getBuffer()
                            .limit() - planes[1].getBuffer()
                            .position()];
                    v = new byte[planes[2].getBuffer()
                            .limit() - planes[2].getBuffer()
                            .position()];
                }
                if (image.getPlanes()[0].getBuffer()
                        .remaining() == y.length) {
                    planes[0].getBuffer().get(y);
                    planes[1].getBuffer().get(u);
                    planes[2].getBuffer().get(v);
                }
                // 回调给应用层数据
                if (camera2Listener != null) {
                    Log.d("Camera2Helper-->getRowStride", planes[0].getRowStride() + "-----------------" + mPreviewSize.getWidth() + "-----------------" + mPreviewSize.getHeight());
                    Log.d("Camera2Helper-->getRowStride",planes[0].getPixelStride() +"-----");
                    camera2Listener.onPreview(y, u, v, mPreviewSize, planes[0].getRowStride());
                }
            } finally {
                //良性循环
                image.close();
            }
        }
    }

    /**
     * getBestSupportedSize
     *
     * @param sizes
     * @return
     */
    private Size getBestSupportedSize(List<Size> sizes) {
        Point maxPreviewSize = new Point(1920, 1080);
        Point minPreviewSize = new Point(1280, 720);
        Size defaultSize = sizes.get(0);
        Size[] tempSizes = sizes.toArray(new Size[0]);
        Arrays.sort(tempSizes, new Comparator<Size>() {
            @Override
            public int compare(Size o1, Size o2) {
                if (o1.getWidth() > o2.getWidth()) {
                    return -1;
                } else if (o1.getWidth() == o2.getWidth()) {
                    return o1.getHeight() > o2.getHeight() ? -1 : 1;
                } else {
                    return 1;
                }
            }
        });
        sizes = new ArrayList<>(Arrays.asList(tempSizes));
        Iterator<Size> iterator = sizes.iterator();
        while (iterator.hasNext()) {
            Size next = iterator.next();
            if (next.getWidth() > maxPreviewSize.x || next.getHeight() > maxPreviewSize.y) {
                iterator.remove();
                continue;
            }
            if (next.getWidth() < minPreviewSize.x || next.getHeight() < minPreviewSize.y) {
                iterator.remove();
            }
        }
        if (sizes.size() == 0) {
            return defaultSize;
        }
        Size bestSize = sizes.get(0);
        float previewViewRatio;
        if (previewViewSize != null) {
            previewViewRatio = (float) previewViewSize.x / (float) previewViewSize.y;
        } else {
            previewViewRatio = (float) bestSize.getWidth() / (float) bestSize.getHeight();
        }
        if (previewViewRatio > 1) {
            previewViewRatio = 1 / previewViewRatio;
        }
        for (Size s : sizes) {
            if (Math.abs((s.getHeight() / (float) s.getWidth()) - previewViewRatio) < Math.abs(bestSize.getHeight() / (float) bestSize.getWidth() - previewViewRatio)) {
                bestSize = s;
            }
        }
        return bestSize;
    }

    /**
     * 相机数据回调
     */
    public interface Camera2Listener {
        /**
         * 预览数据回调
         *
         * @param y           预览数据，Y分量
         * @param u           预览数据，U分量
         * @param v           预览数据，V分量
         * @param previewSize 预览尺寸
         * @param stride      步长
         */
        void onPreview(byte[] y, byte[] u, byte[] v, Size previewSize, int stride);
    }

}
