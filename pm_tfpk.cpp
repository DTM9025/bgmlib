// Music Room BGM Library
// ----------------------
// pm_tfpk.cpp - Parsing for Tasofro PAK archives
// ----------------------
// "©" DTM9025, 2024

#include "platform.h"
#include <FXSystem.h>
#include <FXFile.h>
#include "infostruct.h"
#include "ui.h"
#include "config.h"
#include "packmethod.h"
#include "utils.h"
#include "pm_tfpk.h"
#include "rsa.h"
#include "hashmap_utils.h"

#ifdef WIN32
#include <ctype.h>
#endif

#ifdef SUPPORT_VORBIS_PM
#include "libvorbis.h"
#endif 


// Helper Structs
struct FnHeader
{
	uint compSize;
	uint origSize;
	uint blockCount;
};
struct ListItem
{
	uint FileSize;
	uint Offset;
};


// Info
// ----
bool PM_TFPK::CheckCryptKind(ConfigFile& NewGame, const uchar& CRKind)
{
	if(CRKind == 0 || CRKind < CR_KOKORO || CRKind > CR_SUMIREKO)
	{
		FXString Str;
		Str.format("Invalid encryption kind specified in %s!\n", NewGame.GetFN());
		BGMLib::UI_Error(Str);
		return false;
	}
	return true;
}

bool PM_TFPK::ParseGameInfo(ConfigFile &NewGame, GameInfo *GI)
{
	PF_PGI_BGMFile(NewGame, GI);
	return CheckCryptKind(NewGame, GI->CryptKind);
}

bool PM_TFPK::ParseTrackInfo(ConfigFile &NewGame, GameInfo *GI, ConfigParser* TS, TrackInfo *NewTrack)
{
	TS->GetValue("filename", TYPE_STRING, &NewTrack->NativeFN);

	NewTrack->PosFmt = FMT_SAMPLE;
	GI->Vorbis = true;

	return false;	// Read position info from archive
}

FXString PM_TFPK::DiskFN(GameInfo* GI, TrackInfo* TI)
{
	return GI->BGMFile;
}

bool PM_TFPK::DecryptHeader(GameInfo* GI, FXFile& In)
{
	char magic[4];
	uchar version;

	In.readBlock(&magic, 4);
	if (memcmp(magic, "TFPK", 4) != 0)	return false;

	In.readBlock((char*)&version, sizeof(uchar));
	if (version != PV_KOKORO && version != PV_SUMIREKO)	return false;
	if (version != GI->CryptKind - CR_KOKORO) return false;

	return true;
}

// ----------

// Data
// ----
#ifdef BGMLIB_LIBVORBIS_H
ulong PM_TFPK::DecryptBuffer0(const uchar& CryptKind, char* Out, const ulong& Pos, const ulong& Size, uint* Key)
{
	uchar *key = (uchar*)Key;

	for (ulong i = 0; i < Size; i++)
		Out[i] ^= key[i % 16];

	return Size;
}

ulong PM_TFPK::DecryptBuffer1(const uchar& CryptKind, char* Out, const ulong& Pos, const ulong& Size, uint* Key)
{
	uchar *key = (uchar*)Key;
	uchar  aux[4];

	for (int i = 0; i < 4; i++)
		aux[i] = key[i];

	for (ulong i = 0; i < Size; i++)
	{
		uchar tmp = Out[i];
		Out[i] = Out[i] ^ key[i % 16] ^ aux[i % 4];
		aux[i % 4] = tmp;
	}

	return Size;
}

ulong PM_TFPK::DecryptFileWithKey(GameInfo* GI, FXFile& In, char* Out, const ulong& Pos, const ulong& Size, uint* Key)
{
	if(!Out)	return NULL;

	if(!In.position(Pos)) return 0;
	ulong Ret = In.readBlock(Out, Size);

	switch (GI->CryptKind)
	{
		case CR_KOKORO:
			DecryptBuffer0(GI->CryptKind, Out, Pos, Size, Key);
			break;

		case CR_SUMIREKO:
			DecryptBuffer1(GI->CryptKind, Out, Pos, Size, Key);
			break;
	}

	return Ret;
}

ulong PM_TFPK::DecryptTrack(GameInfo* GI, FXFile& In, char* Out, TrackInfo* TI, volatile FXulong* p)
{
	if (!Out)	return NULL;

	if (TI->isbpack == 0)
	{
		ulong Ret = DecryptFileWithKey(GI, In, Out, TI->GetStart(), TI->FS, TI->okey);
		if (p)	*p = Ret;
		return Ret;
	}
	else if (TI->isbpack == 1)
	{
		FXFile InTemp;
		FXString bfname = FXString(GI->DiskFN(TI)).substitute(".pak", "b.pak");
		if (!InTemp.open(bfname, FXIO::Reading)) return 0;

		ulong Ret = DecryptFileWithKey(GI, InTemp, Out, TI->GetStart(), TI->FS, TI->okey);
		InTemp.close();
		if (p)	*p = Ret;
		return Ret;
	}

	return NULL;
}

void PM_TFPK::MetaData(GameInfo* GI, FX::FXFile& In, const ulong& Pos, const ulong& Size, TrackInfo* TI)
{
	char* SFL = new char[Size];
	DecryptFileWithKey(GI, In, SFL, Pos, Size, TI->skey);

	char* p = SFL + 28;

	memcpy(&TI->Loop, p, 4); p += 4 + 40;
	memcpy(&TI->End, p, 4);

	TI->End += TI->Loop;

	SAFE_DELETE_ARRAY(SFL);
}

void PM_TFPK::ReadFileInfo0(GameInfo* GI, FXFile& In, Rsa& rsa, char isbpak, hashmap* dictOgg, hashmap* dictSfl)
{
	ListItem listItem;
	uint hash;
	uint key[4];

	rsa.read((char*)&listItem, sizeof(listItem));
	rsa.read((char*)&hash, sizeof(hash));
	rsa.read((char*)key, sizeof(key));

	DictItem dkey = { hash };
	DictItem* ret = (DictItem*) hashmap_get(dictOgg, &dkey);
	if (ret != NULL)
	{
		memcpy(ret->TI->okey, key, sizeof(key));
		ret->TI->isbpack = isbpak;
		AudioData(GI, In, listItem.Offset, listItem.FileSize, ret->TI);
		return;
	}

	ret = (DictItem*) hashmap_get(dictSfl, &dkey);
	if (ret != NULL)
	{
		memcpy(ret->TI->skey, key, sizeof(key));
		ret->TI->spos = listItem.Offset;
		ret->TI->sfsize = listItem.FileSize;
		return;
	}
}

void PM_TFPK::ReadFileInfo1(GameInfo* GI, FXFile& In, Rsa& rsa, char isbpak, hashmap* dictOgg, hashmap* dictSfl)
{
	ListItem listItem;
	uint hash[2];
	uint key[4];

	rsa.read((char*)&listItem, sizeof(listItem));
	rsa.read((char*)&hash, sizeof(hash));
	rsa.read((char*)key, sizeof(key));

	listItem.FileSize ^= key[0];
	listItem.Offset ^= key[1];
	hash[0] ^= key[2];
	for (int i = 0; i < 4; i++)
		key[i] *= -1;

	DictItem dkey = { hash[0] };
	DictItem* ret = (DictItem*) hashmap_get(dictOgg, &dkey);
	if (ret != NULL)
	{
		memcpy(ret->TI->okey, key, sizeof(key));
		ret->TI->isbpack = isbpak;
		AudioData(GI, In, listItem.Offset, listItem.FileSize, ret->TI);
		return;
	}

	ret = (DictItem*) hashmap_get(dictSfl, &dkey);
	if (ret != NULL)
	{
		memcpy(ret->TI->skey, key, sizeof(key));
		ret->TI->spos = listItem.Offset;
		ret->TI->sfsize = listItem.FileSize;
		return;
	}
}

void PM_TFPK::GetPosData(GameInfo* GI, FXFile& In, char isbpak)
{
	Rsa rsa(In);
	uint dirCount;
	if (rsa.read(&dirCount, sizeof(uint)) == false)
	{
		BGMLib::UI_Error("Error: unknown RSA key\n");
		return;
	}

	uint dirbuffer[2];
	for (uint i = 0; i < dirCount; i++)
	{
		rsa.read((char*)dirbuffer, sizeof(uint) * 2);
	}

	FnHeader fnHeader;
	rsa.read((char*)&fnHeader, sizeof(FnHeader));
	char* fnbuffer = new char[fnHeader.blockCount * RSA_BLOCK_SIZE];
	rsa.read(fnbuffer, fnHeader.blockCount * RSA_BLOCK_SIZE);
	SAFE_DELETE_ARRAY(fnbuffer);

	uint fileCount;
	rsa.read(&fileCount, sizeof(uint));

	ListEntry<TrackInfo>* CurTrack;
	TrackInfo* TI;

	hashmap *dict_ogg = hashmap_new(sizeof(DictItem), 0, 0, 0, dict_hash, dict_compare, NULL, NULL);
	hashmap *dict_sfl = hashmap_new(sizeof(DictItem), 0, 0, 0, dict_hash, dict_compare, NULL, NULL);
	FXString fnogg;
	FXString fnsfl;

	CurTrack = GI->Track.First();
	while (CurTrack)
	{
		fnogg = FXString(CurTrack->Data.NativeFN).append(".ogg");
		fnsfl = FXString(CurTrack->Data.NativeFN).append(".sfl");
		uint hashogg;
		uint hashsfl;
		switch (GI->CryptKind)
		{
			case CR_KOKORO:
				hashogg = SpecialFNVHash0(fnogg);
				hashsfl = SpecialFNVHash0(fnsfl);
				break;
			case CR_SUMIREKO:
				hashogg = SpecialFNVHash1(fnogg);
				hashsfl = SpecialFNVHash1(fnsfl);
				break;
		}
		DictItem ogg_item = { hashogg, &CurTrack->Data};
		hashmap_set(dict_ogg, &ogg_item);

		DictItem sfl_item = { hashsfl, &CurTrack->Data};
		hashmap_set(dict_sfl, &sfl_item);

		CurTrack = CurTrack->Next();
	}

	for (uint i = 0; i < fileCount; i++)
	{
		switch (GI->CryptKind)
		{
			case CR_KOKORO:
				ReadFileInfo0(GI, In, rsa, isbpak, dict_ogg, dict_sfl);
				break;
			
			case CR_SUMIREKO:
				ReadFileInfo1(GI, In, rsa, isbpak, dict_ogg, dict_sfl);
				break;
		}
	}

	ulong curPos = In.position();
	CurTrack = GI->Track.First();
	while (CurTrack)
	{
		TI = &CurTrack->Data;
		if (TI->isbpack != isbpak)
		{
			CurTrack = CurTrack->Next();
			continue;
		}

		if (TI->Start[0] == 0)
		{
			TI->FS = -1;
			CurTrack = CurTrack->Next();
			continue;
		}

		TI->Start[0] += curPos;
		TI->Start[1] += curPos;

		if (TI->spos != 0 )
		{
			TI->spos += curPos;
			MetaData(GI, In, CurTrack->Data.spos, CurTrack->Data.sfsize, &CurTrack->Data);
		}

		if (TI->Loop == 0)
		{
			// LARGE_INTEGER Time[2], Total;
			// QueryPerformanceCounter(&Time[0]);

			// Right. This is indeed faster than the CryptFile solution.
			VFile Dec(TI->FS);
			DecryptTrack(GI, In, Dec.Buf, TI, &Dec.Write);

			OggVorbis_File SF;
			if (ov_open_callbacks(&Dec, &SF, NULL, 0, OV_CALLBACKS_VFILE))
			{
				char sbuffer[128];
				sprintf(sbuffer, "Error decrypting %s!\n", TI->NativeFN.text());
				BGMLib::UI_Stat(sbuffer);
			}
			else
			{
				TI->Loop = TI->End = ov_pcm_total(&SF, -1);
				ov_clear(&SF);
			}
			Dec.Clear();

			// QueryPerformanceCounter(&Time[1]);
			// Total = Time[1] - Time[0];
		}
		CurTrack = CurTrack->Next();
	}

	FXSystem::setCurrentDirectory(GI->Path);

	// We don't silence scan those
	GI->Scanned = true;
}
#endif
// ----

bool PM_TFPK::TrackData(GameInfo* GI)
{
	FXFile InA;
	FXFile InB;

	//For normal .pak
	if(!InA.open(GI->BGMFile, FXIO::Reading))	return false;
	DecryptHeader(GI, InA);
	GetPosData(GI, InA, 0);
	InA.close();

	//For b.pak
	FXString bfname = FXString(GI->BGMFile).substitute(".pak", "b.pak");
	if(!InB.open(bfname, FXIO::Reading))	return false;
	DecryptHeader(GI, InB);
	GetPosData(GI, InB, 1);
	InB.close();

	// GI->Scanned gets set by SilenceScan
	return true;
}

// Scanning
// --------
GameInfo* PM_TFPK::Scan(const FXString& Path)
{
	return PF_Scan_BGMFile(Path);
}
// --------

// Hashing Utilities
uint PM_TFPK::SpecialFNVHash0(const FXString& path)
{
	const char *cpath = (char*) path.text();
	uint initHash = 0x811C9DC5u;
	uint hash; // eax@1
	uint ch; // esi@2

	for (hash = initHash; *cpath; hash = ch ^ 0x1000193 * hash)
	{
		unsigned char c = *cpath++;
		ch = c;
		if ((c & 0x80) == 0)
		{
			ch = tolower(ch);
			if (ch == '/')
			ch = '\\';
		}
	}
	return hash;
}

uint PM_TFPK::SpecialFNVHash1(const FXString& path)
{
	const char *cpath = (char*) path.text();
	uint initHash = 0x811C9DC5u;
	uint hash; // eax@1
	uint ch; // esi@2

	for (hash = initHash; *cpath; hash = (hash ^ ch) * 0x1000193)
	{
		unsigned char c = *cpath++;
		ch = c;
		if ((c & 0x80) == 0)
		{
			ch = tolower(ch);
			if (ch == '/')
			ch = '\\';
		}
	}
	return hash * -1;
}
