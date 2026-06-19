//-----------------------------------------------------------------------------
//
// OpenAL backend for Zandronum voice chat.
//
//-----------------------------------------------------------------------------

#include "voicechat.h"

#if !defined(NO_SOUND) && defined(NO_FMOD) && !defined(NO_OPENAL)

#include "c_console.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "d_netinf.h"
#include "network.h"
#include "v_text.h"
#include "stats.h"
#include "p_acs.h"
#include "st_hud.h"
#include "st_stuff.h"
#include "team.h"
#include "chat.h"
#include "c_bind.h"
#include "gi.h"
#include "oalsound.h"
#include "s_sound.h"

static ALCdevice *GetCaptureDevice( void *device ) { return static_cast<ALCdevice *>( device ); }

static float GetVoIPAudibility( const FISoundChannel *chan )
{
	if ( chan == nullptr )
		return 0.f;

	return S_GetRolloff( const_cast<FRolloffInfo *>( &chan->Rolloff ),
		sqrt( chan->DistanceSqr ) * chan->DistanceScale, true );
}

//*****************************************************************************
//
// VOIPController
//
//*****************************************************************************

VOIPController::VOIPController( void ) :
	VoIPChannels{ nullptr },
	testRMSVolume( MIN_DECIBELS ),
	renderer( nullptr ),
	captureDevice( nullptr ),
	captureTotalSamples( 0 ),
	captureReadTotal( 0 ),
	encoder( nullptr ),
	repacketizer( nullptr ),
	denoiseModel( nullptr ),
	denoiseState( nullptr ),
	recordDriverID( 0 ),
	framesSent( 0 ),
	lastRecordPosition( 0 ),
	lastPackedTOC( 0 ),
	outputVolume( 1.f ),
	outputPitch( 1.f ),
	outputMuted( false ),
	isInitialized( false ),
	isActive( false ),
	isTesting( false ),
	isRecordButtonPressed( false ),
	isCapturing( false ),
	transmissionType( TRANSMISSIONTYPE_OFF )
{
	memset( recordRing, 0, sizeof( recordRing ));

	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
		channelVolumes[i] = 1.0f;

	proximityInfo.SysChannel = nullptr;
	proximityInfo.StartTime.AsOne = 0;
	proximityInfo.Rolloff.RolloffType = ROLLOFF_Doom;
	proximityInfo.DistanceScale = 1.0f;

	UpdateRolloffDistances( );
	Button_VoiceRecord.Reset( );
}

void VOIPController::Init( OpenALSoundRenderer *mainRenderer )
{
	int opusErrorCode = OPUS_OK;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	renderer = mainRenderer;

	if ( renderer == nullptr )
	{
		Printf( TEXTCOLOR_ORANGE "Invalid OpenALSoundRenderer pointer used to initialize VoIP controller.\n" );
		return;
	}

	encoder = opus_encoder_create( PLAYBACK_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &opusErrorCode );

	if ( opusErrorCode != OPUS_OK )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to create Opus encoder: %s.\n", opus_strerror( opusErrorCode ));
		return;
	}

	opus_encoder_ctl( encoder, OPUS_SET_FORCE_CHANNELS( 1 ));
	opus_encoder_ctl( encoder, OPUS_SET_SIGNAL( OPUS_SIGNAL_VOICE ));

	repacketizer = opus_repacketizer_create( );

	if ( repacketizer == nullptr )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to create Opus repacketizer.\n" );
		return;
	}

	if ( strlen( voice_noisemodelfile ) > 0 )
	{
		const char *fileName = voice_noisemodelfile.GetGenericRep( CVAR_String ).String;
		FILE *modelFile = fopen( fileName, "r" );

		if ( modelFile != nullptr )
		{
			denoiseModel = rnnoise_model_from_file( modelFile );

			if ( denoiseModel == nullptr )
				Printf( TEXTCOLOR_ORANGE "Failed to load RNNoise model \"%s\". Using built-in model instead.\n", fileName );
		}
		else
		{
			Printf( TEXTCOLOR_YELLOW "Couldn't find RNNoise model \"%s\". Using built-in model instead.\n", fileName );
		}
	}

	denoiseState = rnnoise_create( denoiseModel );

	isInitialized = true;
	Printf( "VoIP controller initialized successfully (OpenAL).\n" );

	SetVolume( voice_outputvolume );
}

void VOIPController::Shutdown( void )
{
	Deactivate( );

	if ( encoder != nullptr )
	{
		opus_encoder_destroy( encoder );
		encoder = nullptr;
	}

	if ( repacketizer != nullptr )
	{
		opus_repacketizer_destroy( repacketizer );
		repacketizer = nullptr;
	}

	if ( denoiseModel != nullptr )
	{
		rnnoise_model_free( denoiseModel );
		denoiseModel = nullptr;
	}

	if ( denoiseState != nullptr )
	{
		rnnoise_destroy( denoiseState );
		denoiseState = nullptr;
	}

	renderer = nullptr;
	isInitialized = false;
	isTesting = false;
	isRecordButtonPressed = false;
	Printf( "VoIP controller shutting down.\n" );
}

void VOIPController::Activate( void )
{
	if (( isInitialized == false ) || ( isActive ) || ( CLIENTDEMO_IsPlaying( )))
		return;

	if ( voice_muteself == false )
		StartRecording( );

	isActive = true;
}

void VOIPController::Deactivate( void )
{
	if (( isInitialized == false ) || ( isActive == false ))
		return;

	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
		RemoveVoIPChannel( i );

	if ( players[consoleplayer].statuses & PLAYERSTATUS_TALKING )
		PLAYER_SetStatus( &players[consoleplayer], PLAYERSTATUS_TALKING, false );

	StopRecording( );

	framesSent = 0;
	isActive = false;
}

void VOIPController::PollCapture( void )
{
	ALCdevice *device = GetCaptureDevice( captureDevice );

	if (( device == nullptr ) || ( isCapturing == false ))
		return;

	ALCint samplesAvailable = 0;
	alcGetIntegerv( device, ALC_CAPTURE_SAMPLES, 1, &samplesAvailable );

	if ( samplesAvailable <= 0 )
		return;

	float temp[4096];

	while ( samplesAvailable > 0 )
	{
		const int toRead = MIN<int>( samplesAvailable, 4096 );

		alcCaptureSamples( device, temp, toRead );

		for ( int i = 0; i < toRead; i++ )
		{
			recordRing[captureTotalSamples % RECORD_SOUND_LENGTH] = temp[i];
			captureTotalSamples++;
		}

		samplesAvailable -= toRead;
	}
}

void VOIPController::Tick( void )
{
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if ( playeringame[i] == false )
			continue;

		if (( players[i].ignoreVoice.enabled ) && ( players[i].ignoreVoice.ticks > 0 ))
			players[i].ignoreVoice.ticks--;

		if ( players[i].ignoreVoice.ticks == 0 )
		{
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) || ( i != static_cast<unsigned>( consoleplayer )))
				CHAT_UnignorePlayer( i, true );
		}
	}

	if ( isInitialized == false )
		return;

	if ( IsVoiceChatAllowed( ))
	{
		if ( isActive == false )
			Activate( );
	}
	else if ( isActive )
	{
		Deactivate( );
	}

	const bool isNotIgnored = !players[consoleplayer].ignoreVoice.enabled;

	if ( Button_VoiceRecord.bDown == false )
	{
		if ( isRecordButtonPressed )
		{
			isRecordButtonPressed = false;

			if ( transmissionType == TRANSMISSIONTYPE_BUTTON )
				StopTransmission( );
		}
	}
	else if ( isRecordButtonPressed == false )
	{
		isRecordButtonPressed = true;

		if (( players[consoleplayer].userinfo.GetVoiceEnable( ) == VOICEMODE_PUSHTOTALK ) && ( voice_muteself == false ))
		{
			if ( IsVoiceChatAllowed( ))
			{
				if ( isNotIgnored )
					StartTransmission( TRANSMISSIONTYPE_BUTTON, true );
				else
					CHAT_PrintMutedMessage( true );
			}
			else if ( CLIENTDEMO_IsPlaying( ))
			{
				Printf( "Voice chat can't be used during demo playback.\n" );
			}
			else if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) || ( NETWORK_GetState( ) == NETSTATE_SINGLE_MULTIPLAYER ))
			{
				Printf( "Voice chat can't be used in a singleplayer game.\n" );
			}
			else if ( sv_allowvoicechat == VOICECHAT_OFF )
			{
				Printf( "Voice chat has been disabled by the server.\n" );
			}
		}
	}

	if (( isActive == false ) && ( isTesting == false ))
		return;

	UpdatePlaybackChannels( );

	if ( isTesting || ( NETWORK_GetState( ) != NETSTATE_CLIENT ))
		UpdateAudioStreams( );
}

void VOIPController::UpdateAudioStreams( void )
{
	if ( isInitialized == false )
		return;

	const bool isNotIgnored = !players[consoleplayer].ignoreVoice.enabled;
	const bool isUsingVoiceActivity = ( players[consoleplayer].userinfo.GetVoiceEnable( ) == VOICEMODE_VOICEACTIVITY );

	if (( isNotIgnored && !voice_muteself && ( transmissionType != TRANSMISSIONTYPE_OFF || isUsingVoiceActivity )) || ( isTesting ))
	{
		PollCapture( );

		if ( captureTotalSamples > captureReadTotal )
		{
			const unsigned int recordDelta = captureTotalSamples - captureReadTotal;

			for ( unsigned int frame = 0; frame < recordDelta / RECORD_SAMPLES_PER_FRAME; frame++ )
			{
				TArray<float> frameSamples;
				frameSamples.Resize( RECORD_SAMPLES_PER_FRAME );

				for ( unsigned int s = 0; s < RECORD_SAMPLES_PER_FRAME; s++ )
					frameSamples[s] = recordRing[( captureReadTotal + s ) % RECORD_SOUND_LENGTH];

				ReadRecordSamples( &frameSamples[0], RECORD_SAMPLES_PER_FRAME * sizeof( float ));
				captureReadTotal += RECORD_SAMPLES_PER_FRAME;
			}

			lastRecordPosition = captureTotalSamples % RECORD_SOUND_LENGTH;

			if (( isTesting == false ) && ( opus_repacketizer_get_nb_frames( repacketizer ) > 0 ))
				SendAudioPacket( );

			compressedBuffers.Clear( );
		}
	}

	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if ( VoIPChannels[i] == nullptr )
			continue;

		if ( PLAYER_IsValidPlayer( i ) == false )
		{
			if ( isTesting && ( i == static_cast<unsigned>( consoleplayer )))
				{ /* keep the local monitor channel in menus */ }
			else
			{
				RemoveVoIPChannel( i );
				continue;
			}
		}
		else if (( i != static_cast<unsigned>( consoleplayer )) && ( players[i].ignoreVoice.enabled ))
		{
			RemoveVoIPChannel( i );
			continue;
		}

		if (( VoIPChannels[i]->decoder != nullptr ) && ( VoIPChannels[i]->isPlaying == false ))
		{
			if (( VoIPChannels[i]->jitterBuffer.Size( ) == 0 ) && ( VoIPChannels[i]->pendingSamples.Size( ) == 0 ))
				continue;

			if ( VoIPChannels[i]->playbackTick > gametic )
				continue;

			VoIPChannels[i]->StartPlaying( );
		}

		if ( VoIPChannels[i]->isPlaying )
		{
			const unsigned int oldSamplesPlayed = VoIPChannels[i]->samplesPlayed;

			const int sampleDiff = static_cast<int>( VoIPChannels[i]->samplesRead ) - static_cast<int>( VoIPChannels[i]->samplesPlayed );

			if ( sampleDiff < OPENAL_READ_BUFFER_SIZE )
			{
				const unsigned int samplesToRead = MIN<unsigned int>( VoIPChannels[i]->GetUnreadSamples( ), OPENAL_READ_BUFFER_SIZE - sampleDiff );

				if ( samplesToRead > 0 )
				{
					TArray<float> temp;
					temp.Resize( samplesToRead );
					VoIPChannels[i]->ReadSamples( &temp[0], samplesToRead );
				}
			}

			VoIPChannels[i]->FeedStreamBuffers( );

			if (( i == static_cast<unsigned>( consoleplayer )) && ( isTesting ))
			{
				const unsigned int numNewSamples = VoIPChannels[i]->samplesPlayed - oldSamplesPlayed;

				if ( numNewSamples > 0 )
				{
					float rms = 0.0f;
					unsigned int pos = VoIPChannels[i]->lastPlaybackPosition;

					if ( pos >= numNewSamples )
						pos -= numNewSamples;
					else
						pos = pos + PLAYBACK_SOUND_LENGTH - numNewSamples;

					for ( unsigned int s = 0; s < numNewSamples; s++ )
						rms += powf( VoIPChannels[i]->playbackRing[( pos + s ) % PLAYBACK_SOUND_LENGTH], 2 );

					testRMSVolume = 20 * log10( sqrtf( rms / numNewSamples ));
				}
			}

			VoIPChannels[i]->UpdatePlayback( );
		}
	}
}

void VOIPController::UpdatePlaybackChannels( void )
{
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if ( IsPlayerTalking( i ))
		{
			if (( players[i].statuses & PLAYERSTATUS_TALKING ) == false )
				PLAYER_SetStatus( &players[i], PLAYERSTATUS_TALKING, true );
		}
		else if ( players[i].statuses & PLAYERSTATUS_TALKING )
		{
			PLAYER_SetStatus( &players[i], PLAYERSTATUS_TALKING, false );
		}
	}
}

void VOIPController::ReadRecordSamples( float *soundBuffer, unsigned int length )
{
	float uncompressedBuffer[RECORD_SAMPLES_PER_FRAME];
	float downsizedBuffer[PLAYBACK_SAMPLES_PER_FRAME];
	float rms = 0.0f;

	const unsigned int numSamples = length / sizeof( float );

	for ( unsigned int i = 0; i < numSamples && i < RECORD_SAMPLES_PER_FRAME; i++ )
		uncompressedBuffer[i] = clamp<float>( soundBuffer[i] * voice_recordvolume * 2.5f, -2.5f, 2.5f );

	if (( voice_suppressnoise ) && ( denoiseState != nullptr ))
	{
		for ( unsigned int i = 0; i < RECORD_SAMPLES_PER_FRAME; i++ )
			uncompressedBuffer[i] *= SHRT_MAX;

		rnnoise_process_frame( denoiseState, uncompressedBuffer, uncompressedBuffer );

		for ( unsigned int i = 0; i < RECORD_SAMPLES_PER_FRAME; i++ )
			uncompressedBuffer[i] /= SHRT_MAX;
	}

	if ( transmissionType != TRANSMISSIONTYPE_BUTTON )
	{
		for ( unsigned int i = 0; i < RECORD_SAMPLES_PER_FRAME; i++ )
			rms += powf( uncompressedBuffer[i], 2 );

		rms = 20 * log10( sqrtf( rms / RECORD_SAMPLES_PER_FRAME ));
	}

	if (( transmissionType == TRANSMISSIONTYPE_BUTTON ) || ( rms >= voice_recordsensitivity ) || ( isTesting ))
	{
		if (( isTesting == false ) && ( transmissionType == TRANSMISSIONTYPE_OFF ))
			StartTransmission( TRANSMISSIONTYPE_VOICEACTIVITY, false );

		for ( unsigned int i = 0; i < PLAYBACK_SAMPLES_PER_FRAME; i++ )
			downsizedBuffer[i] = ( uncompressedBuffer[2 * i] + uncompressedBuffer[2 * i + 1] ) / 2.0f;

		if ( isTesting )
		{
			float captureRMS = 0.0f;

			for ( unsigned int i = 0; i < PLAYBACK_SAMPLES_PER_FRAME; i++ )
				captureRMS += powf( downsizedBuffer[i], 2 );

			testRMSVolume = 20 * log10( sqrtf( captureRMS / PLAYBACK_SAMPLES_PER_FRAME ));

			if ( VoIPChannels[consoleplayer] == nullptr )
				VoIPChannels[consoleplayer] = new VOIPChannel( consoleplayer );

			VOIPChannel *monitorChannel = VoIPChannels[consoleplayer];
			monitorChannel->FeedDirectSamples( downsizedBuffer, PLAYBACK_SAMPLES_PER_FRAME );

			if (( monitorChannel->isPlaying == false ) && ( monitorChannel->pendingSamples.Size( ) >= OPENAL_MONITOR_PREFILL ))
				monitorChannel->StartPlaying( );
			else if ( monitorChannel->isPlaying )
				monitorChannel->FeedStreamBuffers( );

			return;
		}

		compressedBuffers.Reserve( 1 );
		int numBytesEncoded = EncodeOpusFrame( downsizedBuffer, PLAYBACK_SAMPLES_PER_FRAME, compressedBuffers.Last( ).data, MAX_PACKET_SIZE );

		if ( numBytesEncoded > 0 )
		{
			if ( isTesting == false )
			{
				const unsigned int numFrames = opus_repacketizer_get_nb_frames( repacketizer );
				const unsigned char toc = compressedBuffers.Last( ).data[0];

				if ( numFrames > 0 )
				{
					if ((( lastPackedTOC & 0xFC ) != ( toc & 0xFC )) || ( numFrames * FRAME_SIZE >= 120 ))
						SendAudioPacket( );
				}

				int opusErrorCode = opus_repacketizer_cat( repacketizer, compressedBuffers.Last( ).data, numBytesEncoded );
				lastPackedTOC = toc;

				if ( opusErrorCode != OPUS_OK )
					Printf( TEXTCOLOR_ORANGE "Failed to merge Opus audio frame: %s.\n", opus_strerror( opusErrorCode ));
			}
		}
	}
	else if ( isTesting == false )
	{
		StopTransmission( );
	}
}

void VOIPController::SendAudioPacket( void )
{
	const unsigned int numFrames = opus_repacketizer_get_nb_frames( repacketizer );
	const unsigned int maxBufferSize = ( MAX_PACKET_SIZE + 1 ) * numFrames;

	unsigned char *mergedBuffer = new unsigned char[maxBufferSize];
	const int mergedBufferSize = opus_repacketizer_out( repacketizer, mergedBuffer, maxBufferSize );

	if ( mergedBufferSize > 0 )
	{
		CLIENTCOMMANDS_VoIPAudioPacket( framesSent, mergedBuffer, mergedBufferSize );
		framesSent += numFrames;
	}
	else
	{
		Printf( TEXTCOLOR_ORANGE "Failed to get merged Opus audio packet: %s.\n", opus_strerror( mergedBufferSize ));
	}

	opus_repacketizer_init( repacketizer );
	delete[] mergedBuffer;
}

void VOIPController::UpdateTestRMSVolume( const float *samples, const unsigned int length )
{
	testRMSVolume = 0.0f;

	for ( unsigned int i = 0; i < length; i++ )
		testRMSVolume += powf( samples[i], 2 );

	if ( length > 0 )
		testRMSVolume = 20 * log10( sqrtf( testRMSVolume / length ));
}

static const char *GetCaptureDeviceName( int index )
{
	const ALCchar *names = alcGetString( NULL, ALC_CAPTURE_DEVICE_SPECIFIER );

	if ( names == nullptr )
		return nullptr;

	for ( int i = 0; i < index && *names; i++ )
		names += strlen( names ) + 1;

	return ( *names != '\0' ) ? names : nullptr;
}

void VOIPController::StartRecording( void )
{
	if ( IsRecording( ))
		return;

	if ( renderer == nullptr )
		return;

	int numRecordDrivers = 0;
	const ALCchar *names = alcGetString( NULL, ALC_CAPTURE_DEVICE_SPECIFIER );

	if ( names != nullptr )
	{
		while ( *names )
		{
			numRecordDrivers++;
			names += strlen( names ) + 1;
		}
	}

	if ( numRecordDrivers == 0 )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to find any connected capture devices.\n" );
		return;
	}

	if ( voice_recorddriver >= numRecordDrivers )
	{
		Printf( "Record driver %d doesn't exist. Using 0 instead.\n", *voice_recorddriver );
		recordDriverID = 0;
	}
	else
	{
		recordDriverID = voice_recorddriver;
	}

	const char *deviceName = GetCaptureDeviceName( recordDriverID );
	ALCdevice *device = alcCaptureOpenDevice( deviceName, RECORD_SAMPLE_RATE, AL_FORMAT_MONO_FLOAT32, RECORD_SOUND_LENGTH );

	if ( device == nullptr )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to open capture device for VoIP recording.\n" );
		return;
	}

	alcCaptureStart( device );

	if ( alcGetError( device ) != ALC_NO_ERROR )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to start VoIP recording.\n" );
		alcCaptureCloseDevice( device );
		return;
	}

	captureDevice = device;
	isCapturing = true;
	captureTotalSamples = 0;
	captureReadTotal = 0;
	lastRecordPosition = 0;
}

void VOIPController::StopRecording( void )
{
	if ( IsRecording( ) == false )
		return;

	StopTransmission( );

	ALCdevice *device = GetCaptureDevice( captureDevice );

	if ( device != nullptr )
	{
		alcCaptureStop( device );
		alcCaptureCloseDevice( device );
	}

	captureDevice = nullptr;
	isCapturing = false;
}

void VOIPController::StartTransmission( const TRANSMISSIONTYPE_e type, const bool getRecordPosition )
{
	if (( isInitialized == false ) || ( isActive == false ) || ( transmissionType != TRANSMISSIONTYPE_OFF ))
		return;

	if ( getRecordPosition )
	{
		lastRecordPosition = captureTotalSamples % RECORD_SOUND_LENGTH;
		captureReadTotal = captureTotalSamples;
	}

	transmissionType = type;
}

void VOIPController::StopTransmission( void )
{
	transmissionType = TRANSMISSIONTYPE_OFF;
}

bool VOIPController::IsVoiceChatAllowed( void ) const
{
	if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
		return false;

	if (( sv_allowvoicechat == VOICECHAT_OFF ) || ( players[consoleplayer].userinfo.GetVoiceEnable( ) == VOICEMODE_OFF ))
		return false;

	if (( gamestate != GS_LEVEL ) && ( gamestate != GS_INTERMISSION ))
		return false;

	return true;
}

bool VOIPController::IsPlayerTalking( const unsigned int player ) const
{
	if ( player == static_cast<unsigned>( consoleplayer ))
	{
		if ( isTesting )
			return false;

		if ( transmissionType != TRANSMISSIONTYPE_OFF )
			return true;
	}

	if (( PLAYER_IsValidPlayer( player )) && ( VoIPChannels[player] != nullptr ) && ( VoIPChannels[player]->isPlaying ))
	{
		if ( VoIPChannels[player]->ShouldPlayIn3DMode( ))
			return GetVoIPAudibility( &VoIPChannels[player]->soundChan ) > 0.0f;

		return true;
	}

	return false;
}

bool VOIPController::IsRecording( void ) const
{
	return isCapturing;
}

float VOIPController::GetChannelVolume( const unsigned int player ) const
{
	return ( player < MAXPLAYERS ? channelVolumes[player] : 0.0f );
}

void VOIPController::SetChannelVolume( const unsigned int player, float volume, const bool updateServer )
{
	if (( isInitialized == false ) || ( player >= MAXPLAYERS ))
		return;

	const float oldVolume = channelVolumes[player];
	channelVolumes[player] = volume;

	if (( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( updateServer ) && ( volume != oldVolume ))
		CLIENTCOMMANDS_SetVoIPChannelVolume( player, volume );

	if (( VoIPChannels[player] == nullptr ) || ( VoIPChannels[player]->isPlaying == false ))
		return;

	VoIPChannels[player]->ApplyGain( );
}

void VOIPController::SetVolume( float volume )
{
	if ( isInitialized == false )
		return;

	outputVolume = volume;

	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if (( VoIPChannels[i] != nullptr ) && ( VoIPChannels[i]->isPlaying ) && ( i != static_cast<unsigned>( consoleplayer )))
			VoIPChannels[i]->ApplyGain( );
	}
}

void VOIPController::SetPitch( float pitch )
{
	if ( isInitialized == false )
		return;

	if ( pitch == outputPitch )
		return;

	outputPitch = pitch;

	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if (( VoIPChannels[i] != nullptr ) && ( VoIPChannels[i]->isPlaying ) && ( VoIPChannels[i]->source != 0 ))
		{
			alSourcef( VoIPChannels[i]->source, AL_PITCH, outputPitch );
			VoIPChannels[i]->UpdateEndDelay( true );
		}
	}
}

void VOIPController::SetMicrophoneTest( const bool enable )
{
	if ( isTesting == enable )
		return;

	const bool isRecording = IsRecording( );

	if ( enable )
	{
		if ( isRecording == false )
			StartRecording( );

		outputMuted = true;
	}
	else
	{
		if ((( IsVoiceChatAllowed( ) == false ) || ( voice_muteself )) && ( isRecording ))
			StopRecording( );

		testRMSVolume = MIN_DECIBELS;
		outputMuted = false;
		RemoveVoIPChannel( consoleplayer );
	}

	isTesting = enable;
}

void VOIPController::RetrieveRecordDrivers( TArray<FString> &list ) const
{
	list.Clear( );

	const ALCchar *names = alcGetString( NULL, ALC_CAPTURE_DEVICE_SPECIFIER );

	if ( names == nullptr )
		return;

	while ( *names )
	{
		list.Push( names );
		names += strlen( names ) + 1;
	}
}

FString VOIPController::GrabStats( void ) const
{
	FString out;

	out.Format( "VoIP controller status: %s", transmissionType != TRANSMISSIONTYPE_OFF ? "transmitting" : ( isActive ? "activated" : "deactivated" ));

	if ( isActive )
		out.AppendFormat( " (%srecording)", IsRecording( ) ? "" : "not " );

	out += '\n';

	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if ( VoIPChannels[i] == nullptr )
			continue;

		out.AppendFormat( "VoIP channel %u (%s): ", i, players[i].userinfo.GetName( ));

		if (( IsPlayerTalking( i )) || (( i == static_cast<unsigned>( consoleplayer )) && ( isTesting )))
		{
			out.AppendFormat( "samples read/played = %u/%u", VoIPChannels[i]->samplesRead, VoIPChannels[i]->samplesPlayed );

			if ( VoIPChannels[i]->samplesRead >= VoIPChannels[i]->samplesPlayed )
				out.AppendFormat( " (diff = %u)", VoIPChannels[i]->samplesRead - VoIPChannels[i]->samplesPlayed );
		}
		else
		{
			out += "not talking";
		}

		out += '\n';
	}

	return out;
}

void VOIPController::ReceiveAudioPacket( const unsigned int player, const unsigned int frame, const unsigned char *data, const unsigned int length )
{
	if (( isActive == false ) && ( player != static_cast<unsigned>( consoleplayer )))
		return;

	if (( PLAYER_IsValidPlayer( player ) == false ) || ( data == nullptr ) || ( length == 0 ))
		return;

	if (( player != static_cast<unsigned>( consoleplayer )) && ( players[player].ignoreVoice.enabled ))
		return;

	if ( VoIPChannels[player] == nullptr )
		VoIPChannels[player] = new VOIPChannel( player );

	if ( frame < VoIPChannels[player]->lastFrameRead )
		return;

	opus_repacketizer_cat( repacketizer, data, length );
	const unsigned int numFrames = opus_repacketizer_get_nb_frames( repacketizer );

	for ( unsigned int i = 0; i < numFrames; i++ )
	{
		unsigned char audioPacket[MAX_PACKET_SIZE];
		const int audioPacketSize = opus_repacketizer_out_range( repacketizer, i, i + 1, audioPacket, MAX_PACKET_SIZE );

		if ( audioPacketSize < 0 )
		{
			Printf( TEXTCOLOR_ORANGE "Failed to split Opus audio packet: %s.\n", opus_strerror( audioPacketSize ));
			continue;
		}

		VOIPChannel::AudioFrame newAudioFrame;
		newAudioFrame.frame = frame + i;

		if ( VoIPChannels[player]->DecodeOpusFrame( audioPacket, audioPacketSize, newAudioFrame.samples, PLAYBACK_SAMPLES_PER_FRAME ) > 0 )
		{
			for ( unsigned int j = 0; j < VoIPChannels[player]->jitterBuffer.Size( ); j++ )
			{
				if ( newAudioFrame.frame < VoIPChannels[player]->jitterBuffer[j].frame )
				{
					VoIPChannels[player]->jitterBuffer.Insert( j, newAudioFrame );
					break;
				}
			}

			if (( VoIPChannels[player]->jitterBuffer.Size( ) == 0 ) && ( VoIPChannels[player]->isPlaying == false ))
				VoIPChannels[player]->playbackTick = gametic + OPENAL_JITTER_TICS;

			VoIPChannels[player]->jitterBuffer.Push( newAudioFrame );
		}
	}

	opus_repacketizer_init( repacketizer );
}

void VOIPController::UpdateProximityChat( void )
{
	for ( unsigned int i = 0; i < MAXPLAYERS; i++ )
	{
		if (( playeringame[i] == false ) || ( VoIPChannels[i] == nullptr ) || ( VoIPChannels[i]->isPlaying == false ))
			continue;

		VoIPChannels[i]->Update3DAttributes( );
	}
}

void VOIPController::UpdateRolloffDistances( void )
{
	proximityInfo.Rolloff.MinDistance = sv_minproximityrolloffdist;
	proximityInfo.Rolloff.MaxDistance = sv_maxproximityrolloffdist;
}

void VOIPController::RemoveVoIPChannel( const unsigned int player )
{
	if (( player < MAXPLAYERS ) && ( VoIPChannels[player] != nullptr ))
	{
		delete VoIPChannels[player];
		VoIPChannels[player] = nullptr;

		PLAYER_SetStatus( &players[player], PLAYERSTATUS_TALKING, false );
	}
}

int VOIPController::EncodeOpusFrame( const float *inBuffer, const unsigned int inLength, unsigned char *outBuffer, const unsigned int outLength )
{
	if (( inBuffer == nullptr ) || ( outBuffer == nullptr ))
		return 0;

	int numBytesEncoded = opus_encode_float( encoder, inBuffer, inLength, outBuffer, outLength );

	if ( numBytesEncoded <= 0 )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to encode Opus audio frame: %s.\n", opus_strerror( numBytesEncoded ));
		return 0;
	}

	return numBytesEncoded;
}

//*****************************************************************************
//
// VOIPController::VOIPChannel
//
//*****************************************************************************

VOIPController::VOIPChannel::VOIPChannel( const unsigned int player ) :
	player( player ),
	source( 0 ),
	decoder( nullptr ),
	playbackTick( 0 ),
	lastReadPosition( 0 ),
	lastPlaybackPosition( 0 ),
	lastFrameRead( 0 ),
	samplesRead( 0 ),
	samplesPlayed( 0 ),
	endDelaySamples( 0 ),
	isPlaying( false ),
	buffersAllocated( false )
{
	memset( streamBuffers, 0, sizeof( streamBuffers ));
	memset( playbackRing, 0, sizeof( playbackRing ));

	int opusErrorCode = OPUS_OK;
	decoder = opus_decoder_create( PLAYBACK_SAMPLE_RATE, 1, &opusErrorCode );

	if ( opusErrorCode != OPUS_OK )
		Printf( TEXTCOLOR_ORANGE "Failed to create Opus decoder for VoIP channel %u: %s.\n", player, opus_strerror( opusErrorCode ));

	soundChan.Rolloff = VOIPController::GetInstance( ).proximityInfo.Rolloff;
	soundChan.Rolloff.RolloffType = ROLLOFF_Doom;
	soundChan.DistanceScale = 1.0f;
	soundChan.ManualRolloff = true;
}

VOIPController::VOIPChannel::~VOIPChannel( void )
{
	StopPlayback( );

	if ( decoder != nullptr )
	{
		opus_decoder_destroy( decoder );
		decoder = nullptr;
	}

	VOIPController::GetInstance( ).channelVolumes[player] = 1.0f;
}

bool VOIPController::VOIPChannel::ShouldPlayIn3DMode( void ) const
{
	if (( sv_proximityvoicechat == false ) || ( gamestate != GS_LEVEL ) || ( PLAYER_IsValidPlayer( player ) == false ))
		return false;

	if (( player == static_cast<unsigned>( consoleplayer )) && ( VOIPController::GetInstance( ).isTesting ))
		return false;

	return (( players[player].bSpectating == false ) && ( players[player].mo != nullptr ) && ( players[player].mo != players[consoleplayer].camera ));
}

int VOIPController::VOIPChannel::GetUnreadSamples( void ) const
{
	return jitterBuffer.Size( ) * PLAYBACK_SAMPLES_PER_FRAME + extraSamples.Size( );
}

int VOIPController::VOIPChannel::DecodeOpusFrame( const unsigned char *inBuffer, const unsigned int inLength, float *outBuffer, const unsigned int outLength )
{
	if (( decoder == nullptr ) || ( inBuffer == nullptr ) || ( outBuffer == nullptr ))
		return 0;

	int numBytesDecoded = opus_decode_float( decoder, inBuffer, inLength, outBuffer, outLength, 0 );

	if ( numBytesDecoded <= 0 )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to decode Opus audio frame: %s.\n", opus_strerror( numBytesDecoded ));
		return 0;
	}

	return numBytesDecoded;
}

void VOIPController::VOIPChannel::StartPlaying( void )
{
	if ( isPlaying )
		return;

	OpenALSoundRenderer *sndRenderer = VOIPController::GetInstance( ).renderer;

	if ( sndRenderer == nullptr )
		return;

	source = sndRenderer->AllocVoIPSource( );

	if ( source == 0 )
	{
		Printf( TEXTCOLOR_ORANGE "Failed to allocate OpenAL source for VoIP channel %u.\n", player );
		return;
	}

	if ( buffersAllocated == false )
	{
		alGenBuffers( STREAM_BUFFER_COUNT, streamBuffers );
		buffersAllocated = ( alGetError( ) == AL_NO_ERROR );
	}

	alSourcei( source, AL_LOOPING, AL_FALSE );
	alSourcef( source, AL_PITCH, VOIPController::GetInstance( ).outputPitch );
	alSourcef( source, AL_ROLLOFF_FACTOR, 0.f );
	alSourcei( source, AL_SOURCE_RELATIVE, AL_FALSE );

	UpdateEndDelay( true );
	Update3DAttributes( );

	if ( player == static_cast<unsigned>( consoleplayer ))
		ApplyGain( );
	else
		ApplyGain( );

	const bool monitor = ( player == static_cast<unsigned>( consoleplayer )) && ( VOIPController::GetInstance( ).isTesting );

	if ( monitor == false )
	{
		const unsigned int prefetch = MIN<unsigned int>( GetUnreadSamples( ), OPENAL_READ_BUFFER_SIZE );

		if ( prefetch > 0 )
		{
			TArray<float> temp;
			temp.Resize( prefetch );
			ReadSamples( &temp[0], prefetch );
		}
	}

	FeedStreamBuffers( );

	if ( monitor )
	{
		ALint queued = 0;
		alGetSourcei( source, AL_BUFFERS_QUEUED, &queued );

		while ( queued < OPENAL_MONITOR_MIN_QUEUED )
		{
			FeedStreamBuffers( );
			alGetSourcei( source, AL_BUFFERS_QUEUED, &queued );
			if ( pendingSamples.Size( ) < OPENAL_STREAM_CHUNK_SAMPLES )
				break;
		}
	}

	alSourcePlay( source );
	isPlaying = ( alGetError( ) == AL_NO_ERROR );
}

void VOIPController::VOIPChannel::ReadSamples( float *outBuffer, const unsigned int numSamples )
{
	unsigned int samplesReadIntoBuffer = 0;

	if ( extraSamples.Size( ) > 0 )
	{
		const unsigned int maxExtraSamples = MIN<unsigned int>( extraSamples.Size( ), numSamples );

		for ( unsigned int i = 0; i < maxExtraSamples; i++ )
		{
			const float sample = extraSamples[0];
			if ( outBuffer != nullptr )
				outBuffer[i] = sample;
			pendingSamples.Push( sample );
			extraSamples.Delete( 0 );
		}

		samplesReadIntoBuffer += maxExtraSamples;
	}

	if ( samplesReadIntoBuffer < numSamples )
	{
		const unsigned int framesRequired = static_cast<unsigned int>( ceil( static_cast<float>( numSamples - samplesReadIntoBuffer ) / PLAYBACK_SAMPLES_PER_FRAME ));
		const unsigned int framesToRead = MIN<unsigned int>( framesRequired, jitterBuffer.Size( ));

		for ( unsigned int frame = 0; frame < framesToRead; frame++ )
		{
			for ( unsigned int i = 0; i < PLAYBACK_SAMPLES_PER_FRAME; i++ )
			{
				const float sample = jitterBuffer[0].samples[i];

				if ( samplesReadIntoBuffer < numSamples )
				{
					if ( outBuffer != nullptr )
						outBuffer[samplesReadIntoBuffer] = sample;
					pendingSamples.Push( sample );
					playbackRing[lastReadPosition] = sample;
					lastReadPosition = ( lastReadPosition + 1 ) % PLAYBACK_SOUND_LENGTH;
					samplesReadIntoBuffer++;
				}
				else
				{
					extraSamples.Push( sample );
				}
			}

			lastFrameRead = jitterBuffer[0].frame;
			jitterBuffer.Delete( 0 );
		}
	}

	samplesRead += samplesReadIntoBuffer;
	UpdateEndDelay( false );
}

void VOIPController::VOIPChannel::FeedDirectSamples( const float *samples, const unsigned int count )
{
	if ( samples == nullptr )
		return;

	for ( unsigned int i = 0; i < count; i++ )
	{
		pendingSamples.Push( samples[i] );
		playbackRing[lastReadPosition] = samples[i];
		lastReadPosition = ( lastReadPosition + 1 ) % PLAYBACK_SOUND_LENGTH;
	}

	samplesRead += count;
}

bool VOIPController::VOIPChannel::IsMonitorChannel( void ) const
{
	return ( player == static_cast<unsigned>( consoleplayer )) && ( VOIPController::GetInstance( ).isTesting );
}

void VOIPController::VOIPChannel::FeedStreamBuffers( void )
{
	if (( source == 0 ) || ( buffersAllocated == false ))
		return;

	const bool monitor = IsMonitorChannel( );
	const unsigned int chunkSize = OPENAL_STREAM_CHUNK_SAMPLES;

	auto queuePendingFrame = [this, chunkSize, monitor]( ALuint buffer ) -> bool
	{
		if ( pendingSamples.Size( ) < chunkSize )
			return false;

		if ( monitor == false )
		{
			const unsigned int inflight = ( samplesRead > samplesPlayed ) ? ( samplesRead - samplesPlayed ) : 0;
			ALint queued = 0;
			alGetSourcei( source, AL_BUFFERS_QUEUED, &queued );

			const unsigned int maxQueuedSamples = inflight + chunkSize * 2;

			if ( queued * chunkSize + chunkSize > maxQueuedSamples )
				return false;
		}

		TArray<float> chunk;
		chunk.Resize( chunkSize );

		for ( unsigned int i = 0; i < chunkSize; i++ )
		{
			chunk[i] = pendingSamples[0];
			pendingSamples.Delete( 0 );
		}

		alBufferData( buffer, AL_FORMAT_MONO_FLOAT32, &chunk[0], chunkSize * sizeof( float ), PLAYBACK_SAMPLE_RATE );
		if ( alGetError( ) != AL_NO_ERROR )
			return false;

		alSourceQueueBuffers( source, 1, &buffer );
		return ( alGetError( ) == AL_NO_ERROR );
	};

	ALint processed = 0;
	alGetSourcei( source, AL_BUFFERS_PROCESSED, &processed );

	while ( processed > 0 )
	{
		ALuint bufid = 0;
		alSourceUnqueueBuffers( source, 1, &bufid );
		processed--;

		ALint size = 0;
		alGetBufferi( bufid, AL_SIZE, &size );
		const unsigned int samples = size / sizeof( float );

		samplesPlayed += samples;
		lastPlaybackPosition = ( lastPlaybackPosition + samples ) % PLAYBACK_SOUND_LENGTH;

		queuePendingFrame( bufid );
	}

	ALint queued = 0;
	alGetSourcei( source, AL_BUFFERS_QUEUED, &queued );

	const int targetQueued = monitor ? OPENAL_MONITOR_MIN_QUEUED : ( STREAM_BUFFER_COUNT - 1 );

	while ( queued < targetQueued )
	{
		if ( queuePendingFrame( streamBuffers[queued % STREAM_BUFFER_COUNT] ) == false )
			break;

		queued++;
	}

	ALint state = AL_INITIAL;
	alGetSourcei( source, AL_SOURCE_STATE, &state );

	if ( state != AL_PLAYING && state != AL_PAUSED )
	{
		alGetSourcei( source, AL_BUFFERS_QUEUED, &queued );

		if ( queued > 0 )
			alSourcePlay( source );
	}
}

void VOIPController::VOIPChannel::Update3DAttributes( void )
{
	if ( source == 0 )
		return;

	soundChan.Rolloff.MinDistance = VOIPController::GetInstance( ).proximityInfo.Rolloff.MinDistance;
	soundChan.Rolloff.MaxDistance = VOIPController::GetInstance( ).proximityInfo.Rolloff.MaxDistance;

	if ( ShouldPlayIn3DMode( ) == false )
	{
		alSourcei( source, AL_SOURCE_RELATIVE, AL_TRUE );
		alSource3f( source, AL_POSITION, 0.f, 0.f, 0.f );
		alSource3f( source, AL_VELOCITY, 0.f, 0.f, 0.f );
		ApplyGain( );
		return;
	}

	if ( players[player].mo == nullptr )
		return;

	const float x = FIXED2FLOAT( players[player].mo->x );
	const float y = FIXED2FLOAT( players[player].mo->z );
	const float z = FIXED2FLOAT( players[player].mo->y );
	const float vx = FIXED2FLOAT( players[player].mo->velx );
	const float vy = FIXED2FLOAT( players[player].mo->velz );
	const float vz = FIXED2FLOAT( players[player].mo->vely );

	soundChan.DistanceSqr = ( players[consoleplayer].camera->x - players[player].mo->x ) * ( players[consoleplayer].camera->x - players[player].mo->x )
		+ ( players[consoleplayer].camera->y - players[player].mo->y ) * ( players[consoleplayer].camera->y - players[player].mo->y );

	alSourcei( source, AL_SOURCE_RELATIVE, AL_FALSE );
	alSource3f( source, AL_POSITION, x, y, -z );
	alSource3f( source, AL_VELOCITY, vx, vy, -vz );
	ApplyGain( );
}

void VOIPController::VOIPChannel::ApplyGain( void )
{
	if ( source == 0 )
		return;

	VOIPController &controller = VOIPController::GetInstance( );
	const bool muted = controller.outputMuted && ( player != static_cast<unsigned>( consoleplayer ));
	float gain = 0.f;

	if ( ShouldPlayIn3DMode( ))
	{
		const float audibility = GetVoIPAudibility( &soundChan );
		const float baseGain = ( player == static_cast<unsigned>( consoleplayer )) ? voice_recordvolume : controller.channelVolumes[player];
		const float master = ( player == static_cast<unsigned>( consoleplayer )) ? 1.f : controller.outputVolume;
		gain = muted ? 0.f : baseGain * master * audibility;
	}
	else if ( player == static_cast<unsigned>( consoleplayer ))
		gain = muted ? 0.f : voice_recordvolume;
	else
		gain = muted ? 0.f : controller.channelVolumes[player] * controller.outputVolume;

	alSourcef( source, AL_GAIN, gain );
}

void VOIPController::VOIPChannel::UpdatePlayback( void )
{
	if ( source == 0 )
		return;

	ALint state = AL_STOPPED;
	alGetSourcei( source, AL_SOURCE_STATE, &state );

	if ( state == AL_STOPPED && GetUnreadSamples( ) == 0 && samplesPlayed >= samplesRead )
		OnPlaybackEnded( );
}

void VOIPController::VOIPChannel::UpdateEndDelay( const bool resetEpoch )
{
	if ( source == 0 )
		return;

	if ( resetEpoch )
	{
		UpdatePlayback( );
		endDelaySamples = samplesPlayed;
	}

	if ( samplesRead <= endDelaySamples )
	{
		alSourcei( source, AL_LOOPING, AL_FALSE );
	}
}

void VOIPController::VOIPChannel::StopPlayback( void )
{
	if ( source != 0 )
	{
		alSourceStop( source );

		ALint queued = 0;
		alGetSourcei( source, AL_BUFFERS_QUEUED, &queued );

		while ( queued > 0 )
		{
			ALuint buffer = 0;
			alSourceUnqueueBuffers( source, 1, &buffer );
			queued--;
		}

		alSourcei( source, AL_BUFFER, 0 );

		if ( buffersAllocated )
		{
			alDeleteBuffers( STREAM_BUFFER_COUNT, streamBuffers );
			memset( streamBuffers, 0, sizeof( streamBuffers ));
			buffersAllocated = false;
		}

		if ( VOIPController::GetInstance( ).renderer != nullptr )
			VOIPController::GetInstance( ).renderer->FreeVoIPSource( source );

		source = 0;
	}

	pendingSamples.Clear( );
	isPlaying = false;
}

void VOIPController::VOIPChannel::OnPlaybackEnded( void )
{
	if ( GetUnreadSamples( ) > 0 )
	{
		samplesPlayed = samplesRead;
		StartPlaying( );
	}
	else
	{
		lastFrameRead = 0;
		samplesRead = 0;
		samplesPlayed = 0;
		lastReadPosition = 0;
		lastPlaybackPosition = 0;
		StopPlayback( );
	}
}

#endif // !NO_SOUND && NO_FMOD && !NO_OPENAL
