#include "stdafx.h"

#include "..\Minecraft.World\StringHelpers.h"
#include "..\Minecraft.World\compression.h"

#include "ArchiveFile.h"

extern void LogMsg(const char* fmt, ...);

void ArchiveFile::_readHeader(DataInputStream *dis)
{
	int numberOfFiles = dis->readInt();

	for (int i = 0; i < numberOfFiles; i++)
	{
		MetaData *meta = new MetaData();
		meta->filename = dis->readUTF();
		meta->ptr = dis->readInt();
		meta->filesize = dis->readInt();

		// Filenames preceeded by an asterisk have been compressed.
		if (meta->filename[0] == '*')
		{
			meta->filename = meta->filename.substr(1);
			meta->isCompressed = true;
		} 
		else meta->isCompressed = false;

		m_index.insert( pair<wstring,PMetaData>(meta->filename,meta) );
	}
}

ArchiveFile::ArchiveFile(File file)
{
	m_cachedData = nullptr;
	m_sourcefile = file;
	app.DebugPrintf("Loading archive file...\n");
	{
		char buf[512];
		wcstombs(buf, file.getPath().c_str(), 512);
		LogMsg("Archive: opening '%s'\n", buf);
		LogMsg("Archive: wstringtofilename='%s'\n", wstringtofilename(file.getPath()));
		LogMsg("Archive: file.exists()=%d file.length()=%lld\n", (int)file.exists(), (long long)file.length());
	}

	if(!file.exists())
	{
		app.DebugPrintf("Failed to load archive file!\n");//,file.getPath());
		app.FatalLoadError();
	}

	FileInputStream fis(file);

#if defined _XBOX_ONE || defined __ORBIS__ || defined _WINDOWS64
	int64_t fileLen = file.length();
	LogMsg("Archive: reading %lld bytes into memory...\n", (long long)fileLen);
	byteArray readArray(static_cast<unsigned int>(fileLen));
	int bytesRead = fis.read(readArray,0,fileLen);
	LogMsg("Archive: fis.read returned %d\n", bytesRead);

	ByteArrayInputStream bais(readArray);
	DataInputStream dis(&bais);

	m_cachedData = readArray.data;
#else
	DataInputStream dis(&fis);
#endif

	_readHeader(&dis);

	LogMsg("Archive: loaded %d files from archive\n", (int)m_index.size());
	// Log first 10 filenames for debugging
	{
		int count = 0;
		for (auto& kv : m_index) {
			char fn[256];
			wcstombs(fn, kv.first.c_str(), 256);
			LogMsg("Archive: [%d] '%s'\n", count, fn);
			if (++count >= 20) { LogMsg("Archive: ... (%d more)\n", (int)m_index.size() - 20); break; }
		}
	}

	dis.close();
	fis.close();
#if defined _XBOX_ONE || defined __ORBIS__ || defined _WINDOWS64
	bais.reset();
#endif
	app.DebugPrintf("Finished loading archive file\n");
}

ArchiveFile::~ArchiveFile()
{
	delete m_cachedData;
}

vector<wstring> *ArchiveFile::getFileList()
{
	vector<wstring> *out = new vector<wstring>();
	
	for ( const auto& it : m_index )
		out->push_back( it.first );

	return out;
}

bool ArchiveFile::hasFile(const wstring &filename)
{
	return m_index.find(filename) != m_index.end();
}

int ArchiveFile::getFileSize(const wstring &filename)
{
	return hasFile(filename) ? m_index.at(filename)->filesize : -1;
}

byteArray ArchiveFile::getFile(const wstring &filename)
{
	byteArray out;
	auto it = m_index.find(filename);

	if(it == m_index.end())
	{
		app.DebugPrintf("Couldn't find file in archive\n");
		app.DebugPrintf("Failed to find file '%ls' in archive\n", filename.c_str());
		// UWP: removed __debugbreak()
		app.FatalLoadError();
	}
	else
	{
		PMetaData data = it->second;

#if defined _XBOX_ONE || defined __ORBIS__ || defined _WINDOWS64
		out = byteArray(data->filesize );

		memcpy( out.data, m_cachedData + data->ptr, data->filesize );
#else

#ifdef _UNICODE
		HANDLE hfile = CreateFile(	m_sourcefile.getPath().c_str(), 
			GENERIC_READ,
			0,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
			);
#else
		app.DebugPrintf("Createfile archive\n");
		HANDLE hfile = CreateFile(	wstringtofilename(m_sourcefile.getPath()), 
			GENERIC_READ,
			0,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
			);
#endif

		if (hfile != INVALID_HANDLE_VALUE)
		{
			app.DebugPrintf("hfile ok\n");
			DWORD ok = SetFilePointer(	hfile,
				data->ptr,
				nullptr,
				FILE_BEGIN
				);

			if (ok != INVALID_SET_FILE_POINTER)
			{
				PBYTE pbData = new BYTE[ data->filesize ];

				DWORD bytesRead = -1;
				BOOL bSuccess = ReadFile(	hfile,
					(LPVOID) pbData,
					data->filesize,
					&bytesRead,
					nullptr
					);

				if(bSuccess==FALSE)
				{
					app.FatalLoadError();
				}
				assert(bytesRead == data->filesize);
				out = byteArray(pbData, data->filesize);
			}
			else
			{
				app.FatalLoadError();
			}

			CloseHandle(hfile);
		}
		else
		{
			app.DebugPrintf("bad hfile\n");
			app.FatalLoadError();
		}
#endif

		// Compressed filenames are preceeded with an asterisk.
		if ( data->isCompressed && out.data != nullptr )
		{
			/* 4J-JEV:
			* If a compressed file is accessed before compression object is 
			* initialized it will crash here (Compression::getCompression).
			*/
			///4 279 553 556

			ByteArrayInputStream bais(out);
			DataInputStream dis(&bais);
			unsigned int decompressedSize = dis.readInt();
			dis.close();

			PBYTE uncompressedBuffer = new BYTE[decompressedSize];
			Compression::getCompression()->Decompress(uncompressedBuffer, &decompressedSize, out.data+4, out.length-4);

			delete [] out.data;

			out.data = uncompressedBuffer;
			out.length = decompressedSize;
		}

		assert(out.data != nullptr); // THERE IS NO FILE WITH THIS NAME!

	}

	return out;
}
