#ifndef LETHAL_NET_CONFIG_PRIVACY_H
#define LETHAL_NET_CONFIG_PRIVACY_H

#include <stdint.h>

#define PRIVACY_LEVEL_BASIC 0
#define PRIVACY_LEVEL_ADVANCED 1
#define PRIVACY_LEVEL_PARANOID 2

#define SYS_PRIVACY_GET   100
#define SYS_PRIVACY_SET   101
#define SYS_PRIVACY_LIST  102
#define SYS_PRIVACY_RESET 103

struct privacy_settings {
    uint8_t mac_randomization;
    uint8_t no_logging;
    uint8_t tcp_timestamp_random;
    uint8_t icmp_no_reply;
    uint8_t random_source_ports;
    uint8_t arp_spoof_protection;
    uint8_t kill_switch;
    uint8_t tor_integration;
    uint8_t dns_over_https;
    uint8_t packet_padding;
    uint8_t no_telemetry;
    uint8_t dhcp_no_hostname;
    uint8_t no_frag_tracking;
    uint8_t protection_level;
};

struct privacy_state {
    struct privacy_settings settings;
    uint8_t initialized;
    uint8_t config_loaded;
};

extern struct privacy_state privacy_state;

int privacy_init(void);
int privacy_load_config(const char *path);
int privacy_save_config(const char *path);
int privacy_apply_config(void);
int privacy_set_defaults(void);
int privacy_validate_settings(struct privacy_settings *settings);


int privacy_get_settings(struct privacy_settings *settings);
int privacy_set_settings(struct privacy_settings *settings);
int privacy_reset_defaults(void);
int privacy_list_settings(void);

void privacy_apply_mac_randomization(uint8_t enabled);
void privacy_apply_no_logging(uint8_t enabled);
void privacy_apply_tcp_timestamp_random(uint8_t enabled);
void privacy_apply_icmp_no_reply(uint8_t enabled);
void privacy_apply_random_source_ports(uint8_t enabled);
void privacy_apply_arp_spoof_protection(uint8_t enabled);
void privacy_apply_kill_switch(uint8_t enabled);
void privacy_apply_tor_integration(uint8_t enabled);
void privacy_apply_dns_over_https(uint8_t enabled);
void privacy_apply_packet_padding(uint8_t enabled);
void privacy_apply_no_telemetry(uint8_t enabled);
void privacy_apply_dhcp_no_hostname(uint8_t enabled);
void privacy_apply_no_frag_tracking(uint8_t enabled);

void privacy_apply_level_basic(void);
void privacy_apply_level_advanced(void);
void privacy_apply_level_paranoid(void);

int privacy_is_mac_randomization_enabled(void);
int privacy_is_logging_enabled(void);
int privacy_is_kill_switch_enabled(void);
int privacy_is_tor_integration_enabled(void);
int privacy_is_dns_over_https_enabled(void);
int privacy_is_packet_padding_enabled(void);

#endif