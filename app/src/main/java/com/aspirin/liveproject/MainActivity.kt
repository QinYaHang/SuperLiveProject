package com.aspirin.liveproject

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.aspirin.ffmpeglib.FFmpegUtils
import com.aspirin.liveproject.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        val findViewById = binding.root.findViewById<TextView>(R.id.sample_text)
        findViewById.text = "stringFromJNI()";

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            val uri = Uri.parse("package:${BuildConfig.APPLICATION_ID}")
            startActivity(
                Intent(
                    Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, uri
                )
            )
        }
    }

    /**
     * A native method that is implemented by the 'liveproject' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'liveproject' library on application startup.
        init {
            System.loadLibrary("life")
        }
    }

    fun onTest(view: View) {
        var path = Environment.getExternalStorageDirectory().absoluteFile
        FFmpegUtils.rgbPcmToMp4("/sdcard/Android/out2.rgb", "/sdcard/Android/out456123.mp4");
    }
}