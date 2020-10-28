// MegaTexture.h
//

//
// iceMegaEntry
//
struct iceMegaEntry {
	idStr texturePath;
	int width;
	int height;
	int x;
	int y;
	byte* data_copy;
};

//
// iceMegaTexture
//
class iceMegaTexture {
public:
	iceMegaTexture();
	~iceMegaTexture();

	void				InitTexture(void);
	void				RegisterTexture(const char* texturePath, int width, int height, byte* data);
	void				BuildMegaTexture(void);
	void				FindMegaTile(const char* name, float& x, float& y, float& width, float& height);

	byte* GetMegaBuffer(void) { return megaTextureBuffer; }
private:	
	byte*				megaTextureBuffer;
	bool				isRegistered;
	idList<iceMegaEntry> megaEntries;	
	idImagePacker*		imagePacker;
};