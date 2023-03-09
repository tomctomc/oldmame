#include "driver.h"
#include <math.h>


#define SOUND_CLOCK 1536000 /* 1.536 Mhz */

#define TONE_LENGTH 2000
#define TONE_PERIOD 4
#define NOISE_LENGTH 8000
#define NOISE_RATE 1000
#define WAVE_AMPLITUDE 70
#define MAXFREQ 200
#define MINFREQ 80

#define STEP 1

static char *tone;
static char *noise;

static int shootsampleloaded = 0;
static int deathsampleloaded = 0;
static int F=2;
static int t=0;
static int LastPort1=0;
static int LastPort2=0;
static int lfo_rate=0;
static int lfo_active=0;
static int freq=MAXFREQ;
static int lforate0=0;
static int lforate1=0;
static int lforate2=0;
static int lforate3=0;

static unsigned char waveform1[32] =
{
   0x88, 0x88, 0x88, 0x88, 0xaa, 0xaa, 0xaa, 0xaa,
   0xcc, 0xcc, 0xcc, 0xcc, 0xee, 0xee, 0xee, 0xee,
   0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22,
   0x44, 0x44, 0x44, 0x44, 0x66, 0x66, 0x66, 0x66,
};
static unsigned char waveform2[32] =
{
   0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
   0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
   0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
   0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc
};

static int channel;



void mooncrst_sound_freq_w(int offset,int data)
{

	if (data && data != 0xff) osd_adjust_sample(channel+0,(SOUND_CLOCK/16)/(256-data)*32*F,128);
	else osd_adjust_sample(channel+0,1000,0);

}


void mooncrst_noise_w(int offset,int data)
{
        if (deathsampleloaded)
        {
           if (data & 1 && !(LastPort1 & 1))
              osd_play_sample(channel+1,Machine->samples->sample[1]->data,
                           Machine->samples->sample[1]->length,
                           Machine->samples->sample[1]->smpfreq,
                           Machine->samples->sample[1]->volume,0);
           LastPort1=data;
        }
        else
        {
  	  if (data & 1) osd_adjust_sample(channel+1,NOISE_RATE,255);
	  else osd_adjust_sample(channel+1,NOISE_RATE,0);
        }
}

void mooncrst_background_w(int offset,int data)
{
       if (data & 1)
       lfo_active=1;
       else
       lfo_active=0;
}

void mooncrst_shoot_w(int offset,int data)
{

      if (data & 1 && !(LastPort2 & 1) && shootsampleloaded)
         osd_play_sample(channel+2,Machine->samples->sample[0]->data,
                           Machine->samples->sample[0]->length,
                           Machine->samples->sample[0]->smpfreq,
                           Machine->samples->sample[0]->volume,0);
      LastPort2=data;
}


int mooncrst_sh_start(void)
{
	int i;


	channel = get_play_channels(5);

	if (Machine->samples != 0 && Machine->samples->sample[0] != 0)    /* We should check also that Samplename[0] = 0 */
	  shootsampleloaded = 1;
	else
	  shootsampleloaded = 0;

	if (Machine->samples != 0 && Machine->samples->sample[1] != 0)    /* We should check also that Samplename[0] = 0 */
	  deathsampleloaded = 1;
	else
	  deathsampleloaded = 0;

	if ((tone = malloc(TONE_LENGTH)) == 0)
		return 1;
	if ((noise = malloc(NOISE_LENGTH)) == 0)
	{
		free(tone);
		return 1;
	}

	for (i = 0;i < NOISE_LENGTH;i++)
		noise[i] = (rand() % (2*WAVE_AMPLITUDE)) - WAVE_AMPLITUDE;
	for (i = 0;i < TONE_LENGTH;i++)
		tone[i] = WAVE_AMPLITUDE * sin(2*M_PI*i/TONE_PERIOD);

        osd_play_sample(channel+0,waveform1,32,1000,0,1);
        if (!deathsampleloaded)
	   	    osd_play_sample(channel+1,noise,NOISE_LENGTH,NOISE_RATE,0,1);

        osd_play_sample(channel+3,waveform2,32,1000,0,1);
        osd_play_sample(channel+4,waveform2,32,1000,0,1);

	return 0;
}



void mooncrst_sh_stop(void)
{
	free(noise);
	free(tone);
	osd_stop_sample(channel+0);
	osd_stop_sample(channel+1);
	osd_stop_sample(channel+2);
	osd_stop_sample(channel+3);
	osd_stop_sample(channel+4);
}

void mooncrst_sound_freq_sel_w(int offset,int data)
{
        if (offset==1 && (data & 1))
            F=1;
        else
            F=2;
}

void mooncrst_lfo_freq_w(int offset,int data)
{
        if (offset==3) lforate3=(data & 1);
        if (offset==2) lforate2=(data & 1);
        if (offset==1) lforate1=(data & 1);
        if (offset==0) lforate0=(data & 1);
        lfo_rate=lforate3*8+lforate2*4+lforate1*2+lforate0;
        lfo_rate=16-lfo_rate;
}

void mooncrst_sh_update(void)
{
    if (lfo_active)
    {
      osd_adjust_sample(channel+3,freq*32,32);
      osd_adjust_sample(channel+4,(freq+60)*32,32);
      if (t==0)
         freq-=lfo_rate;
      if (freq<=MINFREQ)
         freq=MAXFREQ;
    }
    else
    {
      osd_adjust_sample(channel+3,1000,0);
      osd_adjust_sample(channel+4,1000,0);
    }
    t++;
    if (t==3) t=0;
}
