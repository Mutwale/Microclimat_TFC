/**
 * capteurs.h — Microclimat Poulailler v2.1
 * Tout est dans le .h (inline) — le capteurs.cpp doit être VIDE
 */
#pragma once
#include <DHT.h>
#include "config.h"

// ── Enum état capteur ─────────────────────────────────────
typedef enum {
  CAPTEUR_OK = 0,
  CAPTEUR_ERREUR_LECTURE,
  CAPTEUR_PRECHAUFFAGE,
  CAPTEUR_INCONNU
} EtatCapteur;

// ── Structures données ────────────────────────────────────
typedef struct {
  float       temperature;
  float       humidite;
  EtatCapteur etat;
} DonneesDHT;

typedef struct {
  int         valeurBrute;
  float       pourcent;
  bool        prechauffageOk;
  EtatCapteur etat;
} DonneesGaz;

typedef struct {
  int         valeurBrute;
  float       luminositeNorm;
  EtatCapteur etat;
} DonneesLDR;

typedef struct {
  DonneesDHT dht;
  DonneesGaz mq9;
  DonneesGaz mq135;
  DonneesLDR ldr;
} DonneesCapteurs;

// ── Variables internes (static = une seule instance) ──────
static DHT           _dht(PIN_DHT, DHT_TYPE);
static unsigned long _demarrage_ms = 0;

// ── Utilitaires ───────────────────────────────────────────
inline const char* capteurs_etat_vers_chaine(EtatCapteur e) {
  switch (e) {
    case CAPTEUR_OK:             return "OK";
    case CAPTEUR_ERREUR_LECTURE: return "ERREUR";
    case CAPTEUR_PRECHAUFFAGE:   return "PRECHAUFFAGE";
    default:                     return "INCONNU";
  }
}

inline int _lire_adc_lisse(uint8_t pin) {
  long somme = 0;
  for (int i = 0; i < NB_ECHANTILLONS; i++) {
    somme += analogRead(pin);
    delay(10);
  }
  return (int)(somme / NB_ECHANTILLONS);
}

// ── Init ──────────────────────────────────────────────────
inline void capteurs_init() {
  _dht.begin();
  pinMode(PIN_MQ9,   INPUT);
  pinMode(PIN_MQ135, INPUT);
  pinMode(PIN_LDR,   INPUT);
  _demarrage_ms = millis();

  Serial.println(F("[CAPTEURS] Initialises."));
  Serial.printf("  DHT22  -> GPIO %d\n", PIN_DHT);
  Serial.printf("  MQ-9   -> GPIO %d\n", PIN_MQ9);
  Serial.printf("  MQ-135 -> GPIO %d\n", PIN_MQ135);
  Serial.printf("  LDR    -> GPIO %d\n", PIN_LDR);
  Serial.printf("  Prechauffage MQ : %d s\n", PRECHAUFFAGE_MS / 1000);
}

// ── Lecture ───────────────────────────────────────────────
inline bool capteurs_lire(DonneesCapteurs &d) {
  bool ok = false;
  bool preheat = (millis() - _demarrage_ms) >= PRECHAUFFAGE_MS;

  // DHT22
  float t = _dht.readTemperature();
  float h = _dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    d.dht.etat = CAPTEUR_ERREUR_LECTURE;
    Serial.println(F("[CAPTEURS][DHT22] Erreur lecture !"));
  } else {
    d.dht.temperature = t;
    d.dht.humidite    = h;
    d.dht.etat        = CAPTEUR_OK;
    Serial.printf("[CAPTEURS][DHT22] T=%.1f C  H=%.1f%%\n", t, h);
    ok = true;
  }

  // MQ-9
  {
    int brut = _lire_adc_lisse(PIN_MQ9);
    d.mq9.valeurBrute    = brut;
    d.mq9.pourcent       = (brut / 4095.0f) * 100.0f;
    d.mq9.prechauffageOk = preheat;
    d.mq9.etat           = CAPTEUR_OK;
    if (!preheat) Serial.println(F("[CAPTEURS][MQ9] Prechauffage en cours..."));
    else { Serial.printf("[CAPTEURS][MQ9]   ADC=%d (%.1f%%)\n", brut, d.mq9.pourcent); ok = true; }
  }

  // MQ-135
  {
    int brut = _lire_adc_lisse(PIN_MQ135);
    d.mq135.valeurBrute    = brut;
    d.mq135.pourcent       = (brut / 4095.0f) * 100.0f;
    d.mq135.prechauffageOk = preheat;
    d.mq135.etat           = CAPTEUR_OK;
    if (!preheat) Serial.println(F("[CAPTEURS][MQ135] Prechauffage en cours..."));
    else { Serial.printf("[CAPTEURS][MQ135] ADC=%d (%.1f%%)\n", brut, d.mq135.pourcent); ok = true; }
  }

  // LDR
  {
    int brut = _lire_adc_lisse(PIN_LDR);
    d.ldr.valeurBrute    = brut;
    d.ldr.luminositeNorm = (brut / 4095.0f) * 100.0f;
    d.ldr.etat           = CAPTEUR_OK;
    Serial.printf("[CAPTEURS][LDR]   ADC=%d (%.1f%%)\n", brut, d.ldr.luminositeNorm);
    ok = true;
  }

  return ok;
}