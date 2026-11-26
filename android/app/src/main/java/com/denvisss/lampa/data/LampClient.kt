package com.denvisss.lampa.data

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.decodeFromJsonElement
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.json.floatOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.MultipartBody
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import java.util.concurrent.TimeUnit

class LampClient(hostInput: String) {

    val baseUrl: String = normalize(hostInput)
    private val wsUrl: String = wsUrlFor(hostInput)

    private val json = Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
    }
    private val http = OkHttpClient.Builder()
        .connectTimeout(4, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    private var socket: WebSocket? = null
    private var stopped = false
    private var retryMs = 800L
    private var previewOn = false

    var onState: ((LampState) -> Unit)? = null
    var onSunrise: ((SunriseInfo) -> Unit)? = null
    var onPreview: ((ByteArray) -> Unit)? = null
    var onSocket: ((Boolean) -> Unit)? = null
    var onOta: ((phase: String, progress: Int, error: String) -> Unit)? = null

    fun start() {
        stopped = false
        connectWs()
    }

    fun stop() {
        stopped = true
        socket?.close(1000, null)
        socket = null
        scope.cancel()
    }

    fun setPreview(on: Boolean) {
        previewOn = on
        socket?.send("""{"type":"preview","on":$on,"fps":6}""")
    }

    private fun connectWs() {
        if (stopped) return
        val request = Request.Builder().url(wsUrl).build()
        socket = http.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                retryMs = 800
                onSocket?.invoke(true)
                if (previewOn) webSocket.send("""{"type":"preview","on":true,"fps":6}""")
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                try {
                    val root = json.parseToJsonElement(text).jsonObject
                    when (root["type"]?.jsonPrimitive?.content) {
                        "state" -> root["data"]?.let {
                            onState?.invoke(json.decodeFromJsonElement(LampState.serializer(), it))
                        }
                        "sunrise" -> onSunrise?.invoke(
                            SunriseInfo(
                                active = root["active"]?.jsonPrimitive?.booleanOrNull ?: false,
                                snoozing = root["snoozing"]?.jsonPrimitive?.booleanOrNull ?: false,
                                progress = root["progress"]?.jsonPrimitive?.floatOrNull ?: 0f,
                            )
                        )
                        "ota" -> onOta?.invoke(
                            root["phase"]?.jsonPrimitive?.content ?: "idle",
                            root["progress"]?.jsonPrimitive?.intOrNull ?: 0,
                            root["error"]?.jsonPrimitive?.content ?: "",
                        )
                    }
                } catch (_: Exception) {
                }
            }

            override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                val frame = bytes.toByteArray()
                if (frame.size == 768) onPreview?.invoke(frame)
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                onSocket?.invoke(false)
                scheduleReconnect()
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                onSocket?.invoke(false)
                scheduleReconnect()
            }
        })
    }

    private fun scheduleReconnect() {
        if (stopped) return
        scope.launch {
            delay(retryMs)
            retryMs = (retryMs * 1.6).toLong().coerceAtMost(8000)
            connectWs()
        }
    }

    suspend fun fetchState(): LampState =
        json.decodeFromString(execute(Request.Builder().url("$baseUrl/api/state").build()))

    suspend fun fetchEffects(): List<EffectInfo> =
        json.decodeFromString(execute(Request.Builder().url("$baseUrl/api/effects").build()))

    suspend fun fetchTimezones(): List<String> =
        json.decodeFromString(execute(Request.Builder().url("$baseUrl/api/timezones").build()))

    suspend fun checkFirmware(): FirmwareUpdate =
        json.decodeFromString(execute(Request.Builder().url("$baseUrl/api/update/check").build()))

    suspend fun installFirmware() {
        execute(
            Request.Builder()
                .url("$baseUrl/api/update/install")
                .post(ByteArray(0).toRequestBody(JSON))
                .build()
        )
    }

    suspend fun postPatch(patch: JsonObject): LampState {
        val body = patch.toString().toRequestBody(JSON)
        return json.decodeFromString(
            execute(Request.Builder().url("$baseUrl/api/state").post(body).build())
        )
    }

    suspend fun putAlarms(alarms: List<AlarmCfg>): LampState {
        val payload = buildJsonObject { put("alarms", json.encodeToJsonElement(alarms)) }
        val body = payload.toString().toRequestBody(JSON)
        return json.decodeFromString(
            execute(Request.Builder().url("$baseUrl/api/alarms").put(body).build())
        )
    }

    suspend fun action(name: String, extra: JsonObject = JsonObject(emptyMap())): LampState {
        val payload = buildJsonObject {
            put("action", name)
            extra.forEach { (k, v) -> put(k, v) }
        }
        val body = payload.toString().toRequestBody(JSON)
        return json.decodeFromString(
            execute(Request.Builder().url("$baseUrl/api/action").post(body).build())
        )
    }

    suspend fun saveWifi(ssid: String, pass: String) {
        val body = buildJsonObject {
            put("ssid", ssid)
            put("pass", pass)
        }.toString().toRequestBody(JSON)
        execute(Request.Builder().url("$baseUrl/api/wifi").post(body).build())
    }

    suspend fun uploadFirmware(bytes: ByteArray): String {
        val multipart = MultipartBody.Builder()
            .setType(MultipartBody.FORM)
            .addFormDataPart(
                "firmware",
                "firmware.bin",
                bytes.toRequestBody("application/octet-stream".toMediaType())
            )
            .build()
        return execute(Request.Builder().url("$baseUrl/api/ota").post(multipart).build())
    }

    private suspend fun execute(request: Request): String = withContext(Dispatchers.IO) {
        http.newCall(request).execute().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) {
                throw LampException(text.ifBlank { "HTTP ${response.code}" })
            }
            text
        }
    }

    companion object {
        val JSON = "application/json; charset=utf-8".toMediaType()

        fun normalize(input: String): String {
            var host = input.trim()
                .removePrefix("https://")
                .removePrefix("http://")
                .trimEnd('/')
            if (host.isEmpty()) host = "lampa.local"
            host = host.substringBefore('/')
            return "http://$host"
        }

        fun wsUrlFor(input: String): String {
            var host = input.trim()
                .removePrefix("https://")
                .removePrefix("http://")
                .trimEnd('/')
            if (host.isEmpty()) host = "lampa.local"
            host = host.substringBefore('/')
            val authority = if (host.contains(':')) host else "$host:81"
            return "ws://$authority/"
        }
    }
}
