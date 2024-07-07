// Music Room BGM Library
// ----------------------
// PM_TFCGA.h - Twilight Frontier's Pack Methods for TH17.5
// ----------------------
// "�" DTM9025, 2024

#ifndef BGMLIB_PM_TFCGA_H
#define BGMLIB_PM_TFCGA_H
#include <FXHash.h>
#include <FXStream.h>
#include <FXDict.h>

// Pack Methods
#define BMCGA   0x7	// Vorbis files and INI loop info in .cga/b files with footer (th175)

// Encryptions
// -----------
#define CR_NONE   0x0 // No encryption
#define CR_YUUMA  0x6 // CGA Encryption

// Helper Structs
struct FileFooter
{
	uint unk1;
	uint unk2;
	uint unk3; // Non-zero (4 bytes filled)
	uint unk4;
	uint unk5;
	uint file_desc_size; // 0x18
	uint nb_files;
	uint footer_size; // must be 0x20
};
struct FileDesc
{
	u64 key;
	u64 offset;
	u64 size;
};

class PM_TFCGA : public PackMethod
{
private:
	uint SpecialFNVHash(const FXString& fname);

	void DoPartialXOR(char* dst, char* src, ulong size);
	uint DoDecryptStep(uint key);
	inline ulong DecryptBuffer(char* Out, const ulong& Pos, const ulong& Size);	// Contains the decryption algorithm

	void ReadFileInfo(GameInfo* GI, FXFile& In, FileDesc& rsa, char isbpak, FXDict* dictOgg, FXDict* dictIni);
protected:
	PM_TFCGA()	{ID = BMCGA;}

	bool CheckCryptKind(ConfigFile& NewGame, const uchar& CRKind);

	void GetPosData(GameInfo* GI, FX::FXFile& In, char isbpack, uint numfiles);

	bool DecryptFooter(GameInfo* GI, FX::FXFile& In, FileFooter* Out);

	void MetaData(GameInfo* GI, FXFile& In, const ulong& Pos, const ulong& Size, TrackInfo* TI);	// SFL Format
public:
	bool TrackData(GameInfo* GI);

	bool ParseGameInfo(ConfigFile& NewGame, GameInfo* GI);
	bool ParseTrackInfo(ConfigFile& NewGame, GameInfo* GI, ConfigParser* TS, TrackInfo* NewTrack);		// return true if position data should be read from config file

	FXString DiskFN(GameInfo* GI, TrackInfo* TI);

	ulong DecryptFile(GameInfo* GI, FXFile& In, char* Out, const ulong& Pos, const ulong& Size, volatile FXulong* p = NULL);

	GameInfo* Scan(const FXString& Path);	// Scans [Path] for a game packed with this 
	
	SINGLETON(PM_TFCGA);
};

#endif /* BGMLIB_PM_TFCGA_H */
