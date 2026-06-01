package com.example.tst2.network

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