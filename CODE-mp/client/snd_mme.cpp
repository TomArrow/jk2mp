// Copyright (C) 2005 Eugene Bujak.
//
// snd_mme.c -- Movie Maker's Edition sound routines

#include "snd_local.h"
#include "snd_mix.h"
#include "../renderer/tr_local.h"
#include <vector>
#include <cgame\tr_types.h>
#include <bw64/bw64.hpp>
#include "adm/export.h"
#include <adm/adm.hpp>
#include <adm/utilities/object_creation.hpp>
#include <adm/write.hpp>

#define MME_SNDCHANNELS 128
#define MME_LOOPCHANNELS 128

extern	cvar_t	*mme_saveWav;
extern	cvar_t	*mme_rollingShutterPixels;
extern	cvar_t	*mme_rollingShutterMultiplier;
extern glconfig_t	glConfig;
extern	std::vector<int> pboRollingShutterProgresses;






typedef struct {
	// ADM stuff
	std::unique_ptr<bw64::Bw64Writer>	adm_bw64Handle;
	char			adm_baseName[MAX_OSPATH];
	mmeADMChannelInfo_t	adm_channelInfo[MME_LOOPCHANNELS+ MME_SNDCHANNELS];
	long			admAbsoluteTime;

	char			baseName[MAX_OSPATH];
	fileHandle_t	fileHandle;
	long			fileSize;
	float			deltaSamples, sampleRate;
	mixLoop_t		loops[MME_LOOPCHANNELS];
	mixChannel_t	channels[MME_SNDCHANNELS];
	mixEffect_t		effect;
	qboolean		gotFrame;
} mmeWav_t;

static mmeWav_t mmeSound;



/*
=================================================================================

ADM FILE HANDLING FUNCTIONS

=================================================================================
*/
// Make the Bw64Writer accept shorts directly. It will convert to them anyway, why go through float?
template <>
uint64_t bw64::Bw64Writer::write(short* inBuffer, uint64_t frames) {
	if (formatChunk()->bitsPerSample() != 16) {
		throw std::runtime_error("The short overload for Bw64Writer::write can only be called if the Bw64Writer object is set to output 16 bit data.");
	}
	uint64_t bytesWritten = frames * formatChunk()->blockAlignment();
	rawDataBuffer_.resize(bytesWritten);
	//utils::encodePcmSamples(inBuffer, &rawDataBuffer_[0],
	//	frames * formatChunk()->channelCount(),
	//	formatChunk()->bitsPerSample());
	fileStream_.write((char*)inBuffer, bytesWritten);
	dataChunk()->setSize(dataChunk()->size() + bytesWritten);
	chunkHeader(utils::fourCC("data")).size = dataChunk()->size();
	return frames;
}

/*
=================================================================================

WAV FILE HANDLING FUNCTIONS

=================================================================================
*/

#define WAV_HEADERSIZE 44
/*
===================
S_MME_PrepareWavHeader

Fill in wav header so that we can write sound after the header
===================
*/
static void S_MMEFillWavHeader(void* buffer, long fileSize, int sampleRate) {
	((unsigned long*)buffer)[0] = 0x46464952;	// "RIFF"
	((unsigned long*)buffer)[1] = fileSize-8;		// WAVE chunk length
	((unsigned long*)buffer)[2] = 0x45564157;	// "WAVE"

	((unsigned long*)buffer)[3] = 0x20746D66;	// "fmt "
	((unsigned long*)buffer)[4] = 16;			// fmt chunk size
	((unsigned short*)buffer)[(5*2)] = 1;		// audio format. 1 - PCM uncompressed
	((unsigned short*)buffer)[(5*2)+1] = 2;		// number of channels
	((unsigned long*)buffer)[6] = sampleRate;	// sample rate
	((unsigned long*)buffer)[7] = sampleRate * 2 * 2;	// byte rate
	((unsigned short*)buffer)[(8*2)] = 2 * 2;	// block align
	((unsigned short*)buffer)[(8*2)+1] = 16;	// sample bits

	((unsigned long*)buffer)[9] = 0x61746164;	// "data"
	((unsigned long*)buffer)[10] = fileSize-44;	// data chunk length
}

void S_MMEADMMetaCreate(std::string filename) {

	auto admProgramme = adm::AudioProgramme::create(adm::AudioProgrammeName("JK2 JOMME ADM EXPORT"));
	

	
	for (int i = 0; i < (MME_SNDCHANNELS + MME_LOOPCHANNELS); i++) {

		std::string contentName(i >= MME_SNDCHANNELS ? "SNDCHANNEL" : "LOOPCHANNEL");
		contentName.append(std::to_string(i));
		auto contentItem = adm::AudioContent::create(adm::AudioContentName(contentName));
		admProgramme->addReference(contentItem);

		auto channelObject = adm::createSimpleObject(contentName);

		int o = 0;
		for (auto object = mmeSound.adm_channelInfo[i].objects.begin(); object != mmeSound.adm_channelInfo[i].objects.end(); object++,o++) {
			
			int b = 0;
			for (auto block = object->blocks.begin(); block != object->blocks.end(); block++,b++) {
				
				adm::CartesianPosition cartesianCoordinates((adm::X)block->position[0],(adm::Y)block->position[1],(adm::Z)block->position[2]);
				auto blockFormat = adm::AudioBlockFormatObjects(cartesianCoordinates);
				blockFormat.set((adm::Gain)block->gain);
				double timeInSeconds = ((double)block->starttime) / ((double)MME_SAMPLERATE);
				std::chrono::nanoseconds timeInNanoSeconds = (std::chrono::nanoseconds)(long long)(0.5+timeInSeconds* 1000000000.0);
				double durationInSeconds = ((double)block->duration) / ((double)MME_SAMPLERATE);
				std::chrono::nanoseconds durationInNanoSeconds = (std::chrono::nanoseconds)(long long)(0.5+ durationInSeconds * 1000000000.0);
				blockFormat.set(adm::Rtime(timeInNanoSeconds));
				blockFormat.set(adm::Duration(durationInNanoSeconds));
				channelObject.audioChannelFormat->add(blockFormat);

			}
		}

		contentItem->addReference(channelObject.audioObject);
	}

	auto admDocument = adm::Document::create();
	admDocument->add(admProgramme);

	// write XML data to stdout
	adm::writeXml(filename,admDocument);
	/*
	std::string retVal;
	retVal.append("channel;object;block;objectName;gain;starttime;duration;position");
	for (int i = 0; i < (MME_SNDCHANNELS + MME_LOOPCHANNELS); i++) {
		int o = 0;
		for (auto object = mmeSound.adm_channelInfo[i].objects.begin(); object != mmeSound.adm_channelInfo[i].objects.end(); object++,o++) {
			
			int b = 0;
			for (auto block = object->blocks.begin(); block != object->blocks.end(); block++,b++) {
				retVal.append(std::to_string(i));
				retVal.append(";");
				retVal.append(std::to_string(o));
				retVal.append(";");
				retVal.append(std::to_string(b));
				retVal.append(";");
				//retVal.append(object->soundName);
				retVal.append((sfxEntries+ object->sfxHandle)->name);
				retVal.append(";");
				retVal.append(std::to_string(block->gain));
				retVal.append(";");
				retVal.append(std::to_string(block->starttime));
				retVal.append(";");
				retVal.append(std::to_string(block->duration));
				retVal.append(";");
				retVal.append(std::to_string(block->position[0]));
				retVal.append(",");
				retVal.append(std::to_string(block->position[1]));
				retVal.append(",");
				retVal.append(std::to_string(block->position[2]));
				retVal.append("\n");

			}
		}
	}
	return retVal;*/
}

void S_MMEWavClose(void) {
	byte header[WAV_HEADERSIZE];

	if (!!mmeSound.adm_bw64Handle) {

		//std::string admMetaData = S_MMEADMMetaCreate();
		//const char* admMetaDataC = admMetaData.c_str(); 
		char savePath[MAX_OSPATH]; 

		//Com_sprintf(savePath, sizeof(savePath), "%s.ADMsavetest.xml", mmeSound.adm_baseName);
		for (int i = 0; i < 100000; i++) {
			Com_sprintf(savePath, sizeof(savePath), "%s.ADMsavetest.%03d.xml", mmeSound.adm_baseName, i);
			if (!FS_FileExists(savePath))
				break;
		}
		//FS_WriteFile(savePath, admMetaDataC, admMetaData.size());
		const char* realPath = FS_GetSanePath(savePath);

		try {

			S_MMEADMMetaCreate(realPath);
		}
		catch (std::runtime_error e) {
			ri.Printf(PRINT_WARNING, "ADM xml Error: %s. Possible cause: %s\n", e.what(), strerror(errno));
		}


		mmeSound.adm_bw64Handle.reset();
		mmeSound.admAbsoluteTime = 0;
		memset(mmeSound.adm_baseName,0,sizeof(mmeSound.adm_baseName));
		for (int i = 0; i < (MME_SNDCHANNELS + MME_LOOPCHANNELS); i++) {

			mmeSound.adm_channelInfo->objects.clear();
			
		}
	}

	if (!mmeSound.fileHandle)
		return;

	S_MMEFillWavHeader( header, mmeSound.fileSize, MME_SAMPLERATE );
	FS_Seek( mmeSound.fileHandle, 0, FS_SEEK_SET );
	FS_Write( header, sizeof(header), mmeSound.fileHandle );
	FS_FCloseFile( mmeSound.fileHandle );

	// Dirty!!
	int admDataSize = sizeof(mmeSound.admAbsoluteTime) + sizeof(mmeSound.adm_baseName)+ sizeof(mmeSound.adm_bw64Handle)+ sizeof(mmeSound.adm_channelInfo);
	Com_Memset( ((char*)&mmeSound)+admDataSize, 0, sizeof(mmeSound)- admDataSize); // Don't memset the adm stuff because its not all basic C stuff. ADM is cleaned up above.
	//Com_Memset( &mmeSound, 0, sizeof(mmeSound) );
}


static byte wavExportBuf[MME_SAMPLERATE] = {0};
static int bytesInBuf = 0;
qboolean S_MMEAviImport(byte *out, int *size) {
	const char *format = Cvar_VariableString("mme_screenShotFormat");
	const int shot = Cvar_VariableIntegerValue("mme_saveShot");
	const int depth = Cvar_VariableIntegerValue("mme_saveDepth");
	if (mme_saveWav->integer != 2 || Q_stricmp(format, "avi") || (!shot && !depth))
		return qfalse;
	*size = 0;
	if (bytesInBuf >= MME_SAMPLERATE)
		bytesInBuf -= MME_SAMPLERATE;
	if (bytesInBuf <= 0)
		return qtrue;
	Com_Memcpy( out, wavExportBuf, bytesInBuf );
	*size = bytesInBuf;
	bytesInBuf = 0;
	return qtrue;
}

/*
===================
S_MME_Update

Called from CL_Frame() in cl_main.c when shooting avidemo
===================
*/
#define MAXUPDATE 4096
void S_MMEUpdate(float scale) {
	
	//for (int i = 0; i < pboRollingShutterProgresses.size(); i++) {
	//	if (pboRollingShutterProgresses[i] == 0) { // This is the first rolling shutter line/block of lines captured


			int count, speed;
			int mixTemp[MAXUPDATE * 2];
			//short* mixTempADM = new short[MAXUPDATE*(MME_SNDCHANNELS+MME_LOOPCHANNELS)];
			int ADMchannelcount = MME_SNDCHANNELS + MME_LOOPCHANNELS;
			static const std::unique_ptr<short[]>  mixTempADM(new short[MAXUPDATE * (ADMchannelcount)]);
			short mixClip[MAXUPDATE * 2];

			if (!mmeSound.fileHandle && mme_saveWav->integer != 2)
				return;
			if (!mmeSound.gotFrame) {
				if (mme_saveWav->integer != 2)
					S_MMEWavClose();
				return;
			}
			count = (int)mmeSound.deltaSamples;
			if (!count)
				return;
			mmeSound.deltaSamples -= count;
			if (count > MAXUPDATE)
				count = MAXUPDATE;

			speed = (scale * (MIX_SPEED << MIX_SHIFT)) / MME_SAMPLERATE;
			if (speed < 0 || (speed == 0 && scale))
				speed = 1;

			Com_Memset(mixTemp, 0, sizeof(int) * count * 2);
			Com_Memset(mixTempADM.get(), 0, sizeof(short) * count * ADMchannelcount);
			if (speed > 0) {
				S_MixChannels(mmeSound.channels, MME_SNDCHANNELS, speed, count, mixTemp, mixTempADM.get(), ADMchannelcount, mmeSound.adm_channelInfo, mmeSound.admAbsoluteTime);
				S_MixLoops(mmeSound.loops, MME_LOOPCHANNELS, speed, count, mixTemp, mixTempADM.get()+ MME_SNDCHANNELS, ADMchannelcount, mmeSound.adm_channelInfo+ MME_SNDCHANNELS, mmeSound.admAbsoluteTime);
				S_MixEffects(&mmeSound.effect, speed, count, mixTemp); //Todo make underwater stuff work with ADM.
			}
			S_MixClipOutput(count, mixTemp, mixClip, 0, MAXUPDATE - 1);
			if (mme_saveWav->integer != 2) {
				FS_Write(mixClip, count * 4, mmeSound.fileHandle);
			}
			else if (mme_saveWav->integer == 2) {
				Com_Memcpy(&wavExportBuf[bytesInBuf], mixClip, count * 4);
				bytesInBuf += count * 4;
			}
			mmeSound.fileSize += count * 4;
			mmeSound.gotFrame = qfalse;

			if (!!mmeSound.adm_bw64Handle) {
				mmeSound.adm_bw64Handle->write(mixTempADM.get(), count );
				mmeSound.admAbsoluteTime += count;
			}
	//	}
	//}
}

void S_MMERecord( const char *baseName, float deltaTime ) {

	int rollingShutterFactor = glConfig.vidHeight/ mme_rollingShutterPixels->integer;
	deltaTime = deltaTime / (float)rollingShutterFactor * mme_rollingShutterMultiplier->value;
	//for (int i = 0; i < pboRollingShutterProgresses.size(); i++) {
	//	if (pboRollingShutterProgresses[i] == 0) { // This is the first rolling shutter line/block of lines captured
			
			
			
			const char* format = Cvar_VariableString("mme_screenShotFormat");
			const int shot = Cvar_VariableIntegerValue("mme_saveShot");
			const int depth = Cvar_VariableIntegerValue("mme_saveDepth");
			const int ADM = Cvar_VariableIntegerValue("mme_saveADM");

			if (Q_stricmp(baseName, mmeSound.adm_baseName) && ADM) {
				char fileName[MAX_OSPATH];
				for (int i = 0; i < 100000; i++) {
					Com_sprintf(fileName, sizeof(fileName), "%s.ADM.%03d.WAV", baseName, i);
					if (!FS_FileExists(fileName))
						break;
				}
				FS_CreatePath(fileName); 
				try {
					
					mmeSound.adm_bw64Handle = bw64::writeFile(std::string(FS_GetSanePath(fileName)), MME_SNDCHANNELS + MME_LOOPCHANNELS, MME_SAMPLERATE, 16u);
					
				}
				catch (std::runtime_error e) {
					mmeSound.adm_bw64Handle = nullptr;
					ri.Printf(PRINT_WARNING,"ADM Error: %s. Possible cause: %s\n",e.what(), strerror(errno));
				}
				Q_strncpyz(mmeSound.adm_baseName, baseName, sizeof(mmeSound.adm_baseName));
			}

			if (!mme_saveWav->integer || (mme_saveWav->integer == 2 && (Q_stricmp(format, "avi") || (!shot && !depth))))
				return;
			if (Q_stricmp(baseName, mmeSound.baseName) && mme_saveWav->integer != 2) {
				char fileName[MAX_OSPATH];
				if (mmeSound.fileHandle)
					S_MMEWavClose();

				/* First see if the file already exist */
				for (int i = 0; i < 100000; i++) {
					Com_sprintf(fileName, sizeof(fileName), "%s.%03d.wav", baseName, i);
					if (!FS_FileExists(fileName))
						break;
				}
				//Com_sprintf(fileName, sizeof(fileName), "%s.wav", baseName);
				mmeSound.fileHandle = FS_FOpenFileReadWrite(fileName);
				if (!mmeSound.fileHandle) {
					mmeSound.fileHandle = FS_FOpenFileWrite(fileName);
					if (!mmeSound.fileHandle)
						return;
				}
				Q_strncpyz(mmeSound.baseName, baseName, sizeof(mmeSound.baseName));
				mmeSound.deltaSamples = 0;
				mmeSound.sampleRate = MME_SAMPLERATE;
				FS_Seek(mmeSound.fileHandle, 0, FS_SEEK_END);
				mmeSound.fileSize = FS_filelength(mmeSound.fileHandle);
				if (mmeSound.fileSize < WAV_HEADERSIZE) {
					int left = WAV_HEADERSIZE - mmeSound.fileSize;
					while (left) {
						FS_Write(&left, 1, mmeSound.fileHandle);
						left--;
					}
					mmeSound.fileSize = WAV_HEADERSIZE;
				}
			}
			else if (Q_stricmp(baseName, mmeSound.baseName) && mme_saveWav->integer == 2) {
				Q_strncpyz(mmeSound.baseName, baseName, sizeof(mmeSound.baseName));
				mmeSound.deltaSamples = 0;
				mmeSound.sampleRate = MME_SAMPLERATE;
				mmeSound.fileSize = 0;
				bytesInBuf = 0;
			}
			mmeSound.deltaSamples += deltaTime * mmeSound.sampleRate;
			mmeSound.gotFrame = qtrue;


	//	}
	//}

	
}

/* This is a seriously crappy hack, but it'll work for now */
void S_MMEMusic( const char *musicName, float time, float length ) {
	if ( !musicName || !musicName[0] ) {
		s_background.playing = qfalse;
		s_background.override = qfalse;
		return;
	}
	Q_strncpyz( s_background.startName, musicName, sizeof( s_background.startName ));
	/* S_FileExists sets correct extension */
	if (!S_FileExists(s_background.startName)) {
		s_background.playing = qfalse;
		s_background.override = qfalse;
	} else {
		s_background.override = qtrue;
		s_background.seekTime = time;
		s_background.length = length;
		s_background.playing = qtrue;
		s_background.reload = qtrue;
	}
}

void S_MMEStopSound(int entityNum, int entchannel, sfxHandle_t sfxHandle) {
	int i;
	for (i = 0; i < MME_SNDCHANNELS; i++) {
		if (mmeSound.channels[i].entChan == entchannel
			&& mmeSound.channels[i].entNum == entityNum
			&& (mmeSound.channels[i].handle == sfxHandle || sfxHandle < 0)) {
			mmeSound.channels[i].entChan = 0;
			mmeSound.channels[i].entNum = 0;
			mmeSound.channels[i].handle = 0;
			mmeSound.channels[i].hasOrigin = 0;
			VectorClear(mmeSound.channels[i].origin);
			mmeSound.channels[i].index = 0;
			mmeSound.channels[i].wasMixed = 0;
			if (sfxHandle >= -1)
				break;
		}
	}
}
