/**
 * ============================================================
 * MICROCLIMAT POULAILLER — main.ino v2.0
 * Améliorations :
 *   - Relais bascule immédiatement en mode manuel
 *   - Notifications WhatsApp via Twilio (gaz dangereux)
 *   - Historique Firebase (/historique/{timestamp})
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "capteurs.h"
#include "actionneurs.h"

// ── Variables globales ────────────────────────────────────
DonneesCapteurs  capteurs;
EtatActionneurs  actionneurs_etat_courant = {false, false, false, false};
unsigned long    _derniere_lecture   = 0;
unsigned long    _dernier_envoi_fb   = 0;
unsigned long    _derniere_notif_wa  = 0;  // anti-spam WhatsApp
bool             _wifi_ok            = false;


// ── Prototypes ────────────────────────────────────────────
void  connecter_wifi();
bool  envoyer_firebase(const DonneesCapteurs &d, const EtatActionneurs &a);
void  lire_commandes_firebase(EtatActionneurs &a);
void  envoyer_whatsapp(const String &message);
void  verifier_alertes_gaz(const DonneesCapteurs &d);

// ════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=========================================="));
  Serial.println(F("  MICROCLIMAT POULAILLER v2.0"));
  Serial.println(F("  UDBL — MUTWALE ILUNGA Adora — 2026"));
  Serial.println(F("=========================================="));

  capteurs_init();
  actionneurs_init();
  connecter_wifi();
  delay(2000);

  Serial.printf("[RAM] Heap libre : %d bytes\n", ESP.getFreeHeap());
}

// ════════════════════════════════════════════════════════
void loop() {
  unsigned long maintenant = millis();

  // ── Lecture capteurs ──────────────────────────────────
  if (maintenant - _derniere_lecture >= DELAI_LECTURE) {
    _derniere_lecture = maintenant;

    capteurs_lire(capteurs);

    // Régulation auto (ignorée si mode manuel)
    if (!actionneurs_etat_courant.modeManuel) {
      actionneurs_reguler(capteurs, actionneurs_etat_courant);
      actionneurs_appliquer(actionneurs_etat_courant);
    }

    // Vérification alertes gaz (toujours active)
    verifier_alertes_gaz(capteurs);
  }

  // ── Communication Firebase ────────────────────────────
  if (_wifi_ok && (maintenant - _dernier_envoi_fb >= INTERVALLE_FIREBASE)) {
    _dernier_envoi_fb = maintenant;

    // 1. Lire les commandes (mode manuel + état relais)
    lire_commandes_firebase(actionneurs_etat_courant);

    // 2. Envoyer les données capteurs + état actionneurs
    envoyer_firebase(capteurs, actionneurs_etat_courant);
  }

  // ── Reconnexion Wi-Fi ─────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WIFI] Connexion perdue — reconnexion..."));
    _wifi_ok = false;
    connecter_wifi();
  }
}

// ════════════════════════════════════════════════════════
// CONNEXION WI-FI
// ════════════════════════════════════════════════════════
void connecter_wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WIFI] Connexion à %s", WIFI_SSID);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 30) {
    delay(500); Serial.print("."); t++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    _wifi_ok = true;
    Serial.printf("\n[WIFI] Connecté ! IP : %s\n", WiFi.localIP().toString().c_str());
  } else {
    _wifi_ok = false;
    Serial.println(F("\n[WIFI] Échec — hors-ligne."));
  }
}

// ════════════════════════════════════════════════════════
// LECTURE COMMANDES FIREBASE
// ════════════════════════════════════════════════════════
void lire_commandes_firebase(EtatActionneurs &a) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(FIREBASE_URL) + "commandes.json";
  if (!http.begin(client, url)) return;
  http.setTimeout(8000);

  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    if (payload == "null" || payload.length() < 5) { http.end(); return; }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, payload)) { http.end(); return; }

    bool manuel = doc["mode_manuel"] | false;

    if (manuel != a.modeManuel) {
      a.modeManuel = manuel;
      if (!manuel) {
        // Retour en auto : laisser la régulation reprendre au prochain cycle
        Serial.println(F("[CMD] Mode AUTO restauré."));
      } else {
        Serial.println(F("[CMD] Mode MANUEL activé."));
      }
    }

    // ── CLEF : en mode manuel, appliquer immédiatement ──
    // Dès que Firebase reçoit une commande web, l'ESP32 la lit
    // ici et bascule le relais sans attendre le prochain cycle.
    if (a.modeManuel) {
      bool ecl   = doc["lampe_eclairage"]  | false;
      bool chauf = doc["lampe_chauffante"] | false;
      bool vent  = doc["ventilateur"]      | false;

      bool changement = (ecl   != a.lampeNonChauffante) ||
                        (chauf != a.lampeChauffante)    ||
                        (vent  != a.ventilateur);

      a.lampeNonChauffante = ecl;
      a.lampeChauffante    = chauf;
      a.ventilateur        = vent;

      if (changement) {
        actionneurs_appliquer(a);
        Serial.printf("[CMD] Manuel → Ecl=%d  Chauf=%d  Vent=%d\n",
                      ecl, chauf, vent);
      }
    }
  } else {
    Serial.printf("[HTTP] Erreur lecture commandes : %d\n", code);
  }
  http.end();
}

// ════════════════════════════════════════════════════════
// ENVOI DONNÉES → FIREBASE /donnees/live + /historique
// ════════════════════════════════════════════════════════
bool envoyer_firebase(const DonneesCapteurs &d, const EtatActionneurs &a) {
  // Construire le JSON
  StaticJsonDocument<512> doc;
  doc["ts"] = (unsigned long)(millis() / 1000);

  if (d.dht.etat == CAPTEUR_OK) {
    doc["temperature"] = serialized(String(d.dht.temperature, 1));
    doc["humidite"]    = serialized(String(d.dht.humidite, 1));
  } else {
    doc["temperature"] = "ERR"; doc["humidite"] = "ERR";
  }
  doc["dht_etat"] = capteurs_etat_vers_chaine(d.dht.etat);

  if (d.mq9.etat == CAPTEUR_OK && d.mq9.prechauffageOk) {
    doc["mq9_brut"] = d.mq9.valeurBrute;
    doc["mq9_pct"]  = serialized(String(d.mq9.pourcent, 1));
  } else {
    doc["mq9_brut"] = -1; doc["mq9_pct"] = -1;
  }
  doc["mq9_etat"]        = capteurs_etat_vers_chaine(d.mq9.etat);
  doc["mq9_prechauffe"]  = d.mq9.prechauffageOk;

  if (d.mq135.etat == CAPTEUR_OK && d.mq135.prechauffageOk) {
    doc["mq135_brut"] = d.mq135.valeurBrute;
    doc["mq135_pct"]  = serialized(String(d.mq135.pourcent, 1));
  } else {
    doc["mq135_brut"] = -1; doc["mq135_pct"] = -1;
  }
  doc["mq135_etat"]       = capteurs_etat_vers_chaine(d.mq135.etat);
  doc["mq135_prechauffe"] = d.mq135.prechauffageOk;

  if (d.ldr.etat == CAPTEUR_OK) {
    doc["ldr_brut"]   = d.ldr.valeurBrute;
    doc["luminosite"] = serialized(String(d.ldr.luminositeNorm, 1));
  } else {
    doc["ldr_brut"] = -1; doc["luminosite"] = -1;
  }
  doc["ldr_etat"] = capteurs_etat_vers_chaine(d.ldr.etat);

  doc["lampe_eclairage"]  = a.lampeNonChauffante;
  doc["lampe_chauffante"] = a.lampeChauffante;
  doc["ventilateur"]      = a.ventilateur;
  doc["mode_manuel"]      = a.modeManuel;

  String body;
  serializeJson(doc, body);

  bool ok = false;

  // -- 1. Écriture dans /donnees/live (PATCH = mise à jour partielle)
  {
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    String url = String(FIREBASE_URL) + "donnees/live.json";
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "application/json");
      http.setTimeout(8000);
      int code = http.PATCH(body);
      if (code == 200) {
        ok = true;
        Serial.println(F("[HTTP] Live → Firebase OK"));
      } else {
        Serial.printf("[HTTP] Erreur live : %d\n", code);
      }
      http.end();
    }
  }

  // -- 2. Écriture dans /historique/{timestamp_ms} (PUT)
  {
    WiFiClientSecure client2; client2.setInsecure();
    HTTPClient http2;
    String urlH = String(FIREBASE_URL) + "historique/" +
                  String((unsigned long)millis()) + ".json";
    if (http2.begin(client2, urlH)) {
      http2.addHeader("Content-Type", "application/json");
      http2.setTimeout(8000);
      http2.PUT(body);
      http2.end();
    }
  }

  return ok;
}

// ════════════════════════════════════════════════════════
// VÉRIFICATION ALERTES GAZ → WhatsApp
// ════════════════════════════════════════════════════════
void verifier_alertes_gaz(const DonneesCapteurs &d) {
  if (!_wifi_ok) {
    Serial.println(F("[ALERTE] Ignorée — Wi-Fi non connecté."));
    return;
  }

  unsigned long maintenant = millis();
  unsigned long temps_restant = DELAI_NOTIF_WA_MS - (maintenant - _derniere_notif_wa);

  if (maintenant - _derniere_notif_wa < DELAI_NOTIF_WA_MS) {
    Serial.printf("[ALERTE] Anti-spam actif — attente %lu s\n", temps_restant / 1000);
    return;
  }

  // Affiche l'état des capteurs à chaque vérification
  Serial.printf("[ALERTE] MQ9  → etat=%s  prechauffe=%s  ADC=%d  seuil=%d\n",
    capteurs_etat_vers_chaine(d.mq9.etat),
    d.mq9.prechauffageOk ? "OUI" : "NON",
    d.mq9.valeurBrute, SEUIL_MQ9_ADC);

  Serial.printf("[ALERTE] MQ135→ etat=%s  prechauffe=%s  ADC=%d  seuil=%d\n",
    capteurs_etat_vers_chaine(d.mq135.etat),
    d.mq135.prechauffageOk ? "OUI" : "NON",
    d.mq135.valeurBrute, SEUIL_MQ135_ADC);

  bool alerte = false;
  String message = "⚠️ ALERTE POULAILLER\n";

  if (d.mq9.etat == CAPTEUR_OK && d.mq9.prechauffageOk &&
      d.mq9.valeurBrute >= SEUIL_MQ9_ADC) {
    alerte = true;
    message += "🔥 CO/Gaz combustible détecté !\n";
    message += "   MQ-9 ADC = " + String(d.mq9.valeurBrute) +
               " (seuil " + String(SEUIL_MQ9_ADC) + ")\n";
  }

  if (d.mq135.etat == CAPTEUR_OK && d.mq135.prechauffageOk &&
      d.mq135.valeurBrute >= SEUIL_MQ135_ADC) {
    alerte = true;
    message += "☁️ NH3/NOx/Fumée détecté !\n";
    message += "   MQ-135 ADC = " + String(d.mq135.valeurBrute) +
               " (seuil " + String(SEUIL_MQ135_ADC) + ")\n";
  }

  if (alerte) {
    message += "\n👉 Veuillez vérifier et nettoyer le poulailler.";
    Serial.println(F("[ALERTE] Gaz dangereux — envoi WhatsApp..."));
    envoyer_whatsapp(message);
    _derniere_notif_wa = maintenant;
  } else {
    Serial.println(F("[ALERTE] Aucun seuil dépassé."));
  }
}

// ════════════════════════════════════════════════════════
// ENVOI WHATSAPP VIA TWILIO
// ════════════════════════════════════════════════════════
void envoyer_whatsapp(const String &message) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.twilio.com/2010-04-01/Accounts/";
  url += TWILIO_ACCOUNT_SID;
  url += "/Messages.json";

  if (!http.begin(client, url)) {
    Serial.println(F("[TWILIO] Impossible d'ouvrir la connexion."));
    return;
  }

  http.setAuthorization(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.setTimeout(10000);

  // Encodage correct du From et To (whatsapp:+XXX → whatsapp%3A%2BXXX)
  String fromEnc = String(TWILIO_FROM);
  fromEnc.replace(":", "%3A");
  fromEnc.replace("+", "%2B");

  String toEnc = String(TWILIO_TO);
  toEnc.replace(":", "%3A");
  toEnc.replace("+", "%2B");

  // Encodage du message
  String msgEncode = message;
  msgEncode.replace("%", "%25");   // TOUJOURS en premier !
  msgEncode.replace(" ", "+");
  msgEncode.replace("\n", "%0A");
  msgEncode.replace("&", "%26");
  msgEncode.replace("=", "%3D");
  // Emojis
  msgEncode.replace("⚠️", "%E2%9A%A0%EF%B8%8F");
  msgEncode.replace("🔥", "%F0%9F%94%A5");
  msgEncode.replace("☁️", "%E2%98%81%EF%B8%8F");
  msgEncode.replace("👉", "%F0%9F%91%89");

  String body = "From=" + fromEnc +
                "&To="  + toEnc   +
                "&Body=" + msgEncode;

  Serial.println("[TWILIO] Body envoyé : " + body);  // debug temporaire

  int code = http.POST(body);
  if (code == 201) {
    Serial.println(F("[TWILIO] WhatsApp envoyé avec succès !"));
  } else {
    Serial.printf("[TWILIO] Erreur : code %d\n", code);
    Serial.println(http.getString());  // affiche la réponse Twilio complète
  }
  http.end();
}
