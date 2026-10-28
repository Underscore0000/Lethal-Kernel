#include "privacy.h"
#include "../../process/syscall.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

int privacy_get_settings(struct privacy_settings *settings)
{
    if (!settings) return -1;
    memcpy(settings, &privacy_state.settings, sizeof(struct privacy_settings));
    return 0;
}

int privacy_set_settings(struct privacy_settings *settings)
{
    if (!settings) return -1;
    if (privacy_validate_settings(settings) != 0) return -1;
    
    memcpy(&privacy_state.settings, settings, sizeof(struct privacy_settings));
    privacy_apply_config();
    privacy_save_config(NULL);
    return 0;
}

int privacy_reset_defaults(void)
{
    privacy_set_defaults();
    privacy_apply_config();
    privacy_save_config(NULL);
    return 0;
}

int privacy_list_settings(void)
{
    struct privacy_settings *s = &privacy_state.settings;
    
    serial_print("\n");
    serial_print("=== Privacy Settings ===\n");
    serial_print("Protection level: ");
    if (s->protection_level == PRIVACY_LEVEL_BASIC) {
        serial_print("basic\n");
    } else if (s->protection_level == PRIVACY_LEVEL_ADVANCED) {
        serial_print("advanced\n");
    } else {
        serial_print("paranoid\n");
    }
    serial_print("\n");
    
    serial_print("MAC Randomization     : ");
    serial_print(s->mac_randomization ? "enabled\n" : "disabled\n");
    serial_print("Network Logging       : ");
    serial_print(s->no_logging ? "disabled\n" : "enabled\n");
    serial_print("TCP Timestamp Random : ");
    serial_print(s->tcp_timestamp_random ? "enabled\n" : "disabled\n");
    serial_print("ICMP No Reply         : ");
    serial_print(s->icmp_no_reply ? "enabled\n" : "disabled\n");
    serial_print("Random Source Ports   : ");
    serial_print(s->random_source_ports ? "enabled\n" : "disabled\n");
    serial_print("ARP Spoof Protection  : ");
    serial_print(s->arp_spoof_protection ? "enabled\n" : "disabled\n");
    serial_print("Kill Switch           : ");
    serial_print(s->kill_switch ? "enabled\n" : "disabled\n");
    serial_print("Tor Integration       : ");
    serial_print(s->tor_integration ? "enabled\n" : "disabled\n");
    serial_print("DNS over HTTPS        : ");
    serial_print(s->dns_over_https ? "enabled\n" : "disabled\n");
    serial_print("Packet Padding        : ");
    serial_print(s->packet_padding ? "enabled\n" : "disabled\n");
    serial_print("Telemetry             : ");
    serial_print(s->no_telemetry ? "disabled\n" : "enabled\n");
    serial_print("DHCP Hostname         : ");
    serial_print(s->dhcp_no_hostname ? "disabled\n" : "enabled\n");
    serial_print("IP Frag Tracking      : ");
    serial_print(s->no_frag_tracking ? "disabled\n" : "enabled\n");
    serial_print("\n");
    
    return 0;
}

int privacy_syscall_handler(uint32_t cmd, void *arg)
{
    switch(cmd) {
        case SYS_PRIVACY_GET: {
            struct privacy_settings *settings = (struct privacy_settings *)arg;
            return privacy_get_settings(settings);
        }
        case SYS_PRIVACY_SET: {
            struct privacy_settings *settings = (struct privacy_settings *)arg;
            return privacy_set_settings(settings);
        }
        case SYS_PRIVACY_LIST:
            return privacy_list_settings();
        case SYS_PRIVACY_RESET:
            return privacy_reset_defaults();
        default:
            return -1;
    }
}