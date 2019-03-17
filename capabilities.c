/* Pi-hole: A black hole for Internet advertisements
*  (c) 2017 Pi-hole, LLC (https://pi-hole.net)
*  Network-wide ad blocking via your own hardware.
*
*  FTL Engine
*  Linux capability check routines
*
*  This file is copyright under the latest version of the EUPL.
*  Please see LICENSE file for your rights under this license. */

// Definition of LINUX_CAPABILITY_VERSION_*
#define FTLDNS
#include "dnsmasq/dnsmasq.h"
#undef __USE_XOPEN
#include "FTL.h"

const int capabilityIntegers[] = { CAP_CHOWN ,  CAP_DAC_OVERRIDE ,  CAP_DAC_READ_SEARCH ,  CAP_FOWNER ,  CAP_FSETID ,  CAP_KILL ,  CAP_SETGID ,  CAP_SETUID ,  CAP_SETPCAP ,  CAP_LINUX_IMMUTABLE ,  CAP_NET_BIND_SERVICE ,  CAP_NET_BROADCAST ,  CAP_NET_ADMIN ,  CAP_NET_RAW ,  CAP_IPC_LOCK ,  CAP_IPC_OWNER ,  CAP_SYS_MODULE ,  CAP_SYS_RAWIO ,  CAP_SYS_CHROOT ,  CAP_SYS_PTRACE ,  CAP_SYS_PACCT ,  CAP_SYS_ADMIN ,  CAP_SYS_BOOT ,  CAP_SYS_NICE ,  CAP_SYS_RESOURCE ,  CAP_SYS_TIME ,  CAP_SYS_TTY_CONFIG ,  CAP_MKNOD ,  CAP_LEASE ,  CAP_AUDIT_WRITE ,  CAP_AUDIT_CONTROL ,  CAP_SETFCAP ,  CAP_MAC_OVERRIDE ,  CAP_MAC_ADMIN ,  CAP_SYSLOG ,  CAP_WAKE_ALARM ,  CAP_BLOCK_SUSPEND ,  CAP_AUDIT_READ };
const char* capabilityNames[]  = {"CAP_CHOWN", "CAP_DAC_OVERRIDE", "CAP_DAC_READ_SEARCH", "CAP_FOWNER", "CAP_FSETID", "CAP_KILL", "CAP_SETGID", "CAP_SETUID", "CAP_SETPCAP", "CAP_LINUX_IMMUTABLE", "CAP_NET_BIND_SERVICE", "CAP_NET_BROADCAST", "CAP_NET_ADMIN", "CAP_NET_RAW", "CAP_IPC_LOCK", "CAP_IPC_OWNER", "CAP_SYS_MODULE", "CAP_SYS_RAWIO", "CAP_SYS_CHROOT", "CAP_SYS_PTRACE", "CAP_SYS_PACCT", "CAP_SYS_ADMIN", "CAP_SYS_BOOT", "CAP_SYS_NICE", "CAP_SYS_RESOURCE", "CAP_SYS_TIME", "CAP_SYS_TTY_CONFIG", "CAP_MKNOD", "CAP_LEASE", "CAP_AUDIT_WRITE", "CAP_AUDIT_CONTROL", "CAP_SETFCAP", "CAP_MAC_OVERRIDE", "CAP_MAC_ADMIN", "CAP_SYSLOG", "CAP_WAKE_ALARM", "CAP_BLOCK_SUSPEND", "CAP_AUDIT_READ"};

bool check_capabilities()
{
	int capsize = 1; /* for header version 1 */
	cap_user_header_t hdr = NULL;
	cap_user_data_t data = NULL;

	/* find version supported by kernel */
	hdr = calloc(sizeof(*hdr), capsize);
	memset(hdr, 0, sizeof(*hdr));
	capget(hdr, NULL);

	if (hdr->version != LINUX_CAPABILITY_VERSION_1)
	{
	    /* if unknown version, use largest supported version (3) */
	    if (hdr->version != LINUX_CAPABILITY_VERSION_2)
	     hdr->version = LINUX_CAPABILITY_VERSION_3;
	    capsize = 2;
	}

	data = calloc(sizeof(*data), capsize);
	capget(hdr, data); /* Get current values, for verification */

	if(config.debug & DEBUG_CAPS)
	{
		logg("*********************************************************************");
		for(unsigned int i = 0u; i < (sizeof(capabilityIntegers)/sizeof(const int)); i++)
		{
			unsigned int capid = capabilityIntegers[i];

			// Check if capability is valid for the current kernel
			// If not, exit loop early
			if(!cap_valid(capid))
				break;

			logg("DEBUG: Capability %-24s (%02u) = %s%s%s",
			     capabilityNames[capid],
			     capid,
			     ((data->permitted & (1 << capid)) ? "P":"-"),
			     ((data->inheritable & (1 << capid)) ? "I":"-"),
			     ((data->effective & (1 << capid)) ? "E":"-"));
		}
		logg("*********************************************************************");
	}

	bool capabilities_okay = true;
	if (!(data->permitted & (1 << CAP_NET_ADMIN)))
	{
		// Needed for ARP-injection (used when we're the DHCP server)
		logg("*********************************************************************");
		logg("WARNING: Required linux capability CAP_NET_ADMIN not available");
		logg("*********************************************************************");
		capabilities_okay = false;
	}
	if (!(data->permitted & (1 << CAP_NET_RAW)))
	{
		// Needed for raw socket access (necessary for ICMP)
		logg("*********************************************************************");
		logg("WARNING: Required linux capability CAP_NET_RAW not available");
		logg("*********************************************************************");
		capabilities_okay = false;
	}
	if (!(data->permitted & (1 << CAP_NET_BIND_SERVICE)))
	{
		// Necessary for dynamic port binding
		logg("*********************************************************************");
		logg("WARNING: Required linux capability CAP_NET_BIND_SERVICE not available");
		logg("*********************************************************************");
		capabilities_okay = false;
	}
	if (!(data->permitted & (1 << CAP_SETUID)))
	{
		// Necessary for changing our own user ID ("daemonizing")
		logg("*********************************************************************");
		logg("WARNING: Required linux capability CAP_SETUID not available");
		logg("*********************************************************************");
		capabilities_okay = false;
	}

	// All okay!
	return capabilities_okay;
}
