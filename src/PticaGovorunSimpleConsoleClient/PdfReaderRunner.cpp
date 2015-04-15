#include "stdafx.h"
#include <iostream>
#include <stdio.h>

#ifdef HAS_LIBPODOFO

#include "PdfTokenizer.h"
#include "PdfRefCountedInputDevice.h" 
#include "PdfRefCountedBuffer.h"

#endif

namespace PdfReaderRunnerNS
{
#ifdef HAS_LIBPODOFO
	using namespace PoDoFo;

	void read1()
	{
		const char* filePath = R"path("C:\devb\PticaGovorunProj\data\textWorld\linguistic\Історія української літературної мови. Огієнко Іван (Митрополит Іларіон).2001.pdf)path";
		try {
			PdfRefCountedInputDevice device(filePath, "rb");
#define BUFFER_SIZE 4096
			PdfRefCountedBuffer      buffer(BUFFER_SIZE);
			PdfTokenizer             tokenizer(device, buffer);
			const char*              pszToken;

			while (true)
			{
				pszToken = tokenizer.GetNextToken();
				printf("Got token: %s\n", pszToken);
			}

		}
		catch (const PdfError & e) {
			e.PrintErrorMsg();
			std::cout <<e.GetError();
		}
	}
#endif

	void run()
	{
#ifdef HAS_LIBPODOFO
		read1();
#endif
	}
}
