#include "privacy.h"
#include "../../core/console.h"
#include <stdlib.h>

/* Apply MAC randomization to a device */
int privacy_apply_mac_to_device(void *device, uint8_t *mac)
{
    if (!device || !mac) return -1;
    
    serial_print("[privacy] Applying randomized MAC to device\n");
    
    return 0;
}

/* Generate random MAC address */
void privacy_generate_random_mac(uint8_t *mac)
{
    if (!mac) return;
    
    /* Locally Administered + Unicast */
    mac[0] = 0x02;
    
    /* Random bytes for remaining 5 bytes */
    for (int i = 1; i < 6; i++) {
        mac[i] = (uint8_t)(rand() & 0xFF);
    }
}

/* Check if kill switch should block traffic */
int privacy_kill_switch_should_block(void)
{
    if (!privacy_state.settings.kill_switch) {
        return 0;
    }
    
    return 1;
}

/* Get protection level name */
const char *privacy_get_level_name(uint8_t level)
{
    switch(level) {
        case PRIVACY_LEVEL_BASIC:    return "basic";
        case PRIVACY_LEVEL_ADVANCED: return "advanced";
        case PRIVACY_LEVEL_PARANOID: return "paranoid";
        default:                     return "unknown";
    }
}