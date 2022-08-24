// Copyright (C) 2005 Eugene Bujak.
//
// snd_mme.c -- Movie Maker's Edition sound routines

#include "snd_local.h"
#include "snd_mix.h"
#include "../renderer/tr_mme.h"
#include "../renderer/tr_local.h"
#include <vector>
#include <string>
#include <cgame\tr_types.h>
#include <bw64/bw64.hpp>
#include "adm/export.h"
#include <adm/adm.hpp>
#include <adm/utilities/object_creation.hpp>
#include <adm/write.hpp>

#define MME_SNDCHANNELS 128
#define MME_LOOPCHANNELS 128

extern	cvar_t	*mme_saveWav;
extern	cvar_t* mme_rollingShutterEnabled;
extern	cvar_t* mme_rollingShutterPixels;
extern	cvar_t* mme_rollingShutterMultiplier;
extern glconfig_t	glConfig;
extern	std::vector<int> pboRollingShutterProgresses;






typedef struct {
	// ADM stuff
	std::unique_ptr<bw64::Bw64Writer>	adm_bw64Handle;
	char			adm_baseName[MAX_OSPATH];
	mmeADMChannelInfo_t	adm_channelInfo[MME_LOOPCHANNELS+ MME_SNDCHANNELS];
	long			admAbsoluteTime; // Do not touch the ADM stuff! It has special handling in some memset functions.

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

extern shotData_t shotData;

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

// Just a chunk wrapper for the ADM document basically.
// It's not meant to be used normally with the bw64 class, only with our special override "writeChunk" below.
// Basically the goal is to save memory by not having to write to a string first and then to have it copied
// etc. Just want it to be written straight to the file.
class ADMChunk : public bw64::Chunk {
public:
	static uint32_t Id() { return bw64::utils::fourCC("axml"); }
	ADMChunk(std::shared_ptr<adm::Document> document) {
		_document = document;
	}
	~ADMChunk() {
		// Blah
	}
	void write(std::ostream &stream) const override { // writes to stream and returns amount of bytes written.
		// Not implemented
	}
	uint64_t doWrite(std::ostream &stream) { // writes to stream and returns amount of bytes written.
		std::streampos startpos = stream.tellp();
		adm::writeXml(stream,_document);
		std::streampos endpos = stream.tellp();
		uint64_t admXMLlength = endpos - startpos;
		return admXMLlength;
	}
	uint32_t id() const override { return ADMChunk::Id(); }
	uint64_t size() const override { return _size; }
private:
	std::shared_ptr<adm::Document> _document;
	uint64_t _size;
};

// Special class for writing ADM chunk straight into the file without having to save it in a string first.
template<>
void bw64::Bw64Writer::writeChunk(std::shared_ptr<ADMChunk> chunk) {
	if (chunk) {
		uint64_t position = fileStream_.tellp();
		bw64::utils::writeValue(fileStream_, chunk->id());
		uint64_t sizePosition = fileStream_.tellp();
		bw64::utils::writeValue(fileStream_, 0); // placeholder for now
		
		uint64_t size = chunk->doWrite(fileStream_);
		if (size % 2 == 1) {
			bw64::utils::writeValue(fileStream_, '\0');
		}
		uint64_t currentPosition = fileStream_.tellp();
		fileStream_.seekp(sizePosition);
		bw64::utils::writeValue(fileStream_, (uint32_t)size);
		fileStream_.seekp(currentPosition);
		
		// we do this later because we didnt know the size before
		chunkHeaders_.push_back(
			ChunkHeader(chunk->id(), chunk->size(), position));
		//bw64::utils::writeChunk<bw64::ChunkType>(fileStream_, chunk,
		//	chunkSizeForHeader(chunk->id()));
		//chunks_.push_back(chunk); // this will just have to live with not existing...
	}
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

inline long long S_AudioSamplesToNanoSeconds(long &inTime) {
	double timeInSeconds = ((double)inTime) / ((double)MME_SAMPLERATE);
	return (long long)(0.5 + timeInSeconds * 1000000000.0);
}

template<class T>
inline std::string getPrintedString(T something) {
	std::stringstream idStream;
	id.print(idStream);
	return idStream.str();
}

// By how much do we have to divide in-game units to get real units for ADM?
// Real units in ADM are in reference to absoluteDistance in meters
// We set that to 100. 100 meters.
// One game character is roughly 64 units. Let's say that's 2 meters.
// So we divide by 32 to get meters. Then we divide by 100 to scale the values properly to our reference
// screen distance of 100 meters (a bit silly I know, but whatever)
// So overall we divide by 3200
//#define POSITION_UNITS_RATIO 60.0f
//#define POSITION_UNITS_RATIO 3200.0f
// Since the absoluteDistance one seems to be ignored for the most part, we use absolute distances in the file, in meters. so divide by 32
#define POSITION_UNITS_RATIO 32.0f

void S_MMEADMMetaCreate(std::string filename,bw64::Bw64Writer* writer, std::string csvFilename) {

	auto admProgramme = adm::AudioProgramme::create(adm::AudioProgrammeName("JK2 JOMME ADM EXPORT"));

	auto admDocument = adm::Document::create();
	admDocument->add(admProgramme);

	std::vector<bw64::AudioId> audioIds;

	long long minBlockDuration = 0; // We set this according to fps so as not to bloat the ADM metadata in slow capturing modes like rolling shutter
	if (shotData.fps) { // I hope it's (still?) set here
		double timeInSeconds = 1.0/shotData.fps;
		minBlockDuration= (long long)(0.5 + timeInSeconds * 1000000000.0);
		// In short, we don't necessarily want more than one position update per frame. What's the point after all? There's interpolation anyway.
	}
	
	for (int i = 0; i < (MME_SNDCHANNELS + MME_LOOPCHANNELS); i++) {

		std::string contentName(i >= MME_SNDCHANNELS ? "SNDCHANNEL" : "LOOPCHANNEL");
		contentName.append(std::to_string(i));
		auto contentItem = adm::AudioContent::create(adm::AudioContentName(contentName));
		admProgramme->addReference(contentItem);

		// For clarification: Right now we are using one entire object for each channel
		// because ADM has a rather low limit on max amount of objects you can have in a file,
		// therefore having one object per sound would likely exceed the max number quickly.
		auto channelObject = adm::createSimpleObject(contentName);

		//channelObject.audioPackFormat->set(adm::AbsoluteDistance(100.0f)); // 100 meters reference distance

		int64_t blocksAdded = 0;

		int o = 0;
		long long lastBlockEndTimeChannelScope = 0;
		for (auto object = mmeSound.adm_channelInfo[i].objects.begin(); object != mmeSound.adm_channelInfo[i].objects.end(); object++,o++) {
			
			int b = 0;
			int bLast = object->blocks.size() - 1;
			long long thisBlockEndTime=0,lastBlockEndTime=0;
			for (auto block = object->blocks.begin(); block != object->blocks.end(); block++,b++) {
				
				long long timeInNanoSeconds = S_AudioSamplesToNanoSeconds(block->starttime);
				long long durationInNanoSeconds = S_AudioSamplesToNanoSeconds(block->duration);

				if (b>0 && b< bLast && minBlockDuration > (timeInNanoSeconds - lastBlockEndTime)) {
					// Skip blocks if they are too highly resolved. We want only one block per frame roughly.
					// Unless it's literally the first or last block, we need a start and an ending after all.
					// We do not want to bloat the ADM file too much, this is just something to aid in that goal.
					// This helps not only with the ADM XML filesize (potentially immensely!), it also makes
					// the ADM generation much faster and thus the program handling more comfortable.
					continue; 
				}

				adm::CartesianPosition cartesianCoordinates((adm::X)(block->position[0]/ POSITION_UNITS_RATIO),(adm::Y)(block->position[1] / POSITION_UNITS_RATIO),(adm::Z)(block->position[2] / POSITION_UNITS_RATIO));
				auto blockFormat = adm::AudioBlockFormatObjects(cartesianCoordinates);
				blockFormat.set((adm::Gain)block->gain);

				
				thisBlockEndTime = timeInNanoSeconds + durationInNanoSeconds; // The time that this block ends in correct numbers

				if (!!lastBlockEndTime  && lastBlockEndTime != timeInNanoSeconds) { // It's at least the second block in this object. They are contiguous. Simply adjust times.
					// This means that we have a rounding error resulting from conversion of samples to nanoseconds
					long long diff = timeInNanoSeconds - lastBlockEndTime;
					timeInNanoSeconds -= diff; // We remove the diff from the start value
					durationInNanoSeconds += diff; // Since duration is a relative term, we need to add the diff back on
				}
				else if (b==0 && !!lastBlockEndTimeChannelScope && lastBlockEndTimeChannelScope != timeInNanoSeconds) { 
					// This is the first block of this object, but not the first block of this channel. 
					// Not contiguous. Add a filler block.
					// All block formats within an object must be contiguous. Just how it is.
					long long diff = timeInNanoSeconds - lastBlockEndTimeChannelScope;
					if (diff > 0 && diff > 1) { // If it's only a difference of one nanosecond, it's likely a rounding error too

						auto blockFormatFiller = adm::AudioBlockFormatObjects(adm::CartesianPosition((adm::X)0, (adm::Y)0, (adm::Z)0));
						blockFormatFiller.set(adm::Rtime((std::chrono::nanoseconds)lastBlockEndTimeChannelScope));
						blockFormatFiller.set(adm::Duration((std::chrono::nanoseconds)diff));
						channelObject.audioChannelFormat->add(blockFormatFiller);
					}
					else {
						// Special case: There wasn't actually a gap between the sounds but once again a rounding error.
						// Adding a filler here makes no sense because the filler would have a negative value or be tiny.
						// So we just adjust the current block's start time instead. It's not great, but it's only about a 
						// nanosecond of imperfection so who cares I guess
						timeInNanoSeconds -= diff; // We remove the diff from the start value
						durationInNanoSeconds += diff; // Since duration is a relative term, we need to add the diff back on
					}
				}
				if (b == 0) {
					// If this is the first block in this sound, make the position a jumper.
					// TODO technically we are introducing a delay in the position data here because
					// the position data is for the start of the block, not for its end
					// but we'll let it slide for now... the correct way would be to 
					// add the zero-duration blockformats...
					
					blockFormat.set(adm::JumpPosition(adm::JumpPositionFlag(true)));
				}

				blockFormat.set(adm::Rtime((std::chrono::nanoseconds)timeInNanoSeconds));
				blockFormat.set(adm::Duration((std::chrono::nanoseconds)durationInNanoSeconds));

				lastBlockEndTimeChannelScope = lastBlockEndTime = thisBlockEndTime; // Remember for next iteration
				
				channelObject.audioChannelFormat->add(blockFormat);


				blocksAdded++;
			}
		}

		if (blocksAdded > 0) {

			contentItem->addReference(channelObject.audioObject);
			audioIds.push_back(bw64::AudioId(i + 1,
				adm::formatId(channelObject.audioTrackUid->get<adm::AudioTrackUidId>()),
				adm::formatId(channelObject.audioTrackFormat->get<adm::AudioTrackFormatId>()),
				adm::formatId(channelObject.audioPackFormat->get<adm::AudioPackFormatId>())
			));
		}
	}


	// write XML data to stdout
	adm::writeXml(filename,admDocument);

	auto chnaChunk = std::make_shared<bw64::ChnaChunk>(audioIds);

	//writer->writeChunk(chnaChunk);
	writer->setChnaChunk(chnaChunk);

	writer->finalizeDataChunk(); // This will potentially add a padding so that everything is aligned to 2-bytes. 
	auto admChunk = std::make_shared<ADMChunk>(admDocument);
	writer->writeChunk(admChunk);

	std::ofstream myfile;
	myfile.open(csvFilename, std::ios::out | std::ios::app);
	//myfile << "Dims: " << width << "x" << height;
	std::string retVal;
	myfile<<"channel;object;block;objectName;gain;starttime;duration;position";
	for (int i = 0; i < (MME_SNDCHANNELS + MME_LOOPCHANNELS); i++) {
		int o = 0;
		for (auto object = mmeSound.adm_channelInfo[i].objects.begin(); object != mmeSound.adm_channelInfo[i].objects.end(); object++,o++) {
			
			int b = 0;
			for (auto block = object->blocks.begin(); block != object->blocks.end(); block++,b++) {
				myfile << std::to_string(i);
				myfile << ";";
				myfile << std::to_string(o);
				myfile << ";";
				myfile << std::to_string(b);
				myfile << ";";
				//retVal.append(object->soundName);
				myfile << (sfxEntries+ object->sfxHandle)->name;
				myfile << ";";
				myfile << std::to_string(block->gain);
				myfile << ";";
				myfile << std::to_string(block->starttime);
				myfile << ";";
				myfile << std::to_string(block->duration);
				myfile << ";";
				myfile << std::to_string(block->position[0]);
				myfile << ",";
				myfile << std::to_string(block->position[1]);
				myfile << ",";
				myfile << std::to_string(block->position[2]);
				myfile << "\n";

			}
		}
	}

	myfile.close();
}

void S_MMEWavClose(void) {
	byte header[WAV_HEADERSIZE];

	if (!!mmeSound.adm_bw64Handle) {

		//std::string admMetaData = S_MMEADMMetaCreate();
		//const char* admMetaDataC = admMetaData.c_str(); 
		char savePath[MAX_OSPATH]; 
		char savePathCSV[MAX_OSPATH]; 

		//Com_sprintf(savePath, sizeof(savePath), "%s.ADMsavetest.xml", mmeSound.adm_baseName);
		for (int i = 0; i < 100000; i++) {
			Com_sprintf(savePath, sizeof(savePath), "%s.ADM.%03d.xml", mmeSound.adm_baseName, i);
			if (!FS_FileExists(savePath))
				break;
		}
		for (int i = 0; i < 100000; i++) {
			Com_sprintf(savePathCSV, sizeof(savePathCSV), "%s.ADMcsv.%03d.csv", mmeSound.adm_baseName, i);
			if (!FS_FileExists(savePathCSV))
				break;
		}
		std::string realPath = FS_GetSanePath(savePath);
		std::string realPathCSV = FS_GetSanePath(savePathCSV);

		try {

			S_MMEADMMetaCreate(realPath, mmeSound.adm_bw64Handle.get(), realPathCSV);
			//FS_WriteFile(savePathCSV, ret.c_str(), ret.size());
		}
		catch (std::runtime_error e) {
			ri.Printf(PRINT_WARNING, "ADM xml Error: %s. Possible cause: %s\n", e.what(), strerror(errno));
		}


		mmeSound.adm_bw64Handle.reset();
		mmeSound.admAbsoluteTime = 0;
		memset(mmeSound.adm_baseName,0,sizeof(mmeSound.adm_baseName));
		for (int i = 0; i < (MME_SNDCHANNELS + MME_LOOPCHANNELS); i++) {

			mmeSound.adm_channelInfo[i].objects.clear();
			
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

			if (!s_speedAwareAudio->integer) {
				scale = 1.0f;
			}
			else {
				scale = min(max(scale, s_minSpeed->value), s_maxSpeed->value);
			}

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

extern int rollingShutterSuperSampleMultiplier;

void S_MMERecord( const char *baseName, float deltaTime ) {

	mmeRollingShutterInfo_t* rsInfo = R_MME_GetRollingShutterInfo();
	//int rollingShutterFactor = glConfig.vidHeight* rollingShutterSuperSampleMultiplier / mme_rollingShutterPixels->integer;
	if (rsInfo->rollingShutterEnabled) {
		deltaTime /= rsInfo->captureFpsMultiplier;
	}
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
				std::string sanePath = FS_GetSanePath(fileName);
				FS_CreatePath((char*)sanePath.c_str());
				try {
					
					mmeSound.adm_bw64Handle = bw64::writeFile(sanePath, MME_SNDCHANNELS + MME_LOOPCHANNELS, MME_SAMPLERATE, 16u);
					
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
