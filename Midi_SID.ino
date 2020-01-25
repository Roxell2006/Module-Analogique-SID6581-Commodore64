// Synthétiseur SID
// Ecrit par Romuald Hansen
// Decembre 2013

#include <avr/pgmspace.h>
#include <MIDI.h>

// Constants for Waveform
#define TRIANGLE 0x10
#define SAWTOOTH 0x20
#define PULSE    0x40
#define NOISE    0x80

// Frequency table for a 1MHz SID
const unsigned int freq[] PROGMEM = { 
  244,  274,  291,  308,  326,  346,  366,  388,  411,  435,  461, 
  489,  549,  581,  616,  652,  691,  732,  776,  822,  871,  923,
  976,  1036, 1097, 1163, 1232, 1305, 1383, 1465, 1552, 1644, 1742,
  1845, 1955, 2071, 2195, 2325, 2463, 2610, 2765, 2930, 3104, 3288,
  3484, 3691, 3910, 4143, 4389, 4650, 4927, 5220, 5530, 5859, 6207,
  6577, 6968, 7382, 7821, 8286, 8779, 9301, 9854,10440,11060,11718,
  12415,13153,13935,14764,15642,16572,17557,18601,19709,20897,22121,
  23436,24830,26306,27871,29528,31234,33144,35115,37203,39415,41759,
  44242,46873,49660,52613,55741,59056,62567 };
  
const int PROGRAM [5][12]= { // tableau à 2 dimensions
  {0,15,15,8,0,TRIANGLE, 240,0,1,1,0,0},
  {0,15,15,8,0,SAWTOOTH, 0,8,1,0,0,1},
  {0,15,15,7,2080,PULSE, 254,8,1,1,0,0},
  {0,15,15,7,0,NOISE,    64,0,1,0,0,1},
  {10,10,0,10,3200,PULSE,70,15,1,0,0,1}
};  

int MODE = 0;
int note = 0;
int NOTEMEMORY[3];

// Specify ADSR here (0 to 15)
int ATTACK[3] = {0,0,0};
int DECAY[3]  = {15,15,15};
int SUSTAIN[3] = {15,15,15};
int RELEASE[3] = {7,7,7};

// Set filter value from 0 to 255 Resonance is from 0 to 15.
int CUTOFF = 192;
int RESONANCE = 15;
int VOLUME = 15;

// Specify Pulse Width here (0 to 4095)
// 2048 makes a square wave
int PULSEW[3] = {2048,2048,2048};
int WAVEFORM[3] = {SAWTOOTH, TRIANGLE, PULSE};

// Filter settings. Filters are either enabled (1) or disabled (0)
int FILTER = 1;
int LP = 0;
int BP = 1;
int HP = 0;

void setup () 
{
  // First, intialise the ports.
  PORTD = 0;
  PORTB = 0x10;  // PB4 is high to disable Chip Select
  DDRD = 0xFE;   // Keep RxD as an input, everything else output
  DDRB = 0xFF;

  // Initialise Timer 0 OC0A to 1MHz on Pin 6.
  TIMSK0 = 0;
  TCNT0 = 0;     // Reset timer
  OCR0A = 0;     // Invert clock after this many clock cycles + 1
  TCCR0A = (1<<COM0A0) | (2<<WGM00);
  TCCR0B = (2<<CS00); // Prescaler
  Serial.flush();
  
  initSID();
  
  MIDI.begin(MIDI_CHANNEL_OMNI); 
  MIDI.setHandleNoteOn(MyHandleNoteOn);
  MIDI.setHandleNoteOff(MyHandleNoteOff); 
  MIDI.setHandleControlChange(MyHandleControlChange); 
  MIDI.setHandleProgramChange(MyHandleProgramChange); 
  
}

void loop () 
{
  MIDI.read();        
}

void MyHandleNoteOn(byte channel, byte pitch, byte velocity) 
{ 
  int f,vo;
  if(MODE == 1)
  {
    if(channel < 4)
    {
       vo = (channel-1)*7;
       if (velocity > 0)
       {
          f = pgm_read_word(&freq[pitch-12]);  
          wrSID(0+vo,f);                     
          wrSID(1+vo,f>>8);
          wrSID(4+vo,WAVEFORM[channel-1]|1);
        }
        else
          wrSID(4+vo,WAVEFORM[channel-1]);
    }
  }
  if(MODE == 0)
  {
     int f,vo,i;
     if(channel == 1)
     {
        if (velocity > 0)
        {
          note = note + 1;
          if(note > 3)
            note = 1;
          vo = (note-1)*7;  
          f = pgm_read_word(&freq[pitch-12]);  
          wrSID(0+vo,f);                     
          wrSID(1+vo,f>>8);
          wrSID(4+vo,WAVEFORM[0]|1);
          NOTEMEMORY[note-1]=pitch;
        }
        else
        {
          for (i=0;i<3;i++)
          {
            if (NOTEMEMORY[i] == pitch)
            {
               wrSID(4+(i*7),WAVEFORM[0]);
               note = i;
               NOTEMEMORY[i] = 0;
            }
          }
        }
     } 
  }
}

void MyHandleNoteOff(byte channel, byte pitch, byte velocity)
{
  if(MODE == 1)
  {
    if(channel <4)
    {
       wrSID(4+((channel-1)*7),WAVEFORM[channel-1]);
    }
  }
  if(MODE == 0)
  {
     int i;
     for (i=0;i<3;i++)
     {
        if (NOTEMEMORY[i] == pitch)
        {
           wrSID(4+(i*7),WAVEFORM[0]);
           note = i;
           NOTEMEMORY[i] = 0;
        }
     }
  }  
}

void MyHandleControlChange (byte channel, byte number, byte value)
{
  if ( number == 73 )
    ATTACK[channel-1] = value/8;
  if ( number == 70 )
    DECAY[channel-1] = value/8;
  if ( number == 75 )
    SUSTAIN[channel-1] = value/8;
  if ( number == 72 )
    RELEASE[channel-1] = value/8;
  if ( number == 76 )
    PULSEW[channel-1] = ((value+1)*32)-1;    
  if ( number == 74 )
    CUTOFF = value*2;
  if ( number == 71 )
    RESONANCE = value/8;
  if ( number == 7 )
    VOLUME = value/8;
  if ( number == 80)
  {
     if( value < 64 )
        MODE = 0;
     if( value > 63 )
        MODE = 1;
  }  
  if ( number == 77 )
  {
     if( value < 43 )
       LP=1, BP=0, HP=0;
     if( value > 42 && value < 86)
       LP=0, BP=1, HP=0; 
     if( value > 85 )
       LP=0, BP=0, HP=1;
  }  
  
  ChangeSid(channel);
}
void MyHandleProgramChange (byte channel, byte number)
{
  if (number <5)
  {
    ATTACK[channel-1]    = PROGRAM[number][0];
    DECAY[channel-1]     = PROGRAM[number][1];
    SUSTAIN[channel-1]   = PROGRAM[number][2];
    RELEASE[channel-1]   = PROGRAM[number][3];
    PULSEW[channel-1]    = PROGRAM[number][4];
    WAVEFORM[channel-1]  = PROGRAM[number][5];
    CUTOFF               = PROGRAM[number][6];
    RESONANCE            = PROGRAM[number][7];
    FILTER               = PROGRAM[number][8];
    LP                   = PROGRAM[number][9];
    BP                   = PROGRAM[number][10];
    HP                   = PROGRAM[number][11];
  }
  ChangeSid(channel);
}

void initSID() 
{
  char i;     
  // Clear the SID
  for(i=0; i<25;i++)
    wrSID(i,0);

if (FILTER == 1)
{
  wrSID(22,CUTOFF);             // Set filter value
  wrSID(23,(RESONANCE<<4)|7);   // Set resonance and filtered channels
  wrSID(24, (HP<<6)|(BP<<5)|(LP<<4)|VOLUME);
}
else
  wrSID(24,VOLUME);

  // Set ADSR for three channels
  for(i=0; i<21; i+=7) 
  {
    wrSID(5+i,DECAY[i/7]|(ATTACK[i/7]<<4));      // Attack and Decay
    wrSID(6+i,RELEASE[i/7]|(SUSTAIN[i/7]<<4));   // Half volume on sustain, medium release
    wrSID(2+i,PULSEW[i/7]);    // Set pulse width low
    wrSID(3+i,PULSEW[i/7]>>8); // Set pulse width high
  }
}

void ChangeSid(int voix)
{
  if (MODE == 1)
  {
    voix = (voix -1) * 7;
    wrSID(5+voix,DECAY[voix]|(ATTACK[voix]<<4));      // Attack and Decay
    wrSID(6+voix,RELEASE[voix]|(SUSTAIN[voix]<<4));   // Half volume on sustain, medium release 
    wrSID(2+voix,PULSEW[voix]);    // Set pulse width low
    wrSID(3+voix,PULSEW[voix]>>8); // Set pulse width high
  }
  else
  {
    for (voix=0; voix<3; voix++)
    {
      wrSID(5+(voix*7),DECAY[0]|(ATTACK[0]<<4));      // Attack and Decay
      wrSID(6+(voix*7),RELEASE[0]|(SUSTAIN[0]<<4));   // Half volume on sustain, medium release 
      wrSID(2+(voix*7),PULSEW[0]);    // Set pulse width low
      wrSID(3+(voix*7),PULSEW[0]>>8); // Set pulse width high
    }
  }
        
  if (FILTER == 1 )
  {
      wrSID(22,CUTOFF);              // Set filter value
      wrSID(23,(RESONANCE<<4)|7);    // Set resonance and filtered channels
      wrSID(24, (HP<<6)|(BP<<5)|(LP<<4)|VOLUME);
   }
   else
      wrSID(24,VOLUME);
}

void SIDdelay () 
{  
  unsigned char i;
  for(i=0;i<10;i++) __asm("nop\n\t");
}

void wrSID (unsigned char port, unsigned char data) 
{
  // Send byte 'data' to address 'port'.
  // SIDdelay is inserted between operations for settling time. 

  PORTB = (port & 0x0F) | 0x10;  // Lower bit, keep CS deactive
  PORTD = ((port | 0x20) & 0xF0) >> 2; // Upper bit, reset high
  PORTD |= 0x80;    // Clock in Address
  SIDdelay();
  PORTD &= ~(0x80);

  PORTB = (data & 0x0F) | 0x10;  // Lower bit, keep CS deactive
  PORTD = (data & 0xF0) >> 2;    // Upper bit
  PORTB &= ~(0x10);   // Activate /CS
  SIDdelay();
  PORTB |= 0x10;      // Deactivate /CS
}  

      


