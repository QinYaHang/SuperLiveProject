package com.aspirin.liveproject

import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
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
        FFmpegUtils.mp4ToMov("/sdcard/Android/570076217-1-16.mp4","/sdcard/Android/test.mov");
    }
}