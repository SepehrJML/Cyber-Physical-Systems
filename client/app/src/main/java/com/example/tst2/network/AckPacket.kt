package com.example.tst2.network

data class AckPacket(
    val status: String,
    val type: String,
    val timestamp: Long
)