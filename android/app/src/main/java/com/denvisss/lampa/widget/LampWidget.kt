package com.denvisss.lampa.widget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.widget.RemoteViews
import com.denvisss.lampa.MainActivity
import com.denvisss.lampa.R
import com.denvisss.lampa.data.LampClient
import com.denvisss.lampa.data.LampState
import com.denvisss.lampa.data.Prefs
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put

class LampWidget : AppWidgetProvider() {

    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) {
        ids.forEach { id -> render(context, manager, id, null, offline = false) }
        refreshAsync(context)
    }

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == ACTION_TOGGLE) {
            val pending = goAsync()
            val app = context.applicationContext
            scope.launch {
                val host = Prefs(app).host
                if (host != null) {
                    runCatching {
                        val client = LampClient(host)
                        val s = client.fetchState()
                        client.postPatch(buildJsonObject { put("power", !s.power) })
                    }
                }
                refreshInternal(app)
                pending.finish()
            }
            return
        }
        super.onReceive(context, intent)
    }

    companion object {
        const val ACTION_TOGGLE = "com.denvisss.lampa.widget.TOGGLE"

        private val EFFECT_NAMES = mapOf(
            "solid" to "Сплошной цвет",
            "candle" to "Свеча",
            "breathing" to "Дыхание",
            "aurora" to "Северное сияние",
            "ocean" to "Океан",
            "starfield" to "Звёздное небо",
            "rainbow" to "Радуга",
            "plasma" to "Плазма",
            "fire" to "Камин",
            "snowfall" to "Снегопад",
            "garland" to "Гирлянда",
            "spiral" to "Ёлочная спираль",
            "candy" to "Карамель",
            "fireworks" to "Салют",
            "clock" to "Часы",
            "nycountdown" to "До Нового года",
            "nyshow" to "Новогоднее шоу",
        )

        private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

        fun requestRefresh(context: Context) {
            refreshAsync(context.applicationContext)
        }

        private fun refreshAsync(context: Context) {
            scope.launch { refreshInternal(context) }
        }

        private suspend fun refreshInternal(context: Context) {
            val manager = AppWidgetManager.getInstance(context)
            val ids = manager.getAppWidgetIds(ComponentName(context, LampWidget::class.java))
            if (ids.isEmpty()) return
            val host = Prefs(context).host
            val state = if (host == null) null else runCatching { LampClient(host).fetchState() }.getOrNull()
            ids.forEach { id -> render(context, manager, id, state, offline = host != null && state == null) }
        }

        private fun render(
            context: Context,
            manager: AppWidgetManager,
            id: Int,
            state: LampState?,
            offline: Boolean,
        ) {
            val views = RemoteViews(context.packageName, R.layout.widget_lamp)
            val openIntent = PendingIntent.getActivity(
                context,
                0,
                Intent(context, MainActivity::class.java).apply {
                    flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
                },
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
            )
            views.setOnClickPendingIntent(R.id.widget_root, openIntent)
            views.setOnClickPendingIntent(R.id.widget_open, openIntent)

            if (state != null) {
                views.setTextViewText(R.id.widget_status, if (state.power) "включена" else "выключена")
                val name = EFFECT_NAMES[state.effect] ?: state.effect
                views.setTextViewText(R.id.widget_effect, "$name · ${Math.round(state.brightness / 255f * 100f)}%")
                views.setTextViewText(R.id.widget_power, if (state.power) "Выключить" else "Включить")
                views.setOnClickPendingIntent(R.id.widget_power, toggleIntent(context))
            } else {
                views.setTextViewText(R.id.widget_status, if (offline) "нет связи" else "не настроена")
                views.setTextViewText(
                    R.id.widget_effect,
                    if (offline) "Лампа не отвечает в этой сети" else "Откройте приложение и подключитесь",
                )
                views.setTextViewText(R.id.widget_power, if (offline) "Обновить" else "Открыть")
                views.setOnClickPendingIntent(
                    R.id.widget_power,
                    if (offline) toggleIntent(context) else openIntent,
                )
            }
            manager.updateAppWidget(id, views)
        }

        private fun toggleIntent(context: Context): PendingIntent = PendingIntent.getBroadcast(
            context,
            1,
            Intent(context, LampWidget::class.java).setAction(ACTION_TOGGLE),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
    }
}
