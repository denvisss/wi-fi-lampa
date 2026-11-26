package com.denvisss.lampa.ui

import android.content.Intent
import android.net.Uri
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.material.icons.rounded.Upload
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import com.denvisss.lampa.data.DeviceInfo
import com.denvisss.lampa.data.LampState
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import kotlin.math.roundToInt

@Composable
fun SettingsScreen(vm: LampViewModel) {
    val s = vm.state
    var confirmFactory by remember { mutableStateOf(false) }

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp),
    ) {
        Spacer(Modifier.height(6.dp))
        Text("Настройки", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        Text(
            "Устройство, калибровка и обновления",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(Modifier.height(16.dp))

        if (s != null) {
            DeviceSection(s, vm.host)
            Spacer(Modifier.height(14.dp))
            CalibrationSection(vm, s)
            Spacer(Modifier.height(14.dp))
            PowerSection(vm, s)
            Spacer(Modifier.height(14.dp))
        }

        WifiSection(vm)
        Spacer(Modifier.height(14.dp))
        OtaSection(vm)
        Spacer(Modifier.height(14.dp))
        AppSection(vm)
        Spacer(Modifier.height(14.dp))

        SectionCard(title = "Сброс", subtitle = "Удалить настройки и вернуться к точке доступа") {
            OutlinedButton(
                onClick = { confirmFactory = true },
                modifier = Modifier.fillMaxWidth(),
                colors = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.error),
            ) {
                Text("Сбросить до заводских")
            }
        }

        Spacer(Modifier.height(18.dp))
        TextButton(
            onClick = { vm.disconnect() },
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Отключиться от лампы", color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Spacer(Modifier.height(88.dp))
    }

    if (confirmFactory) {
        AlertDialog(
            onDismissRequest = { confirmFactory = false },
            title = { Text("Сбросить лампу?") },
            text = { Text("Настройки, будильники и Wi-Fi будут удалены. Лампа поднимет точку доступа Lampa-Setup.") },
            confirmButton = {
                TextButton(onClick = {
                    confirmFactory = false
                    vm.factoryReset()
                }) {
                    Text("Сбросить", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { confirmFactory = false }) { Text("Отмена") }
            },
        )
    }
}

@Composable
private fun DeviceSection(s: LampState, host: String?) {
    val dev: DeviceInfo = s.device
    SectionCard(title = "Устройство", subtitle = host ?: "лампа") {
        KvRow("IP-адрес", dev.ip.ifBlank { "—" }, mono = true)
        KvRow("Wi-Fi", when {
            dev.ap -> "точка доступа"
            dev.rssi != 0 -> "${dev.rssi} dBm"
            else -> "—"
        })
        KvRow("Прошивка", if (dev.fw.isBlank()) "—" else "v${dev.fw}")
        KvRow("Свободно памяти", if (dev.heap > 0) "${dev.heap / 1024} КБ" else "—", mono = true)
        KvRow("Аптайм", fmtUptime(dev.uptime), mono = true)
        KvRow("Время лампы", dev.time.replace("T", " ").take(19).ifBlank { "нет синхронизации" }, mono = true)
        KvRow("MAC", dev.mac.ifBlank { "—" }, mono = true)
    }
}

@Composable
private fun KvRow(label: String, value: String, mono: Boolean = false) {
    Row(
        Modifier
            .fillMaxWidth()
            .padding(vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            label,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.weight(1f),
        )
        Text(
            value,
            style = MaterialTheme.typography.bodySmall,
            fontFamily = if (mono) FontFamily.Monospace else null,
            fontWeight = FontWeight.Medium,
        )
    }
}

@Composable
private fun CalibrationSection(vm: LampViewModel, s: LampState) {
    SectionCard(
        title = "Калибровка матрицы",
        subtitle = "Если картинка повёрнута или зеркальна — подберите ориентацию",
    ) {
        Row(
            Modifier.horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            OutlinedButton(onClick = { vm.calibrate("grid") }) { Text("Сетка") }
            OutlinedButton(onClick = { vm.calibrate("chase") }) { Text("Бегущая точка") }
            OutlinedButton(onClick = { vm.calibrate("off") }) { Text("Стоп") }
        }
        Spacer(Modifier.height(10.dp))
        SwitchRow(
            title = "Змейка",
            subtitle = "Нечётные ряды идут в обратную сторону",
            checked = s.mapping.serpentine,
            onChange = { vm.setMapping(serpentine = it) },
        )
        SwitchRow(
            title = "Зеркало",
            subtitle = "Обратное направление ленты",
            checked = s.mapping.reverse,
            onChange = { vm.setMapping(reverse = it) },
        )
        SwitchRow(
            title = "Переворот по высоте",
            subtitle = "Если огонь стекает вниз",
            checked = s.mapping.flipY,
            onChange = { vm.setMapping(flipY = it) },
        )
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("Сдвиг по кругу", style = MaterialTheme.typography.bodyLarge)
                Text(
                    "Поворот картинки вокруг трубки",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            IconButton(onClick = { vm.setMapping(offset = (s.mapping.offset + 15) % 16) }) {
                Icon(Icons.Rounded.Remove, contentDescription = "Сдвиг влево")
            }
            Text(
                "${s.mapping.offset}",
                style = MaterialTheme.typography.titleMedium,
                fontFamily = FontFamily.Monospace,
            )
            IconButton(onClick = { vm.setMapping(offset = (s.mapping.offset + 1) % 16) }) {
                Icon(Icons.Rounded.Add, contentDescription = "Сдвиг вправо")
            }
        }
    }
}

@Composable
private fun PowerSection(vm: LampViewModel, s: LampState) {
    var draft by remember(s.currentLimit) { mutableFloatStateOf(s.currentLimit.toFloat()) }
    SectionCard(
        title = "Питание",
        subtitle = "Ограничение тока блока питания",
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Лимит тока", style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
            Text(
                fmtAmps(draft.roundToInt()),
                style = MaterialTheme.typography.titleMedium,
                fontFamily = FontFamily.Monospace,
            )
        }
        Slider(
            value = draft,
            onValueChange = { draft = it },
            onValueChangeFinished = { vm.setCurrentLimit(draft.roundToInt()) },
            valueRange = 500f..15000f,
            steps = 57,
        )
        Text(
            "Если расчётный ток превысит лимит, лампа приглушит матрицу. Все 256 светодиодов на полной яркости — около 15.4 А.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

private fun fmtAmps(ma: Int): String = "${ma / 1000}.${(ma % 1000) / 100} А"

@Composable
private fun WifiSection(vm: LampViewModel) {
    var ssid by rememberSaveable { mutableStateOf("") }
    var pass by rememberSaveable { mutableStateOf("") }
    SectionCard(title = "Wi-Fi", subtitle = "Смена сети — лампа перезагрузится") {
        OutlinedTextField(
            value = ssid,
            onValueChange = { ssid = it },
            label = { Text("Имя сети") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = pass,
            onValueChange = { pass = it },
            label = { Text("Пароль") },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(12.dp))
        Button(
            onClick = { vm.saveWifi(ssid.trim(), pass) },
            enabled = ssid.isNotBlank(),
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Подключить") }
    }
}

@Composable
private fun OtaSection(vm: LampViewModel) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch {
            val bytes = withContext(Dispatchers.IO) {
                runCatching {
                    context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                }.getOrNull()
            }
            if (bytes == null || bytes.isEmpty()) {
                vm.flash("Не удалось прочитать файл")
            } else {
                vm.uploadFirmware(bytes)
            }
        }
    }

    SectionCard(title = "Прошивка", subtitle = "Обновление с GitHub или из файла — лампа перезагрузится") {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(
                    "Текущая версия",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    "v${vm.state?.device?.fw?.ifBlank { "—" } ?: "—"}",
                    style = MaterialTheme.typography.titleSmall,
                    fontFamily = FontFamily.Monospace,
                )
            }
            OutlinedButton(
                onClick = { vm.checkFirmwareUpdate() },
                enabled = !vm.fwChecking && !vm.otaBusy,
            ) {
                Text(if (vm.fwChecking) "Проверяем…" else "Проверить")
            }
        }

        val fw = vm.fwCheck
        if (fw != null) {
            Spacer(Modifier.height(10.dp))
            when {
                fw.error.isNotBlank() -> Text(
                    "Не удалось проверить: ${fw.error}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error,
                )
                fw.available -> {
                    Text(
                        "Доступна версия ${fw.latest}",
                        style = MaterialTheme.typography.bodyMedium,
                        fontWeight = FontWeight.SemiBold,
                    )
                    if (fw.notes.isNotBlank()) {
                        Spacer(Modifier.height(4.dp))
                        Text(
                            fw.notes.take(200).trim(),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Spacer(Modifier.height(10.dp))
                    Button(
                        onClick = { vm.installFirmwareUpdate() },
                        enabled = !vm.otaBusy,
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Обновить с GitHub") }
                }
                else -> Text(
                    "Установлена последняя версия",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        val phase = vm.otaPhase
        val statusText = when (phase) {
            "connecting" -> "Соединяемся с GitHub…"
            "downloading" -> "Скачиваем прошивку… ${vm.otaProgress}%"
            "finalizing" -> "Записываем прошивку…"
            "done" -> "Готово! Лампа перезагружается…"
            "error" -> vm.otaStatus ?: "Ошибка обновления"
            else -> vm.otaStatus
        }
        if (vm.otaBusy || vm.otaPhase != "idle" || vm.otaStatus != null) {
            Spacer(Modifier.height(12.dp))
            when {
                phase == "downloading" ->
                    LinearProgressIndicator(progress = { vm.otaProgress / 100f }, modifier = Modifier.fillMaxWidth())
                phase == "done" ->
                    LinearProgressIndicator(progress = { 1f }, modifier = Modifier.fillMaxWidth())
                phase == "error" -> Unit
                vm.otaBusy -> LinearProgressIndicator(Modifier.fillMaxWidth())
            }
            if (statusText != null) {
                Spacer(Modifier.height(8.dp))
                Text(
                    statusText,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Spacer(Modifier.height(12.dp))
        HorizontalDivider()
        Spacer(Modifier.height(12.dp))
        Text("Вручную", style = MaterialTheme.typography.bodyLarge)
        Text(
            "Скачайте firmware.bin и выберите файл",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(Modifier.height(8.dp))
        OutlinedButton(
            onClick = { picker.launch("*/*") },
            enabled = !vm.otaBusy,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Icon(Icons.Rounded.Upload, contentDescription = null, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
            Text(if (vm.otaBusy) "Идёт обновление…" else "Выбрать файл прошивки")
        }
    }
}

@Composable
private fun AppSection(vm: LampViewModel) {
    val context = LocalContext.current
    val apk = vm.appApkReady
    LaunchedEffect(apk) {
        if (apk != null) {
            val started = installApk(context, apk)
            vm.clearAppApk()
            if (!started) vm.flash("Разрешите установку приложений из этого источника")
        }
    }

    SectionCard(title = "Приложение", subtitle = "Обновление Android-клиента с GitHub") {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(
                    "Версия",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    "v${vm.appVersion}",
                    style = MaterialTheme.typography.titleSmall,
                    fontFamily = FontFamily.Monospace,
                )
            }
            OutlinedButton(
                onClick = { vm.checkAppUpdate() },
                enabled = !vm.appChecking && !vm.appDownloading,
            ) {
                Text(if (vm.appChecking) "Проверяем…" else "Проверить")
            }
        }

        val rel = vm.appRelease
        if (rel != null) {
            Spacer(Modifier.height(10.dp))
            Text(
                "Доступна версия ${rel.tag}",
                style = MaterialTheme.typography.bodyMedium,
                fontWeight = FontWeight.SemiBold,
            )
            if (rel.body.isNotBlank()) {
                Spacer(Modifier.height(4.dp))
                Text(
                    rel.body.take(200).trim(),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Spacer(Modifier.height(10.dp))
            if (vm.appDownloading) {
                LinearProgressIndicator(
                    progress = { vm.appProgress / 100f },
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    "Скачиваем APK… ${vm.appProgress}%",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            } else {
                Button(
                    onClick = { vm.downloadAppUpdate() },
                    modifier = Modifier.fillMaxWidth(),
                ) { Text("Скачать и установить") }
            }
        }
    }
}

private fun installApk(context: android.content.Context, file: File): Boolean {
    if (!context.packageManager.canRequestPackageInstalls()) {
        runCatching {
            context.startActivity(
                Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:${context.packageName}"))
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            )
        }
        return false
    }
    return runCatching {
        val uri = FileProvider.getUriForFile(context, "${context.packageName}.files", file)
        context.startActivity(
            Intent(Intent.ACTION_VIEW)
                .setDataAndType(uri, "application/vnd.android.package-archive")
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        )
    }.isSuccess
}

private fun fmtUptime(seconds: Long): String {
    if (seconds <= 0) return "—"
    val d = seconds / 86400
    val h = (seconds % 86400) / 3600
    val m = (seconds % 3600) / 60
    return when {
        d > 0 -> "$d д $h ч"
        h > 0 -> "$h ч $m мин"
        else -> "$m мин"
    }
}
