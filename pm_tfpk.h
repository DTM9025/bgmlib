// Music Room BGM Library
// ----------------------
// pm_tfpk.h - Twilight Frontier's Pack Methods for TH13.5 to TH15.5
// ----------------------
// "�" DTM9025, 2024

#ifndef BGMLIB_PM_TFPK_H
#define BGMLIB_PM_TFPK_H

// Pack Methods
#define BMPAK   0x6	// Vorbis files and SFL loop info in .pak files with header (th135 - th155)

// Encryptions
// -----------
#define CR_NONE   0x0 // No encryption

// Tasofro TH13.5 - TH15.5
#define CR_KOKORO   0x4 // RSA Encrypted
#define CR_SUMIREKO 0x5 // RSA Encrypted
// -----------

// PAK Versions
// -----------
#define PV_KOKORO   0x0
#define PV_SUMIREKO 0x1

class Rsa;

class PM_TFPK : public PackMethod
{
private:
	uint SpecialFNVHash0(const FXString& path);
	uint SpecialFNVHash1(const FXString& path);

	inline ulong DecryptBuffer0(const uchar& CryptKind, char* Out, const ulong& Pos, const ulong& Size, uint* Key);	// Contains the decryption algorithm
	inline ulong DecryptBuffer1(const uchar& CryptKind, char* Out, const ulong& Pos, const ulong& Size, uint* Key);	// Contains the decryption algorithm

	void PM_TFPK::ReadFileInfo0(GameInfo* GI, FXFile& In, Rsa& rsa, char isbpak);
	void PM_TFPK::ReadFileInfo1(GameInfo* GI, FXFile& In, Rsa& rsa, char isbpak);
protected:
	PM_TFPK()	{ID = BMPAK;}

	void GetPosData(GameInfo* GI, FX::FXFile& In, char isbpack);

	bool DecryptHeader(GameInfo* GI, FX::FXFile& In);

	bool CheckCryptKind(ConfigFile& NewGame, const uchar& CRKind);	// Checks if given encryption kind is valid

	void MetaData(GameInfo* GI, FXFile& In, const ulong& Pos, const ulong& Size, TrackInfo* TI);	// SFL Format

	ulong DecryptFileWithKey(GameInfo* GI, FXFile& In, char* Out, const ulong& Pos, const ulong& Size, uint* Key);
public:
	bool TrackData(GameInfo* GI);

	bool ParseGameInfo(ConfigFile& NewGame, GameInfo* GI);
	bool ParseTrackInfo(ConfigFile& NewGame, GameInfo* GI, ConfigParser* TS, TrackInfo* NewTrack);		// return true if position data should be read from config file

	FXString DiskFN(GameInfo* GI, TrackInfo* TI);

	ulong DecryptFile(GameInfo* GI, FXFile& In, char* Out, const ulong& Pos, const ulong& Size, volatile FXulong* p = NULL);

	GameInfo* Scan(const FXString& Path);	// Scans [Path] for a game packed with this 
	
	SINGLETON(PM_TFPK);
};

#endif /* BGMLIB_PM_TFPK_H */
