/* Modern Combat 2 / Gameloft Sandstorm 2 trace and patches. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <psp2/io/fcntl.h>

#include <kubridge.h>
#include <so_util/so_util.h>

#include "utils/logger.h"

extern so_module so_mod;
static so_hook sound_manager_stop_hook;
static unsigned rejected_sound_stops;

/*
 * The second mission can ask SoundManagerVox to stop the sentinel file id
 * -1 while a cinematic sound is being replaced.  The Android timing normally
 * prevents that path, but on Vita the asynchronous decoder makes it reachable.
 * The original ARM routine passes FileManager::_GetName(-1)'s integer sentinel
 * to strncmp as a pointer and aborts.  Reject only that invalid request and let
 * every valid stop continue through the original function.
 */
static int sound_manager_stop_safe(void *self, int file_id, int instance_id) {
    if (file_id < 0) {
        if (rejected_sound_stops++ < 16)
            port_trace("AUDIO: ignored native SoundManagerVox::Stop "
                       "file_id=%d instance_id=%d", file_id, instance_id);
        return 0;
    }
    return SO_CONTINUE(int, sound_manager_stop_hook,
                       self, file_id, instance_id);
}

/*
 * Network services (Gameloft LIVE!, More Games/IGP, Twitter, multiplayer) are
 * dead and have no Vita backend.  Their main-menu buttons are placed by
 * gameswf with sprite_instance::add_display_object; give those instances a
 * colour transform with alpha 0 (opacity 0%) so the menu shows only what works.
 */
static so_hook add_display_object_hook;
static const char *const hidden_online_buttons[] = {
    "btn_gllive", "btn_gl_live", "btn_glLive_myAccount", "btn_moregames",
    "btn_twitter", "btn_online", "btn_multiplayer",
};

typedef void *(*add_display_object_fn)(void *, unsigned short, const uint8_t *,
                                       const void *, int, int, const float *,
                                       const void *, const void *, float,
                                       unsigned short);

/* gameswf::tu_string: byte 0 == 0xFF means heap text pointer at +12,
 * otherwise the text is stored inline from +1 (confirmed in operator=). */
static const char *tu_string_cstr(const uint8_t *s) {
    if (s[0] != 0xFF)
        return (const char *)s + 1;
    uintptr_t text = *(const uintptr_t *)(s + 12);
    return text >= 0x81000000u && text < 0xA0000000u ? (const char *)text : "";
}

static void *add_display_object_hidden_online(
        void *self, unsigned short id, const uint8_t *name, const void *events,
        int depth, int replace, const float *cx, const void *matrix,
        const void *effect, float ratio, unsigned short clip_depth) {
    /* cxform is float[4][2]: {mult, add} for R, G, B, A. */
    float faded[8] = { 1, 0, 1, 0, 1, 0, 1, 0 };
    const char *text = name ? tu_string_cstr(name) : "";
    for (unsigned i = 0; i < sizeof(hidden_online_buttons) /
                             sizeof(hidden_online_buttons[0]); ++i) {
        if (strcmp(text, hidden_online_buttons[i]) == 0) {
            if (cx)
                memcpy(faded, cx, sizeof(faded));
            faded[6] = 0.0f;
            faded[7] = 0.0f;
            cx = faded;
            port_trace("patch: online button %s hidden (alpha 0)", text);
            break;
        }
    }

    /* Same restore/call/re-patch dance as SO_CONTINUE, but through a real
     * prototype: SO_CONTINUE's unprototyped call would promote the float
     * ratio to double and break the softfp argument layout. */
    so_hook *h = &add_display_object_hook;
    kuKernelCpuUnrestrictedMemcpy((void *)h->addr, h->orig_instr,
                                  sizeof(h->orig_instr));
    kuKernelFlushCaches((void *)h->addr, sizeof(h->orig_instr));
    add_display_object_fn original = (add_display_object_fn)(
        h->thumb_addr ? h->thumb_addr : h->addr);
    void *result = original(self, id, name, events, depth, replace, cx,
                            matrix, effect, ratio, clip_depth);
    kuKernelCpuUnrestrictedMemcpy((void *)h->addr, h->patch_instr,
                                  sizeof(h->patch_instr));
    kuKernelFlushCaches((void *)h->addr, sizeof(h->patch_instr));
    return result;
}

static void patch_swp_instructions(so_module *mod) {
    if (!mod || !mod->text_base || !mod->text_size)
        return;

    uint32_t *text = (uint32_t *)mod->text_base;
    size_t count = mod->text_size / sizeof(uint32_t);
    unsigned patched = 0;

    for (size_t i = 0; i < count; ++i) {
        uint32_t insn = text[i];
        /* Match ARM SWP: cond 0001 0000 Rn Rd 0000 1001 Rm */
        if ((insn & 0x0FB00FF0) == 0x01000090) {
            uint32_t cond = (insn >> 28) & 0xF;
            if (cond == 0xF)
                continue; /* Condition 0xF is not an unconditional instruction in ARMv7 */

            uint32_t rn = (insn >> 16) & 0xF;
            uint32_t rd = (insn >> 12) & 0xF;
            uint32_t rm = insn & 0xF;

            uintptr_t swp_addr = (uintptr_t)&text[i];
            uintptr_t ret_addr = swp_addr + 4;
            uintptr_t tramp_addr = mod->patch_head;

            if (tramp_addr + 28 > mod->patch_base + mod->patch_size) {
                port_trace("patch: ERROR out of patch arena space for SWP at %p",
                           (void *)swp_addr);
                break;
            }

            uint32_t trampoline[7];
            trampoline[0] = 0xe52dc004; /* push {r12} */
            trampoline[1] = 0xe1900f9f | (rn << 16) | (rd << 12); /* ldrex rd, [rn] */
            trampoline[2] = 0xe1800f90 | (rn << 16) | (12 << 12) | rm; /* strex r12, rm, [rn] */
            trampoline[3] = 0xe33c0000; /* teq r12, #0 */
            trampoline[4] = 0x1afffffb; /* bne back to ldrex at index 1 */
            trampoline[5] = 0xe49dc004; /* pop {r12} */

            int32_t ret_off = (int32_t)ret_addr - ((int32_t)(tramp_addr + 24) + 8);
            trampoline[6] = 0xea000000 | (((uint32_t)(ret_off >> 2)) & 0x00ffffff); /* b ret_addr */

            int32_t jump_off = (int32_t)tramp_addr - ((int32_t)swp_addr + 8);
            uint32_t branch_to_tramp = (cond << 28) | 0x0a000000 | (((uint32_t)(jump_off >> 2)) & 0x00ffffff);

            kuKernelCpuUnrestrictedMemcpy((void *)tramp_addr, trampoline, sizeof(trampoline));
            mod->patch_head += sizeof(trampoline);

            kuKernelCpuUnrestrictedMemcpy((void *)swp_addr, &branch_to_tramp, sizeof(branch_to_tramp));
            patched++;
        }
    }

    kuKernelFlushCaches((void *)mod->patch_base, mod->patch_size);
    kuKernelFlushCaches((void *)mod->text_base, mod->text_size);

    port_trace("patch: replaced %u deprecated SWP instructions with LDREX/STREX trampolines",
               patched);
}

void so_patch(void) {
    port_trace("patch: MC2 Vita v26 reached so_patch");
    port_trace("libsandstorm2=%p", (void *)so_mod.text_base);

    patch_swp_instructions(&so_mod);

    uintptr_t sound_stop = so_symbol(
        &so_mod, "_ZN15SoundManagerVox4StopEii");
    if (sound_stop) {
        sound_manager_stop_hook = hook_addr(
            sound_stop, (uintptr_t)&sound_manager_stop_safe);
        port_trace("patch: guarded SoundManagerVox::Stop at %p",
                   (void *)sound_stop);
    } else {
        port_trace("patch: WARNING SoundManagerVox::Stop symbol not found");
    }

    uintptr_t add_display_object = so_symbol(&so_mod,
        "_ZN7gameswf15sprite_instance18add_display_objectEtRKNS_9tu_string"
        "ERKNS_5arrayIPNS_9swf_eventEEEibPKNS_6cxformEPKNS_6matrixE"
        "PKNS_6effectEft");
    if (add_display_object) {
        add_display_object_hook = hook_addr(
            add_display_object, (uintptr_t)&add_display_object_hidden_online);
        port_trace("patch: online menu buttons faded at %p",
                   (void *)add_display_object);
    } else {
        port_trace("patch: WARNING gameswf add_display_object not found");
    }
}
