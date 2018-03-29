#include <Arduino.h>
#include <Homie.h>

const int PIN_OPENED_SENSOR = D1;
const int PIN_CLOSED_SENSOR = D2;
const int PIN_GO_GARAGE = D6;
const int PIN_GO_GATE = D7;

const char DOOR_STATE_NOT_INITIALIZED = '?';
const char DOOR_STATE_CLOSED = 'c';
const char DOOR_STATE_OPENED = 'o';
const char DOOR_STATE_UNKNOWN = 'u';
const char DOOR_STATE_INCOHERENT = 'i';

const int GO_CMD_INTERVAL = 1000;
const int GO_CMD_RETRY_INTERVAL = 3000;

Bounce openedDebouncer = Bounce();
Bounce closedDebouncer = Bounce();
HomieNode garageNode("garage", "garage");

char lastGarageState = DOOR_STATE_NOT_INITIALIZED;

char goGarageInitState = DOOR_STATE_NOT_INITIALIZED;
unsigned long goGarageChrono = 0;
enum State_enum {NONE, INIT, RELAY_WAIT, WAIT_RETRY};
uint8_t garageGoState = NONE;
int goGarageRetry = 0;

bool goGate = false;
unsigned long lastGoGateCmd = 0;

bool garageGoHandler(const HomieRange& range, const String& value)
{
  garageGoState = INIT;
  return true;
}

bool gateGoHandler(const HomieRange& range, const String& value)
{
  goGate=true;
  Homie.getLogger() << "GoGate !!!" << endl;
  return true;
}

void loopHandler()
{
  //1.Mise à jour état garage en fonction des 2 capteurs
  //1.1.Lecture des 2 capteurs
  int openedValue = openedDebouncer.read();
  int closedValue = closedDebouncer.read();

  //1.2.Aggrégation des 2 valeurs de capteurs pour déterminer l'état
  char currentGarageState;

  if(openedValue && !closedValue) { currentGarageState = DOOR_STATE_OPENED; }
  if(closedValue && !openedValue) { currentGarageState = DOOR_STATE_CLOSED; }
  if(!closedValue && !openedValue) { currentGarageState = DOOR_STATE_UNKNOWN; }
  if(closedValue && openedValue) { currentGarageState = DOOR_STATE_INCOHERENT; }

  //1.3.Si l'état a changé, publication de la propriété homie et stockage
  if (currentGarageState != lastGarageState)
  {
    char msg[2] = {0,0};
    msg[0] = currentGarageState;
    garageNode.setProperty("state").send(msg);
    lastGarageState = currentGarageState;
  }

  //2.Gestion porte garage
  //  Si l'ordre est reçue sur la propriété (goGarage)
  //  -On mémorise l'état courant
  //  -On active le relais pendant 1 seconde
  //  -Au bout de 5 secondes, on vérifie si l'état a changé
  //  -Si l'état a changé plus rien à faire
  //  -Si l'état n'a pas changé, on réactive le relais, et ce, 5 fois d'affilé
  //  -Au bout de 5 fois on abandonne
  switch(garageGoState)
  {
    case INIT:
      Homie.getLogger() << "Garage door : Relay on, trying to move garage door" << endl;
      goGarageRetry = 0;
      goGarageInitState = lastGarageState;
      digitalWrite(PIN_GO_GARAGE, LOW);
      goGarageChrono = millis();
      garageGoState = RELAY_WAIT;
      break;
    case RELAY_WAIT:
      if (millis() - goGarageChrono >= GO_CMD_INTERVAL)
      {
        digitalWrite(PIN_GO_GARAGE, HIGH);
        if(goGarageInitState == DOOR_STATE_OPENED || goGarageInitState == DOOR_STATE_CLOSED)
        {
          Homie.getLogger() << "Garage door : Relay off, waiting for check" << endl;
          goGarageChrono = millis();
          garageGoState = WAIT_RETRY;
        }
        else
        {
          Homie.getLogger() << "Garage door : Was not initially fully opened or closed, no check" << endl;
          garageGoState = NONE;
        }
      }
      break;
    case WAIT_RETRY:
      if (millis() - goGarageChrono >= GO_CMD_RETRY_INTERVAL)
      {
        if(lastGarageState != goGarageInitState)
        {
          Homie.getLogger() << "Garage door : Correctly moved" << endl;
          garageGoState = NONE;
        }
        else
        {
          if(goGarageRetry < 5)
          {
            goGarageRetry++;
            Homie.getLogger() << "Garage door : Did not move, retry #" << goGarageRetry << endl;
            goGarageChrono = millis();
            digitalWrite(PIN_GO_GARAGE, LOW);
            garageGoState = RELAY_WAIT;
          }
          else
          {
            Homie.getLogger() << "Garage door : Did not move, too much retries" << endl;
            garageGoState = NONE;
          }
        }
      }
      break;
  }

  // GESTION PORTAIL
  if (goGate)
  {
    if (lastGoGateCmd==0)
    {
      digitalWrite(PIN_GO_GATE, LOW);
      lastGoGateCmd = millis();
    }
    else if (millis() - lastGoGateCmd >= GO_CMD_INTERVAL)
    {
        digitalWrite(PIN_GO_GATE, HIGH);
        lastGoGateCmd = 0;
        goGate = false;
        Homie.getLogger() << "GoGate fini !!!" << endl;
    }
  }
}

void setup()
{
  Serial.begin(115200);

  Homie_setFirmware("garage", "1.0.3");
  Homie.setLoopFunction(loopHandler);
  Homie.setup();

  digitalWrite(PIN_GO_GATE, HIGH);
  digitalWrite(PIN_GO_GARAGE, HIGH);

  pinMode(PIN_OPENED_SENSOR,INPUT_PULLUP);
  pinMode(PIN_CLOSED_SENSOR,INPUT_PULLUP);

  pinMode(PIN_GO_GARAGE,OUTPUT);
  pinMode(PIN_GO_GATE,OUTPUT);

  openedDebouncer.attach(PIN_OPENED_SENSOR);
  openedDebouncer.interval(50);

  closedDebouncer.attach(PIN_CLOSED_SENSOR);
  closedDebouncer.interval(50);

  garageNode.advertise("state");
  garageNode.advertise("goGarage").settable(garageGoHandler);
  garageNode.advertise("goGate").settable(gateGoHandler);
}

void loop()
{
    Homie.loop();
    openedDebouncer.update();
    closedDebouncer.update();
}
