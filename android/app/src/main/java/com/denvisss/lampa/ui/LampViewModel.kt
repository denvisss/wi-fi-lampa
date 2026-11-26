package com.denvisss.lampa.ui

import android.app.Application
import android.os.Handler
import android.os.Looper
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.denvisss.lampa.data.AlarmCfg
import com.denvisss.lampa.data.EffectInfo
import com.denvisss.lampa.data.FirmwareUpdate
import com.denvisss.lampa.data.GhRelease
import com.denvisss.lampa.data.GitHub
import com.denvisss.lampa.data.LampClient
import com.denvisss.lampa.data.LampException
import com.denvisss.lampa.data.LampState
import com.denvisss.lampa.data.Prefs
import com.denvisss.lampa.data.isVersionNewer
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import java.io.File

class LampViewModel(app: Application) : AndroidViewModel(app) {

    private val prefs = Prefs(app)
    private val main = Handler(Looper.getMainLooper())
    private var client: LampClient? = null
    private var pollJob: Job? = null
    private var wsAlive = false

    val appVersion: String = runCatching {
        app.packageManager.getPackageInfo(app.packageName, 0).versionName
    }.getOrNull().orEmpty()

    var host by mutableStateOf(prefs.host)
        private set
    var state by mutableStateOf<LampState?>(null)
        private set
    var effects by mutableStateOf<List<EffectInfo>>(emptyList())
        private set
    var timezones by mutableStateOf<List<String>>(emptyList())
        private set
    var tzLoading by mutableStateOf(false)
        private set
    var connected by mutableStateOf(false)
        private set
    var livePreview by mutableStateOf(false)
        private set
    var previewFrame by mutableStateOf<ByteArray?>(null)
        private set
    var message by mutableStateOf<String?>(null)
        private set
    var otaBusy by mutableStateOf(false)
        private set
    var otaStatus by mutableStateOf<String?>(null)
        private set
    var otaPhase by mutableStateOf("idle")
        private set
    var otaProgress by mutableStateOf(0)
        private set
    var fwCheck by mutableStateOf<FirmwareUpdate?>(null)
        private set
    var fwChecking by mutableStateOf(false)
        private set
    var appRelease by mutableStateOf<GhRelease?>(null)
        private set
    var appChecking by mutableStateOf(false)
        private set
    var appDownloading by mutableStateOf(false)
        private set
    var appProgress by mutableStateOf(0)
        private set
    var appApkReady by mutableStateOf<File?>(null)
        private set

    init {
        prefs.host?.let { open(it) }
    }

    fun connect(input: String) {
        val target = input.trim().ifBlank { "lampa.local" }
        prefs.host = target
        host = target
        open(target)
    }

    fun disconnect() {
        prefs.host = null
        host = null
        client?.stop()
        client = null
        pollJob?.cancel()
        state = null
        effects = emptyList()
        previewFrame = null
        livePreview = false
        connected = false
        wsAlive = false
    }

    fun retry() {
        wsAlive = false
        client?.start()
        refresh()
    }

    private fun open(target: String) {
        client?.stop()
        pollJob?.cancel()
        state = null
        effects = emptyList()
        previewFrame = null
        livePreview = false
        connected = false
        wsAlive = false
        otaStatus = null
        otaPhase = "idle"
        otaProgress = 0
        val c = LampClient(target)
        c.onState = { s ->
            main.post {
                state = s
                connected = true
                wsAlive = true
                if (effects.isEmpty()) fetchEffects()
            }
        }
        c.onSunrise = { sr -> main.post { state = state?.copy(sunrise = sr) } }
        c.onPreview = { bytes -> main.post { previewFrame = bytes } }
        c.onSocket = { ok -> main.post { wsAlive = ok; if (ok) connected = true } }
        c.onOta = { phase, pct, err ->
            main.post {
                otaPhase = phase
                otaProgress = pct
                when (phase) {
                    "connecting", "downloading", "finalizing" -> otaBusy = true
                    "done" -> {
                        otaBusy = false
                        otaStatus = null
                        fwCheck = null
                    }
                    "error" -> {
                        otaBusy = false
                        otaStatus = if (err.isBlank()) "Ошибка обновления" else "Ошибка: ${err.take(80)}"
                    }
                }
            }
        }
        client = c
        c.start()
        refresh()
        fetchEffects()
        startPolling()
    }

    private fun refresh() {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.fetchState() }
                .onSuccess { state = it; connected = true }
                .onFailure { if (!wsAlive) connected = false }
        }
    }

    private fun fetchEffects() {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.fetchEffects() }.onSuccess { effects = it }
        }
    }

    private fun startPolling() {
        pollJob?.cancel()
        pollJob = viewModelScope.launch {
            while (isActive) {
                delay(8000)
                val c = client ?: continue
                if (wsAlive) continue
                runCatching { c.fetchState() }
                    .onSuccess { state = it; connected = true }
                    .onFailure { connected = false }
            }
        }
    }

    fun togglePower() {
        val s = state ?: return
        patch { put("power", !s.power) }
    }

    fun setBrightness(value: Int) {
        patch { put("brightness", value.coerceIn(1, 255)) }
    }

    fun setCurrentLimit(ma: Int) {
        patch { put("currentLimit", ma.coerceIn(500, 15000)) }
    }

    fun setEffect(id: String) {
        patch {
            put("effect", id)
            put("power", true)
        }
    }

    fun setColor(hex: String) = patch { put("color", hex) }

    fun setTz(tz: String) = patch { put("tz", tz) }

    fun loadTimezones(force: Boolean = false) {
        val c = client ?: return
        if (tzLoading) return
        if (timezones.isNotEmpty() && !force) return
        tzLoading = true
        viewModelScope.launch {
            runCatching { c.fetchTimezones() }
                .onSuccess { timezones = it }
                .onFailure { flash("Не удалось получить список поясов") }
            tzLoading = false
        }
    }

    fun setManualTime(epoch: Long) {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.action("settime", buildJsonObject { put("epoch", epoch) }) }
                .onSuccess {
                    state = it
                    flash("Время установлено вручную")
                }
                .onFailure { flash("Не удалось установить время: ${friendly(it)}") }
        }
    }

    fun useAutoTime() {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.action("timens") }
                .onSuccess {
                    state = it
                    flash("Включена синхронизация по сети")
                }
                .onFailure { flash("Не удалось включить NTP: ${friendly(it)}") }
        }
    }

    fun setClock(showDate: Boolean? = null, hour12: Boolean? = null, speed: Int? = null, colorMode: Int? = null) {
        val nested = buildJsonObject {
            showDate?.let { put("showDate", it) }
            hour12?.let { put("hour12", it) }
            speed?.let { put("speed", it) }
            colorMode?.let { put("colorMode", it) }
        }
        patch { put("clock", nested) }
    }

    fun setMapping(serpentine: Boolean? = null, reverse: Boolean? = null, flipY: Boolean? = null, offset: Int? = null) {
        val nested = buildJsonObject {
            serpentine?.let { put("serpentine", it) }
            reverse?.let { put("reverse", it) }
            flipY?.let { put("flipY", it) }
            offset?.let { put("offset", ((it % 16) + 16) % 16) }
        }
        patch { put("mapping", nested) }
    }

    fun setPlaylist(enabled: Boolean? = null, intervalMin: Int? = null) {
        val nested = buildJsonObject {
            enabled?.let { put("enabled", it) }
            intervalMin?.let { put("intervalMin", it) }
        }
        patch { put("playlist", nested) }
    }

    fun setPreview(on: Boolean) {
        livePreview = on
        client?.setPreview(on)
        if (!on) previewFrame = null
    }

    fun saveAlarms(alarms: List<AlarmCfg>) {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.putAlarms(alarms) }
                .onSuccess { state = it; flash("Будильники сохранены") }
                .onFailure { flash("Не удалось сохранить: ${friendly(it)}") }
        }
    }

    fun saveWifi(ssid: String, pass: String) {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.saveWifi(ssid, pass) }
                .onSuccess { flash("Сеть сохранена. Лампа перезагружается…") }
                .onFailure { flash("Не удалось сохранить сеть") }
        }
    }

    fun uploadFirmware(bytes: ByteArray) {
        val c = client ?: return
        if (otaBusy) return
        otaBusy = true
        otaStatus = "Загрузка…"
        viewModelScope.launch {
            runCatching { c.uploadFirmware(bytes) }
                .onSuccess {
                    otaStatus = "Обновлено. Лампа перезагружается…"
                    flash("Прошивка обновлена")
                }
                .onFailure {
                    otaStatus = "Ошибка: ${friendly(it)}"
                    flash("Не удалось обновить прошивку")
                }
            otaBusy = false
        }
    }

    fun checkFirmwareUpdate() {
        val c = client ?: return
        if (fwChecking || otaBusy) return
        fwChecking = true
        viewModelScope.launch {
            runCatching { c.checkFirmware() }
                .onSuccess { fwCheck = it }
                .onFailure {
                    fwCheck = null
                    flash("Не удалось проверить: ${friendly(it)}")
                }
            fwChecking = false
        }
    }

    fun installFirmwareUpdate() {
        val c = client ?: return
        if (otaBusy) return
        otaBusy = true
        otaPhase = "connecting"
        otaProgress = 0
        viewModelScope.launch {
            runCatching { c.installFirmware() }
                .onFailure {
                    otaBusy = false
                    otaPhase = "error"
                    otaStatus = "Не удалось начать обновление: ${friendly(it)}"
                }
        }
    }

    fun checkAppUpdate() {
        if (appChecking) return
        appChecking = true
        viewModelScope.launch {
            runCatching { GitHub.latestRelease() }
                .onSuccess { rel ->
                    if (rel != null && isVersionNewer(rel.tag, appVersion)) {
                        appRelease = rel
                    } else {
                        appRelease = null
                        flash("У приложения последняя версия")
                    }
                }
                .onFailure { flash("Не удалось проверить: ${friendly(it)}") }
            appChecking = false
        }
    }

    fun downloadAppUpdate() {
        val rel = appRelease ?: return
        if (appDownloading) return
        val asset = rel.assets.firstOrNull { it.name == "lampa-android.apk" }
        if (asset == null) {
            flash("В релизе нет APK")
            return
        }
        appDownloading = true
        appProgress = 0
        appApkReady = null
        viewModelScope.launch {
            val dir = File(getApplication<Application>().cacheDir, "updates").apply { mkdirs() }
            val dest = File(dir, "lampa-${rel.tag}.apk")
            runCatching { GitHub.download(asset.url, dest) { p -> main.post { appProgress = p } } }
                .onSuccess { appApkReady = dest }
                .onFailure { flash("Не удалось скачать APK: ${friendly(it)}") }
            appDownloading = false
        }
    }

    fun clearAppApk() {
        appApkReady = null
    }

    fun action(name: String, extra: JsonObject = JsonObject(emptyMap())) {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.action(name, extra) }
                .onSuccess { state = it }
                .onFailure { flash("Действие не выполнено") }
        }
    }

    fun calibrate(pattern: String) {
        action("calibrate", buildJsonObject {
            put("pattern", pattern)
            put("ms", 20000)
        })
    }

    fun sunriseTest() = action("sunrise_test")
    fun sunriseStop() = action("sunrise_stop")
    fun sunriseSnooze() = action("sunrise_snooze")
    fun nyShow() = action("nyshow")
    fun factoryReset() = action("factory")

    fun flash(text: String) {
        message = text
    }

    fun clearMessage() {
        message = null
    }

    private fun patch(build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit) {
        val c = client ?: return
        viewModelScope.launch {
            runCatching { c.postPatch(buildJsonObject(build)) }
                .onSuccess { state = it; connected = true }
                .onFailure { flash("Не удалось: ${friendly(it)}") }
        }
    }

    private fun friendly(e: Throwable): String =
        (e as? LampException)?.message?.take(80) ?: e.message?.take(80) ?: "нет связи"

    override fun onCleared() {
        client?.stop()
        pollJob?.cancel()
        super.onCleared()
    }
}
