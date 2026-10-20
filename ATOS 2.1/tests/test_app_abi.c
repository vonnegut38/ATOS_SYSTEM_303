/* Plan/roadmap item 7: guard the frozen native-app ABI 1.0.  Any layout or
   version drift must fail this host test (and the _Static_asserts in
   include/app_abi.h fail the ROM/app build). */
#include <stddef.h>
#include <stdio.h>
#include "app_abi.h"

#define CHECK(c) do { if (!(c)) { printf("APP_ABI FAIL: %s\n", #c); return 1; } } while (0)

int main(void)
{
    CHECK(ATOS_APP_ABI_MAJOR == 1u);
    /* 1.1: HIRES_CLOCK, 1.2: game mode, 1.3: PSG audio opcodes 75..80 */
    CHECK(ATOS_APP_ABI_MINOR == 5u);
    CHECK(ATOS_APP_ABI_VERSION == 0x00010005UL);
    CHECK(ATOS_APP_SYSCALL_ABI_VERSION == 66u);

    CHECK(ATOS_APP_IMAGE_HEADER_SIZE == 32u);
    CHECK(ATOS_APP_IMAGE_VERSION == 1u);
    CHECK(ATOS_APP_POLICY_MAGIC == 0x41505732UL);

    CHECK(sizeof(atos_app_dir_entry_t) == 24u);
    CHECK(offsetof(atos_app_dir_entry_t, size) == 16u);
    CHECK(sizeof(gui_message_t) == 20u);
    CHECK(sizeof(gui_spawn_t) == 136u);
    CHECK(sizeof(gui_draw_t) == 48u);
    CHECK(offsetof(gui_draw_t, kind) == 0u);
    CHECK(offsetof(gui_draw_t, text) == 16u);

    CHECK(ATOS_CALL_GUI_FRAME == 81 && ATOS_CALL_PCM_CONTROL == 82);
    CHECK(ATOS_CALL_PCM_SUBMIT == 83 && ATOS_CALL_PCM_STATE == 84);
    CHECK(ATOS_CALL_WRITE == 1 && ATOS_CALL_OPEN == 2);
    CHECK(ATOS_CALL_COPY == 49 && ATOS_CALL_MKDIR == 50 && ATOS_CALL_DELETE == 51);
    CHECK(ATOS_CALL_MOVE == 54 && ATOS_CALL_TOS_RUN == 65);
    CHECK(ATOS_CALL_ABI_VERSION == 66);
    CHECK(ATOS_CALL_HIRES_CLOCK == 67);
    CHECK(ATOS_CALL_GUI_FRAME == 81);

    printf("APP_ABI: ALL PASS\n");
    return 0;
}
