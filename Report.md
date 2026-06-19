## CPS CA2 Report - Spring 1405


### Team Members
| Name | Student ID | Contribution |
|---|---|---|
| Sepehr Bazargan | 810102303 | ??? |
| Sepehr Jamali | 810101400 | ??? |
| Mohamad Mahdi Sharifi | 810101449 | UI |
| Mohamad Mahdi Samadi | 810101465 | Server |



## 1. Introduction

The purpose of this project was to develop an air mouse system that allows a smartphone to control a computer mouse through motion. Instead of using a physical mouse, the orientation and movement of the phone are translated into cursor movement. Additional gestures are used for clicking and scrolling.

The system is divided into two components. The first component is an Android application written in Kotlin, responsible for acquiring sensor data, calibrating the sensors, detecting gestures, and sending commands through the network. The second component is a Python server running on a computer. The server receives commands from the phone and executes them using the PyAutoGUI library.

The project uses raw sensor measurements and implements calibration and sensor fusion techniques to improve accuracy. Communication between the phone and the server is based on UDP in order to achieve low latency. Since UDP does not guarantee packet delivery, an acknowledgment mechanism is implemented for click and scroll events.

---

## 2. System Architecture

The overall architecture of the system is illustrated below.

```
Phone Sensors
       ↓
Sensor Calibration
       ↓
Complementary Filter
       ↓
Gesture Detection
       ↓
UDP Client
       ↓
Wi-Fi Network
       ↓
Python Server
       ↓
PyAutoGUI
       ↓
Computer Mouse
```

The Android application continuously reads accelerometer, gyroscope, and magnetometer data. After calibration, the measurements are combined through sensor fusion to estimate the orientation of the device. Changes in orientation are converted into cursor movements, while rapid angular motions are interpreted as click and scroll gestures.

These commands are sent to the Python server, which controls the computer cursor.

---

## 3. Android Application Overview

The Android application consists mainly of two classes:

* `MainActivity`
* `AirMouseSensorManager`

`MainActivity` is responsible for the graphical user interface and network communication, while `AirMouseSensorManager` handles sensor processing and gesture detection.

---

## 4. Main Activity

The application starts from `MainActivity`.

```kotlin
override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContent {
        AirMouseApp()
    }
}
```

The `AirMouseApp()` composable function creates the graphical interface and initializes all required components.

---

### 4.1 State Variables

Several state variables are used to store sensor values, cursor orientation, click status, and scrolling status.

```kotlin
var accelData by remember { mutableStateOf(floatArrayOf(0f,0f,0f)) }
var gyroData by remember { mutableStateOf(floatArrayOf(0f,0f,0f)) }
var magData by remember { mutableStateOf(floatArrayOf(0f,0f,0f)) }

var yaw by remember { mutableStateOf(0f) }
var pitch by remember { mutableStateOf(0f) }
var roll by remember { mutableStateOf(0f) }
```

These variables are updated continuously as new sensor measurements become available.

---

## 4.2 Creating the Sensor Manager

The sensor manager is initialized by providing three callback functions.

```kotlin
val sensorManager = remember {
    AirMouseSensorManager(
        context = context,
        onSensorDataUpdated = { ... },
        onClick = { ... },
        onScroll = { ... }
    )
}
```

The callbacks allow the sensor manager to communicate with the user interface.

* `onSensorDataUpdated()` updates displayed sensor values.
* `onClick()` transmits click commands.
* `onScroll()` transmits scrolling commands.

This design separates sensor processing from user interface logic.

---

### 4.3 Start and Stop Mechanism

The user can start or stop the air mouse.

```kotlin
Button(
    onClick = {
        udpClient = UdpClient(ipAddress)
        isRunning = !isRunning
    }
)
```

When the mouse becomes active, sensor listening starts automatically.

```kotlin
LaunchedEffect(isRunning) {
    if (isRunning)
        sensorManager.startListening()
    else
        sensorManager.stopListening()
}
```

Stopping the application unregisters all sensors and prevents unnecessary CPU usage.

---

### 4.4 Mouse Movement Visualization

The application provides visual feedback by displaying a colored square inside a larger area.

```kotlin
Box(
    modifier = Modifier
        .offset(x = xOffset.dp, y = yOffset.dp)
        .size(30.dp)
)
```

The square changes color according to the current action:

* Green: normal movement.
* Red: click detected.
* Blue: scrolling.

This visual feedback is useful during debugging and calibration.

---

### 4.5 Orientation to Cursor Movement

The cursor position depends on yaw and roll angles.

```kotlin
xOffset = (-yaw * gain)
yOffset = (-roll * gain)
```

These values are clamped to prevent excessive movement.

```kotlin
xOffset = xOffset.coerceIn(-maxOffset,maxOffset)
yOffset = yOffset.coerceIn(-maxOffset,maxOffset)
```

Using relative orientation instead of absolute position results in smoother cursor control.

---

### 4.6 Sending Movement Packets

The application sends cursor movement approximately 60 times per second.

```kotlin
val minIntervalMs = 16L
```

Movement is calculated using orientation differences.

```kotlin
client.sendMove(
    -(yaw-yawPrev)*gain,
    -(roll-rollPrev)*gain
)
```

Using differences between consecutive angles avoids large jumps and improves responsiveness.

---

### 4.7 Calibration Interface

A separate dialog allows calibration procedures.

```kotlin
Button(
    onClick = {
        showCalibrationDialog = true
        sensorManager.calibrationFlag = true
        sensorManager.setReferenceOrientation()
    }
)
```

The calibration screen contains three sections:

1. Gyroscope calibration.
2. Accelerometer calibration.
3. Magnetometer calibration.

Separating calibration from normal operation prevents unwanted cursor movement during data collection.

---

## 5. Sensor Processing and Gesture Recognition

The most important part of the application is the `AirMouseSensorManager` class. This class is responsible for acquiring raw measurements from the sensors, calibrating them, combining them through sensor fusion, and detecting user gestures.

The system uses three sensors:

* Accelerometer
* Gyroscope
* Magnetometer

These sensors complement each other. The gyroscope provides smooth short-term motion measurements but suffers from drift, while the accelerometer and magnetometer provide stable references but are noisy. Combining them improves accuracy.

---

### 5.1 Sensor Initialization

The sensors are obtained from Android's sensor manager.

```kotlin
private val accelerometer =
    sensorManager.getDefaultSensor(
        Sensor.TYPE_ACCELEROMETER
    )

private val gyroscopeUncalibrated =
    sensorManager.getDefaultSensor(
        Sensor.TYPE_GYROSCOPE_UNCALIBRATED
    )

private val magnetometerUncalibrated =
    sensorManager.getDefaultSensor(
        Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED
    )
```

Raw sensors were intentionally selected because the project requires calibration and error correction to be implemented manually.

---

### 5.2 Starting and Stopping Measurements

When the air mouse starts, listeners are registered.

```kotlin
sensorManager.registerListener(
    this,
    accelerometer,
    SensorManager.SENSOR_DELAY_GAME
)
```

Similarly, listeners are registered for the gyroscope and magnetometer.

When the application stops, all listeners are removed.

```kotlin
sensorManager.unregisterListener(this)
```

This prevents unnecessary processing and saves battery power.

---

### 5.3 Sensor Event Processing

Every time new measurements arrive, the callback function `onSensorChanged()` is executed.

Depending on the sensor type, the corresponding values are stored.

```kotlin
when(event.sensor.type){

    Sensor.TYPE_ACCELEROMETER -> {
        rawAccel = event.values.clone()
    }

    Sensor.TYPE_GYROSCOPE_UNCALIBRATED -> {
        rawGyro = event.values.clone()
    }

    Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED -> {
        rawMag = event.values.clone()
    }
}
```

These raw measurements are later corrected using calibration parameters.

---

## 6. Gyroscope Calibration

The gyroscope suffers from bias errors. Even when the phone is stationary, small angular velocities are measured. If these values are integrated over time, the estimated orientation gradually drifts.

To estimate the bias, samples are collected while the phone remains stationary.

```kotlin
if (isCollectingGyroSamples) {
    gyroSamples.add(rawGyro.clone())
}
```

After three seconds, the average value of each axis is calculated.

```kotlin
gyroBias[0] = sumX / n
gyroBias[1] = sumY / n
gyroBias[2] = sumZ / n
```

These offsets are then subtracted from future measurements.

```kotlin
gyroCorrected[0] = rawGyro[0] - gyroBias[0]
```

This significantly reduces orientation drift.

---

## 7. Accelerometer Calibration

Accelerometer measurements may contain offset and scale errors. Therefore, six measurements are recorded by placing the phone in different orientations.

```kotlin
fun recordAccelPosition(index: Int) {
    accelPositions[index] = rawAccel.clone()
}
```

Positive and negative measurements of each axis are used to calculate offsets.

```kotlin
accelOffset[0] =
    (xPlus + xMinus) / 2f
```

Scale factors are also computed.

```kotlin
accelScale[0] =
    xSpan / g
```

Corrected measurements are then obtained by

```kotlin
correctedX =
(rawX - accelOffset[0]) / accelScale[0]
```

This improves the accuracy of tilt estimation.

---

## 8. Magnetometer Calibration

Magnetometers are strongly affected by surrounding magnetic objects. To compensate for these distortions, the user rotates the phone during calibration.

Minimum and maximum values are continuously recorded.

```kotlin
if (currentMag[i] < magMin[i])
    magMin[i] = currentMag[i]

if (currentMag[i] > magMax[i])
    magMax[i] = currentMag[i]
```

The center of these measurements gives the offset.

```kotlin
magOffset[0] =
(magMax[0] + magMin[0]) / 2f
```

Scale factors are also calculated.

```kotlin
magScale[0] = avgSpan / spanX
```

The corrected values produce a more reliable heading angle.

---

## 9. Complementary Filter

A complementary filter combines information from all sensors.

Gyroscope integration provides smooth short-term orientation.

Accelerometer and magnetometer measurements provide long-term stability.

Roll estimation is performed using

```kotlin
rollCF =
alphaRP * rollGyro +
(1f - alphaRP) * rollAcc
```

Pitch is estimated similarly.

```kotlin
pitchCF =
alphaRP * pitchGyro +
(1f - alphaRP) * pitchAcc
```

Yaw combines gyroscope and magnetometer information.

```kotlin
yawCF =
alphaYaw * yawGyro +
(1f - alphaYaw) * yawMag
```

The parameter `alpha` controls the contribution of each sensor. A value close to one gives more weight to the gyroscope while preserving long-term stability through accelerometer and magnetometer measurements.

The complementary filter provides a simple and computationally efficient solution suitable for real-time applications.

---

## 10. Reference Orientation

Before using the mouse, a reference orientation is stored.

```kotlin
sensorManager.setReferenceOrientation()
```

Subsequent orientation values are measured relative to this reference.

This means the cursor movement depends on how much the phone rotates from its initial position rather than its absolute orientation.

This approach makes the system more comfortable for the user.

---

## 11. Click Detection

Mouse clicks are recognized from rapid wrist movements.

The angular velocity around the Y-axis is monitored.

```kotlin
if (
    abs(yRate) > clickThreshold
    &&
    yRate < 0f
)
{
    onClick()
}
```

A cooldown interval prevents multiple detections from a single gesture.

```kotlin
if (
currentTime-lastClickTime
>
clickCooldown
)
```

Without this mechanism, one movement could generate several clicks.

---

## 12. Scroll Detection

Smaller angular velocities are interpreted as scrolling actions.

```kotlin
if (
abs(yRate)
in
scrollMinThreshold..clickThreshold
)
{
    onScroll(direction)
}
```

The direction determines whether the page moves upward or downward.

Another cooldown mechanism is used to avoid repeated scrolling.

```kotlin
currentTime-lastScrollTime
>
scrollCooldown
```

This provides smoother user interaction.

---

## 13. Error Reduction Techniques

Several techniques were implemented to improve stability and reduce noise.

### Gyroscope bias removal

Compensates for constant drift.

### Accelerometer offset correction

Removes systematic errors.

### Magnetometer calibration

Reduces magnetic distortions.

### Complementary filter

Combines advantages of multiple sensors.

### Click cooldown

Prevents accidental multiple clicks.

### Scroll cooldown

Improves scrolling behavior.

### Reference orientation

Allows comfortable cursor control.

Together, these techniques greatly improve the usability of the air mouse system and make the cursor movement smoother and more reliable.

---

## 14. Network Communication

Communication between the Android application and the Python server is implemented using UDP. UDP was chosen because it has lower overhead than TCP and provides lower latency, which is important for responsive cursor movement.

The Android application acts as a client, while the Python program acts as a server.

```
Android Application
       ↓
UDP Packets
       ↓
Python Server
       ↓
PyAutoGUI
       ↓
Mouse Operations
```

Movement packets are generated continuously, while click and scroll packets are transmitted only when a gesture is detected.

---

## 15. Packet Structures

To organize communication, different packet types are defined.

The sealed class `MousePacket` contains all possible packet types.

```kotlin
sealed class MousePacket {

    data class Move(
        val dx: Float,
        val dy: Float,
        val timestamp: Long
    ) : MousePacket()

    data class Click(
        val button: String,
        val timestamp: Long
    ) : MousePacket()

    data class Scroll(
        val scrollDir: Int,
        val timestamp: Long
    ) : MousePacket()
}
```

Using separate classes improves readability and makes it easier to distinguish different operations.

### Move Packet

A movement packet contains the cursor displacement values.

```json
{
    "type":"move",
    "dx":15,
    "dy":-10,
    "timestamp":123456
}
```

### Click Packet

```json
{
    "type":"click",
    "button":"left",
    "timestamp":123456
}
```

### Scroll Packet

```json
{
    "type":"scroll",
    "scroll_dir":1,
    "timestamp":123456
}
```

---

## 16. UDP Client

The Android application communicates with the server through the `UdpClient` class.

The socket is created once.

```kotlin
private val socket = DatagramSocket()
```

A timeout is also specified.

```kotlin
socket.soTimeout = 200
```

This timeout prevents the application from waiting indefinitely for acknowledgements.

---

### 16.1 Sending Cursor Movement

Movement packets are sent using the `sendMove()` function.

```kotlin
json.put("type", "move")
json.put("dx", dx)
json.put("dy", dy)
json.put("timestamp", System.currentTimeMillis())
```

Finally, the packet is transmitted.

```kotlin
sendRaw(json.toString())
```

Movement packets are not guaranteed to arrive. Since cursor updates are generated continuously, losing one packet is not a serious problem.

---

### 16.2 Sending Click Events

Click packets contain the button name and timestamp.

```kotlin
json.put("type", "click")
json.put("button", "left")
json.put("timestamp", timestamp)
```

Instead of sending the packet directly, the reliable transmission mechanism is used.

```kotlin
sendReliable(
    json.toString(),
    "click",
    timestamp
)
```

---

### 16.3 Sending Scroll Events

Scrolling uses a similar procedure.

```kotlin
json.put("type", "scroll")
json.put("scroll_dir", direction)
json.put("timestamp", timestamp)
```

The packet is also sent through the reliability mechanism.

```kotlin
sendReliable(
    json.toString(),
    "scroll",
    timestamp
)
```

---

## 17. Reliable Transmission

UDP does not guarantee delivery. Therefore, losing click or scroll packets would negatively affect the user experience.

To solve this problem, retransmission is implemented.

```kotlin
repeat(5) {

    sendRaw(payload)

    if (waitForAck(expectedType, timestamp)) {
        return
    }
}
```

If an acknowledgement packet is not received, the packet is sent again.

This simple mechanism provides reliability without sacrificing the low latency advantages of UDP.

---

### 17.1 ACK Packets

The acknowledgement packet is represented by

```kotlin
data class AckPacket(
    val status: String,
    val type: String,
    val timestamp: Long
)
```

An example acknowledgement packet is

```json
{
    "status":"ACK",
    "type":"click",
    "timestamp":123456
}
```

The timestamp allows the client to verify that the acknowledgement corresponds to the correct packet.

---

## 18. Python Server

The Python server receives packets and executes mouse operations.

First, a UDP socket is created.

```python
server_socket = socket.socket(
    socket.AF_INET,
    socket.SOCK_DGRAM
)
```

Then the socket is bound to the selected port.

```python
server_socket.bind(("0.0.0.0",50000))
```

The server continuously waits for incoming packets.

```python
while True:
    data, addr = server_socket.recvfrom(1024)
```

---

### 18.1 Decoding Packets

Incoming messages are decoded using JSON.

```python
packet = json.loads(line)
```

The packet type determines which action should be executed.

```python
packet_type = packet.get("type")
```

---

### 18.2 Cursor Movement

For movement packets, displacement values are extracted.

```python
dx = packet.get("dx",0)
dy = packet.get("dy",0)
```

The cursor is moved relative to its current position.

```python
pyautogui.moveRel(dx,dy)
```

This allows smooth cursor motion.

---

### 18.3 Mouse Clicks

When a click packet arrives, the server executes

```python
button = packet.get("button","left")

pyautogui.click(
    button=button
)
```

The corresponding acknowledgement packet is then generated.

```python
ack_packet = {
    "status":"ACK",
    "type":"click",
    "timestamp":packet.get("timestamp")
}
```

The acknowledgement is transmitted back to the phone.

---

### 18.4 Scrolling

Scroll packets are handled similarly.

```python
direction = packet.get("scroll_dir",0)
```

Scrolling is implemented through

```python
pyautogui.scroll(
    direction*100
)
```

The server then sends another acknowledgement packet.

This guarantees reliable scrolling.

---

## 19. Communication Protocol

The communication protocol used in this project is simple and lightweight.

| Packet Type | Purpose         |
| ----------- | --------------- |
| move        | Cursor movement |
| click       | Mouse click     |
| scroll      | Mouse wheel     |
| ACK         | Acknowledgement |

JSON format was selected because it is easy to read, simple to debug, and supported by both Kotlin and Python.

The timestamp field uniquely identifies packets and prevents mismatches between acknowledgements and transmitted messages.

---

## 20. Challenges Encountered

Several challenges were encountered during development.

### Sensor Drift

Gyroscope integration introduces drift over time. This issue was reduced using a complementary filter.

### Sensor Noise

Accelerometer and magnetometer measurements are noisy and require calibration.

### False Gesture Detection

Without cooldown intervals, one hand movement could generate multiple clicks or scroll events.

### Packet Loss

UDP packets may be lost. A simple acknowledgement mechanism was implemented to improve reliability.

### Cursor Stability

Directly using raw sensor values produced unstable cursor movement. Relative orientation and sensor fusion significantly improved smoothness.

---

## 21. Conclusion

In this project, an air mouse system was implemented using an Android smartphone and a Python server. The smartphone collected measurements from the accelerometer, gyroscope, and magnetometer and performed calibration to reduce measurement errors. A complementary filter combined sensor data to estimate orientation and generate cursor movement.

Gesture detection algorithms were implemented to recognize click and scroll actions. Communication between the phone and the computer was performed using UDP to minimize latency. Since UDP does not provide reliable transmission, a lightweight acknowledgement mechanism was added for important events.

Overall, the project demonstrates how smartphone sensors and wireless communication can be combined to create an alternative human-computer interaction device. The resulting system provides smooth cursor control and reliable click and scroll functionality while maintaining low latency.

---

## Perfetto Questions

### Question 1

Answer:

---

---

### Question 2

Answer:

---

---

### Question 3

Answer:

---

---

### Question 4

Answer:

---

---

### Question 5

Answer:

---

---

### Question 6

Answer:

---

---

### Question 7

Answer:

---

---

### Question 8

Answer:

---

---

### Question 9

Answer:

---

---

### Question 10

Answer:

---

---

### Question 11

Answer:

---

---
