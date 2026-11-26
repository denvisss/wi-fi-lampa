package com.denvisss.lampa.data

import kotlinx.serialization.Serializable

@Serializable
data class LampState(
    val power: Boolean = false,
    val brightness: Int = 160,
    val effect: String = "aurora",
    val color: String = "#FFB066",
    val speed: Int = 128,
    val intensity: Int = 160,
    val currentLimit: Int = 9000,
    val tz: String = "Europe/Moscow",
    val manualTime: Boolean = false,
    val clock: ClockCfg = ClockCfg(),
    val mapping: MappingCfg = MappingCfg(),
    val playlist: PlaylistCfg = PlaylistCfg(),
    val alarms: List<AlarmCfg> = emptyList(),
    val device: DeviceInfo = DeviceInfo(),
    val sunrise: SunriseInfo = SunriseInfo(),
    val ny: NyInfo = NyInfo(),
)

@Serializable
data class ClockCfg(
    val colorMode: Int = 0,
    val speed: Int = 80,
    val showDate: Boolean = true,
    val hour12: Boolean = false,
)

@Serializable
data class MappingCfg(
    val serpentine: Boolean = true,
    val reverse: Boolean = false,
    val flipY: Boolean = false,
    val offset: Int = 0,
)

@Serializable
data class PlaylistCfg(
    val enabled: Boolean = false,
    val intervalMin: Int = 15,
)

@Serializable
data class AlarmCfg(
    val enabled: Boolean = true,
    val days: Int = 31,
    val hour: Int = 7,
    val minute: Int = 0,
    val durationMin: Int = 15,
    val maxBrightness: Int = 200,
    val snoozeMin: Int = 9,
    val afterOffMin: Int = 0,
    val lastFiredYday: Int = 0,
)

@Serializable
data class DeviceInfo(
    val fw: String = "",
    val ip: String = "",
    val rssi: Int = 0,
    val uptime: Long = 0,
    val heap: Int = 0,
    val mac: String = "",
    val time: String = "",
    val ap: Boolean = false,
)

@Serializable
data class SunriseInfo(
    val active: Boolean = false,
    val snoozing: Boolean = false,
    val progress: Float = 0f,
    val alarmId: Int = -1,
)

@Serializable
data class NyInfo(val days: Int = -1)

@Serializable
data class EffectInfo(
    val id: String = "",
    val name: String = "",
    val group: String = "atmo",
    val color: Boolean = false,
    val speed: Boolean = false,
    val intensity: Boolean = false,
)

@Serializable
data class WifiNet(
    val ssid: String = "",
    val rssi: Int = 0,
    val secure: Boolean = false,
)

@Serializable
data class FirmwareUpdate(
    val current: String = "",
    val latest: String = "",
    val available: Boolean = false,
    val url: String = "",
    val size: Long = 0,
    val notes: String = "",
    val error: String = "",
)

class LampException(message: String) : Exception(message)
