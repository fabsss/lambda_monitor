#ifndef DNS_HIJACK_H
#define DNS_HIJACK_H

/**
 * @brief Start a minimal DNS server that answers every A-record query
 *        with the SoftAP's own IP address (192.168.4.1).
 *
 * Without this, a client's OS can't resolve real internet hostnames
 * (including its own connectivity-check probe's hostname) through this
 * AP at all - the DNS query just times out, since this AP has no real
 * DNS relay and no internet uplink. That silent DNS failure - not this
 * device's HTTP responses - is what actually determines whether the OS's
 * connectivity check (and therefore its captive-portal / no-internet
 * handling) ever reaches this device in the first place: if the check's
 * hostname can't resolve, the probe never gets far enough to hit
 * web_server.c's 302 redirect. Answering unconditionally with our own
 * IP - the same "DNS hijack" trick used by WLED, ESP-IDF's own
 * captive_portal example, and virtually every AP-only IoT device - makes
 * every hostname resolve locally, so the probe actually reaches this
 * device instead of failing silently at the DNS layer.
 *
 * Adapted from ESP-IDF's official captive_portal example
 * (examples/protocols/http_server/captive_portal/components/dns_server),
 * trimmed to lambda_monitor's single-target-IP, no-rule-table case.
 */
void dns_hijack_start(void);

#endif
