/** Author:       Plyashkevich Viatcheslav <plyashkevich@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * All rights reserved.
 */

#include <stdio.h>
#include <time.h>

/* #include "timer.h" */

#define INT16 short
#define UINT16 unsigned short
#define INT32 int
#define UINT32 unsigned int

static inline INT32 MPY48SR(INT16 o16, INT32 o32)
{
    UINT32   Temp0;
    INT32    Temp1;
    Temp0 = (((UINT16)o32 * o16) + 0x4000) >> 15;
    Temp1 = (INT16)(o32 >> 16) * o16;
    return (Temp1 << 1) + Temp0;
}

static void goertzel_filter(INT16 Koeff0, INT16 Koeff1, const INT16 arraySamples[], INT32 *Magnitude0, INT32 *Magnitude1, UINT32 COUNT)
{
    INT32 Temp0, Temp1;
    UINT16 ii;
    INT32 Vk1_0 = 0, Vk2_0 = 0, Vk1_1 = 0, Vk2_1 = 0;

    for(ii = 0; ii < COUNT; ++ii)
    {
        Temp0 = MPY48SR(Koeff0, Vk1_0 << 1) - Vk2_0 + arraySamples[ii],
        Temp1 = MPY48SR(Koeff1, Vk1_1 << 1) - Vk2_1 + arraySamples[ii];
        Vk2_0 = Vk1_0,
              Vk2_1 = Vk1_1;
        Vk1_0 = Temp0,
              Vk1_1 = Temp1;
    }

    Vk1_0 >>= 10,
          Vk1_1 >>= 10,
          Vk2_0 >>= 10,
          Vk2_1 >>= 10;
    Temp0 = MPY48SR(Koeff0, Vk1_0 << 1),
          Temp1 = MPY48SR(Koeff1, Vk1_1 << 1);
    Temp0 = (INT16)Temp0 * (INT16)Vk2_0,
          Temp1 = (INT16)Temp1 * (INT16)Vk2_1;
    Temp0 = (INT16)Vk1_0 * (INT16)Vk1_0 + (INT16)Vk2_0 * (INT16)Vk2_0 - Temp0;
    Temp1 = (INT16)Vk1_1 * (INT16)Vk1_1 + (INT16)Vk2_1 * (INT16)Vk2_1 - Temp1;
    *Magnitude0 = Temp0,
        *Magnitude1 = Temp1;
    return;
}


// This is a GSM function, for concrete processors she may be replaced
// for same processor's optimized function (norm_l)
static inline INT16 norm_l(INT32 L_var1)
{
    INT16 var_out;

    if (L_var1 == 0)
    {
        var_out = 0;
    }
    else
    {
        if (L_var1 == (INT32)0xffffffff)
        {
            var_out = 31;
        }
        else
        {
            if (L_var1 < 0)
            {
                L_var1 = ~L_var1;
            }

            for(var_out = 0;L_var1 < (INT32)0x40000000;var_out++)
            {
                L_var1 <<= 1;
            }
        }
    }

    return(var_out);
}

#define COEFF_NUMBER				10

/*
   Country		Busy_Tone		Dial_Tone
   USA  		480/620Hz		350/440Hz
   Singapore	270/320Hz		270/320Hz
   China		450Hz			450Hz			0.35sec
   */

/* cos(2pi*f/sample_rate)*2^15 */
static const INT16 CONSTANTS[COEFF_NUMBER] =
{32210, 31778, 31226, 30555, 0/* 28869*/,
    /*  270, 320-350, 420 ,440-480    620  */
    29769,   -1009, -12773, 22811, 0};
/* 520-570, 2000, 2500,   3000,  */
/* k = round(N*f/sample_rate), when N is too small, the difference between fi is small */
static INT32 T[COEFF_NUMBER];

#define powerThreshold 				328
#define dialTonesThreshold 			2500
/* in milli-second */
#define busyTonesInterval			350

/* samples per detect */
//#define SAMPLES 408					/* 408/8000=51ms */
#define SAMPLES 102						/* 105/8000=13ms */
/* Samples per packet in SPEEX */
#define SPEEX_SAMPLES	480

/* Array for input samples */
static INT16 pArraySamples[SPEEX_SAMPLES + SAMPLES];
static INT16 internalArray[SAMPLES];
static int  frameCount = 0;

static char prevDialButton = ' ';
/* Counter for tone changes */
static int ToneChanges = 0;
/* Counter for same tone */
static int counter = 0;

//-----------------------------------------------------------------
char Tone_detection(INT16 short_array_samples[])
{
    INT32 Dial = 32, Sum = 0;
    /* char return_value = ' '; */
    unsigned ii;

    INT32 Temp = 0;
    INT32 Tone = 0;

    /* calculate the average power */
    for(ii = 0; ii < SAMPLES; ii ++){
        if(short_array_samples[ii] >= 0)
            Sum += short_array_samples[ii];
        else
            Sum -= short_array_samples[ii];
    }
    Sum /= SAMPLES;
    if(Sum < powerThreshold)
        return ' ';

    /* Normalization */
    for(ii = 0; ii < SAMPLES; ii++){
        T[0] = (INT32)(short_array_samples[ii]);
        if(T[0] != 0){
            if(Dial > norm_l(T[0])){
                Dial = norm_l(T[0]);
            }
        }
    }
    Dial -= 16;

    for(ii = 0; ii < SAMPLES; ii++){
        T[0] = short_array_samples[ii];
        internalArray[ii] = (INT16)(T[0] << Dial);
    }

    //Frequency detection
    goertzel_filter(CONSTANTS[0], CONSTANTS[1], internalArray, &T[0], &T[1], SAMPLES);
    goertzel_filter(CONSTANTS[2], CONSTANTS[3], internalArray, &T[2], &T[3], SAMPLES);
    goertzel_filter(CONSTANTS[4], CONSTANTS[5], internalArray, &T[4], &T[5], SAMPLES);
    goertzel_filter(CONSTANTS[6], CONSTANTS[7], internalArray, &T[6], &T[7], SAMPLES);
    goertzel_filter(CONSTANTS[8], CONSTANTS[9], internalArray, &T[8], &T[9], SAMPLES);
    /*
       goertzel_filter(CONSTANTS[10], CONSTANTS[11], internalArray, &T[10], &T[11], SAMPLES);
       goertzel_filter(CONSTANTS[12], CONSTANTS[13], internalArray, &T[12], &T[13], SAMPLES);
       goertzel_filter(CONSTANTS[14], CONSTANTS[15], internalArray, &T[14], &T[15], SAMPLES);
       goertzel_filter(CONSTANTS[16], CONSTANTS[17], internalArray, &T[16], &T[17], SAMPLES);
       */

    //Find max tones
    for(ii = 0; ii < 7; ii++){
        if(Temp < T[ii]){
            Tone = ii;
            Temp = T[ii];
        }
    }

    if (T[Tone] > Sum/*dialTonesThreshold*/)
        return '0'+Tone;
    else
        return ' ';
}

/*
 * Tone detect alg.
 * Usage :
 * 2. toneDetecting(INT16 input_array[])
 *		input_array = samples captured, length = SPEEX_SAMPLES;
 *		return value : Detected string, NULL for detecting...
 *	Example : keep calling dtmfDetecting, untill get a valid char*
 *  Notice : A. input_array MUST has size of SPEEX_SAMPLES.
 *			 B. timeout MUST be determined by the caller.
 *
 */

int toneDetecting(INT16 input_array[])
{
    UINT32 ii;
    char temp_dial_button;

    UINT32 temp_index = 0;

    /* char log[256]; */

    /* append the incoming data */
    for(ii=0; ii < SPEEX_SAMPLES; ii++)
        pArraySamples[ii + frameCount] = input_array[ii];

    frameCount += SPEEX_SAMPLES;

    if(frameCount >= SAMPLES){
        while(frameCount >= SAMPLES){
            /* consumes SAMPLES and get a tone */
            temp_dial_button = Tone_detection(&pArraySamples[temp_index]);

            //if (ToneChanges > 1){
            //	printf("Tone: ToneChanges = %d, tone = %c\n",
            //		ToneChanges, temp_dial_button);
            //	write_log(log);
            //}

            /* Tone changed */
            if (temp_dial_button != prevDialButton){
                if ((temp_dial_button != ' ') && (prevDialButton != ' ') )
                    counter ++;
                else{
                    //	if ((counter < 16) && (counter > 4))			/* Last for 200ms(4x51)~900ms(16x51) */
                    if ((counter < 69) && (counter > 6))				/* Last for 91ms(7x13)~900ms(69x13) */
                        /* Tone changes successful */
                        ToneChanges++;
                    else
                        /* reset tone changes */
                        ToneChanges = 0;

                    /* reset counter for this tone */
                    counter = 0;
                    /* remember this tone */
                    prevDialButton = temp_dial_button;
                }
            }
            else{
                /* no changing for a while, reset everything. */
                if ((counter ++) > 72)   //20)
                    ToneChanges = 0;
            }

            temp_index += SAMPLES;
            frameCount -= SAMPLES;
        }

        /* store the un-consumed data */
        for(ii=0; ii < frameCount; ii++){
            pArraySamples[ii] = pArraySamples[ii + temp_index];
        }
    }

    return ToneChanges;
}



