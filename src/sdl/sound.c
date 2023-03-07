
#include "osd_sdl.h"

#define AUDIO_FORMAT_IN  AUDIO_S16
#define AUDIO_FORMAT_OUT AUDIO_S16
#define AUDIO_FREQ_OUT   48000 /*44100*/

void init_sound(void)
{
    samples_per_frame = (double)Machine->sample_rate / (double)Machine->drv->frames_per_second;

    if( play_sound ) {
        // init sound
        audioChannel->desired.freq     = AUDIO_FREQ_OUT;
        audioChannel->desired.format   = AUDIO_FORMAT_OUT;
        audioChannel->desired.channels = 1;
        audioChannel->desired.samples  = pow(2,ceil(log(samples_per_frame)/log(2))); // power of two big enough to hold samples per frame
        audioChannel->desired.callback = audioCallback;
        audioChannel->desired.userdata = audioChannel;
        SDLCHK(30101,audioChannel->audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &audioChannel->desired, &audioChannel->obtained, 0));
        //printf( "TOMCXXX: desired.obtained freq     = %8d/%8d\n", audioChannel->desired.freq,     audioChannel->obtained.freq);
        //printf( "TOMCXXX: desired.obtained format   = %8d/%8d\n", audioChannel->desired.format,   audioChannel->obtained.format);
        //printf( "TOMCXXX: desired.obtained samples  = %8d/%8d\n", audioChannel->desired.samples,  audioChannel->obtained.samples);
        //printf( "TOMCXXX: desired.obtained channels = %8d/%8d\n", audioChannel->desired.channels, audioChannel->obtained.channels);
    }
}

int osd_start_audio_stream(int stereo)
{
    if (stereo) stereo = 1;    /* make sure it's either 0 or 1 */

    stream_cache_stereo = stereo;

    /* determine the number of samples per frame */
    samples_per_frame = (double)Machine->sample_rate / (double)Machine->drv->frames_per_second;

    /* compute how many samples to generate this frame */
    samples_left_over = samples_per_frame;
    samples_this_frame = (UINT32)samples_left_over;
    samples_left_over -= (double)samples_this_frame;

    audio_buffer_length = NUM_BUFFERS * samples_per_frame + 20;


    if (Machine->sample_rate == 0) return 0;

    //printf( "TOMCXXX: osd_start_audio_stream(stereo=%d)   samples_per_frame=%d   samples_left_over=%f   samples_this_frame=%d\n", stereo, (int) samples_per_frame, (float) samples_left_over, (int) samples_this_frame);

    stream_playing = 1;
    voice_pos = 0;

    return samples_this_frame;
}

void osd_stop_audio_stream(void)
{
    if (Machine->sample_rate == 0) return;

    stop_all_sound();

    stream_playing = 0;
}

void new_audio_channel_data(void *data,int len,int isstream)
{
    //printf("TOMCXXX: new_audio_channel_data(%p, %d, %d, %d, %d)\n", data,len,isstream);
    if( audioChannel->audioStream && !isstream  ) {
        // going from stream to non-stream
        free_audio_channel_stream(audioChannel);
    }
    else if( !audioChannel->audioStream && isstream ) {
        // going from non-stream to stream
        free_audio_channel_stream(audioChannel);
        audioChannel->audioStream = SDL_NewAudioStream( AUDIO_FORMAT_IN,  1, Machine->sample_rate,
                                                        AUDIO_FORMAT_OUT, 1, AUDIO_FREQ_OUT );
    }

    if( audioChannel->audioStream ) {
        //printf( "TOMCXXX ********** PUTTING AUDIOSTREAM %p  d=%p l=%d\n", audioChannel->audioStream, data, len);
        SDLCHK(70101,!SDL_AudioStreamPut(audioChannel->audioStream, data, 2*len));
    }
}

int osd_update_audio_stream(INT16 *buffer)
{
    stream_cache_data = buffer;
    stream_cache_len = samples_this_frame;

    new_audio_channel_data( buffer, samples_this_frame, 1);
    SDL_PauseAudioDevice(audioChannel->audioDeviceID, 0);

    /* compute how many samples to generate next frame */
    samples_left_over += samples_per_frame;
    samples_this_frame = (UINT32)samples_left_over;
    samples_left_over -= (double)samples_this_frame;

    return samples_this_frame;
}

void osd_set_mastervolume(int attenuation)
{
    //printf("TOMCXXX: osd_set_mastervolume(%d)\n", volume);
}

int osd_get_mastervolume(void)
{
    return 0;
}

void osd_sound_enable(int enable_it)
{
    play_sound = enable_it;
}

void osd_opl_control(int chip,int reg)
{
	printf( "TOMCXXX: osd_opl_control(%d,%d)\n", chip, reg);
}

void osd_opl_write(int chip,int data)
{
	printf( "TOMCXXX: osd_opl_write(%d,%d)\n", chip, data);
}

void free_audio_channel_stream(AudioChannel *audioChannel)
{
    void *tmp = (void *) audioChannel->audioStream;
    if( tmp ) {
        audioChannel->audioStream = NULL;
        SDL_FreeAudioStream(tmp);
    }
}

void audioCallback(void *userdata, Uint8 *stream, int len)
{
    AudioChannel *audioChannel = (AudioChannel *) userdata;

    if( audioChannel->audioStream ) {
        int avail = SDL_AudioStreamAvailable(audioChannel->audioStream);
        if( avail >= len && len > 0) {
            // we have enough data to fill the rest
			while( len > 0 ) {
				int actual = SDL_AudioStreamGet(audioChannel->audioStream, stream, len);
				// actual should be len, but isn't always
				len -= actual;
				if( len > 0 ) {
					fprintf(stderr,"TOMCXXX: SDL_AudioStreamGet() actual=%d  remaining=%d\n", actual, len );
				}
			}
        }
        // else do nothing?
    }
}

void stop_all_sound()
{
    if( play_sound ) {
        SDL_PauseAudioDevice(audioChannel->audioDeviceID, 1);
    }
}
