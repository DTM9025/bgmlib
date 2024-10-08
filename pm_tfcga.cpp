// Music Room BGM Library
// ----------------------
// pm_tfpk.cpp - Parsing for Tasofro CGA archives
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
#include "pm_tfcga.h"
#include "ini.h"
#include "hashmap_utils.h"

#ifdef WIN32
#include <ctype.h>
#endif

#ifdef SUPPORT_VORBIS_PM
#include "libvorbis.h"
#endif 


struct OggIni
{
	ulong repeatstart;
	ulong repeatend;
};

static int handler(void* user, const char* section, const char* name, const char* value)
{
	OggIni* oggini = (OggIni*)user;

	#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	if (MATCH("loop0", "repeatstart")) {
		oggini->repeatstart = atoi(value);
	} else if (MATCH("loop0", "repeatend")) {
		oggini->repeatend = atoi(value);
	} else {
		return 0;  /* unknown section/name, error */
	}
	return 1;
}


// Info
// ----
bool PM_TFCGA::CheckCryptKind(ConfigFile& NewGame, const uchar& CRKind)
{
	if(CRKind != CR_YUUMA)
	{
		FXString Str;
		Str.format("Invalid encryption kind specified in %s!\n", NewGame.GetFN());
		BGMLib::UI_Error(Str);
		return false;
	}
	return true;
}

bool PM_TFCGA::ParseGameInfo(ConfigFile &NewGame, GameInfo *GI)
{
	PF_PGI_BGMFile(NewGame, GI);
	return CheckCryptKind(NewGame, GI->CryptKind);
}

bool PM_TFCGA::ParseTrackInfo(ConfigFile &NewGame, GameInfo *GI, ConfigParser* TS, TrackInfo *NewTrack)
{
	TS->GetValue("filename", TYPE_STRING, &NewTrack->NativeFN);

	NewTrack->PosFmt = FMT_SAMPLE;
	GI->Vorbis = true;

	return false;	// Read position info from archive
}

FXString PM_TFCGA::DiskFN(GameInfo* GI, TrackInfo* TI)
{
	return GI->BGMFile;
}

bool PM_TFCGA::DecryptFooter(GameInfo* GI, FXFile& In, FileFooter* Out)
{
	if (!In.position(In.size() - sizeof(FileFooter))) return false;

	if (In.readBlock(Out, sizeof(FileFooter)) == 0) return false;

	DecryptBuffer((char*)Out, In.size() - sizeof(FileFooter), sizeof(FileFooter));

	if (Out->footer_size != 0x20 || sizeof(FileDesc) != 0x18 || Out->file_desc_size != 0x18)
		return false;
	
	return true;
}

// ----------

// Data
// ----
#ifdef BGMLIB_LIBVORBIS_H

void PM_TFCGA::DoPartialXOR(char* dst, char* src, ulong size)
{
	for (ulong i = 0; i < size; i++)
	{
		dst[i] ^= src[i];
	}
}

uint PM_TFCGA::DoDecryptStep(uint key)
{
    i64 a = key * 0x5E4789C9ll;
	uint b = (a >> 0x2E) + (a >> 0x3F);
	uint ret = (key - b * 0xADC8) * 0xBC8F + b * 0xFFFFF2B9;
	if ((int)ret <= 0) {
        ret += 0x7FFFFFFF;
    }
	return ret;
}

ulong PM_TFCGA::DecryptBuffer(char* Out, const ulong& Pos, const ulong& Size)
{
	uint file_key = Size ^ Pos;

	for (size_t pos = 0; pos < Size; pos += 4) {
		uint xor = 0;
		uint tmp_key = file_key;
		for (size_t i = 0; i < 4; i++) {
			tmp_key = DoDecryptStep(tmp_key);
			xor = (xor << 8) | (tmp_key & 0xFF);
		}

		if (pos + 4 <= Size) {
			*(uint*)(Out + pos) ^= xor;
		}
		else {
			DoPartialXOR(Out + pos, (char*)&xor, Size - pos);
		}
		file_key++;
	}
	return Size;
}

ulong PM_TFCGA::DecryptTrack(GameInfo* GI, FXFile& In, char* Out, TrackInfo* TI, volatile FXulong* p)
{
	if (!Out)	return NULL;

	if (TI->isbpack == 0)
	{
		if(!In.position(TI->GetStart())) return 0;
		ulong Ret = In.readBlock(Out, TI->FS);
		Ret = DecryptBuffer(Out, TI->GetStart(), TI->FS);
		if (p)	*p = Ret;
		return Ret;
	}
	else if (TI->isbpack == 1)
	{
		FXFile InTemp;
		FXString bfname = FXString(GI->DiskFN(TI)).substitute(".cga", ".cgb");
		if (!InTemp.open(bfname, FXIO::Reading)) return 0;

		if(!InTemp.position(TI->GetStart())) return 0;
		ulong Ret = InTemp.readBlock(Out, TI->FS);
		Ret = DecryptBuffer(Out, TI->GetStart(), TI->FS);
		InTemp.close();
		if (p)	*p = Ret;
		return Ret;
	}

	return NULL;
}

void PM_TFCGA::MetaData(GameInfo* GI, FX::FXFile& In, const ulong& Pos, const ulong& Size, TrackInfo* TI)
{
	char* buf = new char[Size + 1];
	buf[Size] = '\0';

	In.position(Pos);
	In.readBlock(buf, Size);

	DecryptBuffer(buf, Pos, Size);

	OggIni oggini;
	ini_parse_string(buf, handler, &oggini);

	TI->Loop = oggini.repeatstart;
	TI->End = oggini.repeatend;

	SAFE_DELETE_ARRAY(buf);
}

void PM_TFCGA::ReadFileInfo(GameInfo* GI, FXFile& In, FileDesc& fdesc, char isbpak, hashmap* dictOgg, hashmap* dictIni)
{
	uint hash = fdesc.key;

	DictItem dkey = { hash };
	DictItem* ret = (DictItem*) hashmap_get(dictOgg, &dkey);
	if (ret != NULL)
	{
		ret->TI->isbpack = isbpak;
		AudioData(GI, In, fdesc.offset, fdesc.size, ret->TI);
		return;
	}

	ret = (DictItem*) hashmap_get(dictIni, &dkey);
	if (ret != NULL)
	{
		ret->TI->spos = fdesc.offset;
		ret->TI->sfsize = fdesc.size;
		return;
	}
}

void PM_TFCGA::GetPosData(GameInfo* GI, FXFile& In, char isbpak, uint numfiles)
{
	ListEntry<TrackInfo>* CurTrack;
	TrackInfo* TI;

	hashmap *dict_ogg = hashmap_new(sizeof(DictItem), 0, 0, 0, dict_hash, dict_compare, NULL, NULL);
	hashmap *dict_ini = hashmap_new(sizeof(DictItem), 0, 0, 0, dict_hash, dict_compare, NULL, NULL);
	FXString fnogg;
	FXString fnini;

	CurTrack = GI->Track.First();
	while (CurTrack)
	{
		fnogg = FXString(CurTrack->Data.NativeFN).append(".ogg");
		fnini = FXString(CurTrack->Data.NativeFN).append(".ogg.ini");
		uint hashogg = SpecialFNVHash(fnogg);
		uint hashini = SpecialFNVHash(fnini);

		DictItem ogg_item = { hashogg, &CurTrack->Data};
		hashmap_set(dict_ogg, &ogg_item);

		DictItem ini_item = { hashini, &CurTrack->Data};
		hashmap_set(dict_ini, &ini_item);
		CurTrack = CurTrack->Next();
	}

	size_t ftable_size = sizeof(FileDesc) * numfiles;
	size_t ftable_offset = In.size() - sizeof(FileFooter) - ftable_size;

	FileDesc* ftable = (FileDesc*)malloc(ftable_size);
	In.position(ftable_offset);
	In.readBlock(ftable, ftable_size);
	DecryptBuffer((char*)ftable, ftable_offset, ftable_size);

	for (uint i = 0; i < numfiles; i++)
	{
		ReadFileInfo(GI, In, ftable[i], isbpak, dict_ogg, dict_ini);
	}

	free(ftable);

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

		if (TI->spos != 0 )
		{
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

bool PM_TFCGA::TrackData(GameInfo* GI)
{
	FXFile InA;
	FXFile InB;

	//For normal .cga
	FileFooter Ffa;
	if(!InA.open(GI->BGMFile, FXIO::Reading))	return false;
	DecryptFooter(GI, InA, &Ffa);
	GetPosData(GI, InA, 0, Ffa.nb_files);
	InA.close();

	//For .cgb
	FileFooter Ffb;
	FXString bfname = FXString(GI->BGMFile).substitute(".cga", ".cgb");
	if(!InB.open(bfname, FXIO::Reading))	return false;
	DecryptFooter(GI, InB, &Ffb);
	GetPosData(GI, InB, 1, Ffb.nb_files);
	InB.close();

	// GI->Scanned gets set by SilenceScan
	return true;
}

// Scanning
// --------
GameInfo* PM_TFCGA::Scan(const FXString& Path)
{
	return PF_Scan_BGMFile(Path);
}
// --------

// Hashing Utilities
uint PM_TFCGA::SpecialFNVHash(const FXString& fname)
{
	const char* filename = fname.text();
	u64 hash = 0x811C9DC5;
	for (int i = 0; filename[i]; i++) {
		hash = ((hash ^ filename[i]) * 0x1000193) & 0xFFFFFFFF;
	}
	return (uint)hash;
}
