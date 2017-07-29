
// TODO Consider using MIDI library
// TODO Mirror to MIDI-over-USB
void programChange(int channel, int program)
{
  Serial1.write(0xC0 + ((channel - 1) & 0xF));
  Serial1.write(program & 0x7F);
}

void noteOn(int channel, int pitch, int velocity) {
  Serial1.write(0x90 + ((channel- 1) & 0xF));
  Serial1.write(pitch & 0x7F);
  Serial1.write(velocity & 0x7F);
}

void noteOff(int channel, int pitch, int velocity) {
  Serial1.write(0x80 + ((channel- 1) & 0xF));
  Serial1.write(pitch & 0x7F);
  Serial1.write(velocity & 0x7F);
}

int octave = -2;

// TODO Consider replacing this with some sort of counter for bounce rejection
bool keyscanWasOn[36] = {false, false, false, false, false, false,
                         false, false, false, false, false, false,
                         false, false, false, false, false, false,
                         false, false, false, false, false, false,
                         false, false, false, false, false, false,
                         false, false, false, false, false, false};

const int rows[6] = {3, 4, 16, 17, 22, 23};
const int columns[6] = {2, 14, 7, 8, 6, 20};

void setup() {

  Serial1.begin(31250); // MIDI on Serial1

  for (int i = 0; i < 6; i++)
  {
     int row = rows[i];
     digitalWrite(row, HIGH);
     pinMode(row, OUTPUT);
  }

  for (int i = 0; i < 6; i++)
  {
     int column = columns[i];
     pinMode(column, INPUT_PULLUP);
  }
  
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  delayMicroseconds(50); // Wait for pull-up resistors
}

void loop() {
  bool isOn[36]; // Not sure this is necessary, just trying to make the read as fast as possible

  for (int rowIdx = 0; rowIdx < 6; rowIdx++)
  {
    int row = rows[rowIdx];
    digitalWrite(row, LOW);
    delayMicroseconds(5); // This seems to be necessary, otherwise the pins don't seem to update fast enough

    // TODO Optimize this - column pins are all on the same port (PT-Dx)
    for (int colIdx = 0; colIdx < 6; colIdx++)
    {
      int col = columns[colIdx];
      isOn[(rowIdx * 6) + colIdx] = !digitalRead(col);
    }

    digitalWrite(row, HIGH);
    delayMicroseconds(5); // This seems to be necessary, otherwise the pins don't seem to update fast enough
  }

  for (int i = 0; i < 26; i += 2)
  {
    bool noteIsOn = isOn[i];
    if (keyscanWasOn[i] != noteIsOn)
    {
      keyscanWasOn[i] = noteIsOn;
      int note = (i >> 1) + 48 + octave*12;
      // TODO Remember what note the pedal key mapped to, such that it can be properly turned off even if transposing (to avoid "hanging" tones)
      if (noteIsOn)
      {
        noteOn(1, note, 120);
        digitalWrite(13, HIGH);
      }
      else
      {
        noteOff(1, note, 0x01);
        digitalWrite(13, LOW);
      }
    }
  }

  delay(5);
}
