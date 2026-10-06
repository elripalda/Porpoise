/* Porpoise - the notification relay: a tiny payload that posts one PS5 toast.
 *
 * Porpoise's own process can't load libSceNotification on recent firmware
 * (the system refuses the module), so the trophy-style popups go through the
 * jailbreak's ELF loader: Porpoise (src/porpoise_notify.cpp) writes the
 * toast's JSON over g_payload's marker in a copy of this program and sends it
 * to 127.0.0.1:9021, where the loader runs it as a process of its own.
 *
 * The approach and its findings (logged toasts only, the 3 s wait, _exit) are
 * PS5SX2's (Spyros, ps5/coreorbis/notify-relay/notify_relay.c, GPL-3.0-or-later).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdbool.h>
#include <unistd.h>

int sceNotificationSend(int userId, bool isLogged, const char *payload);

/* Initialized, so the whole array is in the file: the room Porpoise fills.
 * volatile: what Porpoise wrote is read, not the marker seen here. */
__attribute__((used)) volatile char g_payload[16384] = "PORPOISE-NOTIFY-RELAY-JSON-V1";

int main(void)
{
    if (g_payload[0] != '{')
        return 1;
    /* 0xFE: the system user. Only a toast kept in the notification list shows. */
    const int rc = sceNotificationSend(0xFE, true, (const char *)g_payload);
    /* The library posts from a thread of its own: give it time, then end
     * without unloading anything under it. */
    sleep(3);
    _exit(rc == 0 ? 0 : 2);
}
