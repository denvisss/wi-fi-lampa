package com.denvisss.lampa.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.PowerSettingsNew
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.denvisss.lampa.data.SunriseInfo

fun pct(value: Int): Int = Math.round(value / 255f * 100f)

fun daysLabel(mask: Int): String {
    if (mask == 0) return "нет дней"
    if (mask == 0x7f) return "каждый день"
    if (mask == 0x1f) return "будни"
    if (mask == 0x60) return "выходные"
    val names = listOf("Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс")
    return names.filterIndexed { i, _ -> mask and (1 shl i) != 0 }.joinToString(", ")
}

fun effectBrush(id: String): Brush = when (id) {
    "solid" -> Brush.linearGradient(listOf(Color(0xFFE7A16B), Color(0xFFFFD6AE)))
    "breathing" -> Brush.radialGradient(listOf(Color(0xFFFFD3A8), Color(0xFFB97044)))
    "candle" -> Brush.verticalGradient(listOf(Color(0xFF3A1602), Color(0xFFB4440E), Color(0xFFFFC069)))
    "aurora" -> Brush.linearGradient(
        listOf(Color(0xFF0B2B30), Color(0xFF39D3C0), Color(0xFF7A6BFF), Color(0xFF0E1F33))
    )
    "ocean" -> Brush.verticalGradient(listOf(Color(0xFF06263B), Color(0xFF1177A8), Color(0xFF58C8D8)))
    "starfield" -> Brush.radialGradient(listOf(Color(0xFF233A5A), Color(0xFF060B18)))
    "rainbow" -> Brush.linearGradient(
        listOf(
            Color(0xFFFF6B57), Color(0xFFFFD166), Color(0xFF59E3A7),
            Color(0xFF4FC3F7), Color(0xFFB16BFF),
        )
    )
    "plasma" -> Brush.linearGradient(listOf(Color(0xFF5B2A86), Color(0xFF2E6BFF), Color(0xFFC05CFF)))
    "fire" -> Brush.verticalGradient(listOf(Color(0xFF1B0700), Color(0xFF8A2A00), Color(0xFFFF8A3C)))
    "snowfall" -> Brush.verticalGradient(listOf(Color(0xFF2A3C55), Color(0xFF6E8AA8)))
    "garland" -> Brush.linearGradient(
        listOf(Color(0xFF2B1B00), Color(0xFFC79A3A), Color(0xFF2B1B00), Color(0xFFC79A3A))
    )
    "spiral" -> Brush.linearGradient(listOf(Color(0xFF2B2100), Color(0xFFD6B24A), Color(0xFF2B2100)))
    "candy" -> Brush.linearGradient(listOf(Color(0xFFE8404F), Color(0xFFFFF3E4), Color(0xFFE8404F)))
    "fireworks" -> Brush.radialGradient(listOf(Color(0xFF6A3AA8), Color(0xFF0B0820)))
    "clock" -> Brush.linearGradient(listOf(Color(0xFF3A2A12), Color(0xFFA87B45)))
    "nycountdown" -> Brush.linearGradient(listOf(Color(0xFF0A1633), Color(0xFF274B8F)))
    "nyshow" -> Brush.linearGradient(listOf(Color(0xFF23113A), Color(0xFFC1863E), Color(0xFF4A1F6B)))
    else -> Brush.linearGradient(listOf(Color(0xFF3A332A), Color(0xFF6F6559)))
}

@Composable
fun SectionCard(
    modifier: Modifier = Modifier,
    title: String? = null,
    subtitle: String? = null,
    content: @Composable ColumnScope.() -> Unit,
) {
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(24.dp),
        color = MaterialTheme.colorScheme.surface,
        border = androidx.compose.foundation.BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
    ) {
        Column(Modifier.padding(20.dp)) {
            if (title != null) {
                Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            }
            if (subtitle != null) {
                Spacer(Modifier.height(2.dp))
                Text(
                    subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(14.dp))
            } else if (title != null) {
                Spacer(Modifier.height(12.dp))
            }
            content()
        }
    }
}

@Composable
fun SwitchRow(
    title: String,
    subtitle: String? = null,
    checked: Boolean,
    onChange: (Boolean) -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .clickable { onChange(!checked) }
            .padding(vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            if (subtitle != null) {
                Text(
                    subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        Spacer(Modifier.width(12.dp))
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

@Composable
fun PowerOrb(on: Boolean, onClick: () -> Unit, modifier: Modifier = Modifier) {
    val scheme = MaterialTheme.colorScheme
    val brush = if (on) {
        Brush.radialGradient(listOf(Color(0xFFFFE9D2), Color(0xFFFFA25E), Color(0xFFF2734D)))
    } else {
        Brush.radialGradient(listOf(scheme.surfaceVariant, scheme.surface))
    }
    Box(
        modifier
            .size(88.dp)
            .clip(CircleShape)
            .background(brush)
            .border(1.dp, if (on) Color.White.copy(alpha = 0.35f) else scheme.outline, CircleShape)
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            Icons.Rounded.PowerSettingsNew,
            contentDescription = if (on) "Выключить" else "Включить",
            tint = if (on) Color.White else scheme.onSurfaceVariant,
            modifier = Modifier.size(36.dp),
        )
    }
}

@Composable
fun StatusChip(connected: Boolean, modifier: Modifier = Modifier) {
    val color = if (connected) Mint else MaterialTheme.colorScheme.error
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(50),
        color = MaterialTheme.colorScheme.surfaceVariant,
    ) {
        Row(
            Modifier.padding(horizontal = 12.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Box(Modifier.size(7.dp).clip(CircleShape).background(color))
            Text(
                if (connected) "Онлайн" else "Нет связи",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
fun EffectArt(id: String, modifier: Modifier = Modifier, radius: Int = 18) {
    Box(
        modifier
            .clip(RoundedCornerShape(radius.dp))
            .background(effectBrush(id))
    )
}

@Composable
fun SunriseBanner(
    sunrise: SunriseInfo,
    onSnooze: () -> Unit,
    onStop: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.primaryContainer,
        modifier = modifier.fillMaxWidth(),
    ) {
        Column(Modifier.padding(horizontal = 18.dp, vertical = 14.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(
                        if (sunrise.snoozing) "Отсрочка" else "Рассвет",
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold,
                    )
                    Text(
                        if (sunrise.snoozing) "лампа снова разгорится через несколько минут"
                        else "плавное пробуждение",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.7f),
                    )
                }
                TextButton(onClick = onSnooze) { Text("+9 мин") }
                Button(
                    onClick = onStop,
                    colors = ButtonDefaults.buttonColors(
                        containerColor = MaterialTheme.colorScheme.onPrimaryContainer,
                        contentColor = MaterialTheme.colorScheme.primaryContainer,
                    ),
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 14.dp, vertical = 6.dp),
                ) { Text("Стоп") }
            }
            Spacer(Modifier.height(10.dp))
            LinearProgressIndicator(
                progress = { sunrise.progress.coerceIn(0f, 1f) },
                modifier = Modifier.fillMaxWidth().height(5.dp).clip(RoundedCornerShape(50)),
            )
        }
    }
}
