#include <vector>
#include <boost/format.hpp>
#include "ComponentsInfrastructure.h" // ErrMsgList
#include "SphinxIf.h"

#ifdef PG_SPHINX_VAD_HACKING
extern "C" {
#include <sphinxbase/fe.h>
#include <sphinxbase/err.h> // err_set_logfp
#include <sphinxbase/cmd_ln.h>
#include <sphinxbase/hash_table.h>
#include <sphinxbase/ckd_alloc.h>
#include <sphinxbase/agc.h>
#include <sphinxbase/src/sphinx_fe/cmd_ln_defn.h>
#include <sphinxbase/src/libsphinxbase/fe/fe_internal.h>
}
#endif

namespace PticaGovorun
{
#ifdef PG_SPHINX_VAD_HACKING
	namespace 
	{
		typedef struct cmd_ln_val_s {
			anytype_t val;
			int type;
		} cmd_ln_val_t;

		struct cmd_ln_s {
			int refcount;
			hash_table_t *ht;
			char **f_argv;
			uint32 f_argc;
		};
	}
#endif

	// Really dirty implementation which tries to leverage VAD (Voice Activity Detection) functionality of PocketSphinx library.
	bool detectVoiceActivitySphinx(gsl::span<const short> samples, float sampRate, const SphinxVadParams& vadArgs, std::vector<SegmentSpeechActivity>& activity, ErrMsgList* errMsg)
	{
#ifndef PG_SPHINX_VAD_HACKING
		if (errMsg != nullptr) errMsg->utf8Msg = "Sphinx VAD is not compiled in. Set PG_SPHINX_VAD_HACKING flag.";
		return false;
#else
		PG_Assert(sampRate > 0);

		err_set_logfp(nullptr);

		int argc = 1;
		char* argv[] = { "" };
		int strict = 0;
		cmd_ln_t *config = (cmd_ln_t*)cmd_ln_parse_r(nullptr, defnPG(), argc, argv, strict); // default command line args
		if (config == nullptr)
		{
			if (errMsg != nullptr) errMsg->utf8Msg = "Can't initialize Sphinx config";
			return false;
		}
		cmd_ln_s *configS = (cmd_ln_s*)config;


		bool errFlag = false;
		auto set = [&errFlag, &configS, &errMsg](const char* key, cmd_ln_val_t value)
		{
			if (errFlag)
				return;
			
			// the value is in the heap and is released by
			cmd_ln_val_t* v = (cmd_ln_val_t *)ckd_calloc(1, sizeof(*v));
			memcpy(v, &value, sizeof(value));
			if (hash_table_replace(configS->ht, key, (void *)v) == nullptr)
			{
				errMsg->utf8Msg = str(boost::format("Can't create Sphinx config, key: %1%") % key);
				errFlag = true;
				return;
			}
			cmd_ln_val_t valTmp;
			cmd_ln_val_t* pval = &valTmp;
			hash_table_lookup(configS->ht, "-samprate", (void **)&pval);
		};

		cmd_ln_val_t val;

		val.type = ARG_FLOAT32;
		val.val.fl = sampRate;
		set("-samprate", val);

		val.type = ARG_INT32;
		val.val.i = vadArgs.FrameRate;
		set("-frate", val);

		val.type = ARG_FLOAT32;
		val.val.fl = vadArgs.WindowLength;
		set("-wlen", val);

		val.type = ARG_INT32;
		val.val.i = vadArgs.PreSpeech;
		set("-vad_prespeech", val);

		val.type = ARG_INT32;
		val.val.i = vadArgs.PostSpeech;
		set("-vad_postspeech", val);

		val.type = ARG_INT32;
		val.val.i = vadArgs.StartSpeech;
		set("-vad_startspeech", val);

		val.type = ARG_FLOAT32;
		val.val.fl = vadArgs.VadThreashold;
		set("-vad_threshold", val);

		int blocksize = 640000;
		val.type = ARG_INT32;
		val.val.i = blocksize;
		set("-blocksize", val);

		val.type = ARG_BOOLEAN;
		val.val.i = 1;
		set("-remove_silence", val);

		if (errFlag)
			return false;

		//sphinx_wave2feat_t *wtf = sphinx_wave2feat_init(config);
		// decode pcm
		fe_t* fe = fe_init_auto_r((cmd_ln_t*)config);
		if (fe == nullptr)
		{
			if (errMsg != nullptr) errMsg->utf8Msg = "Sphinx, fe_init_auto_r() failed";
			return false;
		}
		fe_s* feS = (fe_s*)fe;
		fe_start_stream(fe);
		fe_start_utt(fe);

		int fsize = feS->frame_size;
		int fshift = feS->frame_shift;
		int nchans = 1;
		int featsize = (blocksize / nchans - fsize) / fshift;
		int veclen = 13;
		//size_t nvec = wtf->featsize;
		size_t nvec = featsize;

		mfcc_t** feat = (mfcc_t**)ckd_calloc_2d(featsize, veclen, sizeof(mfcc_t));

		//
		std::vector<int32> framesVadMask;
		framesVadMask.resize(nvec);
		const int NotInit = -1;
		std::fill(std::begin(framesVadMask), std::end(framesVadMask), NotInit);
		int32 framesCountPG = 0;

		//
		uint32 nfloat = 0;
		size_t nsamp = samples.size();
		int32 n, nfr;
		//while ((nsamp = fread(wtf->samples, sizeof(int16), wtf->blocksize, wtf->infh)) != 0) 
		{
			int16 const *inspeech = samples.data();

			/* Consume all samples. */
			while (nsamp) {
				nfr = nvec;
				//fe_process_framesPG(wtf->fe, &inspeech, &nsamp, wtf->feat, &nfr, NULL, framesVadMaskPG, &framesCountPG);
				fe_process_framesPG(fe, &inspeech, &nsamp, feat, &nfr, NULL, framesVadMask.data(), &framesCountPG);
				if (nfr) {
					//if ((n = (*wtf->ot->output_frames)(wtf, wtf->feat, nfr)) < 0)
					//	return -1;
					//nfloat += n;
					nfloat += nfr * nvec;
				}
			}
		}
		/* Now process any leftover samples frames. */
		//fe_end_utt(wtf->fe, wtf->feat[0], &nfr);
		fe_end_utt(fe, feat[0], &nfr);
		if (nfr) {
			//if ((n = (*wtf->ot->output_frames)(wtf, wtf->feat, nfr)) < 0)
			//	return -1;
			//nfloat += n;
			nfloat += nfr * nvec;
		}
		//return nfloat;
		cmd_ln_free_r(config);

		// fix mask
		// these calls of Sphinx routines is buggy, try to recover
		// fix start of mask
		if (framesVadMask.size() > 1 && framesVadMask[0] == NotInit && framesVadMask[1] >= 0)
			framesVadMask[0] = framesVadMask[1];

		{
			// fix the counter of initialized mask's vlaues
			for (size_t i = framesCountPG - 1; i >= 0 && i < framesVadMask.size(); ++i)
			{
				if (framesVadMask[i] != NotInit)
					break;
				
				framesCountPG = i;
			}
		}

		// populate result
		framesVadMask.resize(framesCountPG);
		convertVadMaskToSegments(framesVadMask, fsize, fshift, activity);
		return true;
#endif
	}
}
