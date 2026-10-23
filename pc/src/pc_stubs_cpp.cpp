/* pc_stubs_cpp.cpp - JSystem vtable stubs the decomp headers declare but never define */
#include "pc_platform.h"

/* JSUOutputStream — method bodies missing from decomp, needed for vtable */
#include "JSystem/JSupport/JSUInputStream.h"
#include "JSystem/JSupport/JSURandomInputStream.h"

JSUOutputStream::~JSUOutputStream() {}

int JSUOutputStream::skip(s32 amount) {
    (void)amount;
    return 0;
}

int JSURandomOutputStream::getAvailable() const {
    return 0;
}

int JSURandomOutputStream::skip(s32 amount) {
    int s = this->seekPos(amount, SEEK_CUR);
    return s;
}

/* JKRHeap::destroy — declared but never compiled in decomp */
#include "JSystem/JKernel/JKRHeap.h"

void JKRHeap::destroy() {}


#include "JSystem/JKernel/JKRThread.h"

JKRTask::Request* JKRTask::searchBlank() {
    return nullptr;
}

/* famicom.cpp still references the GameCube NES core's frame entry (C++
 * linkage). ks_nes_core.cpp is PPC asm and compiled out; fixNES runs instead. */
#include "Famicom/ks_nes_core.h"

void ksNesEmuFrame(ksNesCommonWorkObj* wp, ksNesStateObj* sp, u32 flags) {
    (void)wp; (void)sp; (void)flags;
}
