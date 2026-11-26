package com.denvisss.lampa.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Celebration
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DatePicker
import androidx.compose.material3.DatePickerDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TimePicker
import androidx.compose.material3.rememberDatePickerState
import androidx.compose.material3.rememberTimePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.denvisss.lampa.data.LampState
import java.time.Instant
import java.time.LocalDate
import java.time.LocalDateTime
import java.time.LocalTime
import java.time.ZoneId
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter
import kotlin.math.roundToInt

@Composable
fun ClockScreen(vm: LampViewModel) {
    val s = vm.state
    var showTz by remember { mutableStateOf(false) }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp),
    ) {
        Spacer(Modifier.height(6.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("Часы", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                Text(
                    "Время бегущей строкой вокруг цилиндра",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Button(onClick = { vm.setEffect("clock") }) { Text("Показать") }
        }
        Spacer(Modifier.height(16.dp))

        ClockFace(s)

        Spacer(Modifier.height(14.dp))
        if (s != null) {
            TimeCard(vm, s)

            Spacer(Modifier.height(14.dp))
            TimeZoneCard(s) {
                vm.loadTimezones()
                showTz = true
            }

            Spacer(Modifier.height(14.dp))
            SectionCard(title = "Настройки строки") {
                SwitchRow(
                    title = "Показывать дату",
                    subtitle = "День недели и число после времени",
                    checked = s.clock.showDate,
                    onChange = { vm.setClock(showDate = it) },
                )
                SwitchRow(
                    title = "12-часовой формат",
                    subtitle = "Вместо 24-часового",
                    checked = s.clock.hour12,
                    onChange = { vm.setClock(hour12 = it) },
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Цвет строки",
                    style = MaterialTheme.typography.bodyLarge,
                )
                Text(
                    "Радуга переливается по буквам",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(8.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(
                        selected = s.clock.colorMode == 0,
                        onClick = { vm.setClock(colorMode = 0) },
                        label = { Text("Выбранный") },
                    )
                    FilterChip(
                        selected = s.clock.colorMode == 1,
                        onClick = { vm.setClock(colorMode = 1) },
                        label = { Text("Радуга") },
                    )
                }
                Spacer(Modifier.height(12.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text(
                        "Скорость",
                        style = MaterialTheme.typography.bodyLarge,
                    )
                    Text(
                        "${s.clock.speed}",
                        style = MaterialTheme.typography.labelLarge,
                        fontWeight = FontWeight.SemiBold,
                    )
                }
                ClockSpeedSlider(s.clock.speed) { vm.setClock(speed = it) }
            }

            Spacer(Modifier.height(14.dp))
            NewYearCard(s, onShow = { vm.nyShow() }, onCountdown = { vm.setEffect("nycountdown") })
        }
        Spacer(Modifier.height(88.dp))
    }

    if (showTz && s != null) {
        TimeZonePickerDialog(
            current = s.tz,
            zones = vm.timezones,
            loading = vm.tzLoading,
            onDismiss = { showTz = false },
            onPick = {
                vm.setTz(it)
                showTz = false
                vm.flash("Часовой пояс: ${it.replace('_', ' ')}")
            },
        )
    }
}

@Composable
private fun ClockFace(s: LampState?) {
    val iso = s?.device?.time.orEmpty()
    val time = if (iso.length >= 16) iso.substring(11, 16) else "--:--"
    val date = if (iso.length >= 10) {
        val p = iso.substring(0, 10).split("-")
        if (p.size == 3) "${p[2]}.${p[1]}.${p[0]}" else iso.substring(0, 10)
    } else ""

    Surface(
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        border = androidx.compose.foundation.BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Box(
            Modifier
                .background(
                    Brush.radialGradient(
                        listOf(MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.55f), Color.Transparent)
                    )
                )
                .padding(vertical = 28.dp, horizontal = 20.dp),
            contentAlignment = Alignment.Center,
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(
                    "ВРЕМЯ ЛАМПЫ",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    time,
                    style = MaterialTheme.typography.displayLarge,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center,
                )
                Text(
                    if (date.isNotEmpty()) date else "ожидаем синхронизацию времени",
                    style = MaterialTheme.typography.titleSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (s != null) {
                    Spacer(Modifier.height(6.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            "пояс ${s.tz}",
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        if (s.manualTime) {
                            Spacer(Modifier.width(8.dp))
                            Surface(
                                shape = RoundedCornerShape(50),
                                color = MaterialTheme.colorScheme.tertiaryContainer,
                            ) {
                                Text(
                                    "вручную",
                                    style = MaterialTheme.typography.labelSmall,
                                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp),
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TimeCard(vm: LampViewModel, s: LampState) {
    val zone = remember(s.tz) { zoneOf(s.tz) }
    val init = remember { runCatching { LocalDateTime.parse(s.device.time.take(19)) }.getOrNull() }
    var pickedDate by remember { mutableStateOf(init?.toLocalDate() ?: LocalDate.now(zone)) }
    var pickedTime by remember {
        mutableStateOf(
            init?.toLocalTime()?.withSecond(0)?.withNano(0) ?: LocalTime.now(zone).withSecond(0).withNano(0)
        )
    }
    var showDate by remember { mutableStateOf(false) }
    var showTime by remember { mutableStateOf(false) }

    SectionCard(
        title = "Установка времени",
        subtitle = "Если нет доступа к интернету или время сбилось",
    ) {
        if (s.manualTime) {
            Text(
                "Время идёт вручную. NTP выключен до возврата автосинхронизации.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(8.dp))
            TextButton(onClick = { vm.useAutoTime() }) {
                Text("Вернуть автосинхронизацию (NTP)")
            }
        } else {
            Text(
                "Сейчас время синхронизируется по сети. Ручная установка выключит NTP.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(10.dp))
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(
                onClick = { showDate = true },
                modifier = Modifier.weight(1f),
            ) {
                Text(pickedDate.format(DateTimeFormatter.ofPattern("dd.MM.yyyy")))
            }
            OutlinedButton(
                onClick = { showTime = true },
                modifier = Modifier.weight(1f),
            ) {
                Text(pickedTime.format(DateTimeFormatter.ofPattern("HH:mm")))
            }
        }
        Spacer(Modifier.height(10.dp))
        Button(
            onClick = {
                val epoch = LocalDateTime.of(pickedDate, pickedTime).atZone(zone).toEpochSecond()
                vm.setManualTime(epoch)
            },
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Установить на лампе") }
        Spacer(Modifier.height(6.dp))
        Text(
            "Будет применено в поясе ${s.tz.replace('_', ' ')}",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }

    if (showDate) {
        val dp = rememberDatePickerState(
            initialSelectedDateMillis = pickedDate.atStartOfDay(ZoneOffset.UTC).toInstant().toEpochMilli()
        )
        DatePickerDialog(
            onDismissRequest = { showDate = false },
            confirmButton = {
                TextButton(onClick = {
                    dp.selectedDateMillis?.let {
                        pickedDate = Instant.ofEpochMilli(it).atZone(ZoneOffset.UTC).toLocalDate()
                    }
                    showDate = false
                }) { Text("Готово") }
            },
            dismissButton = {
                TextButton(onClick = { showDate = false }) { Text("Отмена") }
            },
        ) {
            DatePicker(state = dp, showModeToggle = false)
        }
    }

    if (showTime) {
        val tp = rememberTimePickerState(
            initialHour = pickedTime.hour,
            initialMinute = pickedTime.minute,
            is24Hour = true,
        )
        AlertDialog(
            onDismissRequest = { showTime = false },
            title = { Text("Время") },
            text = { TimePicker(state = tp) },
            confirmButton = {
                TextButton(onClick = {
                    pickedTime = LocalTime.of(tp.hour, tp.minute)
                    showTime = false
                }) { Text("Готово") }
            },
            dismissButton = {
                TextButton(onClick = { showTime = false }) { Text("Отмена") }
            },
        )
    }
}

@Composable
private fun TimeZoneCard(s: LampState, onOpen: () -> Unit) {
    SectionCard(title = "Часовой пояс", subtitle = "Влияет на часы, будильники и рассвет") {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(
                    s.tz.replace('_', ' '),
                    style = MaterialTheme.typography.titleSmall,
                    fontFamily = FontFamily.Monospace,
                )
                Text(
                    "Сейчас на лампе " + s.device.time.substringAfter("T").take(5).ifBlank { "—" },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            OutlinedButton(onClick = onOpen) { Text("Изменить") }
        }
    }
}

@Composable
private fun TimeZonePickerDialog(
    current: String,
    zones: List<String>,
    loading: Boolean,
    onDismiss: () -> Unit,
    onPick: (String) -> Unit,
) {
    var query by remember { mutableStateOf("") }
    val filtered = remember(zones, query) {
        val q = query.trim()
        if (q.isEmpty()) zones
        else zones.filter { it.contains(q.replace(' ', '_'), ignoreCase = true) }
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Часовой пояс") },
        text = {
            Column {
                OutlinedTextField(
                    value = query,
                    onValueChange = { query = it },
                    placeholder = { Text("Поиск, например Moscow") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                when {
                    loading && zones.isEmpty() -> {
                        LinearProgressIndicator(Modifier.fillMaxWidth())
                        Spacer(Modifier.height(8.dp))
                        Text(
                            "Загружаем список с лампы…",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    zones.isEmpty() -> Text(
                        "Список недоступен — нет связи с лампой",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    filtered.isEmpty() -> Text(
                        "Ничего не найдено",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    else -> LazyColumn(Modifier.heightIn(max = 360.dp)) {
                        items(filtered, key = { it }) { zone ->
                            val active = zone == current
                            Row(
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { onPick(zone) }
                                    .padding(vertical = 10.dp),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                Text(
                                    zone.replace('_', ' '),
                                    style = MaterialTheme.typography.bodyMedium,
                                    fontWeight = if (active) FontWeight.SemiBold else null,
                                    modifier = Modifier.weight(1f),
                                )
                                if (active) {
                                    Text(
                                        "сейчас",
                                        style = MaterialTheme.typography.labelSmall,
                                        color = MaterialTheme.colorScheme.primary,
                                    )
                                }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Закрыть") }
        },
    )
}

private fun zoneOf(tz: String): ZoneId {
    runCatching { return ZoneId.of(tz) }
    val hours = Regex("([+-]?\\d{1,2})").find(tz)?.groupValues?.get(1)?.toIntOrNull()
    if (hours != null && hours in -14..14) return ZoneOffset.ofHours(-hours)
    return ZoneId.systemDefault()
}

@Composable
private fun ClockSpeedSlider(serverValue: Int, onCommit: (Int) -> Unit) {
    var dragging by remember { mutableStateOf(false) }
    var local by remember { mutableFloatStateOf(serverValue.toFloat()) }
    LaunchedEffect(serverValue, dragging) {
        if (!dragging) local = serverValue.toFloat()
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
        valueRange = 10f..150f,
    )
}

@Composable
private fun NewYearCard(s: LampState, onShow: () -> Unit, onCountdown: () -> Unit) {
    val days = s.ny.days
    Surface(
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        border = androidx.compose.foundation.BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(Modifier.padding(20.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    Icons.Rounded.Celebration,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.primary,
                )
                Spacer(Modifier.width(10.dp))
                Text(
                    "Новогоднее шоу",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                )
            }
            Spacer(Modifier.height(6.dp))
            Text(
                if (days > 0) "До Нового года ${days} ${pluralDays(days)}. Салют включится сам в полночь."
                else if (days == 0) "Новый год уже сегодня! Салют включится в полночь."
                else "Салют, поздравление и обратный отсчёт — в полночь 1 января.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(14.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Button(onClick = onShow) { Text("Запустить сейчас") }
                OutlinedButton(onClick = onCountdown) { Text("До Нового года") }
            }
        }
    }
}

private fun pluralDays(n: Int): String {
    val d = n % 10
    val dd = n % 100
    return when {
        dd in 11..14 -> "дней"
        d == 1 -> "день"
        d in 2..4 -> "дня"
        else -> "дней"
    }
}
