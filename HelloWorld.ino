#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>


U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);


#define PIN_DHT 4
#define TIPO_DHT DHT11

#define PIN_HUMEDAD_SUELO 34
#define PIN_SENSOR_LUZ 35

#define PIN_RELE_1 26
#define PIN_RELE_2 27


#define RELE_ENCENDIDO LOW
#define RELE_APAGADO HIGH

const int UMBRAL_HUMEDAD_MIN = 40;
const int UMBRAL_LUZ_MIN = 30;

const float TEMP_HELADA = 0.0;      // <= 0 °C: riesgo de helada
const float TEMP_FRIO_DANINO = 10.0; // < 10 °C: frio danino
const float TEMP_CALOR_MAX = 30.0;   // > 30 °C: calor alto

enum EstadoTemperatura {
  TEMP_SIN_DATO,
  TEMP_SIN_PROBLEMA,
  TEMP_FRIO_DANINO_ESTADO,
  TEMP_HELADA_ESTADO,
  TEMP_CALOR_ESTADO
};

EstadoTemperatura estadoTempActual = TEMP_SIN_DATO;


const int VALOR_SECO = 3600;
const int VALOR_MOJADO = 1100;

const int VALOR_OSCURO = 3800;
const int VALOR_BRILLANTE = 400;

// ==========================================
// SENSOR DHT11
// ==========================================

DHT dht(PIN_DHT, TIPO_DHT);

float ultimaTemp = 0;
float ultimaHumAmb = 0;

bool dhtValido = false;


int ultimaLecturaSueloCruda = 0;
int ultimoSuelo = 0;
int ultimaLuz = 0;

bool ultimoRele1 = false;
bool ultimoRele2 = false;


unsigned long tiempoLecturaAnterior = 0;
const unsigned long intervaloLectura = 2000;

unsigned long tiempoOLEDAnterior = 0;
const unsigned long intervaloOLED = 500;

unsigned long tiempoPantallaAnterior = 0;


unsigned long tiempoCicloBombaAnterior = 0;
unsigned long tiempoInicioEncendidoBomba = 0;

// Cada 3 minutos = 180,000 ms
const unsigned long intervaloBomba = 180000UL;

// Encendida durante 15 segundos = 15,000 ms
const unsigned long duracionEncendidoBomba = 15000UL;

bool bombaCicloActiva = false;


const int NUM_PANTALLAS = 5;

int pantallaActual = 0;

const unsigned long duracionPantalla[NUM_PANTALLAS] = {
  8000, // 0: Monitor ambiental
  5000, // 1: Alerta temperatura / helada o sin problema
  2500, // 2: Autores
  2500, // 3: Proyecto integrador
  2500  // 4: Huerta inteligente
};


int leerHumedadSuelo();
void evaluarEstadoTemperatura();
const char* etiquetaCortaTemperatura();

void actualizarBombaAgua(unsigned long ahora);
void actualizarRele1();

void actualizarOLED();
void mostrarAlertaTemperatura();
void mostrarCreditos();
void mostrarProyecto();
void mostrarHuertaInteligente();


void setup() {

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);

  pinMode(PIN_RELE_1, OUTPUT);
  pinMode(PIN_RELE_2, OUTPUT);

  digitalWrite(PIN_RELE_1, RELE_APAGADO);
  digitalWrite(PIN_RELE_2, RELE_APAGADO);

  dht.begin();

  u8g2.setBusClock(100000);
  u8g2.begin();
  // Si se ve al reves o volteada, cambia entre 0 y 1:
  u8g2.setFlipMode(1);

  unsigned long inicio = millis();

  tiempoLecturaAnterior = inicio;
  tiempoOLEDAnterior = inicio;
  tiempoPantallaAnterior = inicio;
  tiempoCicloBombaAnterior = inicio;

  Serial.println("SISTEMA INICIADO");
  Serial.println("RIEGO: 15 SEGUNDOS");
  Serial.println("INTERVALO: 3 MINUTOS (180 SEGUNDOS)");
}


void loop() {

  unsigned long ahora = millis();

  actualizarBombaAgua(ahora);

  if (ahora - tiempoLecturaAnterior >= intervaloLectura) {

    tiempoLecturaAnterior = ahora;

    float temp = dht.readTemperature();
    float humAmb = dht.readHumidity();

    if (!isnan(temp) && !isnan(humAmb)) {

      ultimaTemp = temp;
      ultimaHumAmb = humAmb;
      dhtValido = true;

      Serial.println("DHT11: LECTURA CORRECTA");

    } else {

      dhtValido = false;
      Serial.println("ERROR: EL DHT11 NO RESPONDE");
    }

    evaluarEstadoTemperatura();

    int lecturaSuelo = leerHumedadSuelo();
    ultimaLecturaSueloCruda = lecturaSuelo;

    ultimoSuelo = constrain(
      map(
        lecturaSuelo,
        VALOR_SECO,
        VALOR_MOJADO,
        0,
        100
      ),
      0,
      100
    );

    int lecturaLuz = analogRead(PIN_SENSOR_LUZ);

    ultimaLuz = constrain(
      map(
        lecturaLuz,
        VALOR_OSCURO,
        VALOR_BRILLANTE,
        0,
        100
      ),
      0,
      100
    );

    actualizarRele1();

    ultimoRele2 = (ultimaLuz < UMBRAL_LUZ_MIN);

    digitalWrite(
      PIN_RELE_2,
      ultimoRele2 ? RELE_ENCENDIDO : RELE_APAGADO
    );

    Serial.println("----------------------");

    if (dhtValido) {

      Serial.print("Temperatura: ");
      Serial.print(ultimaTemp, 1);
      Serial.println(" C");

      Serial.print("Humedad ambiente: ");
      Serial.print(ultimaHumAmb, 1);
      Serial.println("%");

    } else {

      Serial.println("Temperatura: ERROR DHT11");
      Serial.println("Humedad ambiente: ERROR DHT11");
    }

    Serial.print("Estado temperatura: ");
    Serial.println(etiquetaCortaTemperatura());

    if (estadoTempActual == TEMP_HELADA_ESTADO) {
      Serial.println("ALERTA: RIESGO DE HELADA -> GUARDA LA PLANTA AHORA");
    } else if (estadoTempActual == TEMP_FRIO_DANINO_ESTADO) {
      Serial.println("AVISO: FRIO DANINO (<10 C) -> GUARDA LA PLANTA");
    } else if (estadoTempActual == TEMP_CALOR_ESTADO) {
      Serial.println("AVISO: TEMPERATURA ALTA (>30 C)");
    } else if (estadoTempActual == TEMP_SIN_PROBLEMA) {
      Serial.println("TEMPERATURA: SIN PROBLEMA");
    }

    Serial.print("ADC Suelo: ");
    Serial.print(ultimaLecturaSueloCruda);

    Serial.print(" -> Humedad suelo: ");
    Serial.print(ultimoSuelo);
    Serial.println("%");

    Serial.print("Luz: ");
    Serial.print(ultimaLuz);
    Serial.println("%");

    Serial.print("Rele 1: ");
    Serial.println(ultimoRele1 ? "ENCENDIDO" : "APAGADO");

    Serial.print("Rele 2: ");
    Serial.println(ultimoRele2 ? "ENCENDIDO" : "APAGADO");

    Serial.printf("CSV,%lu,%.1f,%.1f,%d,%d,%d,%d,%d,%d\n",
      ahora,
      dhtValido ? ultimaTemp : -99.0,
      dhtValido ? ultimaHumAmb : -1.0,
      ultimaLecturaSueloCruda,
      ultimoSuelo,
      ultimaLuz,
      ultimoRele1 ? 1 : 0,
      ultimoRele2 ? 1 : 0,
      (int)estadoTempActual
    );
  }

  if (
    ahora - tiempoPantallaAnterior >=
    duracionPantalla[pantallaActual]
  ) {

    tiempoPantallaAnterior = ahora;

    pantallaActual++;

    if (pantallaActual >= NUM_PANTALLAS) {
      pantallaActual = 0;
    }
  }


  if (ahora - tiempoOLEDAnterior >= intervaloOLED) {

    tiempoOLEDAnterior = ahora;

    if (pantallaActual == 0) {

      actualizarOLED();

    } else if (pantallaActual == 1) {

      mostrarAlertaTemperatura();

    } else if (pantallaActual == 2) {

      mostrarCreditos();

    } else if (pantallaActual == 3) {

      mostrarProyecto();

    } else {

      mostrarHuertaInteligente();
    }
  }
}

void evaluarEstadoTemperatura() {

  if (!dhtValido) {
    estadoTempActual = TEMP_SIN_DATO;
    return;
  }

  if (ultimaTemp <= TEMP_HELADA) {
    estadoTempActual = TEMP_HELADA_ESTADO;
  } else if (ultimaTemp < TEMP_FRIO_DANINO) {
    estadoTempActual = TEMP_FRIO_DANINO_ESTADO;
  } else if (ultimaTemp > TEMP_CALOR_MAX) {
    estadoTempActual = TEMP_CALOR_ESTADO;
  } else {
    estadoTempActual = TEMP_SIN_PROBLEMA;
  }
}

const char* etiquetaCortaTemperatura() {

  switch (estadoTempActual) {
    case TEMP_HELADA_ESTADO:
      return "HELADA!";
    case TEMP_FRIO_DANINO_ESTADO:
      return "FRIO!";
    case TEMP_CALOR_ESTADO:
      return "CALOR!";
    case TEMP_SIN_PROBLEMA:
      return "OK";
    default:
      return "ERR";
  }
}


void actualizarRele1() {

  bool riegoPorHumedad =
    (ultimoSuelo < UMBRAL_HUMEDAD_MIN);

  bool estadoDeseado =
    bombaCicloActiva || riegoPorHumedad;

  if (estadoDeseado != ultimoRele1) {

    ultimoRele1 = estadoDeseado;

    digitalWrite(
      PIN_RELE_1,
      ultimoRele1 ? RELE_ENCENDIDO : RELE_APAGADO
    );
  }
}


void actualizarBombaAgua(unsigned long ahora) {

  // INICIO DEL CICLO
  if (
    !bombaCicloActiva &&
    ahora - tiempoCicloBombaAnterior >= intervaloBomba
  ) {

    tiempoCicloBombaAnterior = ahora;
    tiempoInicioEncendidoBomba = ahora;
    bombaCicloActiva = true;

    Serial.println("BOMBA: INICIO DE RIEGO");
  }

  // FINALIZAR DESPUES DE 15 SEGUNDOS
  if (
    bombaCicloActiva &&
    ahora - tiempoInicioEncendidoBomba >= duracionEncendidoBomba
  ) {

    bombaCicloActiva = false;
    Serial.println("BOMBA: FIN DE RIEGO");
  }

  actualizarRele1();
}


int leerHumedadSuelo() {

  const int muestras = 10;
  long suma = 0;
  int tomadas = 0;
  unsigned long ultimaMuestra = micros();

  while (tomadas < muestras) {
    if (micros() - ultimaMuestra >= 5000) {
      ultimaMuestra = micros();
      suma += analogRead(PIN_HUMEDAD_SUELO);
      tomadas++;
    }
    yield();
  }

  int promedio = suma / muestras;

  Serial.print("LECTURA REAL DEL SUELO: ");
  Serial.println(promedio);

  return promedio;
}


void actualizarOLED() {

  char texto[28];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);

  u8g2.drawStr(2, 8, "MONITOR AMBIENTAL");
  u8g2.drawHLine(0, 10, 128);

  if (dhtValido) {
    snprintf(texto, sizeof(texto), "T:%.1fC [%s]", ultimaTemp, etiquetaCortaTemperatura());
  } else {
    snprintf(texto, sizeof(texto), "T:ERR [ERR]");
  }
  u8g2.drawStr(2, 19, texto);

  if (dhtValido) {
    snprintf(texto, sizeof(texto), "H.amb:%.1f%%", ultimaHumAmb);
  } else {
    snprintf(texto, sizeof(texto), "H.amb:ERR");
  }
  u8g2.drawStr(2, 28, texto);

  snprintf(texto, sizeof(texto), "H.suelo:%d%%", ultimoSuelo);
  u8g2.drawStr(2, 37, texto);

  snprintf(texto, sizeof(texto), "Luz:%d%% R1:%s R2:%s",
    ultimaLuz,
    ultimoRele1 ? "ON" : "OFF",
    ultimoRele2 ? "ON" : "OFF"
  );
  u8g2.drawStr(2, 46, texto);

  if (estadoTempActual == TEMP_HELADA_ESTADO || estadoTempActual == TEMP_FRIO_DANINO_ESTADO) {
    u8g2.drawStr(2, 56, "GUARDA LA PLANTA");
  } else if (estadoTempActual == TEMP_SIN_PROBLEMA) {
    u8g2.drawStr(2, 56, "Sin problema");
  } else {
    u8g2.drawStr(2, 56, "Revisar sensor");
  }

  u8g2.sendBuffer();
}

void mostrarAlertaTemperatura() {

  char lineaTemp[24];

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(4, 11, "ESTADO TERMICO");
  u8g2.drawHLine(0, 14, 128);

  if (!dhtValido) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(2, 28, "Sin lectura DHT11");
    u8g2.drawStr(2, 40, "Revisa conexion/sensor");
    u8g2.drawStr(2, 52, "para alerta de helada");
  } else {
    snprintf(lineaTemp, sizeof(lineaTemp), "Temp: %.1f C", ultimaTemp);
    u8g2.drawStr(4, 26, lineaTemp);

    u8g2.setFont(u8g2_font_5x7_tf);

    if (estadoTempActual == TEMP_HELADA_ESTADO) {
      u8g2.drawStr(2, 39, "RIESGO DE HELADA");
      u8g2.drawStr(2, 50, "GUARDA LA PLANTA YA!");
      u8g2.drawStr(2, 61, "Temp <= 0 C");
    } else if (estadoTempActual == TEMP_FRIO_DANINO_ESTADO) {
      u8g2.drawStr(2, 39, "FRIO DANINO (<10 C)");
      u8g2.drawStr(2, 50, "GUARDA LA PLANTA");
      u8g2.drawStr(2, 61, "Sitio cubierto");
    } else if (estadoTempActual == TEMP_CALOR_ESTADO) {
      u8g2.drawStr(2, 39, "CALOR ALTO (>30 C)");
      u8g2.drawStr(2, 50, "Evita sol fuerte");
      u8g2.drawStr(2, 61, "Revisa ventilacion");
    } else {
      u8g2.drawStr(2, 40, "NO HAY PROBLEMA");
      u8g2.drawStr(2, 52, "Temperatura normal");
      u8g2.drawStr(2, 62, "para la planta");
    }
  }

  u8g2.sendBuffer();
}


void mostrarCreditos() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(28, 14, "AUTORES");
  u8g2.drawHLine(0, 18, 128);

  u8g2.drawStr(6, 35, "Erik Quiroga");
  u8g2.drawStr(6, 52, "David Quiroga");

  u8g2.sendBuffer();
}


void mostrarProyecto() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(24, 24, "PROYECTO");
  u8g2.drawStr(14, 44, "INTEGRADOR :)");

  u8g2.sendBuffer();
}


void mostrarHuertaInteligente() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(26, 24, "HUERTA");
  u8g2.drawStr(10, 44, "INTELIGENTE :)");

  u8g2.sendBuffer();
}
