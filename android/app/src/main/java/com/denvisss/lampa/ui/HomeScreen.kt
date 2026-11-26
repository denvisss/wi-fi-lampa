package com.denvisss.lampa.ui

import android.graphics.Bitmap
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import com.denvisss.lampa.data.LampState
import java.util.Calendar
import kotlin.math.roundToInt

private val COLOR_PRESETS = listOf(
    "#FFB066", "#FF6B57", "#FF2E63", "#B16BFF",
    "#4FC3F7", "#59E3A7", "#FFE27A", "#FFFFFF",
)

@Composable
fun HomeScreen(vm: LampViewModel) {
    val s = vm.state
    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Spacer(Modifier.height(6.dp))
        Text(greeting(), style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        Text(
            "Управление лампой в вашей домашней сети",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (s == null) {
            SectionCard {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(22.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(12.dp))
                    Text("Подключение к лампе…", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        } else {
            HeroCard(s, vm)
        }
        PreviewCard(vm)
        QuickEffects(vm, s)
        ColorCard(vm, s)
        PlaylistCard(vm, s)
        Spacer(Modifier.height(78.dp))
    }
}

@Composable
private fun HeroCard(s: LampState, vm: LampViewModel) {
    val effectName = vm.effects.firstOrNull { it.id == s.effect }?.name ?: s.effect
    val scheme = MaterialTheme.colorScheme
    Surface(
        shape = RoundedCornerShape(28.dp),
        color = Color.Transparent,
        border = androidx.compose.foundation.BorderStroke(1.dp, scheme.outlineVariant),
    ) {
        Box(
            Modifier
                .fillMaxWidth()
                .background(
                    Brush.linearGradient(
                        listOf(
                            scheme.primaryContainer,
                            scheme.surface,
                        )
                    )
                )
                .padding(22.dp)
        ) {
            Column {
                Text(effectName, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                Spacer(Modifier.height(4.dp))
                Text(
                    "Сейчас: $effectName · яркость ${pct(s.brightness)}%",
                    style = MaterialTheme.typography.bodySmall,
                    color = scheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(18.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    PowerOrb(s.power, onClick = { vm.togglePower() })
                    Spacer(Modifier.width(16.dp))
                    Column {
                        Text(
                            if (s.power) "Включено" else "Выключено",
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.SemiBold,
                        )
                        Text(
                            if (s.power) "Нажмите, чтобы выключить" else "Нажмите, чтобы включить",
                            style = MaterialTheme.typography.bodySmall,
                            color = scheme.onSurfaceVariant,
                        )
                    }
                }
                Spacer(Modifier.height(16.dp))
                BrightnessSlider(s.brightness) { vm.setBrightness(it) }
            }
        }
    }
}

@Composable
private fun BrightnessSlider(serverValue: Int, onCommit: (Int) -> Unit) {
    var dragging by remember { mutableStateOf(false) }
    var local by remember { mutableFloatStateOf(serverValue.toFloat()) }
    LaunchedEffect(serverValue, dragging) {
        if (!dragging) local = serverValue.toFloat()
    }
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text("Яркость", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text("${pct(local.roundToInt())}%", style = MaterialTheme.typography.bodySmall, fontWeight = FontWeight.SemiBold)
    }
    Slider(
        value = local,
        onValueChange = {
            dragging = true
            local = it
        },
        onValueChangeFinished = {
            dragging = false
            onCommit(local.roundToInt())
        },
        valueRange = 1f..255f,
    )
}

@Composable
private fun PreviewCard(vm: LampViewModel) {
    val frame = vm.previewFrame
    val image: ImageBitmap? = remember(frame) { frame?.let { decodePreview(it) } }
    SectionCard(
        title = "Превью матрицы 16×16",
        subtitle = if (vm.livePreview) "живой кадр с лампы" else "включите LIVE, чтобы видеть лампу",
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier
                    .size(196.dp)
                    .clip(RoundedCornerShape(18.dp))
                    .background(Color(0xFF0A0808))
                    .padding(12.dp),
                contentAlignment = Alignment.Center,
            ) {
                if (image != null) {
                    Canvas(Modifier.fillMaxSize()) {
                        drawImage(
                            image = image,
                            srcOffset = IntOffset.Zero,
                            srcSize = IntSize(16, 16),
                            dstOffset = IntOffset.Zero,
                            dstSize = IntSize(size.width.roundToInt(), size.height.roundToInt()),
                            filterQuality = FilterQuality.None,
                        )
                    }
                } else {
                    Text(
                        if (vm.livePreview) "ожидаем кадр…" else "16×16",
                        style = MaterialTheme.typography.labelMedium,
                        color = Color(0xFF554D42),
                    )
                }
            }
            Spacer(Modifier.width(16.dp))
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        "LIVE",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(Modifier.width(8.dp))
                    Switch(checked = vm.livePreview, onCheckedChange = { vm.setPreview(it) })
                }
                Text(
                    if (vm.livePreview) "Матрица шлёт кадры в реальном времени"
                    else "Поток кадров с матрицы по Wi-Fi",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun QuickEffects(vm: LampViewModel, s: LampState?) {
    Column {
        Text("Эффекты", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.height(10.dp))
        LazyRow(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            items(vm.effects, key = { it.id }) { e ->
                val active = s?.effect == e.id
                Column(
                    Modifier
                        .width(92.dp)
                        .clickable { vm.setEffect(e.id) },
                ) {
                    EffectArt(
                        e.id,
                        Modifier
                            .fillMaxWidth()
                            .height(64.dp)
                            .clip(RoundedCornerShape(16.dp))
                            .then(
                                if (active) Modifier.border(
                                    2.dp,
                                    MaterialTheme.colorScheme.primary,
                                    RoundedCornerShape(16.dp)
                                ) else Modifier
                            ),
                    )
                    Spacer(Modifier.height(6.dp))
                    Text(
                        e.name,
                        style = MaterialTheme.typography.labelMedium,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        }
    }
}

@Composable
private fun ColorCard(vm: LampViewModel, s: LampState?) {
    SectionCard(title = "Цвет", subtitle = "акцент для цветовых эффектов") {
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            COLOR_PRESETS.forEach { hex ->
                val selected = s?.color.equals(hex, ignoreCase = true)
                Box(
                    Modifier
                        .size(34.dp)
                        .clip(CircleShape)
                        .background(parseColor(hex))
                        .then(
                            if (selected) Modifier.border(
                                3.dp,
                                MaterialTheme.colorScheme.primary,
                                CircleShape
                            ) else Modifier
                        )
                        .clickable { vm.setColor(hex) }
                )
            }
        }
    }
}

@Composable
private fun PlaylistCard(vm: LampViewModel, s: LampState?) {
    if (s == null) return
    SectionCard(title = "Плейлист", subtitle = "автосмена эффектов") {
        SwitchRow(
            title = "Включён",
            subtitle = "лампа сама меняет сцены",
            checked = s.playlist.enabled,
            onChange = { vm.setPlaylist(enabled = it) },
        )
        Spacer(Modifier.height(6.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(5, 15, 30, 60).forEach { minutes ->
                FilterChip(
                    selected = s.playlist.intervalMin == minutes,
                    onClick = { vm.setPlaylist(intervalMin = minutes) },
                    label = { Text("$minutes мин") },
                )
            }
        }
    }
}

private fun decodePreview(bytes: ByteArray): ImageBitmap {
    val pixels = IntArray(256)
    for (i in 0 until 256) {
        val r = bytes[i * 3].toInt() and 0xFF
        val g = bytes[i * 3 + 1].toInt() and 0xFF
        val b = bytes[i * 3 + 2].toInt() and 0xFF
        pixels[i] = (0xFF shl 24) or (r shl 16) or (g shl 8) or b
    }
    val bitmap = Bitmap.createBitmap(16, 16, Bitmap.Config.ARGB_8888)
    bitmap.setPixels(pixels, 0, 16, 0, 0, 16, 16)
    return bitmap.asImageBitmap()
}

fun parseColor(hex: String): Color = runCatching {
    Color(android.graphics.Color.parseColor(hex))
}.getOrDefault(Color(0xFFFFB066))

private fun greeting(): String {
    val hour = Calendar.getInstance().get(Calendar.HOUR_OF_DAY)
    return when {
        hour < 5 -> "Доброй ночи"
        hour < 12 -> "Доброе утро"
        hour < 18 -> "Добрый день"
        else -> "Добрый вечер"
    }
}
