// Sketch completo — Leituras sincronizadas sem delay()

#include <GyverMAX6675.h>

// ---------- Botão (toggle) ----------
const int BUTTON_PIN = 9;
int estadoBotao = 0;             // 0 = desligado, 1 = ligado
int buttonReading = HIGH;
int lastButtonReading = HIGH;
unsigned long lastButtonDebounceTime = 0;
const unsigned long debounceDelay = 50; // ms para debouncing

// ---------- Relé ----------
const int relePin = 5;

// ---------- Sensor de vazão YF-S401 ----------
const byte flowSensorPin = 2;
volatile unsigned int pulseCount = 0;
const float cali = 30.8;
unsigned long lastFlowMillis = 0;
const unsigned long intervaloFlow = 1000;
float ultimaVazao = 0.0;

// ---------- Ultrassom ----------
const int trigPin = 7;
const int echoPin = 3;
unsigned long lastUltrasonicMillis = 0;
const unsigned long intervaloUltrassom = 500;
float ultimoNivel = 0.0;

// ---------- Temperatura ----------
#define CLK_PIN 13
#define DATA_PIN 11
#define CS_PIN 12
GyverMAX6675<CLK_PIN, DATA_PIN, CS_PIN> sens;
unsigned long lastTempMillis = 0;
const unsigned long intervaloTemp = 800;
float ultimaTemp = 0.0;

// ---------- Impressão ----------
const unsigned long printInterval = 1000;
unsigned long lastPrintMillis = 0;

bool sistemaDesligadoImpressao = true;

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, LOW);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);

  pinMode(flowSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), countPulse, RISING);

  // Cabeçalho CSV
  Serial.println("vazao,nivel,temp,ms");
}

void loop() {
  unsigned long now = millis();

  // ---------- Botão com debounce ----------
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastButtonDebounceTime = now;
  }
  if ((now - lastButtonDebounceTime) > debounceDelay) {
    if (reading != buttonReading) {
      buttonReading = reading;
      if (buttonReading == LOW) {
        estadoBotao = !estadoBotao; // toggle
        if (estadoBotao) {
          digitalWrite(relePin, HIGH);
          // reinicia timers e contadores ao ligar
          lastFlowMillis = now;
          lastUltrasonicMillis = now;
          lastTempMillis = now;
          lastPrintMillis = now;
          pulseCount = 0;
        } else {
          digitalWrite(relePin, LOW);
        }
      }
    }
  }
  lastButtonReading = reading;

  // ---------- Sistema Ligado ----------
  if (estadoBotao == 1) {
    sistemaDesligadoImpressao = true;

    // --- Vazão ---
    if (now - lastFlowMillis >= intervaloFlow) {
      lastFlowMillis += intervaloFlow;
      noInterrupts();
      unsigned int pulses = pulseCount;
      pulseCount = 0;
      interrupts();
      ultimaVazao = pulses / cali;
    }

    // --- Ultrassom ---
    if (now - lastUltrasonicMillis >= intervaloUltrassom) {
      lastUltrasonicMillis += intervaloUltrassom;
      float acumula = 0.0;
      int leiturasValidas = 0;

      for (int i = 0; i < 5; i++) {
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);

        unsigned long duration = pulseIn(echoPin, HIGH, 25000);
        if (duration > 0) {
          acumula += duration;
          leiturasValidas++;
        }
      }
      if (leiturasValidas > 0) {
        float cm = (acumula / leiturasValidas) / 29.0 / 2.0;
        ultimoNivel = (16.0 - cm) * 1.037 + 0.145;
      }
    }

    // --- Temperatura ---
    if (now - lastTempMillis >= intervaloTemp) {
      lastTempMillis += intervaloTemp;
      float tempAcumula = 0.0;
      int tempValidas = 0;
      for (int i = 0; i < 5; i++) {
        if (sens.readTemp()) {
          tempAcumula += sens.getTemp() - 1.0;
          tempValidas++;
        }
      }
      if (tempValidas > 0) {
        ultimaTemp = tempAcumula / tempValidas;
      }
    }

    // --- Impressão periódica ---
    if (now - lastPrintMillis >= printInterval) {
      lastPrintMillis += printInterval;
      Serial.print(ultimaVazao, 3);
      Serial.print(",");
      Serial.print(ultimoNivel, 3);
      Serial.print(",");
      Serial.print(ultimaTemp, 2);
      Serial.print(",");
      Serial.println(now);
    }
  }

  // ---------- Sistema Desligado ----------
  else {
    if (sistemaDesligadoImpressao) {
      Serial.println("Sistema desligado");
      sistemaDesligadoImpressao = false;
    }
    // zera pulsos acumulados
    noInterrupts();
    pulseCount = 0;
    interrupts();
  }
}

void countPulse() {
  pulseCount++;
}
