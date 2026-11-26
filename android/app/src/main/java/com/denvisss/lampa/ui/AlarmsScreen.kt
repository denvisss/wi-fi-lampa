package com.denvisss.lampa.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TimePicker
import androidx.compose.material3.rememberTimePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import com.denvisss.lampa.data.AlarmCfg
import com.denvisss.lampa.data.LampState
import kotlin.math.roundToInt

private val WEEK = listOf("Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс")
private val SNOOZE_OPTIONS = listOf(0, 5, 9, 10, 15)
private val AUTO_OFF_OPTIONS = listOf(0, 15, 30, 45, 60, 90)

private fun two(n: Int) = n.toString().padStart(2, '0')

@Composable
fun AlarmsScreen(vm: LampViewModel) {
    val s = vm.state
    var editor by remember { mutableStateOf<Int?>(null) }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp),
    ) {
        Spacer(Modifier.height(6.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("Рассвет", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                Text(
                    "Плавное пробуждение от тёплого огонька",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Button(onClick = { editor = -1 }, enabled = (s?.alarms?.size ?: 0) < 4) {
                Icon(Icons.Rounded.Add, contentDescription = null, modifier = Modifier.size(18.dp))
                Spacer(Modifier.width(6.dp))
                Text("Добавить")
            }
        }
        Spacer(Modifier.height(16.dp))

        when {
            s == null -> SectionCard {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(22.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(12.dp))
                    Text("Подключение к лампе…", color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            s.alarms.isEmpty() -> SectionCard {
                Text("Пока нет будильников", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
                Spacer(Modifier.height(4.dp))
                Text(
                    "Добавьте первый — и утро начнётся с мягкого света. До четырёх будильников.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(Modifier.height(12.dp))
                OutlinedButton(onClick = { editor = -1 }) { Text("Создать будильник") }
            }
            else -> s.alarms.forEachIndexed { index, alarm ->
                AlarmCard(
                    alarm = alarm,
                    onToggle = { on ->
                        vm.saveAlarms(s.alarms.toMutableList().also { it[index] = alarm.copy(enabled = on) })
                    },
                    onEdit = { editor = index },
                )
                Spacer(Modifier.height(10.dp))
            }
        }

        Spacer(Modifier.height(6.dp))
        SectionCard(title = "Проверка", subtitle = "Рассвет на 45 секунд, затем настройки вернутся") {
            OutlinedButton(onClick = { vm.sunriseTest() }, modifier = Modifier.fillMaxWidth()) {
                Text("Запустить тест рассвета")
            }
        }
        Spacer(Modifier.height(8.dp))
        Text(
            "Тап по сенсору во время рассвета откладывает его, удержание — выключает.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(Modifier.height(88.dp))
    }

    val idx = editor
    if (s != null && idx != null) {
        val initial = if (idx >= 0 && idx < s.alarms.size) s.alarms[idx] else null
        key(idx) {
            AlarmEditor(
                initial = initial,
                onDismiss = { editor = null },
                onSave = { updated ->
                    val list = s.alarms.toMutableList()
                    if (initial != null) list[idx] = updated else list.add(updated)
                    vm.saveAlarms(list)
                    editor = null
                },
                onDelete = if (initial != null) {
                    {
                        val list = s.alarms.toMutableList().also { it.removeAt(idx) }
                        vm.saveAlarms(list)
                        editor = null
                    }
                } else null,
                onTest = { vm.sunriseTest() },
            )
        }
    }
}

@Composable
private fun AlarmCard(alarm: AlarmCfg, onToggle: (Boolean) -> Unit, onEdit: () -> Unit) {
    Surface(
        shape = RoundedCornerShape(24.dp),
        color = MaterialTheme.colorScheme.surface,
        border = androidx.compose.foundation.BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onEdit),
    ) {
        Row(Modifier.padding(horizontal = 20.dp, vertical = 16.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.Bottom) {
                    Text(
                        "${two(alarm.hour)}:${two(alarm.minute)}",
                        style = MaterialTheme.typography.headlineMedium,
                        fontWeight = FontWeight.Bold,
                    )
                    Spacer(Modifier.width(10.dp))
                    Text(
                        if (alarm.enabled) "активен" else "выключен",
                        style = MaterialTheme.typography.labelMedium,
                        color = if (alarm.enabled) MaterialTheme.colorScheme.secondary
                        else MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(bottom = 5.dp),
                    )
                }
                Spacer(Modifier.height(4.dp))
                DaysDots(alarm.days, dotSize = 26)
                Spacer(Modifier.height(6.dp))
                Text(
                    "${daysLabel(alarm.days)} · ${alarm.durationMin} мин · яркость ${pct(alarm.maxBrightness)}%",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Switch(checked = alarm.enabled, onCheckedChange = onToggle)
            IconButton(onClick = onEdit) {
                Icon(
                    Icons.Rounded.Edit,
                    contentDescription = "Изменить",
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(19.dp),
                )
            }
        }
    }
}

@Composable
private fun DaysDots(mask: Int, dotSize: Int) {
    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        WEEK.forEachIndexed { i, label ->
            val on = mask and (1 shl i) != 0
            Box(
                Modifier
                    .size(dotSize.dp)
                    .clip(CircleShape)
                    .background(if (on) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    label,
                    style = MaterialTheme.typography.labelSmall,
                    color = if (on) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun AlarmEditor(
    initial: AlarmCfg?,
    onDismiss: () -> Unit,
    onSave: (AlarmCfg) -> Unit,
    onDelete: (() -> Unit)?,
    onTest: () -> Unit,
) {
    var draft by remember { mutableStateOf(initial ?: AlarmCfg()) }
    var confirmDelete by remember { mutableStateOf(false) }
    val picker = rememberTimePickerState(
        initialHour = draft.hour,
        initialMinute = draft.minute,
        is24Hour = true,
    )

    Dialog(onDismissRequest = onDismiss) {
        Surface(
            shape = RoundedCornerShape(28.dp),
            color = MaterialTheme.colorScheme.surface,
            border = androidx.compose.foundation.BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(max = 640.dp),
        ) {
                Column(
                    Modifier
                        .verticalScroll(rememberScrollState())
                        .padding(horizontal = 16.dp, vertical = 18.dp),
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            if (initial == null) "Новый будильник" else "Будильник",
                            style = MaterialTheme.typography.titleLarge,
                            fontWeight = FontWeight.Bold,
                            modifier = Modifier.weight(1f),
                        )
                        Switch(
                            checked = draft.enabled,
                            onCheckedChange = { draft = draft.copy(enabled = it) },
                        )
                    }
                    Spacer(Modifier.height(8.dp))
                    Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                        TimePicker(state = picker)
                    }
                    Spacer(Modifier.height(4.dp))
                    FieldLabel("Дни недели")
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                        WEEK.forEachIndexed { i, label ->
                            val on = draft.days and (1 shl i) != 0
                            Box(
                                Modifier
                                    .weight(1f)
                                    .aspectRatio(1f)
                                    .clip(CircleShape)
                                    .background(
                                        if (on) MaterialTheme.colorScheme.primary
                                        else MaterialTheme.colorScheme.surfaceVariant
                                    )
                                    .clickable {
                                        val bit = 1 shl i
                                        draft = draft.copy(days = draft.days xor bit)
                                    },
                                contentAlignment = Alignment.Center,
                            ) {
                                Text(
                                    label,
                                    style = MaterialTheme.typography.labelMedium,
                                    color = if (on) MaterialTheme.colorScheme.onPrimary
                                    else MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                    Spacer(Modifier.height(16.dp))
                    Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                        FieldLabel("Длительность")
                        Text(
                            "${draft.durationMin} мин",
                            style = MaterialTheme.typography.labelLarge,
                            fontWeight = FontWeight.SemiBold,
                        )
                    }
                    Slider(
                        value = draft.durationMin.toFloat(),
                        onValueChange = { draft = draft.copy(durationMin = it.roundToInt()) },
                        valueRange = 5f..30f,
                        steps = 24,
                    )
                    Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                        FieldLabel("Макс. яркость")
                        Text(
                            "${pct(draft.maxBrightness)}%",
                            style = MaterialTheme.typography.labelLarge,
                            fontWeight = FontWeight.SemiBold,
                        )
                    }
                    Slider(
                        value = draft.maxBrightness.toFloat(),
                        onValueChange = { draft = draft.copy(maxBrightness = it.roundToInt()) },
                        valueRange = 5f..255f,
                    )
                    Spacer(Modifier.height(10.dp))
                    FieldLabel("Отсрочка")
                    Row(
                        Modifier.horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        SNOOZE_OPTIONS.forEach { m ->
                            FilterChip(
                                selected = draft.snoozeMin == m,
                                onClick = { draft = draft.copy(snoozeMin = m) },
                                label = { Text(if (m == 0) "Выкл" else "$m мин") },
                            )
                        }
                    }
                    Spacer(Modifier.height(10.dp))
                    FieldLabel("Авто-выключение")
                    Row(
                        Modifier.horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        AUTO_OFF_OPTIONS.forEach { m ->
                            FilterChip(
                                selected = draft.afterOffMin == m,
                                onClick = { draft = draft.copy(afterOffMin = m) },
                                label = { Text(if (m == 0) "Не выкл." else "$m мин") },
                            )
                        }
                    }
                    Spacer(Modifier.height(18.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        if (onDelete != null) {
                            TextButton(onClick = { confirmDelete = true }) {
                                Text("Удалить", color = MaterialTheme.colorScheme.error)
                            }
                        } else {
                            TextButton(onClick = onTest) { Text("Тест рассвета") }
                        }
                        Spacer(Modifier.weight(1f))
                        TextButton(onClick = onDismiss) { Text("Отмена") }
                        Spacer(Modifier.width(6.dp))
                        Button(
                            onClick = {
                                onSave(
                                    draft.copy(
                                        hour = picker.hour,
                                        minute = picker.minute,
                                        days = draft.days and 0x7f,
                                    )
                                )
                            },
                            contentPadding = PaddingValues(horizontal = 22.dp, vertical = 10.dp),
                        ) { Text("Сохранить") }
                    }
                }
            }
    }

    if (confirmDelete && onDelete != null) {
        AlertDialog(
            onDismissRequest = { confirmDelete = false },
            title = { Text("Удалить будильник?") },
            text = { Text("Расписание ${two(draft.hour)}:${two(draft.minute)} будет удалено.") },
            confirmButton = {
                TextButton(onClick = { confirmDelete = false; onDelete() }) {
                    Text("Удалить", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { confirmDelete = false }) { Text("Отмена") }
            },
        )
    }
}

@Composable
private fun FieldLabel(text: String) {
    Text(
        text,
        style = MaterialTheme.typography.labelLarge,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )
}
