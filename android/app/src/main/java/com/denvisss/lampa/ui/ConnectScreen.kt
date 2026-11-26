package com.denvisss.lampa.ui

import android.os.Handler
import android.os.Looper
import androidx.compose.foundation.background
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
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Wifi
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.denvisss.lampa.data.LampDiscovery

@Composable
fun ConnectScreen(vm: LampViewModel) {
    var input by rememberSaveable { mutableStateOf("lampa.local") }
    val found = remember { mutableStateListOf<String>() }
    val context = LocalContext.current
    val main = remember { Handler(Looper.getMainLooper()) }

    DisposableEffect(Unit) {
        val discovery = LampDiscovery(context) { ip ->
            main.post { if (!found.contains(ip)) found.add(ip) }
        }
        discovery.start()
        onDispose { discovery.stop() }
    }

    Column(
        Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 24.dp, vertical = 20.dp),
    ) {
        Spacer(Modifier.height(28.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier
                    .size(44.dp)
                    .clip(CircleShape)
                    .background(
                        Brush.radialGradient(
                            listOf(Color(0xFFFFE2C4), Color(0xFFFFA25E), Color(0xFFF2734D))
                        )
                    )
            )
            Spacer(Modifier.width(14.dp))
            Column {
                Text("Лампа", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                Text(
                    "управление светом",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Spacer(Modifier.height(36.dp))
        Text("Подключитесь к лампе", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(8.dp))
        Text(
            "Телефон и лампа должны быть в одной сети Wi-Fi. Обычно адрес — lampa.local, но можно ввести IP.",
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )

        Spacer(Modifier.height(24.dp))
        OutlinedTextField(
            value = input,
            onValueChange = { input = it },
            label = { Text("Адрес лампы") },
            placeholder = { Text("lampa.local или 192.168.1.42") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(14.dp))
        Button(
            onClick = { vm.connect(input) },
            enabled = input.isNotBlank(),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Icon(Icons.Rounded.Wifi, contentDescription = null, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
            Text("Подключиться")
        }

        if (found.isNotEmpty()) {
            Spacer(Modifier.height(22.dp))
            Text(
                "Найдено в сети",
                style = MaterialTheme.typography.titleSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(8.dp))
            found.forEach { ip ->
                AssistChip(
                    onClick = { input = ip; vm.connect(ip) },
                    label = { Text(ip) },
                    modifier = Modifier.padding(bottom = 4.dp),
                )
            }
        }

        Spacer(Modifier.height(28.dp))
        SectionCard(title = "Если лампа ещё не настроена") {
            Text(
                "1. Подключите телефон к сети Lampa-Setup.\n" +
                    "2. Откройте 192.168.4.1 и выберите домашний Wi-Fi.\n" +
                    "3. Вернитесь сюда и подключитесь к lampa.local.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Spacer(Modifier.height(24.dp))
    }
}
