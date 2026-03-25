#include "m_common_data.h"

#include "libultra/libultra.h"
#ifdef TARGET_PC
#include "m_card.h"
#endif

common_data_t common_data;

extern void common_data_reinit(){

    u8 state;

    state = Common_Get(pad_connected);

    bzero(&common_data, sizeof(common_data));
    Common_Set(transition.wipe_type, -1);
    Common_Set(game_started,1);
    Common_Set(last_scene_no, -1);
    Common_Set(demo_profiles[0], mAc_PROFILE_NUM); /* cleared state */
    Common_Set(demo_profiles[1], mAc_PROFILE_NUM); /* cleared state */
    Common_Set(pad_connected, state);

#ifdef TARGET_PC
    /* GC re-reads the memory card here. We re-read the GCI file, same idea.
     * Title demo trashes the in-memory save (player/animal slots), so we
     * need a fresh copy from disk before anything touches it again. */
    if (pc_save_loaded) {
        pc_save_reload();
    } else
#endif
    mFRm_ClearSaveCheckData(Save_GetPointer(save_check));
}

extern void common_data_init(){
    common_data_reinit();
}

extern void common_data_clear(){
    clip_clear();
}
