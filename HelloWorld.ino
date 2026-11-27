
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>

// ==========================================
// PANTALLA OLED
// ==========================================

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

// ==========================================
// PINES
// ==========================================

#define PIN_DHT 4
#define TIPO_DHT DHT11

#define PIN_HUMEDAD_SUELO 34
#define PIN_SENSOR_LUZ 35

#define PIN_RELE_1 26
#define PIN_RELE_2 27

// ==========================================
// RELES
// ==========================================

#define RELE_ENCENDIDO LOW
#define RELE_APAGADO HIGH

// ==========================================
// UMBRALES
// ==========================================

const int UMBRAL_HUMEDAD_MIN = 40;
const int UMBRAL_LUZ_MIN = 30;

// ==========================================
// CALIBRACION
// ==========================================

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

// ==========================================
// VARIABLES
// ==========================================

int ultimaLecturaSueloCruda = 0;
int ultimoSuelo = 0;
int ultimaLuz = 0;

bool ultimoRele1 = false;
bool ultimoRele2 = false;

// ==========================================
// TIEMPOS
// ==========================================

unsigned long tiempoLecturaAnterior = 0;
const unsigned long intervaloLectura = 2000;

unsigned long tiempoOLEDAnterior = 0;
const unsigned long intervaloOLED = 500;

unsigned long tiempoPantallaAnterior = 0;

// ==========================================
// CONTROL DE BOMBA
// ==========================================

unsigned long tiempoCicloBombaAnterior = 0;
unsigned long tiempoInicioEncendidoBomba = 0;

// Cada 50 segundos
const unsigned long intervaloBomba = 50000;

// Encendida durante 15 segundos
const unsigned long duracionEncendidoBomba = 15000;

bool bombaCicloActiva = false;

// ==========================================
// PANTALLAS
// ==========================================

const int NUM_PANTALLAS = 4;

int pantallaActual = 0;

const unsigned long duracionPantalla[NUM_PANTALLAS] = {
  10000,
  2000,
  3000,
  3000
};

// ==========================================
// FUNCIONES
// ==========================================

int leerHumedadSuelo();

void actualizarBombaAgua(unsigned long ahora);
void actualizarRele1();

void actualizarOLED();
void mostrarCreditos();
void mostrarProyecto();
void mostrarHuertaInteligente();

// ==========================================
// SETUP
// ==========================================

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

  unsigned long inicio = millis();

  tiempoLecturaAnterior = inicio;
  tiempoOLEDAnterior = inicio;
  tiempoPantallaAnterior = inicio;
  tiempoCicloBombaAnterior = inicio;

  Serial.println("SISTEMA INICIADO");
  Serial.println("RIEGO: 15 SEGUNDOS");
  Serial.println("INTERVALO: 50 SEGUNDOS");
}

// ==========================================
// LOOP
// ==========================================

void loop() {

  unsigned long ahora = millis();

  // CONTROL DE BOMBA
  actualizarBombaAgua(ahora);

  // ========================================
  // LECTURA DE SENSORES
  // ========================================

  if (ahora - tiempoLecturaAnterior >= intervaloLectura) {

    tiempoLecturaAnterior = ahora;

    // DHT11

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

    // HUMEDAD DEL SUELO

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

    // SENSOR DE LUZ

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

    // CONTROL DE RELES

    actualizarRele1();

    ultimoRele2 = (ultimaLuz < UMBRAL_LUZ_MIN);

    digitalWrite(
      PIN_RELE_2,
      ultimoRele2 ? RELE_ENCENDIDO : RELE_APAGADO
    );

    // MONITOR SERIAL

    Serial.println("----------------------");

    if (dhtValido) {

      Serial.print("Temperatura: ");
      Serial.print(ultimaTemp);
      Serial.println(" C");

      Serial.print("Humedad ambiente: ");
      Serial.print(ultimaHumAmb);
      Serial.println("%");

    } else {

      Serial.println("Temperatura: ERROR DHT11");
      Serial.println("Humedad ambiente: ERROR DHT11");
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
  }

  // ========================================
  // CAMBIO DE PANTALLAS
  // ========================================

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

  // ========================================
  // ACTUALIZAR OLED
  // ========================================

  if (ahora - tiempoOLEDAnterior >= intervaloOLED) {

    tiempoOLEDAnterior = ahora;

    if (pantallaActual == 0) {

      actualizarOLED();

    } else if (pantallaActual == 1) {

      mostrarCreditos();

    } else if (pantallaActual == 2) {

      mostrarProyecto();

    } else {

      mostrarHuertaInteligente();
    }
  }
}

// ==========================================
// ACTUALIZAR RELE 1
// ==========================================

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

// ==========================================
// CONTROL DE BOMBA
// ==========================================

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

// ==========================================
// LECTURA PROMEDIO SENSOR DE SUELO
// ==========================================

int leerHumedadSuelo() {

  long suma = 0;

  for (int i = 0; i < 10; i++) {

    suma += analogRead(PIN_HUMEDAD_SUELO);

    delay(5);
  }

  int promedio = suma / 10;

  Serial.print("LECTURA REAL DEL SUELO: ");
  Serial.println(promedio);

  return promedio;
}

// ==========================================
// PANTALLA PRINCIPAL
// ==========================================

void actualizarOLED() {

  char texto[30];

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_5x7_tf);

  u8g2.drawStr(5, 8, "MONITOR AMBIENTAL");

  u8g2.drawHLine(0, 11, 128);

  if (dhtValido) {

    snprintf(
      texto,
      sizeof(texto),
      "Temperatura: %.1f C",
      ultimaTemp
    );

  } else {

    snprintf(texto, sizeof(texto), "Temperatura: ERROR");
  }

  u8g2.drawStr(2, 21, texto);

  if (dhtValido) {

    snprintf(
      texto,
      sizeof(texto),
      "Hum. ambiente: %.1f%%",
      ultimaHumAmb
    );

  } else {

    snprintf(texto, sizeof(texto), "Hum. ambiente: ERROR");
  }

  u8g2.drawStr(2, 31, texto);

  snprintf(
    texto,
    sizeof(texto),
    "Hum. suelo: %d%%",
    ultimoSuelo
  );

  u8g2.drawStr(2, 41, texto);

  snprintf(
    texto,
    sizeof(texto),
    "Luz: %d%%",
    ultimaLuz
  );

  u8g2.drawStr(2, 51, texto);

  snprintf(
    texto,
    sizeof(texto),
    "R1:%s  R2:%s",
    ultimoRele1 ? "ON" : "OFF",
    ultimoRele2 ? "ON" : "OFF"
  );

  u8g2.drawStr(2, 61, texto);

  u8g2.sendBuffer();
}

// ==========================================
// PANTALLA DE AUTORES
// ==========================================

void mostrarCreditos() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(35, 18, "AUTORES");

  u8g2.drawStr(15, 37, "Erik Quiroga");

  u8g2.drawStr(15, 52, "David Quiroga");

  u8g2.sendBuffer();
}

// ==========================================
// PANTALLA DEL PROYECTO
// ==========================================

void mostrarProyecto() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(25, 25, "PROYECTO");

  u8g2.drawStr(10, 43, "INTEGRADOR :)");

  u8g2.sendBuffer();
}

// ==========================================
// PANTALLA FINAL
// ==========================================

void mostrarHuertaInteligente() {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(12, 25, "HUERTA");

  u8g2.drawStr(12, 43, "INTELIGENTE :)");

  u8g2.sendBuffer();
}
