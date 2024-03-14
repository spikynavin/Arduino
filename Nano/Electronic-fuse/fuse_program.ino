// Fuse program by naveenraj

#include <SoftwareSerial.h>

SoftwareSerial BTSerial(7, 6); // RX | TX

String readString;

const int fusepin = 5;


void setup()
{
  BTSerial.begin(115200);
  BTSerial.println("Electric fuse ready");
  pinMode(fusepin, OUTPUT);
  digitalWrite(fusepin, HIGH);
}

void loop()
{
  while (BTSerial.available() > 0)
  {
    char cmd = BTSerial.read();
    Serial.println(cmd);

    switch(cmd)
    {
      case 'F':
          fuse_on();
          break;
      case 'S':
          fuse_off();
          break;
    }
  }
}

void fuse_on()
{
  digitalWrite(fusepin, LOW);
  //BTSerial.write("fuse on\n");
}

void fuse_off()
{
  digitalWrite(fusepin, HIGH);
  //BTSerial.write("fuse off\n");
}