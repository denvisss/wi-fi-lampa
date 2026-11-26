package com.denvisss.lampa.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val Warm = Color(0xFFFFB066)
val Mint = Color(0xFF59E3A7)

private val DarkColors = darkColorScheme(
    primary = Warm,
    onPrimary = Color(0xFF2A1400),
    primaryContainer = Color(0xFF3A2410),
    onPrimaryContainer = Color(0xFFFFDCC0),
    secondary = Mint,
    onSecondary = Color(0xFF06281D),
    secondaryContainer = Color(0xFF3A2410),
    onSecondaryContainer = Color(0xFFFFDCC0),
    tertiary = Color(0xFFEBA25A),
    background = Color(0xFF12100D),
    onBackground = Color(0xFFF4EFE6),
    surface = Color(0xFF1B1813),
    onSurface = Color(0xFFF4EFE6),
    surfaceVariant = Color(0xFF272117),
    onSurfaceVariant = Color(0xFFB8AC9C),
    surfaceDim = Color(0xFF12100D),
    surfaceBright = Color(0xFF3A342B),
    surfaceContainerLowest = Color(0xFF0D0B09),
    surfaceContainerLow = Color(0xFF16130F),
    surfaceContainer = Color(0xFF1B1813),
    surfaceContainerHigh = Color(0xFF221E18),
    surfaceContainerHighest = Color(0xFF2A251D),
    inverseSurface = Color(0xFFF4EFE6),
    inverseOnSurface = Color(0xFF1B1813),
    outline = Color(0xFF463E33),
    outlineVariant = Color(0xFF2C261E),
    error = Color(0xFFFF6B7A),
)

private val LightColors = lightColorScheme(
    primary = Color(0xFFB4571B),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFFFE3CC),
    onPrimaryContainer = Color(0xFF3A1A00),
    secondary = Color(0xFF0E6B4F),
    onSecondary = Color.White,
    secondaryContainer = Color(0xFFFFE3CC),
    onSecondaryContainer = Color(0xFF3A1A00),
    tertiary = Color(0xFF9A5B12),
    background = Color(0xFFF7F5F0),
    onBackground = Color(0xFF211D18),
    surface = Color(0xFFFFFDF9),
    onSurface = Color(0xFF211D18),
    surfaceVariant = Color(0xFFF1E9DC),
    onSurfaceVariant = Color(0xFF6F6559),
    surfaceDim = Color(0xFFE7E1D6),
    surfaceBright = Color(0xFFFDFBF7),
    surfaceContainerLowest = Color(0xFFFFFFFF),
    surfaceContainerLow = Color(0xFFFBF8F2),
    surfaceContainer = Color(0xFFF5F0E7),
    surfaceContainerHigh = Color(0xFFEFE9DE),
    surfaceContainerHighest = Color(0xFFE9E2D5),
    inverseSurface = Color(0xFF211D18),
    inverseOnSurface = Color(0xFFF7F5F0),
    outline = Color(0xFFDCD2C2),
    outlineVariant = Color(0xFFEBE2D4),
    error = Color(0xFFB3233F),
)

@Composable
fun LampaTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = if (isSystemInDarkTheme()) DarkColors else LightColors,
        content = content,
    )
}
