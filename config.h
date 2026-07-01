/*
 * config.h v2.1 — Microclimat Poulailler
 * Toute la configuration centralisée ici
 */
#pragma once

// ── Wi-Fi ─────────────────────────────────────────────────
#define WIFI_SSID       "El-dorado"
#define WIFI_PASSWORD   "9876543210"

// ── Firebase (REST API) ───────────────────────────────────
#define FIREBASE_URL    "https://microclimat001-default-rtdb.europe-west1.firebasedatabase.app/"

// ── Twilio WhatsApp ───────────────────────────────────────
#define TWILIO_ACCOUNT_SID  "AC418b6d32adb03008530c94b5c83abbf8"
#define TWILIO_AUTH_TOKEN   "6561f62289172c7cb1abbf11d429e8cd"
#define TWILIO_FROM         "whatsapp:+14155238886"   // numéro sandbox Twilio (fixe)
#define TWILIO_TO           "whatsapp:+243973681009"  // ← TON numéro avec indicatif pays

// ── Broches GPIO capteurs ─────────────────────────────────
#define PIN_DHT           4    // DHT11 signal
#define PIN_MQ9           32   // MQ-9  analogique (ADC1_CH4)
#define PIN_MQ135         33   // MQ-135 analogique (ADC1_CH5)
#define PIN_LDR           34   // LDR   analogique (ADC1_CH6 — entrée seule)

// ── Broches GPIO actionneurs (relais) ─────────────────────
#define RELAIS_ACTIF_BAS        true   // true  → LOW  = ON  (module relais standard)
                                       // false → HIGH = ON  (relais actif haut)
#define PIN_LAMPE_ECLAIRAGE     25
#define PIN_LAMPE_CHAUFFANTE    26
#define PIN_VENTILATEUR         27

// ── Type capteur DHT ──────────────────────────────────────
#define DHT_TYPE          DHT11   // DHT11 ou DHT22

// ── Paramètres capteurs ───────────────────────────────────
#define NB_ECHANTILLONS   5           // lissage ADC (moyenne de N lectures)
#define PRECHAUFFAGE_MS   30000UL     // 30 s de préchauffage MQ

// ── Seuils de régulation ──────────────────────────────────
#define SEUIL_TEMP_MIN    18.0f
#define SEUIL_TEMP_MAX    30.0f
#define SEUIL_HUM_MIN     40.0f
#define SEUIL_HUM_MAX     70.0f
#define SEUIL_MQ9_ADC     250
#define SEUIL_MQ135_ADC   300
#define SEUIL_LUM_MIN     200
#define SEUIL_LUM_MAX     800

// ── Timing ───────────────────────────────────────────────
#define DELAI_LECTURE         5000UL    // lecture capteurs toutes les 5 s
#define INTERVALLE_FIREBASE   10000UL   // envoi Firebase toutes les 10 s
#define DELAI_NOTIF_WA_MS   600000UL   // 10 minutes