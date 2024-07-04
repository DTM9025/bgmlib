#ifndef RSA_HPP_
#define RSA_HPP_

extern "C"
{
#include "miracl.h"
}

static const int RSA_BLOCK_SIZE = 32;

class Rsa
{
public:
	enum class CryptMode
	{
		TH135,
		TH145
	};

private:
	static bool miraclInitialized;

	FX::FXFile *ifile;
	bool publicKeyInitialized = false;
	CryptMode cryptMode;
	big RSA_N;
	big RSA_d;
	big RSA_e;

	void init();
	bool initRsaPublicKey(const unsigned char *crypted_sample);
	bool skipPadding(const unsigned char *data, size_t &i);
	bool checkPadding(const unsigned char *data);
	bool skipPaddingAndCopy(const unsigned char *src, unsigned char *dst, size_t size);
	void DecryptBlock(const unsigned char *src, unsigned char *dst);
	bool Decrypt6432(const unsigned char *src, unsigned char *dst, size_t dstLen);
	bool read32(void *buffer, size_t size);

public:
	// The File object needs to be alive as long as the Rsa object is.
	Rsa(FX::FXFile &file);
	~Rsa();
	bool read(void *buffer, size_t size);

	static void initMiracl();
	static void freeMiracl();
};

#endif /* RSA_HPP_ */
