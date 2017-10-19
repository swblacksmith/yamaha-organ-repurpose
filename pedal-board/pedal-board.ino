#include <FastLED.h>

#define ONBOARD_LED_PIN 13

#define NUM_LEDS 6
#define LED_DATA_PIN 5
CRGB leds[NUM_LEDS];

#define OCTAVE_LEFT_LED 1
#define OCTAVE_RIGHT_LED 0

#define PROGRAM_LEFT_LED 3
#define PROGRAM_RIGHT_LED 2

#define CRGB_BACKGROUND CRGB::Black


// TODO Consider using MIDI library
// TODO Mirror to MIDI-over-USB
void programChange(int channel, int program)
{
  Serial1.write(0xC0 + ((channel - 1) & 0xF));
  Serial1.write(program & 0x7F);
}

void noteOn(int channel, int pitch, int velocity) {
  Serial1.write(0x90 + ((channel - 1) & 0xF));
  Serial1.write(pitch & 0x7F);
  Serial1.write(velocity & 0x7F);
}

void noteOff(int channel, int pitch, int velocity) {
  Serial1.write(0x80 + ((channel - 1) & 0xF));
  Serial1.write(pitch & 0x7F);
  Serial1.write(velocity & 0x7F);
}

typedef struct colorPairRgb {
  CRGB left;
  CRGB right;
} ColorPairRGB;

ColorPairRGB standardColorPairs[] = {
  {CRGB::Green, CRGB_BACKGROUND},
  {CRGB::Cyan, CRGB_BACKGROUND},
  {CRGB::Blue, CRGB_BACKGROUND},
  {CRGB::Purple, CRGB_BACKGROUND},
  {CRGB::Red, CRGB_BACKGROUND},
  {CRGB::OrangeRed, CRGB_BACKGROUND},
  {CRGB_BACKGROUND, CRGB_BACKGROUND},
  {CRGB_BACKGROUND, CRGB::OrangeRed},
  {CRGB_BACKGROUND, CRGB::Red},
  {CRGB_BACKGROUND, CRGB::Purple},
  {CRGB_BACKGROUND, CRGB::Blue},
  {CRGB_BACKGROUND, CRGB::Cyan},
  {CRGB_BACKGROUND, CRGB::Green},
};

#define NUM_COLOR_PAIR 13


#define MIN_OCTAVE 0
#define MAX_OCTAVE 8

#define MIN_PROGRAM 0
#define MAX_PROGRAM (NUM_COLOR_PAIR - 1 + MIN_PROGRAM)

int octave = 1;
int program = 0;

// TODO Consider replacing this with some sort of counter for bounce rejection
bool keyscanWasOn[] = {false, false, false, false, false, false,
                       false, false, false, false, false, false,
                       false, false, false, false, false, false,
                       false, false, false, false, false, false,
                       false, false, false, false, false, false,
                       false, false, false, false, false, false
                      };

const int rows[] = {3, 4, 16, 17, 22, 23};
#define NUM_ROWS 6
const int columns[] = {2, 14, 7, 8, 6, 20};
#define NUM_COLUMNS 6

bool updateLeds = false;

#define NOTE_NONE -1
#define NUM_KEYS 13
int notesHeldByKeys[NUM_KEYS] = {NOTE_NONE};

void releasePreviousNoteByKey(int key)
{
  int noteHeld = notesHeldByKeys[key];
  if (noteHeld != NOTE_NONE)
  {
    noteOff(1, noteHeld, 0x01); // TODO Figure out whether to use note off velocity 0 or 1
    digitalWrite(ONBOARD_LED_PIN, LOW);
    notesHeldByKeys[key] = NOTE_NONE;
  }
}


void setup() {

  // sanity check delay - allows reprogramming if accidently blowing power w/leds
  delay(2000);

  FastLED.addLeds<WS2811, LED_DATA_PIN, RGB>(leds, NUM_LEDS);

  for (int i = 0; i < NUM_LEDS; ++i)
  {
    leds[i] = CRGB_BACKGROUND;
  }

  FastLED.show();
  updateLeds = true;

  Serial1.begin(31250); // MIDI on Serial1

  for (int i = 0; i < NUM_ROWS; i++)
  {
    int row = rows[i];
    digitalWrite(row, HIGH);
    pinMode(row, OUTPUT);
  }

  for (int i = 0; i < NUM_COLUMNS; i++)
  {
    int column = columns[i];
    pinMode(column, INPUT_PULLUP);
  }

  pinMode(ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(ONBOARD_LED_PIN, LOW);

  delayMicroseconds(50); // Wait for pull-up resistors

  programChange(1, program);
}

void loop() {
  bool isOn[NUM_ROWS * NUM_COLUMNS]; // Not sure this is necessary, just trying to make the read as fast as possible

  for (int rowIdx = 0; rowIdx < NUM_ROWS; rowIdx++)
  {
    int row = rows[rowIdx];
    digitalWrite(row, LOW);
    delayMicroseconds(5); // This seems to be necessary, otherwise the pins don't seem to update fast enough

    // TODO Optimize this - column pins are all on the same port (PT-Dx)
    for (int colIdx = 0; colIdx < NUM_COLUMNS; colIdx++)
    {
      int col = columns[colIdx];
      isOn[(rowIdx * NUM_ROWS) + colIdx] = !digitalRead(col);
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
      int key = (i >> 1);

      releasePreviousNoteByKey(key);

      int note = key + 12 + octave * 12; // Assuming key/center C is C4 (octave 4)
      // TODO Remember what note the pedal key mapped to, such that it can be properly turned off even if transposing (to avoid "hanging" tones)
      if (noteIsOn)
      {
        noteOn(1, note, 120);
        notesHeldByKeys[key] = note;
        digitalWrite(ONBOARD_LED_PIN, HIGH);
      }
    }
  }

  for (int i = 30; i < 36; i++)
  {
    bool noteIsOn = isOn[i];
    if (keyscanWasOn[i] != noteIsOn)
    {
      keyscanWasOn[i] = noteIsOn;
      switch (i)
      {
        case 32:
          if (noteIsOn)
          {
            program--;
            if (program < MIN_PROGRAM)
            {
              program = MIN_PROGRAM;
            }
            programChange(1, program);
            updateLeds = true;
          }
          break;
        case 33:
          if (noteIsOn)
          {
            program++;
            if (program > MAX_PROGRAM)
            {
              program = MAX_PROGRAM;
            }
            programChange(1, program);
            updateLeds = true;
          }
          break;
        case 34:
          if (noteIsOn)
          {
            octave--;
            if (octave < MIN_OCTAVE)
            {
              octave = MIN_OCTAVE;
            }

            updateLeds = true;
          }
          break;
        case 35:
          if (noteIsOn)
          {
            octave++;
            if (octave > MAX_OCTAVE)
            {
              octave = MAX_OCTAVE;
            }

            updateLeds = true;
          }
          break;
      }
    }
  }

  if (updateLeds)
  {
    ColorPairRGB octaveRgb = standardColorPairs[octave + 2];
    leds[OCTAVE_LEFT_LED] = octaveRgb.left;
    leds[OCTAVE_RIGHT_LED] = octaveRgb.right;

    ColorPairRGB programRgb = standardColorPairs[program - MIN_PROGRAM];
    leds[PROGRAM_LEFT_LED] = programRgb.left;
    leds[PROGRAM_RIGHT_LED] = programRgb.right;

    FastLED.show();
    updateLeds = false;
  }

  delay(5);
}
