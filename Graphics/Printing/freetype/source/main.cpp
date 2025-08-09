/*-----------------------------------------------------------------------------

Basic FreeType tests to validate the devkitARM package and loose builds.

-----------------------------------------------------------------------------*/
#include "main.h"

#include <dirent.h>
#include <fat.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H
#include <nds.h>
#include <stdio.h>


void print(FT_Bitmap bitmap) {
	//! Crops (upper-left) bitmaps to fit the console.
    if (!bitmap.buffer) {
        printf("no bitmap buffer\n");
        return;
    }
 
	const FT_UInt max = 8;
    for (u8 row = 0; row < bitmap.rows && row < max; row++) {
        for (u8 col = 0; col < bitmap.width && col < max; col++) {
            unsigned char pixel = bitmap.buffer[row * bitmap.pitch + col];
            printf("%02x ", pixel);
        }
        printf("\n");
    }
}

void display(FT_Bitmap bitmap, u16* gfx) {
        for (u8 row = 0; row < bitmap.rows; row++) {
        for (u8 col = 0; col < bitmap.pitch; col++) {
            u8 a = 255;
            if (bitmap.buffer)
                a = bitmap.buffer[row * bitmap.pitch + col];
            if (a)
                gfx[row * SCREEN_WIDTH + col] = RGB15(a, a, a) | BIT(15);
        }
    }
}

void clear(u16* gfx) {
    u16 color = RGB15(0, 0, 0) | BIT(15);
    for (u8 row = 0; row < SCREEN_HEIGHT; row++) {
        for (u8 col = 0; col < SCREEN_WIDTH; col++) {
            gfx[row * SCREEN_WIDTH + col] = color;
        }
    }
}

void glyph_format_as_magic(int format, char *magic) {
    magic[0] = format >> 24;
    magic[1] = format >> 16;
    magic[2] = format >> 8;
    magic[3] = format;
}

FT_Error load_glyph(FT_Face face, FT_UInt glyph_index) {
	FT_Error error;
    
    error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    if(error) { printf("FT_Load_Glyph error=%d\n", error); return error; }

    auto advance = face->glyph->advance.x;
    printf("advance=%ld %ld\n", advance, advance >> 6);
    
    char magic[4];
    glyph_format_as_magic(face->glyph->format, magic);
    printf("format=%s",
        face->glyph->format == FT_GLYPH_FORMAT_OUTLINE ? "outline" : "bitmap");
    printf(" magic=%c%c%c%c\n",
        magic[0], magic[1], magic[2], magic[3]);

    // Create a bitmap in the glyph slot.
    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if(error) { printf("FT_Render_Glyph error=%xl\n", error); }
    if(!face->glyph->bitmap.buffer)
	    printf("no bitmap buffer\n");
    printf("ok\n");

    return error;
}

FT_Error set_size(FT_Face face, FT_UInt size = 12) {
    FT_Error error = FT_Set_Pixel_Sizes(face, size, size);
	if (error) {
		printf("FT_Set_Pixel_Sizes error=%d\n", error);
		error = FT_Set_Char_Size(face,
						size << 6,
						size << 6,
						72,
						72);
	}
	return error;
}

FT_Error request_size(FT_Face face, FT_UInt size = 12) {
	//! Request a pixel size.  Call this before rasterizing.

    FT_Size_RequestRec s;
    s.type = FT_SIZE_REQUEST_TYPE_NOMINAL;
    s.width = size << 6;
    s.height = size << 6;
    s.horiResolution = 110;
    s.vertResolution = 110;
    
    FT_Error error = FT_Request_Size(face, &s);
    if(error) printf("FT_Size_Request error=%d\n", error);

    return error;
}

FT_Error open_face(FT_Library &library, FT_Face &face) {
    FT_Error error;
    char pathname[64];
    
    strncpy(pathname, FONTFILE, 63);

    FT_Open_Args args;
    args.flags = FT_OPEN_PATHNAME;
    args.pathname = pathname;

    error = FT_Open_Face(library, &args, 0, &face);
    if(error) {
        printf("FT_Open_Face error=%d\n", error);
        return error;
    }

    printf(
        "family=%s\n"
        "style=%s\n"
	    "faces=%ld glyphs=%ld\n"
	    "fixed_sizes=%d charmaps=%d\n",
	    face->family_name, face->style_name,
	    face->num_faces, face->num_glyphs, 
	    face->num_fixed_sizes, face->num_charmaps);

    char magic[5];
    for (int i = 0; i < face->num_charmaps; i++) {
        glyph_format_as_magic(face->charmap[i].encoding, magic);
        magic[4] = 0;
        printf("encoding%d=%s\n", i, magic);
    }

    return error;
}

void pause(FT_UInt vblanks) {
  //! Spin for {vblanks} vertical blanking periods.
  // Blocking, so no I/O etc while spinning.

  int timer = vblanks;
  while (pmMainLoop() && timer) {
    swiWaitForVBlank();
    timer--;
  }
}

int main(void) {
    // Enable this to get an address at crash.
	defaultExceptionHandler();

    //set the mode for 2 text layers and two extended background layers
	videoSetMode(MODE_5_2D);

    // Set the first two banks as background memory and the third as sub background memory.
	// D is not used.
    // If you need a bigger background, then you will need to map more vram banks consecutively.
    // VRAM A-D are all 0x20000 bytes in size.
	vramSetPrimaryBanks(
        VRAM_A_MAIN_BG_0x06000000, 
        VRAM_B_MAIN_BG_0x06020000,
		VRAM_C_SUB_BG,
        VRAM_D_LCD);

    int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0,0);
	u16* backBuffer = (u16*)bgGetGfxPtr(bg) + 256*256;

	consoleDemoInit();

	auto success = fatInitDefault();
	if(!success) {
		printf("no filesystem\n");
        pause(120); return 1;
    }

    FT_UInt16 charcode = 70; // Unicode 0x0046 'F'
    FT_Int size = 48; // pixels

    FT_Library library;
    FT_Error error = FT_Init_FreeType(&library);
        if (error) {
            printf("FT_Init_FreeType error=%d\n", error);
            pause(120); return 1;
    }

    FT_Face face;
    error = open_face(library, face);
   
	// Assure that a glyph size is established before rasterization.
    error = request_size(face, size);
    if (error) {
        printf("request_size error=%d\n", error);
        error = set_size(face, size);
        if (error) printf("set_size error=%d\n", error);
    }
    printf("requested size=%d\n", size);

    // The EM size should be roughly the requested size.
    printf("x_ppem=%d y_ppem=%d\n",
        face->size->metrics.x_ppem,
        face->size->metrics.y_ppem);

    // Load {charcode}'s bitmap into {face}'s slot.

    // Select the Unicode character map, our charcode is in Unicode.
    error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if(!error) printf("selected unicode charmap\n");

	// Now we can find out which glyph represents this character code.
    	FT_UInt glyph_index = FT_Get_Char_Index(face, charcode);
	printf("charcode 0x%04x is glyph %d\n", charcode, glyph_index);

    // Put the outline in the glyph slot.
    error = load_glyph(face, glyph_index);

    auto bitmap = face->glyph->bitmap;
    print(bitmap);
    printf("bitmap is %dx%d pitch %d\n",
        bitmap.width, bitmap.rows, bitmap.pitch);
 
    // Roughly centered.
    int x = SCREEN_WIDTH/2 - bitmap.width/2;
    int y = SCREEN_HEIGHT/2 - bitmap.rows/2;
    int offset = y * SCREEN_WIDTH + x;

	while(pmMainLoop()) {

        // Print the bitmap to the console and background.
        // clear(backBuffer);
        display(face->glyph->bitmap, backBuffer + offset);
		
        swiWaitForVBlank();
		
        scanKeys();
		int keys = keysDown();
		if (keys & KEY_START) break;
        if (keys & KEY_RIGHT) offset++;
        if (keys & KEY_DOWN) offset += SCREEN_WIDTH;
        
        // Swap the back buffer to the current buffer.
		backBuffer = (u16*)bgGetGfxPtr(bg);

        // Swap the current buffer by changing the base.
        bgGetMapBase(bg) == 8 ? bgSetMapBase(bg, 0) : bgSetMapBase(bg, 8);
    }

    FT_Done_FreeType(library);
	return 0;
}
