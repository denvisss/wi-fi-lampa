package com.denvisss.lampa.data

import android.content.Context

class Prefs(context: Context) {
    private val sp = context.getSharedPreferences("lampa", Context.MODE_PRIVATE)

    var host: String?
        get() = sp.getString("host", null)
        set(value) {
            sp.edit().apply {
                if (value.isNullOrBlank()) remove("host") else putString("host", value.trim())
            }.apply()
        }
}
