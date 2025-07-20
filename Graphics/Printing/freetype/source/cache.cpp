#include <dirent.h>
#include <fat.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_CACHE_H
#include <nds.h>
#include <stdio.h>

#include "main.h"

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
