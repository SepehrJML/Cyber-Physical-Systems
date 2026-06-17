package com.example.tst2

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.example.tst2.network.UdpClient
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            AirMouseApp()
        }
    }
}

@Composable
fun AirMouseApp() {
    var ipAddress by remember { mutableStateOf("") }
    val scope = rememberCoroutineScope()
    var udpClient by remember {
        mutableStateOf<UdpClient?>(null)
    }
    var isRunning by remember { mutableStateOf(false) }
    var showCalibrationDialog by remember { mutableStateOf(false) }

    var accelData by remember { mutableStateOf(floatArrayOf(0f, 0f, 0f)) }
    var gyroData by remember { mutableStateOf(floatArrayOf(0f, 0f, 0f)) }
    var magData by remember { mutableStateOf(floatArrayOf(0f, 0f, 0f)) }

    var clickStatus by remember { mutableStateOf("NO CLICK") }
    var scrollStatus by remember { mutableStateOf("NO SCROLL") }
    var yaw by remember { mutableStateOf(0f) }
    var pitch by remember { mutableStateOf(0f) }
    var roll by remember { mutableStateOf(0f) }
    var yawPrev by remember { mutableStateOf(0f) }
    var pitchPrev by remember { mutableStateOf(0f) }
    var rollPrev by remember { mutableStateOf(0f) }

    val context = androidx.compose.ui.platform.LocalContext.current

    val sensorManager = remember {
        AirMouseSensorManager(
            context = context,
            onSensorDataUpdated = { accel, gyro, mag, yawIn, pitchIn,rollIn ->
                accelData = accel
                gyroData = gyro
                magData = mag
                yaw = yawIn
                pitch = pitchIn
                roll = rollIn
            },
            onClick = {
                clickStatus = "CLICKED!"
                scope.launch {
                    delay(200)
                    clickStatus = "NO CLICK"
                }
            }
        )
    }



    LaunchedEffect(isRunning) {
        if (isRunning) {
            sensorManager.startListening()
        } else {
            sensorManager.stopListening()
        }

    }
    // Add these states near yaw/pitch/roll
    var xOffset by remember { mutableStateOf(0f) }
    var yOffset by remember { mutableStateOf(0f) }

    // Whenever yaw/roll change, update clamped offsets:
    LaunchedEffect(yaw, roll, showCalibrationDialog) {
        if (showCalibrationDialog) {
            // Freeze in center during calibration
            xOffset = 0f
            yOffset = 0f
        } else {
            val gain = 50f
            val maxOffset = 110f

            xOffset = (-yaw * gain).coerceIn(-maxOffset, maxOffset)
            yOffset = (-roll * gain).coerceIn(-maxOffset, maxOffset)
        }
    }
    LaunchedEffect(yaw, roll, isRunning) {
        if (!isRunning) return@LaunchedEffect
        val gain = 70f
        val client = udpClient
        if (client != null) {
            scope.launch {
                try {
                    client.sendMove(-(yaw-yawPrev)*gain, -(roll-rollPrev)*gain)
                    yawPrev = yaw
                    rollPrev = roll
                } catch (e: Exception) {
                    // temporary debug log
                    android.util.Log.e("AirMouse", "sendMove failed", e)
                }
            }
        } else {
            android.util.Log.w("AirMouse", "udpClient is null, not sending")
        }
    }
    /*
    LaunchedEffect(yawRateKState,pitchRateKState) {
        val gain = 1f
        x += -yawRateKState * gain
        y += -pitchRateKState * gain

        x = x.coerceIn(-110f, 110f)
        y = y.coerceIn(-110f, 110f)
    }
    */


    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        Text(text = "AIR MOUSE", fontSize = 24.sp, fontWeight = FontWeight.Bold)

        // فیلد ورود IP لپ‌تاپ با بک‌گراند اختصاصی سفید
        OutlinedTextField(
            value = ipAddress,
            onValueChange = { ipAddress = it },
            label = { Text("Laptop IP Address") },
            modifier = Modifier.fillMaxWidth(),
            colors = OutlinedTextFieldDefaults.colors(
                focusedContainerColor = Color.White,
                unfocusedContainerColor = Color.White,
                disabledContainerColor = Color.White
            )
        )

        // محوطه نمایش بازخورد بصری حرکت
        Box(
            modifier = Modifier
                .size(250.dp)
                .background(Color.DarkGray),
            contentAlignment = Alignment.Center
        ) {
            Box(
                modifier = Modifier
                    .offset(x = xOffset.dp, y = yOffset.dp)
                    .size(30.dp)
                    .background(if (clickStatus == "CLICKED!") Color.Red else Color.Green)
            )
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            Text(text = clickStatus, color = if(clickStatus == "CLICKED!") Color.Red else Color.Black)
            Text(text = scrollStatus)
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            Button(
                onClick = {
                        udpClient = UdpClient(ipAddress)
                    yawPrev = yaw
                    rollPrev = roll
                    isRunning = !isRunning },
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (isRunning) Color.Red else Color.Green
                )
            ) {
                Text(if (isRunning) "STOP MOUSE" else "START MOUSE")
            }
            /*
            Button( // test button for move
                onClick = {

                    scope.launch {

                        udpClient?.sendMove(
                            50f,
                            0f
                        )
                    }
                }
            ) {
                Text("TEST MOVE")
            }

            Button( // test button for click
                onClick = {

                    scope.launch {

                        udpClient?.sendClick()
                    }
                }
            ) {
                Text("TEST CLICK")
            } */

            Button(onClick = {
                // سنسورها برای کالیبراسیون باید فعال باشند تا دیتای لحظه‌ای دریافت شود
                if (!isRunning) {
                    sensorManager.startListening()
                }
                sensorManager.setReferenceOrientation()
                showCalibrationDialog = true
            }) {
                Text("CALIBRATE")
            }
        }

        Surface(color = Color.LightGray, modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(8.dp)) {
                Text(text = "Gyro: [%.2f, %.2f, %.2f]".format(gyroData[0], gyroData[1], gyroData[2]))
                Text(text = "Accel: [%.2f, %.2f, %.2f]".format(accelData[0], accelData[1], accelData[2]))
                Text(text = "Mag: [%.2f, %.2f, %.2f]".format(magData[0], magData[1], magData[2]))
            }
        }
    }

    if (showCalibrationDialog) {
        CalibrationScreen(
            sensorManager = sensorManager,
            onDismiss = {
                showCalibrationDialog = false
                if (!isRunning) {
                    sensorManager.stopListening()
                }
            }
        )
    }
}

@Composable
fun CalibrationScreen(sensorManager: AirMouseSensorManager, onDismiss: () -> Unit) {
    val scope = rememberCoroutineScope()
    var gyroStatusText by remember { mutableStateOf("Ready to collect.") }
    var isGyroCalibrating by remember { mutableStateOf(false) }

    // لیست استیت در Compose برای نمایش زنده وضعیت مقادیر ذخیره شده شتاب‌سنج در UI
    val registeredAccelPositions = remember { mutableStateListOf(*Array(6) { "Not recorded" }) }

    Dialog(onDismissRequest = onDismiss) {
        Surface(
            shape = MaterialTheme.shapes.medium,
            color = MaterialTheme.colorScheme.background,
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
                .heightIn(max = 550.dp) // محدود کردن ارتفاع جهت ریسپانسیو بودن در تمام نمایشگرها
        ) {
            Column(
                modifier = Modifier
                    .padding(16.dp)
                    .verticalScroll(rememberScrollState()), // قابلیت اسکرول برای جلوگیری از به هم ریختن قالب
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text("Calibration Data Gathering", fontWeight = FontWeight.Bold, fontSize = 18.sp)
                Spacer(modifier = Modifier.height(16.dp))

                // --- ۱. پیاده‌سازی وظیفه شما در بخش ژیروسکوپ (جمع‌آوری ۳ ثانیه‌ای) ---
                Card(
                    modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
                ) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        Text("1. Gyroscope Data Gathering", fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary)
                        Text("Keep the phone completely still on a flat surface.", fontSize = 12.sp)
                        Spacer(modifier = Modifier.height(8.dp))

                        Button(
                            onClick = {
                                scope.launch {
                                    isGyroCalibrating = true
                                    sensorManager.gyroSamples.clear() // پاک کردن دیتای قبلی
                                    sensorManager.isCollectingGyroSamples = true

                                    // پیاده‌سازی زمان‌بندی دقیق ۳ ثانیه‌ای با نمایش معکوس به کاربر
                                    for (i in 3 downTo 1) {
                                        gyroStatusText = "Collecting raw data... $i s left."
                                        delay(1000)
                                    }

                                    sensorManager.isCollectingGyroSamples = false
                                    isGyroCalibrating = false
                                    sensorManager.computeGyroBiasFromSamples()
                                    gyroStatusText = "Done! Saved ${sensorManager.gyroSamples.size} raw samples for Member 2."
                                }
                            },
                            enabled = !isGyroCalibrating,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(if (isGyroCalibrating) "Recording..." else "Calibrate Gyro (Collect 3s)")
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(text = gyroStatusText, fontSize = 12.sp, fontWeight = FontWeight.Medium)
                    }
                }

                Spacer(modifier = Modifier.height(12.dp))

                // --- ۲. پیاده‌سازی وظیفه شما در بخش شتاب‌سنج (آرایه ۶ تایی وضعیت فیزیکی) ---
                Card(
                    modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
                ) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        Text("2. Accelerometer 6-Pos Gathering", fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary)
                        Text("Place phone flat in 6 physical orientations (X+, X-, Y+, Y-, Z+, Z-). Click Record for each state.", fontSize = 11.sp)
                        Spacer(modifier = Modifier.height(8.dp))

                        val orientations = listOf(
                            "Pos 1: X+ axis up", "Pos 2: X- axis up",
                            "Pos 3: Y+ axis up", "Pos 4: Y- axis up",
                            "Pos 5: Z+ axis up", "Pos 6: Z- axis up"
                        )

                        orientations.forEachIndexed { index, label ->
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(label, fontSize = 12.sp, modifier = Modifier.weight(1f))
                                Button(
                                    onClick = {
                                        sensorManager.recordAccelPosition(index) // ذخیره در خانه مربوطه از آرایه
                                        val recorded = sensorManager.accelPositions[index]
                                        registeredAccelPositions[index] = "[%.2f, %.2f, %.2f]".format(recorded[0], recorded[1], recorded[2])
                                    },
                                    modifier = Modifier.padding(start = 4.dp)
                                ) {
                                    Text("Record", fontSize = 11.sp)
                                }
                            }
                            Text(
                                text = "Saved: ${registeredAccelPositions[index]}",
                                fontSize = 11.sp,
                                color = if (registeredAccelPositions[index] == "Not recorded") Color.Gray else Color.DarkGray,
                                modifier = Modifier.padding(bottom = 6.dp)
                            )
                        }
                    }
                    Button(
                        onClick = {
                            // maybe check that all 6 positions are recorded (not "Not recorded")
                            sensorManager.computeAccelCalibration()
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Compute Accel Calibration")
                    }
                }
                // inside Column in CalibrationScreen, after the accel card

                Spacer(modifier = Modifier.height(12.dp))

                // 3. Magnetometer calibration (5s)
                var magStatusText by remember { mutableStateOf("Ready. Rotate phone in all directions.") }
                var isMagCalibrating by remember { mutableStateOf(false) }

                Card(
                    modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
                ) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        Text(
                            "3. Magnetometer Calibration (5s)",
                            fontWeight = FontWeight.Bold,
                            color = MaterialTheme.colorScheme.primary
                        )
                        Text(
                            "Move phone in a \"figure 8\" and rotate in all directions for 5 seconds.",
                            fontSize = 11.sp
                        )
                        Spacer(modifier = Modifier.height(8.dp))

                        Button(
                            onClick = {
                                scope.launch {
                                    isMagCalibrating = true
                                    sensorManager.isCollectingMagSamples = true
                                    // reset min/max so this run is clean
                                    sensorManager.magMin = floatArrayOf(
                                        Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE
                                    )
                                    sensorManager.magMax = floatArrayOf(
                                        Float.MIN_VALUE, Float.MIN_VALUE, Float.MIN_VALUE
                                    )

                                    for (i in 5 downTo 1) {
                                        magStatusText = "Collecting mag data... $i s left."
                                        delay(1000)
                                    }

                                    // compute offset/scale from the collected min/max
                                    sensorManager.computeMagCalibration()
                                    isMagCalibrating = false
                                    sensorManager.isCollectingMagSamples = false
                                    magStatusText = "Mag calibration done."
                                }
                            },
                            enabled = !isMagCalibrating,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(if (isMagCalibrating) "Recording..." else "Calibrate Mag (Collect 5s)")
                        }

                        Spacer(modifier = Modifier.height(4.dp))
                        Text(
                            text = magStatusText,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Medium
                        )
                    }
                }
                Spacer(modifier = Modifier.height(16.dp))
                Button(onClick = onDismiss, modifier = Modifier.fillMaxWidth()) {
                    Text("CLOSE & BACK TO MOUSE")
                }
            }
        }
    }
}