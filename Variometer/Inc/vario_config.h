#ifndef VARIO_CONFIG_H
#define VARIO_CONFIG_H

/* ----- General system configuration parameters ----- */
typedef struct {
  float sensitivity; // Coeficiente del filtro de velocidad, mas alto = MAS reacción  

  float lift_threshold; // Umbral de velocidad vertical para inciar sonidos de ascenso en m/s
  float sink_threshold; // Umbral de velocidad vertical para inciar sonidos de descenso en m/s

  float lift_hz_base; // Frecuencia inicial de tono de acsenso en Hz
  float lift_hz_scale; // Aumento de frecuencia por cada 1m/s de ascenso en Hz
  float sink_hz_base; // Frecuencia inicial de tono de descenso en Hz
  float sink_hz_scale; // Aumento de frecuencia por cada 1m/s de descenso en Hz
  float sink_hz_min; // Frecuencia mínima de tono de descenso en Hz

  float sealevel_hPa; // Presion al nivel del mar en hPa
} varioConfig_td;

static const varioConfig_td defaultConfig = {
  .sensitivity = 4,
  .lift_threshold = 0.2f,
  .sink_threshold = -0.3f,
  .lift_hz_base = 800,
  .lift_hz_scale = 100,
  .sink_hz_base = 300,
  .sink_hz_scale = 100,
  .sink_hz_min = 100,
  .sealevel_hPa = 1014.0f,
 };

// Variables shared by all tasks
extern varioConfig_td varioConfig;

#endif /* VARIO_CONFIG_H */