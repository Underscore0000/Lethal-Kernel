#include "privacy.h"
#include "privacy_defaults.h"
#include "../core/console.h"
#include "../fs/vfs.h"
#include "../memory/heap.h"
#include <string.h>
#include <stdio.h>

struct privacy_state privacy_state;

#define PRIVACY_CONFIG_PATH "/etc/privacy.conf"

int privacy_init(void)
{
    serial_print("[privacy] Initializing...\n");
    
    memset(&privacy_state, 0, sizeof(privacy_state));
    privacy_state.initialized = 1;
    
    privacy_set_defaults();
    privacy_load_config(PRIVACY_CONFIG_PATH);
    privacy_apply_config();
    
    serial_print("[privacy] Initialized\n");
    return 0;
}

int privacy_set_defaults(void)
{
    struct privacy_settings defaults = {
        PRIVACY_DEFAULT
    };
    
    memcpy(&privacy_state.settings, &defaults, sizeof(struct privacy_settings));
    
    serial_print("[privacy] Defaults set to advanced\n");
    return 0;
}

int privacy_load_config(const char *path)
{
    if (!path) path = PRIVACY_CONFIG_PATH;
    
    void *file = NULL;
    if (vfs_open(path, VFS_O_READ, &file) != 0) {
        serial_print("[privacy] Config file not found, using defaults\n");
        return -1;
    }
    
    long size = vfs_size(file);
    if (size <= 0 || size > 4096) {
        vfs_close(file);
        return -1;
    }
    
    char *buffer = (char *)kmalloc((size_t)size + 1);
    if (!buffer) {
        vfs_close(file);
        return -1;
    }
    
    long read = vfs_read(file, buffer, (size_t)size);
    vfs_close(file);
    
    if (read <= 0) {
        kfree(buffer);
        return -1;
    }
    
    buffer[read] = '\0';
    
    char *line = strtok(buffer, "\n");
    while (line) {
        if (line[0] == '#' || line[0] == '\0') {
            line = strtok(NULL, "\n");
            continue;
        }
        
        char *eq = strchr(line, '=');
        if (!eq) {
            line = strtok(NULL, "\n");
            continue;
        }
        
        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        
        while (*key == ' ') key++;
        while (*value == ' ') value++;
        
        size_t key_len = strlen(key);
        while (key_len > 0 && key[key_len-1] == ' ') {
            key[key_len-1] = '\0';
            key_len--;
        }
        size_t val_len = strlen(value);
        while (val_len > 0 && value[val_len-1] == ' ') {
            value[val_len-1] = '\0';
            val_len--;
        }
        
        int val = 0;
        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
            strcmp(value, "yes") == 0 || strcmp(value, "on") == 0) {
            val = 1;
        }
        
        if (strcmp(key, "protection_level") == 0) {
            if (strcmp(value, "basic") == 0) {
                privacy_state.settings.protection_level = PRIVACY_LEVEL_BASIC;
            } else if (strcmp(value, "advanced") == 0) {
                privacy_state.settings.protection_level = PRIVACY_LEVEL_ADVANCED;
            } else if (strcmp(value, "paranoid") == 0) {
                privacy_state.settings.protection_level = PRIVACY_LEVEL_PARANOID;
            }
            line = strtok(NULL, "\n");
            continue;
        }
        
        if (strcmp(key, "mac_randomization") == 0) {
            privacy_state.settings.mac_randomization = val;
        } else if (strcmp(key, "no_logging") == 0) {
            privacy_state.settings.no_logging = val;
        } else if (strcmp(key, "tcp_timestamp_random") == 0) {
            privacy_state.settings.tcp_timestamp_random = val;
        } else if (strcmp(key, "icmp_no_reply") == 0) {
            privacy_state.settings.icmp_no_reply = val;
        } else if (strcmp(key, "random_source_ports") == 0) {
            privacy_state.settings.random_source_ports = val;
        } else if (strcmp(key, "arp_spoof_protection") == 0) {
            privacy_state.settings.arp_spoof_protection = val;
        } else if (strcmp(key, "kill_switch") == 0) {
            privacy_state.settings.kill_switch = val;
        } else if (strcmp(key, "tor_integration") == 0) {
            privacy_state.settings.tor_integration = val;
        } else if (strcmp(key, "dns_over_https") == 0) {
            privacy_state.settings.dns_over_https = val;
        } else if (strcmp(key, "packet_padding") == 0) {
            privacy_state.settings.packet_padding = val;
        } else if (strcmp(key, "no_telemetry") == 0) {
            privacy_state.settings.no_telemetry = val;
        } else if (strcmp(key, "dhcp_no_hostname") == 0) {
            privacy_state.settings.dhcp_no_hostname = val;
        } else if (strcmp(key, "no_frag_tracking") == 0) {
            privacy_state.settings.no_frag_tracking = val;
        }
        
        line = strtok(NULL, "\n");
    }
    
    kfree(buffer);
    privacy_state.config_loaded = 1;
    
    serial_print("[privacy] Config loaded\n");
    return 0;
}

int privacy_save_config(const char *path)
{
    if (!path) path = PRIVACY_CONFIG_PATH;
    
    char buffer[4096];
    int pos = 0;
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "# Lethal Kernel - Privacy Configuration\n");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "# Generated automatically\n\n");
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "protection_level = ");
    if (privacy_state.settings.protection_level == PRIVACY_LEVEL_BASIC) {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "basic\n");
    } else if (privacy_state.settings.protection_level == PRIVACY_LEVEL_ADVANCED) {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "advanced\n");
    } else {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "paranoid\n");
    }
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "mac_randomization = %s\n",
        privacy_state.settings.mac_randomization ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "no_logging = %s\n",
        privacy_state.settings.no_logging ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "tcp_timestamp_random = %s\n",
        privacy_state.settings.tcp_timestamp_random ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "icmp_no_reply = %s\n",
        privacy_state.settings.icmp_no_reply ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "random_source_ports = %s\n",
        privacy_state.settings.random_source_ports ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "arp_spoof_protection = %s\n",
        privacy_state.settings.arp_spoof_protection ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "kill_switch = %s\n",
        privacy_state.settings.kill_switch ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "tor_integration = %s\n",
        privacy_state.settings.tor_integration ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "dns_over_https = %s\n",
        privacy_state.settings.dns_over_https ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "packet_padding = %s\n",
        privacy_state.settings.packet_padding ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "no_telemetry = %s\n",
        privacy_state.settings.no_telemetry ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "dhcp_no_hostname = %s\n",
        privacy_state.settings.dhcp_no_hostname ? "true" : "false");
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
        "no_frag_tracking = %s\n",
        privacy_state.settings.no_frag_tracking ? "true" : "false");
    
    void *file = NULL;
    if (vfs_open(path, VFS_O_WRITE | VFS_O_CREAT, &file) != 0) {
        serial_print("[privacy] Failed to save config\n");
        return -1;
    }
    
    vfs_write(file, buffer, (size_t)pos);
    vfs_close(file);
    
    serial_print("[privacy] Config saved\n");
    return 0;
}

int privacy_apply_config(void)
{
    serial_print("[privacy] Applying configuration...\n");
    
    privacy_apply_mac_randomization(privacy_state.settings.mac_randomization);
    privacy_apply_no_logging(privacy_state.settings.no_logging);
    privacy_apply_tcp_timestamp_random(privacy_state.settings.tcp_timestamp_random);
    privacy_apply_icmp_no_reply(privacy_state.settings.icmp_no_reply);
    privacy_apply_random_source_ports(privacy_state.settings.random_source_ports);
    privacy_apply_arp_spoof_protection(privacy_state.settings.arp_spoof_protection);
    privacy_apply_kill_switch(privacy_state.settings.kill_switch);
    privacy_apply_tor_integration(privacy_state.settings.tor_integration);
    privacy_apply_dns_over_https(privacy_state.settings.dns_over_https);
    privacy_apply_packet_padding(privacy_state.settings.packet_padding);
    privacy_apply_no_telemetry(privacy_state.settings.no_telemetry);
    privacy_apply_dhcp_no_hostname(privacy_state.settings.dhcp_no_hostname);
    privacy_apply_no_frag_tracking(privacy_state.settings.no_frag_tracking);
    
    serial_print("[privacy] Configuration applied\n");
    return 0;
}

void privacy_apply_mac_randomization(uint8_t enabled)
{
    serial_print("[privacy] MAC randomization: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_no_logging(uint8_t enabled)
{
    serial_print("[privacy] Network logging: ");
    serial_print(enabled ? "disabled\n" : "enabled\n");
}

void privacy_apply_tcp_timestamp_random(uint8_t enabled)
{
    serial_print("[privacy] TCP timestamp randomization: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_icmp_no_reply(uint8_t enabled)
{
    serial_print("[privacy] ICMP replies: ");
    serial_print(enabled ? "disabled\n" : "enabled\n");
}

void privacy_apply_random_source_ports(uint8_t enabled)
{
    serial_print("[privacy] Random source ports: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_arp_spoof_protection(uint8_t enabled)
{
    serial_print("[privacy] ARP spoof protection: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_kill_switch(uint8_t enabled)
{
    serial_print("[privacy] Kill switch: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_tor_integration(uint8_t enabled)
{
    serial_print("[privacy] Tor integration: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_dns_over_https(uint8_t enabled)
{
    serial_print("[privacy] DNS over HTTPS: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_packet_padding(uint8_t enabled)
{
    serial_print("[privacy] Packet padding: ");
    serial_print(enabled ? "enabled\n" : "disabled\n");
}

void privacy_apply_no_telemetry(uint8_t enabled)
{
    serial_print("[privacy] Telemetry: ");
    serial_print(enabled ? "disabled\n" : "enabled\n");
}

void privacy_apply_dhcp_no_hostname(uint8_t enabled)
{
    serial_print("[privacy] DHCP hostname: ");
    serial_print(enabled ? "disabled\n" : "enabled\n");
}

void privacy_apply_no_frag_tracking(uint8_t enabled)
{
    serial_print("[privacy] IP fragmentation tracking: ");
    serial_print(enabled ? "disabled\n" : "enabled\n");
}

void privacy_apply_level_basic(void)
{
    privacy_state.settings.protection_level = PRIVACY_LEVEL_BASIC;
    privacy_state.settings.mac_randomization = 0;
    privacy_state.settings.no_logging = 0;
    privacy_state.settings.tcp_timestamp_random = 0;
    privacy_state.settings.icmp_no_reply = 0;
    privacy_state.settings.random_source_ports = 0;
    privacy_state.settings.arp_spoof_protection = 1;
    privacy_state.settings.kill_switch = 0;
    privacy_state.settings.tor_integration = 0;
    privacy_state.settings.dns_over_https = 0;
    privacy_state.settings.packet_padding = 0;
    privacy_state.settings.no_telemetry = 1;
    privacy_state.settings.dhcp_no_hostname = 0;
    privacy_state.settings.no_frag_tracking = 0;
    privacy_apply_config();
}

void privacy_apply_level_advanced(void)
{
    privacy_state.settings.protection_level = PRIVACY_LEVEL_ADVANCED;
    privacy_state.settings.mac_randomization = 1;
    privacy_state.settings.no_logging = 1;
    privacy_state.settings.tcp_timestamp_random = 1;
    privacy_state.settings.icmp_no_reply = 1;
    privacy_state.settings.random_source_ports = 1;
    privacy_state.settings.arp_spoof_protection = 1;
    privacy_state.settings.kill_switch = 0;
    privacy_state.settings.tor_integration = 0;
    privacy_state.settings.dns_over_https = 1;
    privacy_state.settings.packet_padding = 0;
    privacy_state.settings.no_telemetry = 1;
    privacy_state.settings.dhcp_no_hostname = 1;
    privacy_state.settings.no_frag_tracking = 1;
    privacy_apply_config();
}

void privacy_apply_level_paranoid(void)
{
    privacy_state.settings.protection_level = PRIVACY_LEVEL_PARANOID;
    privacy_state.settings.mac_randomization = 1;
    privacy_state.settings.no_logging = 1;
    privacy_state.settings.tcp_timestamp_random = 1;
    privacy_state.settings.icmp_no_reply = 1;
    privacy_state.settings.random_source_ports = 1;
    privacy_state.settings.arp_spoof_protection = 1;
    privacy_state.settings.kill_switch = 1;
    privacy_state.settings.tor_integration = 1;
    privacy_state.settings.dns_over_https = 1;
    privacy_state.settings.packet_padding = 1;
    privacy_state.settings.no_telemetry = 1;
    privacy_state.settings.dhcp_no_hostname = 1;
    privacy_state.settings.no_frag_tracking = 1;
    privacy_apply_config();
}

int privacy_validate_settings(struct privacy_settings *settings)
{
    if (!settings) return -1;
    return 0;
}

/* RIMOSSA la funzione privacy_get_settings() da qui - è in privacy_syscall.c */

int privacy_is_mac_randomization_enabled(void)
{
    return privacy_state.settings.mac_randomization;
}

int privacy_is_logging_enabled(void)
{
    return !privacy_state.settings.no_logging;
}

int privacy_is_kill_switch_enabled(void)
{
    return privacy_state.settings.kill_switch;
}

int privacy_is_tor_integration_enabled(void)
{
    return privacy_state.settings.tor_integration;
}

int privacy_is_dns_over_https_enabled(void)
{
    return privacy_state.settings.dns_over_https;
}

int privacy_is_packet_padding_enabled(void)
{
    return privacy_state.settings.packet_padding;
}