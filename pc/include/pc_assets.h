/* pc_assets.h - Runtime binary asset loader (auto-generated) */
#ifndef PC_ASSETS_H
#define PC_ASSETS_H

void pc_load_asset(const char* bin_path, void* dest, unsigned int size,
                   unsigned int rom_off, int rom_src, int swap_type);
void pc_bswap_asset_u16(void* data, unsigned int size);
void pc_bswap_asset_u32(void* data, unsigned int size);
void pc_bswap_asset_vtx(void* data, unsigned int size);
void pc_assets_pal_n64_to_gc(unsigned short* pal, int count);
void pc_assets_init(void);

#endif /* PC_ASSETS_H */
