#ifndef LETHAL_NET_CONFIG_PRIVACY_DEFAULTS_H
#define LETHAL_NET_CONFIG_PRIVACY_DEFAULTS_H

#include "privacy.h"

#define PRIVACY_DEFAULT_BASIC \
    .mac_randomization = 0, \
    .no_logging = 0, \
    .tcp_timestamp_random = 0, \
    .icmp_no_reply = 0, \
    .random_source_ports = 0, \
    .arp_spoof_protection = 1, \
    .kill_switch = 0, \
    .tor_integration = 0, \
    .dns_over_https = 0, \
    .packet_padding = 0, \
    .no_telemetry = 1, \
    .dhcp_no_hostname = 0, \
    .no_frag_tracking = 0, \
    .protection_level = PRIVACY_LEVEL_BASIC

#define PRIVACY_DEFAULT_ADVANCED \
    .mac_randomization = 1, \
    .no_logging = 1, \
    .tcp_timestamp_random = 1, \
    .icmp_no_reply = 1, \
    .random_source_ports = 1, \
    .arp_spoof_protection = 1, \
    .kill_switch = 0, \
    .tor_integration = 0, \
    .dns_over_https = 1, \
    .packet_padding = 0, \
    .no_telemetry = 1, \
    .dhcp_no_hostname = 1, \
    .no_frag_tracking = 1, \
    .protection_level = PRIVACY_LEVEL_ADVANCED

#define PRIVACY_DEFAULT_PARANOID \
    .mac_randomization = 1, \
    .no_logging = 1, \
    .tcp_timestamp_random = 1, \
    .icmp_no_reply = 1, \
    .random_source_ports = 1, \
    .arp_spoof_protection = 1, \
    .kill_switch = 1, \
    .tor_integration = 1, \
    .dns_over_https = 1, \
    .packet_padding = 1, \
    .no_telemetry = 1, \
    .dhcp_no_hostname = 1, \
    .no_frag_tracking = 1, \
    .protection_level = PRIVACY_LEVEL_PARANOID

#define PRIVACY_DEFAULT PRIVACY_DEFAULT_ADVANCED

#define PRIVACY_LEVEL_STR_BASIC     "basic"
#define PRIVACY_LEVEL_STR_ADVANCED  "advanced"
#define PRIVACY_LEVEL_STR_PARANOID  "paranoid"

#endif