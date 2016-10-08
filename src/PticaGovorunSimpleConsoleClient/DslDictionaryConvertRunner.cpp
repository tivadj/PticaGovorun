#include <vector>
#include <array>
#include <set>
#include <iterator>
#include <fstream>
#include <iostream>
#include "StringUtils.h"
#include <boost/utility/string_view.hpp>
#include <QFile>
#include <QtEndian>
#include <PhoneticService.h>
#include <sndfile.h>
#include <SpeechProcessing.h>
#include <WavUtils.h>
#include <SpeechAnnotation.h>
#include <XmlAudioMarkup.h>
#include "assertImpl.h"

namespace DslDictionaryConvertRunnerNS
{
	using namespace PticaGovorun;

	struct DslWordCard
	{
		QString Word;
		QString SoundFileName;
	};

	// Parses word dictionary file (DSL).
	// Each line contains a mungled word. The meta data associated with the word follows, indented with one TAB and
	// may span multiple lines.
	class DslDictionaryParser
	{
	public:
		std::wstring errMsg_;
		std::vector<DslWordCard> dictionaryWords;
		std::map<QString, int> wordToDslEntryInd;
		std::set<QString> wordsWithoutSoundFile;
	public:
		bool removeWordAdornments(const QString& adornedWord, QString& plainWord);
		bool processDslFile(boost::wstring_view filePath);
		void finishWordCard(const QString& word, const QStringList& wordDataLines);


		void run();
	};

	bool DslDictionaryParser::removeWordAdornments(const QString& adornedWord, QString& plainWord)
	{
		plainWord = adornedWord;

		while (true)
		{
			int openInd = plainWord.indexOf(QLatin1String("{["));
			if (openInd == -1)
				return true;
			
			int insideInd = openInd+ 2; // +2 length("{[")=2

			int closeInd = plainWord.indexOf(QLatin1String("]}"), insideInd);
			if (closeInd == -1)
			{
				errMsg_ = QString("Can't remove adornments from word=%1").arg(adornedWord).toStdWString();
				return false;
			}

			closeInd += 2; // +2 length("]}")=2
			plainWord = plainWord.remove(openInd, closeInd - openInd);
		}
	}

	bool DslDictionaryParser::processDslFile(boost::wstring_view filePath)
	{
		QFile wordCardsFile(toQString(filePath));
		if (!wordCardsFile.open(QIODevice::ReadOnly))
		{
			errMsg_ = L"Can't open DSL file";
			return false;
		}
		QTextStream txtStream;
		txtStream.setDevice(&wordCardsFile);

		int wordsCount = 0;
		QString currentWordMungled;
		QString currentWord;
		QStringList wordDataLines;

		auto finishWord = [&]() -> void
		{
			if (currentWord.isEmpty())
				return;
			
			// finish prev word
			finishWordCard(currentWord, wordDataLines);
			currentWord.clear();
			currentWordMungled.clear();
			wordDataLines.clear();
		};

		while (!txtStream.atEnd())
		{
			QString line = txtStream.readLine();
			if (line.isEmpty())
				continue;
			if (line.startsWith(QChar('#'))) // comment
				continue;
			if (line.startsWith(QChar('\t')))
			{
				// data associated with current word
				wordDataLines.append(line);
			}
			else
			{
				// next word
				wordsCount++;
				finishWord();

				currentWordMungled = line;
				removeWordAdornments(line, currentWord);
			}
		}
		// finish the last word
		finishWord();

		std::wcout << L"wordsCount=" << wordsCount << std::endl;
		return true;
	}

	std::tuple<bool, const char*> readVorbisFrames(wv::slice<uchar> vorbisData, std::vector<short>& samples, float *sampleRate)
	{
		struct DataReadingOp
		{
			uchar* Data;
			size_t DataSize;
			size_t ReadingDataOffset;
		};
		DataReadingOp context {vorbisData.data(), vorbisData.size(), 0};

		SF_VIRTUAL_IO virtualIO;
		virtualIO.get_filelen = [](void *user_data)->sf_count_t 
		{
			DataReadingOp* context = reinterpret_cast<DataReadingOp*>(user_data);
			return context->DataSize;
		};
		virtualIO.seek = [](sf_count_t offset, int whence, void *user_data)->sf_count_t
		{
			DataReadingOp* context = reinterpret_cast<DataReadingOp*>(user_data);
			if (whence == SEEK_SET)
				context->ReadingDataOffset = offset;
			else PG_Assert(false);
			return offset;
		};
		virtualIO.tell = [](void *user_data)->sf_count_t
		{
			DataReadingOp* context = reinterpret_cast<DataReadingOp*>(user_data);
			return (sf_count_t)context->ReadingDataOffset;
		};
		virtualIO.read = [](void *ptr, sf_count_t count, void *user_data)->sf_count_t
		{
			uchar* outBuff = reinterpret_cast<uchar*>(ptr);

			DataReadingOp* context = reinterpret_cast<DataReadingOp*>(user_data);
			size_t availSize = std::min(count, (sf_count_t)(context->DataSize - context->ReadingDataOffset));
			std::copy_n(context->Data + context->ReadingDataOffset, availSize, outBuff);
			
			context->ReadingDataOffset += availSize; // update stream offset

			return availSize;
		};

		//
		SF_INFO sfInfo;
		memset(&sfInfo, 0, sizeof(sfInfo));
		sfInfo.channels = 1;
		sfInfo.format = SF_FORMAT_OGG | SF_FORMAT_PCM_16;

		SNDFILE* sf = sf_open_virtual(const_cast<SF_VIRTUAL_IO*>(&virtualIO), SFM_READ, &sfInfo, &context);
		if (sf == nullptr)
		{
			auto err = sf_strerror(nullptr);
			return std::make_tuple(false, err);
		}

		samples.resize(sfInfo.frames);
		if (sampleRate != nullptr)
			*sampleRate = sfInfo.samplerate;

		long long count = 0;
		std::array<short, 32> buf;
		auto targetIt = std::begin(samples);
		while (true)
		{
			auto readc = sf_read_short(sf, &buf[0], buf.size());
			if (readc == 0)
				break;
			count += readc;

			targetIt = std::copy(std::begin(buf), std::begin(buf) + readc, targetIt);
		}

		int code = sf_close(sf);
		if (code != 0)
			return std::make_tuple(false, sf_strerror(nullptr));

		return std::make_tuple(true, nullptr);
	}

	void DslDictionaryParser::finishWordCard(const QString& word, const QStringList& wordDataLines)
	{
		auto wordIt = wordToDslEntryInd.find(word);
		if (wordIt != wordToDslEntryInd.end())
		{
			std::wcout << L"duplicate word=" << word.toStdWString() << std::endl;
		}
			
		QStringRef soundFileName;
		for (int i = 0; i < wordDataLines.size(); ++i)
		{
			const QString& line = wordDataLines.at(i);

			int soundFileOpenInd = line.indexOf("[s]");
			int soundFileCloseInd = line.indexOf("[/s]");
			bool hasSoundFile = soundFileOpenInd != -1 && soundFileCloseInd != -1;
			if (!hasSoundFile)
				continue;

			soundFileOpenInd += 3; // +3 length("[s]")=3
			soundFileName = line.midRef(soundFileOpenInd, soundFileCloseInd - soundFileOpenInd);
			break;
		}

		if (soundFileName.isNull())
		{
			wordsWithoutSoundFile.insert(word);
			return;
		}

		QString soundFileNameQ = soundFileName.toString();
		DslWordCard item;
		item.Word = word;
		item.SoundFileName = soundFileNameQ;
		dictionaryWords.push_back(item);
	}

	void DslDictionaryParser::run()
	{
		std::wstring fileDsl = LR"path(C:\devb\PticaGovorunProj\data\BrownBear\wordCardsUk.dsl)path";
		if (!processDslFile(fileDsl))
			return;
	}

	struct AudioFileSegmentRef
	{
		std::string FileName; // the file name associated with the segment
		uint32_t Offset; // offset (in bytes) in resource file RIDX where OGG file is lay out
		uint32_t Size; // the size of OGG file
	};

	// http://www.stardict.org/StarDictFileFormat
	/* (cut from the previous link)
	The format of the res.ridx file:
	filename;	// A string end with '\0'.
	offset;		// 32 or 64 bits unsigned number in network byte order.
	size;		// 32 bits unsigned number in network byte order.
	filename can include a path too, such as "pic/example.png". filename is 
	case sensitive, and there should have no two same filenames in all the 
	entries.
	if "idxoffsetbits=64", then offset is 64 bits.
	These three items are repeated as each entry.
	The entries are sorted by the strcmp() function with the filename field.
	It is possible that different filenames have the same offset and size.
	*/
	void readStarDictRidxFile32(boost::wstring_view resIndexfilePath, std::vector<AudioFileSegmentRef>& segmentRefs)
	{
		std::ifstream input(resIndexfilePath.data(), std::ios::binary);
		std::vector<uchar> buffer((
			std::istreambuf_iterator<char>(input)),
			(std::istreambuf_iterator<char>()));
		
		std::string fileName;
		size_t pos = 0;
		while (pos < buffer.size())
		{
			// read file name
			size_t nameInd = pos;
			while (buffer[nameInd] != '\0')
				nameInd++;

			fileName.assign(reinterpret_cast<char*>(&buffer[pos]), nameInd - pos);
			
			// read offset
			pos = nameInd + 1; // step next to \0
			uint32_t offset = qFromBigEndian<uint32_t>(&buffer[pos]);

			pos += 4;
			uint32_t size = qFromBigEndian<uint32_t>(&buffer[pos]);

			pos += 4; // to next record

			AudioFileSegmentRef s;
			s.FileName = fileName;
			s.Offset = offset;
			s.Size = size;
			segmentRefs.push_back(s);
		}
	}

	// 'phrase273.wav' -> 273
	int parseSoundFileNameId(const QString& fileName)
	{
		int extInd = fileName.indexOf(QLatin1String(".wav"));
		const int prefixLen = 6;

		QStringRef numStr = fileName.midRef(prefixLen, extInd - prefixLen);
		bool conv = false;
		int id =  numStr.toInt(&conv);
		if (!conv)
			return -1;
		return id;
	}


	// Composes StarDict dictionary DSL file with audio data (in OMG, Vorbis format) to speech annotation
	// format acceptable by PticaGovorun transcriber.
	class DslAndAudioResDictIntoSpeechAnnotConverter
	{
		PhoneRegistry phoneReg;
		DslDictionaryParser extr;
		
		std::vector<AudioFileSegmentRef> segmentRefs; // pointers to audio data
		std::map<std::string, const AudioFileSegmentRef*> fileNameToSegmentRefs;

		std::vector<uchar> buffer; // concatenated audio OGG files per each word

		std::unique_ptr<GrowOnlyPinArena<wchar_t>> stringArena;
		std::vector<PhoneticWord> phoneticDict; // phonetic transcription of words in DSL dictionary
		SpeechAnnotation annot;
		const float outputSampleRate = 22050;
		std::vector<short> framesConcat; // concatenated audio frames for all audio files
	public:
		std::wstring errMsg_;

		DslAndAudioResDictIntoSpeechAnnotConverter();

		void run();

		void buildPhoneticDictionary();
		void buildSpeechAnnot();
	};

	DslAndAudioResDictIntoSpeechAnnotConverter::DslAndAudioResDictIntoSpeechAnnotConverter()
	{
		stringArena = std::make_unique<GrowOnlyPinArena<wchar_t>>(1024 * 512);

		bool allowSoftHardConsonant = true;
		bool allowVowelStress = true;
		// write in most specific way
		phoneReg.setPalatalSupport(PalatalSupport::AsPalatal);
		initPhoneRegistryUk(phoneReg, allowSoftHardConsonant, allowVowelStress);
	}

	void DslAndAudioResDictIntoSpeechAnnotConverter::buildPhoneticDictionary()
	{
		// load 'word to stressed syllable' dictionary 
		bool loadPhoneDict;
		const char* errMsg = nullptr;

		const wchar_t* stressDict = LR"path(C:\devb\PticaGovorunProj\data\stressDictUk.xml)path";
		std::unordered_map<std::wstring, int> wordToStressedSyllable;
		std::tie(loadPhoneDict, errMsg) = loadStressedSyllableDictionaryXml(stressDict, wordToStressedSyllable);
		if (!loadPhoneDict)
		{
			errMsg_ = QString::fromLatin1(errMsg).toStdWString();
			return;
		}

		auto getStressedSyllableIndFun = [&wordToStressedSyllable](boost::wstring_view word, std::vector<int>& stressedSyllableInds) -> bool
		{
			auto it = wordToStressedSyllable.find(std::wstring(word.data(), word.size()));
			if (it == wordToStressedSyllable.end())
				return false;
			stressedSyllableInds.push_back(it->second);
			return true;
		};

		WordPhoneticTranscriber phoneticTranscriber;
		phoneticTranscriber.setStressedSyllableIndFun(getStressedSyllableIndFun);

		for (const DslWordCard& card : extr.dictionaryWords)
		{
			const QString& word = card.Word.toLower();
			std::wstring wordW = word.toStdWString();

			phoneticTranscriber.transcribe(phoneReg, wordW);
			if (phoneticTranscriber.hasError())
			{
				errMsg_ = std::wstring(L"Can't transcribe word=") + wordW;
				return;
			}

			boost::wstring_view wordArena;
			if (!registerWord(word, *stringArena, wordArena))
			{
				errMsg_ = L"Can't register word in the arena";
				return;
			}

			PronunciationFlavour pron;
			pron.PronCode = wordArena;
			phoneticTranscriber.copyOutputPhoneIds(pron.Phones);

			PhoneticWord phWord;
			phWord.Word = wordArena;
			phWord.Pronunciations.push_back(pron);
			phoneticDict.push_back(phWord);
		}
	}

	void DslAndAudioResDictIntoSpeechAnnotConverter::buildSpeechAnnot()
	{
		float sampleRate = 0;
		std::vector<short> samples;
		std::vector<short> samplesResampled;

		const wchar_t* Speaker = L"BrownBear1";
		annot.addSpeaker(Speaker, Speaker);

		int maxMarkerId = 0;
		size_t frameInd = 0;
		int i = 0;
		for (const DslWordCard& card : extr.dictionaryWords)
		{
			//if (i++ > 100)
			//	break;
			const QString& word = card.Word.toLower();
			const QString& soundFileName = card.SoundFileName;

			int fileId = parseSoundFileNameId(soundFileName);
			if (fileId == -1)
			{
				std::wcerr << L"Can't get id from file name" << soundFileName.toStdWString() << std::endl;
				continue;
			}
			maxMarkerId = std::max(maxMarkerId, fileId);

			QString soundFileNameFixed(soundFileName);
			soundFileNameFixed = soundFileNameFixed.replace(QLatin1String(".wav"), QLatin1String(".ogg"), Qt::CaseSensitive);

			auto segRefIt = fileNameToSegmentRefs.find(soundFileNameFixed.toStdString());
			if (segRefIt == fileNameToSegmentRefs.end())
			{
				std::cerr << "Can't find the reference to audio data" << std::endl;
				continue;
			}
			const AudioFileSegmentRef& segRef = *segRefIt->second;

			samples.clear();

			// binary data is the OGG container with audio in Vorbis format
			wv::slice<uchar> vorbisData = wv::make_view(&buffer[segRef.Offset], segRef.Size);

			bool readOp;
			const char* errMsg;
			std::tie(readOp, errMsg) = readVorbisFrames(vorbisData, samples, &sampleRate);
			if (!readOp)
			{
				std::wcout << errMsg << std::endl;
				return;
			}

			// resample samples
			ErrMsgList errMsgL;
			if (!resampleFrames(samples, sampleRate, outputSampleRate, samplesResampled, &errMsgL))
			{
				std::cout << str(errMsgL) << std::endl;
				return;
			}

			//
			std::copy(samplesResampled.begin(), samplesResampled.end(), std::back_inserter(framesConcat));

			TimePointMarker m;
			m.Id = fileId;
			m.SampleInd = frameInd;
			m.TranscripText = word;
			m.Language = SpeechLanguage::Ukrainian;
			m.LevelOfDetail = MarkerLevelOfDetail::Word;
			m.SpeakerBriefId = Speaker;
			annot.attachMarker(m);

			frameInd += samplesResampled.size();
		}

		// add last marker
		{
			TimePointMarker m;
			m.Id = maxMarkerId + 1;
			m.SampleInd = frameInd;
			m.Language = SpeechLanguage::NotSet;
			m.LevelOfDetail = MarkerLevelOfDetail::Word;
			annot.attachMarker(m);
		}
	}

	void DslAndAudioResDictIntoSpeechAnnotConverter::run()
	{
		// load word to sound file name
		extr.run();
		if (!extr.errMsg_.empty())
		{
			errMsg_ = extr.errMsg_;
			return;
		}

		buildPhoneticDictionary();
		if (!errMsg_.empty())
			return;

		// load sound file name to the reference to the sound binary data
		std::wstring fileRidx = LR"path(C:\devb\PticaGovorunProj\data\BrownBear\wordSoundUk.ridx)path";
		readStarDictRidxFile32(fileRidx, segmentRefs);
		for (const AudioFileSegmentRef& segRef : segmentRefs)
			fileNameToSegmentRefs.insert({ segRef.FileName, &segRef });

		// load audio
		std::wstring fileRdic = LR"path(C:\devb\PticaGovorunProj\data\BrownBear\wordSoundUk.rdic)path";
		std::ifstream input(fileRdic.data(), std::ios::binary);
		buffer = std::vector<uchar>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());

		//
		buildSpeechAnnot();

		// dump results
		std::string timeStampStr;
		appendTimeStampNow(timeStampStr);
		std::wstring timeStampStrW = QString::fromStdString(timeStampStr).toStdWString();

		std::wstringstream dumpFileName;
		dumpFileName << L"dict." << timeStampStrW << L".phonetic.xml";
		savePhoneticDictionaryXml(phoneticDict, dumpFileName.str(), phoneReg);

		dumpFileName.str(L"");
		dumpFileName << L"dict." << timeStampStrW << L".markers.xml";
		saveAudioMarkupToXml(annot, dumpFileName.str());

		std::stringstream dumpFileNameS;
		dumpFileNameS << "dict." << timeStampStr << ".wav";
		writeAllSamplesWav(framesConcat.data(), framesConcat.size(), dumpFileNameS.str(), (int)outputSampleRate);
	}

	void run()
	{
		DslAndAudioResDictIntoSpeechAnnotConverter c;
		c.run();
		if (!c.errMsg_.empty())
			std::wcerr << c.errMsg_ << std::endl;
	}
}