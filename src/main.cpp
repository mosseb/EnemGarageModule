#include <Arduino.h>
#include <Homie.h>

const int PIN_OPENED_SENSOR = D1;
const int PIN_CLOSED_SENSOR = D2;
const int PIN_GO_GARAGE = D6;
const int PIN_GO_GATE = D7;

const char DOOR_STATE_NOT_INITIALIZED = 'X';
const char DOOR_STATE_CLOSED = '0';
const char DOOR_STATE_OPENED = '1';
const char DOOR_STATE_UNKNOWN = '2';


const int GO_CMD_INTERVAL = 1000;

Bounce openedDebouncer = Bounce();
Bounce closedDebouncer = Bounce();
HomieNode garageNode("garage", "garage");

char lastDoorState = DOOR_STATE_NOT_INITIALIZED;

bool goGarage = false;
bool goGate = false;
unsigned long lastGoGarageCmd = 0;
unsigned long lastGoGateCmd = 0;


bool garageGoHandler(const HomieRange& range, const String& value)
{
  goGarage=true;
  return true;
}

bool gateGoHandler(const HomieRange& range, const String& value)
{
  goGate=true;
  return true;
}

void loopHandler()
{
  int openedValue = openedDebouncer.read();
  int closedValue = closedDebouncer.read();

  char currentDoorState;

  if (openedValue && !closedValue)
  {
    currentDoorState = DOOR_STATE_OPENED;
  }
  if(closedValue && !openedValue)
  {
    currentDoorState = DOOR_STATE_CLOSED;
  }
  if((!closedValue && !openedValue) || (closedValue && openedValue))
  {
    currentDoorState = DOOR_STATE_UNKNOWN;
  }

  if (currentDoorState != lastDoorState)
  {
    char msg[2] = {0,0};
    msg[0] = currentDoorState;
    garageNode.setProperty("state").send(msg);
    lastDoorState = currentDoorState;
  }


  // GESTION PORTE GARAGE
  if (goGarage)
  {
    if (lastGoGarageCmd==0)
    {
      digitalWrite(PIN_GO_GARAGE, HIGH);
      lastGoGarageCmd = millis();
    }
    else if (millis() - lastGoGarageCmd >= GO_CMD_INTERVAL)
    {
        digitalWrite(PIN_GO_GARAGE, LOW);
        lastGoGarageCmd = 0;
        goGarage = false;
    }
  }

  // GESTION PORTAIL
  if (goGate)
  {
    if (lastGoGateCmd==0)
    {
      digitalWrite(PIN_GO_GATE, HIGH);
      lastGoGateCmd = millis();
    }
    else if (millis() - lastGoGateCmd >= GO_CMD_INTERVAL)
    {
        digitalWrite(PIN_GO_GATE, LOW);
        lastGoGateCmd = 0;
        goGate = false;
    }
  }
}

void setup()
{
  Serial.begin(115200);

  Homie_setFirmware("garage", "1.0.0");
  Homie.setLoopFunction(loopHandler);
  Homie.setup();

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
