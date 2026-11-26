package com.denvisss.lampa

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Alarm
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Schedule
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.denvisss.lampa.ui.AlarmsScreen
import com.denvisss.lampa.ui.ClockScreen
import com.denvisss.lampa.ui.ConnectScreen
import com.denvisss.lampa.ui.EffectsScreen
import com.denvisss.lampa.ui.HomeScreen
import com.denvisss.lampa.ui.LampViewModel
import com.denvisss.lampa.ui.LampaTheme
import com.denvisss.lampa.ui.SettingsScreen
import com.denvisss.lampa.ui.StatusChip
import com.denvisss.lampa.ui.SunriseBanner
import com.denvisss.lampa.widget.LampWidget

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            LampaTheme {
                LampApp()
            }
        }
    }

    override fun onStop() {
        super.onStop()
        LampWidget.requestRefresh(this)
    }
}

@Composable
private fun LampApp(vm: LampViewModel = viewModel()) {
    val snackbar = remember { SnackbarHostState() }
    LaunchedEffect(vm.message) {
        vm.message?.let {
            snackbar.showSnackbar(it)
            vm.clearMessage()
        }
    }
    Box(
        Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
    ) {
        if (vm.host == null) {
            ConnectScreen(vm)
        } else {
            MainScaffold(vm)
        }
        SnackbarHost(
            snackbar,
            Modifier
                .align(Alignment.BottomCenter)
                .navigationBarsPadding()
                .padding(horizontal = 16.dp, vertical = if (vm.host != null) 84.dp else 16.dp),
        )
    }
}

private data class Tab(val label: String, val icon: ImageVector)

private val TABS = listOf(
    Tab("Главная", Icons.Rounded.Home),
    Tab("Эффекты", Icons.Rounded.AutoAwesome),
    Tab("Рассвет", Icons.Rounded.Alarm),
    Tab("Часы", Icons.Rounded.Schedule),
    Tab("Настройки", Icons.Rounded.Settings),
)

@Composable
private fun MainScaffold(vm: LampViewModel) {
    var tab by rememberSaveable { mutableIntStateOf(0) }
    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        bottomBar = {
            NavigationBar {
                TABS.forEachIndexed { i, item ->
                    NavigationBarItem(
                        selected = tab == i,
                        onClick = { tab = i },
                        icon = { Icon(item.icon, contentDescription = null) },
                        label = { Text(item.label) },
                    )
                }
            }
        },
    ) { pad ->
        Column(
            Modifier
                .fillMaxSize()
                .padding(pad)
        ) {
            TopBar(vm)
            val sunrise = vm.state?.sunrise
            if (sunrise != null && sunrise.active) {
                SunriseBanner(
                    sunrise,
                    onSnooze = { vm.sunriseSnooze() },
                    onStop = { vm.sunriseStop() },
                    modifier = Modifier.padding(horizontal = 16.dp),
                )
                Spacer(Modifier.height(10.dp))
            }
            Box(Modifier.weight(1f)) {
                when (tab) {
                    0 -> HomeScreen(vm)
                    1 -> EffectsScreen(vm)
                    2 -> AlarmsScreen(vm)
                    3 -> ClockScreen(vm)
                    else -> SettingsScreen(vm)
                }
            }
        }
    }
}

@Composable
private fun TopBar(vm: LampViewModel) {
    Row(
        Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier
                .size(16.dp)
                .clip(CircleShape)
                .background(
                    Brush.radialGradient(
                        listOf(Color(0xFFFFE2C4), Color(0xFFFFA25E), Color(0xFFF2734D))
                    )
                )
        )
        Spacer(Modifier.width(8.dp))
        Text("Лампа", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
        Spacer(Modifier.weight(1f))
        if (!vm.connected) {
            TextButton(onClick = { vm.retry() }, contentPadding = PaddingValues(horizontal = 10.dp)) {
                Text("Повторить")
            }
        }
        StatusChip(vm.connected)
    }
}
