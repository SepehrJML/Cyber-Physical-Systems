package com.example.tst2
import android.util.Log
import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Trace

class AirMouseSensorManager(
    context: Context,
    private val onSensorDataUpdated: (accel: FloatArray, gyro: FloatArray, mag: FloatArray) -> Unit,
    private val onClick: () -> Unit
) : SensorEventListener {

    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager

    // استفاده از سنسورهای کاملاً خام طبق الزامات پروژه
    private val accelerometer: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    //private val gyroscopeUncalibrated: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE_UNCALIBRATED)
    private val gyroscopeUncalibrated: Sensor? =
        sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE_UNCALIBRATED)
            ?: sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
    private val magnetometerUncalibrated: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED)
    private val gravitySensor: Sensor? = sensorManager.getDefaultSensor(Sensor.TYPE_GRAVITY)

    //click detection variables
    private var lastClickTime = 0L
    private val clickThreshold = 2.5f
    private val clickCooldownMs = 400L

    var rawAccel = FloatArray(3)
    var rawGyro = FloatArray(3)
    var rawMag = FloatArray(3)
    var rawGravity = FloatArray(3)

    var correctedAccel = FloatArray(3)
    var correctedGyro = FloatArray(3)
    var correctedMag = FloatArray(3)

    // وظیفه شما: جمع‌آوری نمونه‌های ۳ ثانیه‌ای ژیروسکوپ در این لیست (بدون محاسبه میانگین)
    val gyroSamples = mutableListOf<FloatArray>()
    var isCollectingGyroSamples = false
    var isCollectingMagSamples = false
    // وظیفه شما: ساختار ۶ خانه‌ای برای ذخیره وضعیت‌های شتاب‌سنج جهت تحویل به نفر دوم
    val accelPositions = Array(6) { FloatArray(3) }

    // متغیرهای کالیبراسیون مگنتومتر
    var magMin = floatArrayOf(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE)
    var magMax = floatArrayOf(Float.MIN_VALUE, Float.MIN_VALUE, Float.MIN_VALUE)

    var currentSamplingRate = SensorManager.SENSOR_DELAY_GAME
    var gyroBias = floatArrayOf(0f, 0f, 0f)

    var accelOffset = floatArrayOf(0f, 0f, 0f)
    var accelScale  = floatArrayOf(1f, 1f, 1f)
    var magOffset = floatArrayOf(0f, 0f, 0f)
    var magScale  = floatArrayOf(1f, 1f, 1f)

    fun startListening(samplingRate: Int = SensorManager.SENSOR_DELAY_GAME) {
        currentSamplingRate = samplingRate
        accelerometer?.let { sensorManager.registerListener(this, it, currentSamplingRate) }
        gyroscopeUncalibrated?.let { sensorManager.registerListener(this, it, currentSamplingRate) }
        if (gyroscopeUncalibrated == null) {
            Log.e("AirMouse", "No TYPE_GYROSCOPE_UNCALIBRATED on this device")
        } else {
            Log.d("AirMouse", "Registered uncalibrated gyro: ${gyroscopeUncalibrated.name}")
        }
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
                val raw = event.values.clone()
                rawAccel = raw
                Log.d("AirMouse0", "GYRO EVENT: [${rawGyro[0]}, ${rawGyro[1]}, ${rawGyro[2]}]")
                // subtract offset and divide by scale
                correctedAccel = floatArrayOf(
                    (rawAccel[0] - accelOffset[0]) / accelScale[0],
                    (rawAccel[1] - accelOffset[1]) / accelScale[1],
                    (rawAccel[2] - accelOffset[2]) / accelScale[2]
                )
                // correctedAccel for further use


            }
            Sensor.TYPE_GYROSCOPE_UNCALIBRATED -> { // Change to TYPE_GYROSCOPE_UNCALIBRATED for wired connection, TYPE_GYROSCOPE for virtual

                // ۳ خانه اول حاوی دیتای کاملاً خام سیستم هستند
                rawGyro = floatArrayOf(event.values[0], event.values[1], event.values[2])
                Log.d("AirMouse1", "GYRO EVENT: [${rawGyro[0]}, ${rawGyro[1]}, ${rawGyro[2]}]")

                // subtract bias
                correctedGyro = floatArrayOf(
                    rawGyro[0] - gyroBias[0],
                    rawGyro[1] - gyroBias[1],
                    rawGyro[2] - gyroBias[2]
                )
                // correctedGyro for further use

                // اگر دکمه کالیبراسیون زده شده باشد، دیتای خام به مدت ۳ ثانیه در لیست ذخیره می‌شود
                if (isCollectingGyroSamples) {
                    gyroSamples.add(rawGyro.clone())
                }

                val now = System.currentTimeMillis()
                val yRate = rawGyro[1] 

                if (kotlin.math.abs(yRate) > clickThreshold && now - lastClickTime > clickCooldownMs) {
                    lastClickTime = now
                    onClick()
                }

            }
            Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED -> {
                val raw = floatArrayOf(event.values[0], event.values[1], event.values[2])
                rawMag = raw
                if (isCollectingMagSamples)
                    updateMagCalibration(rawMag)

                // apply calibration
                correctedMag = floatArrayOf(
                    (rawMag[0] - magOffset[0]) * magScale[0],
                    (rawMag[1] - magOffset[1]) * magScale[1],
                    (rawMag[2] - magOffset[2]) * magScale[2]
                )

                // if you want UI/fusion to use calibrated values:
                // correctedMag for further use
            }
            Sensor.TYPE_GRAVITY -> {
                rawGravity = event.values.clone()
            }
        }

        // به‌روزرسانی زنده مقادیر در رابط کاربری
        onSensorDataUpdated(correctedAccel, correctedGyro, correctedMag)

        Trace.endSection()
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    // وظیفه شما: ضبط شتاب خام لحظه‌ای در خانه مشخص شده از آرایه ۶ تایی (ایندکس ۰ تا ۵)
    fun recordAccelPosition(index: Int) {
        if (index in 0..5) {
            accelPositions[index] = rawAccel.clone()
        }
    }

    fun computeGyroBiasFromSamples() {
        if (gyroSamples.isEmpty()) return

        var sumX = 0f
        var sumY = 0f
        var sumZ = 0f

        for (sample in gyroSamples) {
            sumX += sample[0]
            sumY += sample[1]
            sumZ += sample[2]
        }

        val n = gyroSamples.size.toFloat()
        gyroBias[0] = sumX / n
        gyroBias[1] = sumY / n
        gyroBias[2] = sumZ / n
    }
    fun computeAccelCalibration(g: Float = 9.81f) {
        if (accelPositions.any { it.contentEquals(FloatArray(3)) }) {
            // Not all positions recorded; you might want checks here
            return
        }

        // X axis: positions 0 (X+) and 1 (X-)
        val xPlus  = accelPositions[0][0]
        val xMinus = accelPositions[1][0]

        // Y axis: positions 2 (Y+) and 3 (Y-)
        val yPlus  = accelPositions[2][1]
        val yMinus = accelPositions[3][1]

        // Z axis: positions 4 (Z+) and 5 (Z-)
        val zPlus  = accelPositions[4][2]
        val zMinus = accelPositions[5][2]

        // Offsets (bias)
        accelOffset[0] = (xPlus + xMinus) / 2f
        accelOffset[1] = (yPlus + yMinus) / 2f
        accelOffset[2] = (zPlus + zMinus) / 2f

        // Scales: measured span / true span (2g)
        val xSpan = (xPlus - xMinus) / 2f
        val ySpan = (yPlus - yMinus) / 2f
        val zSpan = (zPlus - zMinus) / 2f

        accelScale[0] = xSpan / g
        accelScale[1] = ySpan / g
        accelScale[2] = zSpan / g
    }
    private fun updateMagCalibration(currentMag: FloatArray) {
        for (i in 0..2) {
            if (currentMag[i] < magMin[i]) magMin[i] = currentMag[i]
            if (currentMag[i] > magMax[i]) magMax[i] = currentMag[i]
        }
    }

    fun computeMagCalibration() {
        // center (offset) per axis
        magOffset[0] = (magMax[0] + magMin[0]) / 2f
        magOffset[1] = (magMax[1] + magMin[1]) / 2f
        magOffset[2] = (magMax[2] + magMin[2]) / 2f

        // span (half-range) per axis
        val spanX = (magMax[0] - magMin[0]) / 2f
        val spanY = (magMax[1] - magMin[1]) / 2f
        val spanZ = (magMax[2] - magMin[2]) / 2f

        // choose an average span as reference radius
        val avgSpan = (spanX + spanY + spanZ) / 3f

        // scale factors: make each axis have same radius
        magScale[0] = avgSpan / spanX
        magScale[1] = avgSpan / spanY
        magScale[2] = avgSpan / spanZ
    }
}