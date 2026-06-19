package com.example.tst2.network

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.SocketTimeoutException

class UdpClient(
    private val serverIp: String,
    private val serverPort: Int = 50000
) {

    private val socket = DatagramSocket()
    private var resolvedAddress: InetAddress? = null

    private suspend fun getAddress(): InetAddress {
        var addr = resolvedAddress
        if (addr == null) {
            addr = withContext(Dispatchers.IO) {
                InetAddress.getByName(serverIp)
            }
            resolvedAddress = addr
        }
        return addr!!
    }

    init {
        socket.soTimeout = 200
    }

    suspend fun sendMove(
        dx: Float,
        dy: Float
    ) {
        android.os.Trace.beginSection("AirMouse_Udp_SendMove")
        try {
            val json = JSONObject()

            json.put("type", "move")
            json.put("dx", dx)
            json.put("dy", dy)
            json.put("timestamp", System.currentTimeMillis())

            sendRaw(json.toString())
        } finally {
            android.os.Trace.endSection()
        }
    }

    suspend fun sendClick() {
        android.os.Trace.beginSection("AirMouse_Udp_SendClick")
        try {
            val timestamp = System.currentTimeMillis()

            val json = JSONObject()

            json.put("type", "click")
            json.put("button", "left")
            json.put("timestamp", timestamp)

            sendReliable(
                json.toString(),
                "click",
                timestamp
            )
        } finally {
            android.os.Trace.endSection()
        }
    }

    suspend fun sendScroll(
        direction: Int
    ) {
        android.os.Trace.beginSection("AirMouse_Udp_SendScroll")
        try {
            val timestamp = System.currentTimeMillis()

            val json = JSONObject()

            json.put("type", "scroll")
            json.put("scroll_dir", direction)
            json.put("timestamp", timestamp)

            sendReliable(
                json.toString(),
                "scroll",
                timestamp
            )
        } finally {
            android.os.Trace.endSection()
        }
    }

    private suspend fun sendReliable(
        payload: String,
        expectedType: String,
        timestamp: Long
    ) {

        repeat(5) {

            sendRaw(payload)

            if (waitForAck(expectedType, timestamp)) {
                return
            }
        }
    }

    private suspend fun waitForAck(
        expectedType: String,
        timestamp: Long
    ): Boolean {

        return withContext(Dispatchers.IO) {

            try {

                val buffer = ByteArray(1024)

                val packet = DatagramPacket(
                    buffer,
                    buffer.size
                )

                socket.receive(packet)

                val response = String(
                    packet.data,
                    0,
                    packet.length
                )

                val json = JSONObject(response)

                json.getString("status") == "ACK"
                        &&
                        json.getString("type") == expectedType
                        &&
                        json.getLong("timestamp") == timestamp

            } catch (e: SocketTimeoutException) {

                false
            }
        }
    }

    private suspend fun sendRaw(
        message: String
    ) {
        val address = getAddress()
        withContext(Dispatchers.IO) {
            android.os.Trace.beginSection("AirMouse_Udp_SendRaw")
            try {
                val data =
                    (message + "\n").toByteArray()

                val packet = DatagramPacket(
                    data,
                    data.size,
                    address,
                    serverPort
                )

                socket.send(packet)
            } finally {
                android.os.Trace.endSection()
            }
        }
    }
}