package com.example.tst2

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Trace

class AirMouseSensorManager(
    context: Context,
    private val onSensorDataUpdated: (accel: FloatArray, gyro: FloatArray, mag: FloatArray) -> Unit
) : SensorEventListener {

    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager

    // استفاده از سنسورهای کاملاً خام طبق الزامات پروژه
    private val accelerometer: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    private val gyroscopeUncalibrated: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE_UNCALIBRATED)
    private val magnetometerUncalibrated: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED)
    private val gravitySensor: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_GRAVITY)

    var rawAccel = FloatArray(3)
    var rawGyro = FloatArray(3)
    var rawMag = FloatArray(3)
    var rawGravity = FloatArray(3)

    // وظیفه شما: جمع‌آوری نمونه‌های ۳ ثانیه‌ای ژیروسکوپ در این لیست (بدون محاسبه میانگین)
    val gyroSamples = mutableListOf<FloatArray>()
    var isCollectingGyroSamples = false

    // وظیفه شما: ساختار ۶ خانه‌ای برای ذخیره وضعیت‌های شتاب‌سنج جهت تحویل به نفر دوم
    val accelPositions = Array(6) { FloatArray(3) }

    // متغیرهای کالیبراسیون مگنتومتر
    var magMin = floatArrayOf(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE)
    var magMax = floatArrayOf(Float.MIN_VALUE, Float.MIN_VALUE, Float.MIN_VALUE)

    var currentSamplingRate = SensorManager.SENSOR_DELAY_GAME

    fun startListening(samplingRate: Int = SensorManager.SENSOR_DELAY_GAME) {
        currentSamplingRate = samplingRate
        accelerometer?.let { sensorManager.registerListener(this, it, currentSamplingRate) }
        gyroscopeUncalibrated?.let { sensorManager.registerListener(this, it, currentSamplingRate) }
        magnetometerUncalibrated?.let { sensorManager.registerListener(this, it, currentSamplingRate) }
        gravitySensor?.let { sensorManager.registerListener(this, it, currentSamplingRate) }
    }

    fun stopListening() {
        sensorManager.unregisterListener(this)
    }

    override fun onSensorChanged(event: SensorEvent?) {
        event ?: return

        // تعریف رویدادها در سطح کد برای ابزار Perfetto
        Trace.beginSection("AirMouse_Sensor_Read_${event.sensor.name}")

        when (event.sensor.type) {
            Sensor.TYPE_ACCELEROMETER -> {
                rawAccel = event.values.clone()
            }
            Sensor.TYPE_GYROSCOPE_UNCALIBRATED -> {
                // ۳ خانه اول حاوی دیتای کاملاً خام سیستم هستند
                val currentGyro = floatArrayOf(event.values[0], event.values[1], event.values[2])
                rawGyro = currentGyro

                // اگر دکمه کالیبراسیون زده شده باشد، دیتای خام به مدت ۳ ثانیه در لیست ذخیره می‌شود
                if (isCollectingGyroSamples) {
                    gyroSamples.add(currentGyro.clone())
                }
            }
            Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED -> {
                rawMag = floatArrayOf(event.values[0], event.values[1], event.values[2])
                updateMagCalibration(rawMag)
            }
            Sensor.TYPE_GRAVITY -> {
                rawGravity = event.values.clone()
            }
        }

        // به‌روزرسانی زنده مقادیر در رابط کاربری
        onSensorDataUpdated(rawAccel, rawGyro, rawMag)

        Trace.endSection()
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    // وظیفه شما: ضبط شتاب خام لحظه‌ای در خانه مشخص شده از آرایه ۶ تایی (ایندکس ۰ تا ۵)
    fun recordAccelPosition(index: Int) {
        if (index in 0..5) {
            accelPositions[index] = rawAccel.clone()
        }
    }

    private fun updateMagCalibration(currentMag: FloatArray) {
        for (i in 0..2) {
            if (currentMag[i] < magMin[i]) magMin[i] = currentMag[i]
            if (currentMag[i] > magMax[i]) magMax[i] = currentMag[i]
        }
    }
}