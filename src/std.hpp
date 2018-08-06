#ifndef _STD_H
#define _STD_H

#define INITIAL_START_LEVEL 10.0

#define INITIAL_BW_IN_SEMITONES 4.0

#define INITIAL_CARR_BW_IN_SEMITONES 4.0
#define INITIAL_MOD_BW_IN_SEMITONES 4.0
#define INITIAL_GAIN 1.0

#define LOWER_ENVELOPE_ATTACK_TEMPERATURE 0.01
#define INITIAL_ENVELOPE_ATTACK_TEMPERATURE 1
#define UPPER_ENVELOPE_ATTACK_TEMPERATURE 100
#define LOWER_ENVELOPE_RELEASE_TEMPERATURE 0.01
#define INITIAL_ENVELOPE_RELEASE_TEMPERATURE 5
#define UPPER_ENVELOPE_RELEASE_TEMPERATURE 100
#define ENVELOPE_TEMPERATURE 10

#define MIN_PAN         -0.999 // Right
#define MAX_PAN          0.999 // Left
#define INITIAL_PANNING 0.5
#define MAX_PANNING 1.0

#define CENTER_PAN_VALUE 0.0
#define INITIAL_PAN   CENTER_PAN_VALUE
#define INITIAL_PAN_OFFSET ((MAX_PAN - MIN_PAN) / 5)

#define UPPER_SMOOTHING_BAND 20
#define SMOOTHING_FACTOR 10
#define PI 3.1415828
#define FFSAMP 44100.0
#define NR_OF_BANDS 31
#define MAX_PAN 0.999
#define FALSE 0
#define TRUE 1

#endif