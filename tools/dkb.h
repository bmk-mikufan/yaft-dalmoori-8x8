#define JUNG_OFFSET 5120
#define JONG_OFFSET 7936

union bitmap_t { //for big endian
    struct {
        uint8_t high;
        uint8_t low;
    };
    uint16_t raw;
};

struct jamo_offset_t {
	uint8_t length;
	uint8_t use_codemap;
	uint16_t *codemap;
	uint16_t start_code;
	uint16_t file_index;
};

const char cho_map1[] = {0,0,0,0,0,0,0,0,1,3,3,3,1,2,4,4,4,2,1,3,0}; //받침 없는 경우
const char cho_map2[] = {5,5,5,5,5,5,5,5,6,7,7,7,6,6,7,7,7,6,6,7,5}; //받침 있는 경우

const char jong_map[] = {0,2,0,2,1,2,1,2,3,0,2,1,3,3,1,2,1,3,3,1,1};

const uint16_t compat_code_map[30] = { 21, 22, 0, 23, 0, 0, 24, 25, 26, 0, 0, 0, 0, 0, 0, 0, 27, 28, 29, 0, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39};

struct jamo_offset_t jamo_offset[5] = {{19, false, NULL, 0x1100, 21}, {21, false, NULL, 0x1161, 161}, {27, false, NULL, 0x11A8, 249}, {30, true, compat_code_map, 0x3131, 0}, {27, false, NULL, 0x314F, 161}};


void get_eng_bitmap(bitmap_width_t *bitmap, FILE *font, uint16_t code)
{
    fseek(font, 16*code, SEEK_SET);
    for(int i=0; i<16; i++)
		bitmap[i] = (uint8_t) fgetc(font);
}
	

void get_bitmap(bitmap_width_t *bitmap, FILE *font, char cho, char jung, char jong)
{
    union bitmap_t bitmap_data;
    char cho_bul=0, jung_bul=0, jong_bul=0;

    if(jong) { //받침 있음
        cho_bul = cho_map2[jung];
        jung_bul = 2;
    }
    else
        cho_bul = cho_map1[jung];

    if(cho != 0 && cho != 15) //초성이 ㄱ,ㅋ 둘 중에 어느것도 아님
        jung_bul++;

    jong_bul = jong_map[jung];

    fseek(font, 32*(20*cho_bul + cho + 1), SEEK_SET);
    for(int i=0; i<16; i++) {
        fread(&bitmap_data.raw, sizeof(uint16_t), 1, font);
        bitmap[i] = bitmap_data.high<<8 | bitmap_data.low;
    }
    fseek(font, JUNG_OFFSET + 32*(22*jung_bul + jung + 1), SEEK_SET);
    for(int i=0; i<16; i++) {
        fread(&bitmap_data.raw, sizeof(uint16_t), 1, font);
        bitmap[i] |= bitmap_data.high<<8 | bitmap_data.low;
    }
    fseek(font, JONG_OFFSET + 32*(28*jong_bul + jong), SEEK_SET);
    for(int i=0; i<16; i++) {
        fread(&bitmap_data.raw, sizeof(uint16_t), 1, font);
        bitmap[i] |= bitmap_data.high<<8 | bitmap_data.low;
    }
}


void get_jamo_bitmap(bitmap_width_t *bitmap, FILE *font, uint16_t code)
{
    union bitmap_t bitmap_data;
    
    fseek(font, 32*code, SEEK_SET);
    for(int i=0; i<16; i++) {
        fread(&bitmap_data.raw, sizeof(uint16_t), 1, font);
		bitmap[i] = bitmap_data.high<<8 | bitmap_data.low;
    }
}


bool load_dkb_eng_glyph(struct glyph_list_t **glist_head, struct glyph_t *default_glyph, char *path)
{
	FILE *fp;
	struct glyph_list_t  *listp;
	struct glyph_t *glyph;

	if ((fp = efopen(path, "r")) == NULL)
		return false;

	for (int i = 0; i < 256; i++) {
		listp = ecalloc(1, sizeof(struct glyph_list_t));
		glyph = ecalloc(1, sizeof(struct glyph_t));
		glyph->width = 8;
		glyph->height = 16;
		glyph->bitmap = (bitmap_width_t *) ecalloc(glyph->height, sizeof(bitmap_width_t));

		get_eng_bitmap(glyph->bitmap, fp, i);

		listp->code = i;
		listp->glyph = glyph;
		listp->next = *glist_head;
		*glist_head = listp;

		if (i == DEFAULT_CHAR)
			*default_glyph = *glyph;
	}

	efclose(fp);
	return true;
}

bool load_dkb_han_glyph(struct glyph_list_t **glist_head, char *path)
{
	FILE *fp;
	struct glyph_list_t  *listp;
	struct glyph_t *glyph;

	if ((fp = efopen(path, "r")) == NULL)
		return false;

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < jamo_offset[i].length; j++) {
			listp = ecalloc(1, sizeof(struct glyph_list_t));
			glyph = ecalloc(1, sizeof(struct glyph_t));
			glyph->width = 16;
			glyph->height = 16;
			glyph->bitmap = (bitmap_width_t *) ecalloc(glyph->height, sizeof(bitmap_width_t));

			if (jamo_offset[i].use_codemap)
				get_jamo_bitmap(glyph->bitmap, fp, jamo_offset[i].codemap[j]);
			else
				get_jamo_bitmap(glyph->bitmap, fp, jamo_offset[i].file_index + j);

			listp->code = jamo_offset[i].start_code + j;
			listp->glyph = glyph;
			listp->next = *glist_head;
			*glist_head = listp;
		}
	}
	for (int i = 0; i < 11172; i++) {
		listp = ecalloc(1, sizeof(struct glyph_list_t));
		glyph = ecalloc(1, sizeof(struct glyph_t));
		glyph->width = 16;
		glyph->height = 16;
		glyph->bitmap = (bitmap_width_t *) ecalloc(glyph->height, sizeof(bitmap_width_t));
		get_bitmap(glyph->bitmap, fp, i/588, (i/28)%21, i%28);
		listp->code = 0xac00+i;
		listp->glyph = glyph;
		listp->next = *glist_head;
		*glist_head = listp;
	}
	efclose(fp);
	return true;
}
