/**
 * actionneurs.h — Microclimat Poulailler v2.1
 * Tout est dans le .h (inline) — le actionneurs.cpp doit être VIDE
 */
#pragma once
#include "config.h"
#include "capteurs.h"

// ── Structure état actionneurs ────────────────────────────
typedef struct {
  bool lampeNonChauffante;
  bool lampeChauffante;
  bool ventilateur;
  bool modeManuel;
} EtatActionneurs;

// ── Macros relais (gère actif-bas et actif-haut) ──────────
#define _RELAIS_ON(pin)   digitalWrite((pin), RELAIS_ACTIF_BAS ? LOW  : HIGH)
#define _RELAIS_OFF(pin)  digitalWrite((pin), RELAIS_ACTIF_BAS ? HIGH : LOW)

// ── Init ──────────────────────────────────────────────────
inline void actionneurs_init() {
  pinMode(PIN_LAMPE_ECLAIRAGE,  OUTPUT);
  pinMode(PIN_LAMPE_CHAUFFANTE, OUTPUT);
  pinMode(PIN_VENTILATEUR,      OUTPUT);

  _RELAIS_OFF(PIN_LAMPE_ECLAIRAGE);
  _RELAIS_OFF(PIN_LAMPE_CHAUFFANTE);
  _RELAIS_OFF(PIN_VENTILATEUR);

  Serial.println(F("[ACTIONNEURS] Initialises (tout OFF)."));
  Serial.printf("  Lampe eclairage  -> GPIO %d\n", PIN_LAMPE_ECLAIRAGE);
  Serial.printf("  Lampe chauffante -> GPIO %d\n", PIN_LAMPE_CHAUFFANTE);
  Serial.printf("  Ventilateur      -> GPIO %d\n", PIN_VENTILATEUR);
  Serial.printf("  Relais actif-bas : %s\n",        RELAIS_ACTIF_BAS ? "OUI" : "NON");
}

// ── Appliquer l'état aux broches ──────────────────────────
inline void actionneurs_appliquer(const EtatActionneurs &a) {
  if (a.lampeNonChauffante) _RELAIS_ON(PIN_LAMPE_ECLAIRAGE);
  else                      _RELAIS_OFF(PIN_LAMPE_ECLAIRAGE);

  if (a.lampeChauffante)    _RELAIS_ON(PIN_LAMPE_CHAUFFANTE);
  else                      _RELAIS_OFF(PIN_LAMPE_CHAUFFANTE);

  if (a.ventilateur)        _RELAIS_ON(PIN_VENTILATEUR);
  else                      _RELAIS_OFF(PIN_VENTILATEUR);

  Serial.printf("[ACT] Eclair=%s  Chauf=%s  Vent=%s\n",
    a.lampeNonChauffante ? "ON" : "off",
    a.lampeChauffante    ? "ON" : "off",
    a.ventilateur        ? "ON" : "off");
}

// ── Régulation automatique (logique floue) ────────────────
inline void actionneurs_reguler(const DonneesCapteurs &d, EtatActionneurs &a) {
  if (a.modeManuel) return;

  float froid = 0, chaud = 0, humide = 0, sombre = 0, airMauvais = 0;

  if (d.dht.etat == CAPTEUR_OK) {
    float T = d.dht.temperature;
    float H = d.dht.humidite;
    if (T < SEUIL_TEMP_MIN)
      froid  = constrain((SEUIL_TEMP_MIN - T) / 5.0f, 0.0f, 1.0f);
    else if (T > SEUIL_TEMP_MAX)
      chaud  = constrain((T - SEUIL_TEMP_MAX) / 5.0f, 0.0f, 1.0f);
    if (H > SEUIL_HUM_MAX)
      humide = constrain((H - SEUIL_HUM_MAX) / 10.0f, 0.0f, 1.0f);
  }

  if (d.ldr.etat == CAPTEUR_OK && d.ldr.valeurBrute < SEUIL_LUM_MIN)
    sombre = constrain((float)(SEUIL_LUM_MIN - d.ldr.valeurBrute) / (float)SEUIL_LUM_MIN, 0.0f, 1.0f);

  if (d.mq9.etat == CAPTEUR_OK && d.mq9.prechauffageOk && d.mq9.valeurBrute >= SEUIL_MQ9_ADC)
    airMauvais = constrain((float)(d.mq9.valeurBrute - SEUIL_MQ9_ADC) / 500.0f, 0.0f, 1.0f);

  if (d.mq135.etat == CAPTEUR_OK && d.mq135.prechauffageOk && d.mq135.valeurBrute >= SEUIL_MQ135_ADC) {
    float am2 = constrain((float)(d.mq135.valeurBrute - SEUIL_MQ135_ADC) / 500.0f, 0.0f, 1.0f);
    if (am2 > airMauvais) airMauvais = am2;
  }

  a.lampeChauffante    = (froid      > 0.3f);
  a.ventilateur        = (chaud      > 0.3f || humide > 0.3f || airMauvais > 0.2f);
  a.lampeNonChauffante = (sombre     > 0.3f);

  Serial.printf("[AUTO] Froid=%.2f Chaud=%.2f Air=%.2f Sombre=%.2f | Chauf=%s Eclair=%s Vent=%s\n",
    froid, chaud, airMauvais, sombre,
    a.lampeChauffante    ? "ON" : "off",
    a.lampeNonChauffante ? "ON" : "off",
    a.ventilateur        ? "ON" : "off");
}

// ── Retour mode auto ──────────────────────────────────────
inline void actionneurs_mode_auto(EtatActionneurs &a) {
  a.modeManuel = false;
  Serial.println(F("[ACTIONNEURS] Retour mode automatique."));
}