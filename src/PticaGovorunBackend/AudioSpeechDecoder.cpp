#include "stdafx.h"
#include "AudioSpeechDecoder.h"
#include <QTextCodec>
#include "ClnUtils.h"
#include "fe_internal.h" // fe_t(front end)
#include "SpeechProcessing.h"

namespace PticaGovorun
{
	const float CmuSphinxFrameRate = 16000;

	namespace
	{
		struct ParsedSpeechSegment
		{
			std::wstring Word;
			int StartFrame;
			int EndFrame;
		};

		// Mergeds word parts, when one the previous part ends and next part starts in '~'.
		void mergeWordParts(wv::slice<ParsedSpeechSegment> parts, std::vector<ParsedSpeechSegment>& mergedParts)
		{
			if (!parts.empty())
			{
				mergedParts.push_back(parts.front());
			}
			for (size_t wordInd = 1; wordInd < parts.size(); ++wordInd)
			{
				const ParsedSpeechSegment& prev = mergedParts.back();
				const ParsedSpeechSegment& cur = parts[wordInd];
				if (prev.Word.back() == L'~' && cur.Word.front() == L'~')
				{
					ParsedSpeechSegment merged = prev;
					merged.Word.pop_back();
					merged.Word.append(cur.Word.data() + 1, cur.Word.size() - 1);
					merged.EndFrame = cur.EndFrame;

					mergedParts.back() = merged;
				}
				else
				{
					mergedParts.push_back(cur);
				}
			}
		}
	}

	void AudioSpeechDecoder::init(const char* hmmPath, const char* langModelPath, const char* dictPath)
	{
		if (hasError_)
			return;

		cmd_ln_t *config = cmd_ln_init(nullptr, ps_args(), true,
			"-hmm", hmmPath,
			"-lm", langModelPath,
			"-dict", dictPath,
			nullptr);
		if (config == nullptr)
		{
			setError("Error: Can't create Sphinx config");
			return;
		}

		ps_ = ps_init(config);
		if (ps_ == nullptr)
		{
			setError("Error: Can't create Sphinx engine");
			return;
		}

		fe_t * frontEnd = ps_get_fe(ps_);
		frameSize_ = frontEnd->frame_size;
		frameShift_ = frontEnd->frame_shift;
	}

	AudioSpeechDecoder::AudioSpeechDecoder()
	{
	}

	AudioSpeechDecoder::~AudioSpeechDecoder()
	{
		if (ps_ != nullptr)
		{
			ps_free(ps_);
			ps_ = nullptr;
		}
	}

	void AudioSpeechDecoder::decode(const wv::slice<short> frames, float frameRate, std::vector<std::wstring>& words, int& framesProcessed)
	{
		if (hasError_)
			return;

		framesProcessed = 0;
		if (frameRate != CmuSphinxFrameRate)
		{
			setError("Error: Sphinx supports only 16KHz audio");
			return;
		}

		int rv = ps_start_utt(ps_, "goforward");
		if (rv < 0)
		{
			setError("Error: Can't start utterance");
			return;
		}

		bool fullUtterance = true;
		rv = ps_process_raw(ps_, frames.data(), frames.size(), false, fullUtterance);
		if (rv < 0)
		{
			setError("Error: Can't process utterance");
			return;
		}

		rv = ps_end_utt(ps_);
		if (rv < 0)
		{
			setError("Error: Can't end utterance");
			return;
		}

		// find words and word probabilities
		__int32 score;
		QTextCodec* textCodec = QTextCodec::codecForName("utf8");
		std::vector<ParsedSpeechSegment> wordsActual;
		for (ps_seg_t *recogSeg = ps_seg_iter(ps_, &score); recogSeg; recogSeg = ps_seg_next(recogSeg))
		{
			const char* word = ps_seg_word(recogSeg);
			std::wstring wordWStr = textCodec->toUnicode(word).toStdWString();

			int startFrame, endFrame;
			ps_seg_frames(recogSeg, &startFrame, &endFrame);
			
			long begSample = -1;
			long endSample = -1;
			frameRangeToSampleRange(startFrame, endFrame, LastFrameSample::EndOfThisFrame, frameSize_, frameShift_, begSample, endSample);

			ParsedSpeechSegment seg;
			seg.Word = wordWStr;
			seg.StartFrame = begSample;
			seg.EndFrame = endSample;
			wordsActual.push_back(seg);
		}

		std::vector<ParsedSpeechSegment> wordsMerged;
		mergeWordParts(wordsActual, wordsMerged);

		// some recognition error is accumulated in the end of the utterance
		// hence the words after chosen one are not included into result
		// (w1,w2,w3) shuld be truncated into (w1,w2)
		// (w1,w2,<sil>) -> (w1,w2)
		// (w1,w2,</s>) -> (w1)
		size_t excludeWordInd = 0;
		if (wordsActual.empty())
		{
			excludeWordInd = 0;
		}
		else if (wordsActual.size() == 1)
		{
			excludeWordInd = 1;
		}
		else
		{
			// skip couple of words at right end (except <s> and </s>)
			int wordsSkipped = 0;
			for (excludeWordInd = wordsMerged.size() - 1; excludeWordInd >= 0; --excludeWordInd)
			{
				if (wordsMerged[excludeWordInd].Word == L"<s>" || wordsMerged[excludeWordInd].Word == L"</s>")
					continue;
				
				// do not count <s> and </s>
				wordsSkipped++;
				const int skipWordsCount = 1;
				if (wordsSkipped >= skipWordsCount)
					break;
			}
		}
		for (size_t i = 0; i < excludeWordInd; ++i)
		{
			words.push_back(wordsMerged[i].Word);
		}

		// determine how many frames were processed
		framesProcessed = frames.size();
		if (excludeWordInd > 0)
			framesProcessed = wordsMerged[excludeWordInd - 1].EndFrame;
	}

	bool AudioSpeechDecoder::hasError() const
	{
		return hasError_;
	}

	const char* AudioSpeechDecoder::getErrorText() const
	{
		return errorMsg_;
	}

	void AudioSpeechDecoder::setError(const char* errorText)
	{
		hasError_ = true;
		errorMsg_ = errorText;
	}
}