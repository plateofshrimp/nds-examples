/*---------------------------------------------------------------------------------

Basic FreeType tests to validate the devkitARM package and loose builds.

---------------------------------------------------------------------------------*/
#include <dirent.h>
#include <fat.h>
#include <nds.h>
#include <stdio.h>

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_CACHE_H

// melonDS needs to point to a FAT disk image containing this file.
#define FONTFILE "/font/LiberationSans-Regular.ttf"

void print_bitmap(FT_Bitmap bitmap) {
	//! Crops (upper-left) bitmaps to fit the console.
	FT_UInt max = 8;
    printf("bitmap is %dx%d pitch %d\n", bitmap.width, bitmap.rows, bitmap.pitch);
    if (!bitmap.buffer) {
        printf("no bitmap buffer\n");
        return;
    }
    for (u8 row = 0; row < bitmap.rows && row < max; row++) {
        for (u8 col = 0; col < bitmap.width && col < max; col++) {
            unsigned char pixel = bitmap.buffer[row * bitmap.pitch + col];
            printf("%02x ", pixel);
        }
        printf("\n");
    }
}

void print_bitmap(FT_Bitmap bitmap, u8* origin) {
	//! {origin} is some offset into VRAM
    for (u8 row = 0; row < bitmap.rows; row++) {
        for (u8 col = 0; col < bitmap.width; col++) {
            u8 pixel = bitmap.buffer[row * bitmap.pitch + col];
			origin[row * SCREEN_WIDTH + col] = pixel;
        }
        printf("\n");
    }
}

void glyph_format_as_magic(int format, char *magic) {
    magic[0] = format >> 24;
    magic[1] = format >> 16;
    magic[2] = format >> 8;
    magic[3] = format;
}

FT_Error load_glyph(FT_Face face, FT_UInt glyph_index) {
	
    FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

    if(error) { printf("FT_Load_Glyph error=%d\n", error); return error; }
    
    char magic[4];
    glyph_format_as_magic(face->glyph->format, magic);
    printf("%c%c%c%c bitmap=%dx%d\n", 
        magic[0], magic[1], magic[2], magic[3],
        face->glyph->bitmap.width,
        face->glyph->bitmap.rows
    );
    if(!face->glyph->bitmap.buffer)
	    printf("no bitmap buffer\n");
    return error;
}

FT_Error set_sizes(FT_Face face, FT_UInt size = 12) {

    FT_Error error = FT_Set_Pixel_Sizes(face, size, size);
	if (error) {
		printf("FT_Set_Pixel_Sizes error=%d\n", error);
		error = FT_Set_Char_Size(face,
						10 << 6,
						10 << 6,
						72,
						72);
	}
	return error;
}

FT_Error request_pixel_size(FT_Face face, FT_UInt size = 12) {
	//! Request a pixel size.  Call this before rasterizing.

    FT_Size_RequestRec s;
    s.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
    s.width = size << 6;
    s.height = size << 6;
    s.horiResolution = 0;
    s.vertResolution = 0;
    
    FT_Error error = FT_Request_Size(face, &s);
    if(error) printf("FT_Size_Request error=%d\n", error);

    return error;
}

void pause(FT_UInt jiffies) {
  //! A jiffie is 1 vertical blanking period.
  // Blocking, so no I/O etc while spinning.

  int timer = jiffies;
  while (pmMainLoop() && timer) {
    swiWaitForVBlank();
    timer--;
  }
}

int render_glyph(FT_ULong charcode)
{
	//! Actually prints the glyph for {charcode} to the console.

	FT_Error error;

    FT_Library library;
    error = FT_Init_FreeType(&library);
    if (error) { printf("FT_Init_FreeType error=%d\n", error); return error; }

    FT_Open_Args args;
    args.flags = FT_OPEN_PATHNAME;
    char pathname[64];
    strncpy(pathname, FONTFILE, 63);
    args.pathname = pathname;
	printf("path=%s\n", args.pathname);

    FT_Face face;
    error = FT_Open_Face(library, &args, 0, &face);
    if(error) printf("FT_Open_Face error=%d\n", error);

    printf("family=%s\n"
	   "faces=%ld\n"
	   "glyphs=%ld\n"
	   "fixed_sizes=%d\n"
	   "charmaps=%d\n"
	   "charmap[0] is %s\n",
	   face->family_name,
	   face->num_faces, face->num_glyphs, 
	   face->num_fixed_sizes, face->num_charmaps,
	   face->charmap[0].encoding == FT_ENCODING_UNICODE ? "unicode" : "not unicode");
	
	// Select the Unicode character map, our charcode is in Unicode.
    error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if(!error) printf("selected unicode charmap\n");

	// Now we can find out which glyph represents this character code.
	FT_UInt glyph_index = FT_Get_Char_Index(face, charcode);
	printf("charcode %ld is glyph %d\n", charcode, glyph_index);
   
	// Assure that a glyph size is established before rasterization.
    error = request_pixel_size(face, 24);  // not points.
	if (error) printf("request_size error=%d\n", error);

    // error = set_sizes(face);
	// 10 pixels at 72 DPI.
    // error = FT_Set_Char_Size(face, 10 << 6, 0, 72, 0);

	// The EM should be roughly the size.
    printf("x_ppem=%d y_ppem=%d\n",
        face->size->metrics.x_ppem,
        face->size->metrics.y_ppem);

	// Put the outline in the glyph slot.
    // error = load_glyph(face, glyph_index);
    error = FT_Load_Char(face, charcode, FT_LOAD_DEFAULT);
    if(error) { printf("FT_Load_Char error=0x%x\n", error); return error; }

	// Create a bitmap in the glyph slot.
    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if(error) { printf("FT_Render_Glyph error=%xl\n", error); return error; }

    print_bitmap(face->glyph->bitmap);

    FT_Done_FreeType(library);
    return error;
}

typedef struct FaceIDRec_ {
    const char* file_path;
    FT_ULong    face_index;
} FaceIDRec, *FaceID;

static FT_Error
TextFaceRequester(
    FTC_FaceID  face_id,
    FT_Library  library,
    FT_Pointer  request_data,
    FT_Face*    aface )
{
	FaceID face = (FaceID)face_id;
	return FT_New_Face(library,
		face->file_path,
		face->face_index,
		aface);
}

int render_glyph_with_cache()
{
    // Render a glyph from a font using FreeType's cache sub-system.

    FT_Error error;

    FT_Library library;
    FT_Init_FreeType(&library);

    FTC_Manager manager;
	FTC_Manager_New(library,
        0, 0, 0,
	&TextFaceRequester, NULL,
        &manager);

    FTC_ImageCache  imagecache;
	FTC_ImageCache_New(manager, &imagecache);

    FTC_SBitCache sbitcache;
	FTC_SBitCache_New(manager, &sbitcache);

    FTC_CMapCache cmapcache;
	FTC_CMapCache_New(manager, &cmapcache);

    // Author the request structs.

    FaceIDRec id;
    id.file_path = FONTFILE;
    id.face_index = 0;
    FTC_FaceID face_id = (FTC_FaceID)&id;

    FTC_ImageTypeRec rec;
    rec.face_id = face_id;
    rec.flags = FT_LOAD_DEFAULT;
    rec.height = 12; // Example height
    rec.width = 0;   // Width can be 0 for variable width fonts
    const FTC_ImageType imagetype = &rec;

    const FT_UInt32 codepoint = 0x0046; // UCS 'F'

    FT_Face face;
    error = FTC_Manager_LookupFace(manager, face_id, &face);

    if (error)
        printf("FTC_Manager_LookupFace error(%d)\n", error);
    else
        printf("%s\n%ld face(s)\n", face->family_name, face->num_faces);

    // Get the glyph index for the codepoint.

    FT_UInt glyph_index = FTC_CMapCache_Lookup(cmapcache, face_id, -1, codepoint);

    printf("char=%d glyph=%d\n", codepoint, glyph_index);

    // Get a glyph from the image cache.

    FT_Glyph glyph = nullptr;
    FTC_Node n1;
    error = FTC_ImageCache_Lookup(imagecache, imagetype, glyph_index, &glyph, &n1);

    if (error)
        printf("%d\n", error);
    else
        printf("Glyph:\n"
            "  Advance: %f\n",
            glyph->advance.x / 65336.0);

    // Assure that there is a bitmap.

    if ( glyph->format != FT_GLYPH_FORMAT_BITMAP ) {
        error = FT_Glyph_To_Bitmap( &glyph, FT_RENDER_MODE_LCD_V, 0, 1 );
        if (error)
            printf("FT_Glyph_To_Bitmap had no effect (%d)\n", error);
    }

    FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
    printf("Bitmap:\n"
	   "  Size: %dx%d\n"
	   "  Left: %d\n"
	   "  Top: %d\n",
	   bitmap_glyph->bitmap.width, bitmap_glyph->bitmap.rows,
	   bitmap_glyph->left, bitmap_glyph->top);

    // Alternately, get a small bitmap from the sbit cache.

    FTC_SBit sbit;
    FTC_Node n2;
    error = FTC_SBitCache_Lookup(sbitcache, imagetype, glyph_index, &sbit, &n2);

    printf("FTC_SBitCache_Lookup error=%d\n", error);
    
    if (glyph)
        FT_Done_Glyph(glyph);
    FT_Done_FreeType(library);

    return error;
}

int main(void) {
	touchPosition touchXY;

	defaultExceptionHandler();
	consoleDemoInit();

	// Font is stored in DLDI FAT.
	auto success = fatInitDefault();
	if(!success) {
		fprintf(stderr, "no filesystem\n");
		return 1;
	}

	render_glyph('F');
	// render_glyph_with_cache('F');

	while(pmMainLoop()) {

		swiWaitForVBlank();
		scanKeys();
		int keys = keysDown();
		if (keys & KEY_START) break;

		touchRead(&touchXY);
	}

	return 0;
}
