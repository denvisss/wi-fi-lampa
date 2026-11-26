package com.denvisss.lampa.tile

import android.content.Intent
import android.graphics.drawable.Icon
import android.os.Build
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import com.denvisss.lampa.MainActivity
import com.denvisss.lampa.R
import com.denvisss.lampa.data.LampClient
import com.denvisss.lampa.data.Prefs
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put

class LampTileService : TileService() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    override fun onStartListening() {
        super.onStartListening()
        val host = Prefs(this).host
        if (host == null) {
            applyState(null, offline = false)
            return
        }
        qsTile?.apply {
            state = Tile.STATE_INACTIVE
            label = "Wi-Fi Лампа"
            updateTile()
        }
        scope.launch {
            val state = runCatching { LampClient(host).fetchState() }.getOrNull()
            withContext(Dispatchers.Main) {
                if (state == null) applyState(null, offline = true) else applyState(state.power, offline = false)
            }
        }
    }

    override fun onClick() {
        super.onClick()
        val host = Prefs(this).host
        if (host == null) {
            openApp()
            return
        }
        scope.launch {
            val result = runCatching {
                val client = LampClient(host)
                val current = client.fetchState()
                val next = client.postPatch(buildJsonObject { put("power", !current.power) })
                next.power
            }
            withContext(Dispatchers.Main) {
                applyState(result.getOrNull(), offline = result.isFailure)
            }
        }
    }

    private fun applyState(power: Boolean?, offline: Boolean) {
        val tile = qsTile ?: return
        tile.state = when {
            offline -> Tile.STATE_UNAVAILABLE
            power == true -> Tile.STATE_ACTIVE
            power == false -> Tile.STATE_INACTIVE
            else -> Tile.STATE_UNAVAILABLE
        }
        tile.label = "Wi-Fi Лампа"
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            tile.subtitle = when {
                offline -> "нет связи"
                power == true -> "включена"
                power == false -> "выключена"
                else -> "не настроена"
            }
        }
        tile.icon = Icon.createWithResource(this, R.drawable.ic_tile)
        tile.updateTile()
    }

    @Suppress("DEPRECATION")
    private fun openApp() {
        val intent = Intent(this, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        startActivityAndCollapse(intent)
    }
}
