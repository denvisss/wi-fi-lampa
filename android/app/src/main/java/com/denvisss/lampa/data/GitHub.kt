package com.denvisss.lampa.data

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.util.concurrent.TimeUnit

@Serializable
data class GhAsset(
    val name: String = "",
    @SerialName("browser_download_url") val url: String = "",
    val size: Long = 0,
)

@Serializable
data class GhRelease(
    @SerialName("tag_name") val tag: String = "",
    val body: String = "",
    val draft: Boolean = false,
    val prerelease: Boolean = false,
    val assets: List<GhAsset> = emptyList(),
)

object GitHub {
    private const val API = "https://api.github.com/repos/denvisss/lampa/releases?per_page=5"

    private val json = Json { ignoreUnknownKeys = true }
    private val http = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()

    suspend fun latestRelease(): GhRelease? = withContext(Dispatchers.IO) {
        val request = Request.Builder()
            .url(API)
            .header("Accept", "application/vnd.github+json")
            .header("User-Agent", "lampa-android")
            .build()
        http.newCall(request).execute().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw LampException("GitHub HTTP ${response.code}")
            json.decodeFromString<List<GhRelease>>(text).firstOrNull { !it.draft }
        }
    }

    suspend fun download(url: String, dest: File, onProgress: (Int) -> Unit) =
        withContext(Dispatchers.IO) {
            val request = Request.Builder().url(url).build()
            http.newCall(request).execute().use { response ->
                if (!response.isSuccessful) throw LampException("HTTP ${response.code}")
                val body = response.body ?: throw LampException("пустой ответ")
                val total = body.contentLength()
                dest.parentFile?.mkdirs()
                body.byteStream().use { input ->
                    dest.outputStream().use { out ->
                        val buffer = ByteArray(64 * 1024)
                        var read = 0L
                        while (true) {
                            val n = input.read(buffer)
                            if (n < 0) break
                            out.write(buffer, 0, n)
                            read += n
                            if (total > 0) onProgress((read * 100 / total).toInt())
                        }
                    }
                }
            }
        }
}

fun isVersionNewer(candidate: String, current: String): Boolean {
    fun split(v: String): Pair<List<Int>, String> {
        val clean = v.trim().removePrefix("v")
        val dash = clean.indexOf('-')
        val base = if (dash >= 0) clean.substring(0, dash) else clean
        val pre = if (dash >= 0) clean.substring(dash + 1) else ""
        return base.split('.').map { it.takeWhile(Char::isDigit).toIntOrNull() ?: 0 } to pre
    }

    fun cmpPre(a: String, b: String): Int {
        val ra = Regex("^(\\D*)(\\d*)$").find(a)
        val rb = Regex("^(\\D*)(\\d*)$").find(b)
        val pa = ra?.groupValues?.get(1) ?: a
        val pb = rb?.groupValues?.get(1) ?: b
        if (pa != pb) return pa.compareTo(pb)
        val na = ra?.groupValues?.get(2)?.toIntOrNull() ?: 0
        val nb = rb?.groupValues?.get(2)?.toIntOrNull() ?: 0
        return na.compareTo(nb)
    }

    val (cb, cp) = split(candidate)
    val (nb, np) = split(current)
    for (i in 0 until maxOf(cb.size, nb.size)) {
        val a = cb.getOrElse(i) { 0 }
        val b = nb.getOrElse(i) { 0 }
        if (a != b) return a > b
    }
    if (cp == np) return false
    if (cp.isEmpty()) return false
    if (np.isEmpty()) return true
    return cmpPre(cp, np) > 0
}
