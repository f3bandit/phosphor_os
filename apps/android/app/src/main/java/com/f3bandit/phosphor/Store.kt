// Phosphor for Android: the paired MiSTer (address, device key). The key is encrypted with an
// Android Keystore key that never leaves the phone; only the encrypted form is in app storage.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package com.f3bandit.phosphor

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

data class Pairing(val address: String, val token: String)

interface PairingStore {
    fun load(): Pairing?
    fun save(p: Pairing)
    fun clear()
}

class KeystorePairingStore(context: Context) : PairingStore {
    private val prefs = context.getSharedPreferences("pairing", Context.MODE_PRIVATE)
    private val alias = "phosphor_device_key"

    private fun key(): SecretKey {
        val ks = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (ks.getKey(alias, null) as? SecretKey)?.let { return it }
        val g = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        g.init(
            KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build()
        )
        return g.generateKey()
    }

    override fun load(): Pairing? {
        val addr = prefs.getString("address", null) ?: return null
        val blob = prefs.getString("token", null) ?: return null
        return try {
            val raw = Base64.decode(blob, Base64.NO_WRAP)
            val c = Cipher.getInstance("AES/GCM/NoPadding")
            c.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(128, raw, 0, 12))
            Pairing(addr, String(c.doFinal(raw, 12, raw.size - 12)))
        } catch (e: Exception) {
            null   // key gone (restored backup, new phone): pair again
        }
    }

    override fun save(p: Pairing) {
        val c = Cipher.getInstance("AES/GCM/NoPadding")
        c.init(Cipher.ENCRYPT_MODE, key())
        val enc = c.iv + c.doFinal(p.token.toByteArray())
        prefs.edit().putString("address", p.address).putString("token", Base64.encodeToString(enc, Base64.NO_WRAP)).apply()
    }

    override fun clear() {
        prefs.edit().clear().apply()
    }
}

/** for tests and screenshots */
class MemoryPairingStore(var p: Pairing? = null) : PairingStore {
    override fun load() = p
    override fun save(p: Pairing) { this.p = p }
    override fun clear() { p = null }
}
