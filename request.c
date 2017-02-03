/* Pi-hole: A black hole for Internet advertisements
*  (c) 2017 Pi-hole, LLC (https://pi-hole.net)
*  Network-wide ad blocking via your own hardware.
*
*  FTL Engine
*  Socket request handling routines
*
*  This file is copyright under the latest version of the EUPL.
*  Please see LICENSE file for your rights under this license. */

#include "FTL.h"

// Private
// int cmpdomains(const void *a, const void *b);
int cmpdomains(int *elem1, int *elem2);
#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

void process_request(void)
{
	char EOT[2];
	EOT[0] = 0x04;
	EOT[1] = 0x00;
	if(command(">stats"))
	{
		float percentage = 0.0;
		// Avoid 1/0 condition
		if(counters.queries > 0)
		{
			percentage = 1e2*counters.blocked/counters.queries;
		}
		sprintf(socketsendbuffer,"domains_being_blocked %i\ndns_queries_today %i\nads_blocked_today %i\nads_percentage_today %f\n",counters.gravity,counters.queries,counters.blocked,percentage);
		swrite();
#if defined(DEBUG)
		logg("Sent stats data to client");
#endif
	}
	else if(command(">overTime"))
	{
		int i;
		bool sendit = false;
		for(i=0; i < counters.overtime; i++)
		{
			if((overTime[i].total > 0 || overTime[i].blocked > 0) && !sendit)
			{
				sendit = true;
			}
			if(sendit)
			{
				sprintf(socketsendbuffer,"%i %i %i\n",i,overTime[i].total,overTime[i].blocked);
				swrite();
			}
		}
#if defined(DEBUG)
		logg("Sent overTime data to client");
#endif
	}
	else if(command(">top-domains") || command(">top-ads"))
	{
		int i, temparray[counters.domains][2], count=10, num;
		bool blocked = command(">top-ads");

		// Match both top-domains and top-ads
		if(sscanf(socketrecvbuffer, ">top-%*[^ ] %i", &num) > 0)
		{
			// User wants a different number of requests
			count = num;
		}

		for(i=0; i < counters.domains; i++)
		{
			temparray[i][0] = i;
			if(blocked)
				temparray[i][1] = domains[i].blockedcount;
			else
				// Count only permitted queries
				temparray[i][1] = (domains[i].count - domains[i].blockedcount);
		}

		// Sort temporary array
		qsort(temparray, counters.domains, sizeof(int[2]), (__compar_fn_t)cmpdomains);

		// Get filter
		char * filter = read_setupVarsconf("API_QUERY_LOG_SHOW");
		bool showpermitted = true, showblocked = true;
		if(filter != NULL)
		{
			if((strcmp(filter, "permittedonly")) == 0)
			{
				showblocked = false;
			}
			else if((strcmp(filter, "blockedonly")) == 0)
			{
				showpermitted = false;
			}
			else if((strcmp(filter, "nothing")) == 0)
			{
				showpermitted = false;
				showblocked = false;
			}
		}
		clearSetupVarsArray();

		// Get domains which the user doesn't want to see
		char * excludedomains = read_setupVarsconf("API_EXCLUDE_DOMAINS");
		if(excludedomains != NULL)
			getSetupVarsArray(excludedomains);


		int j, skip = 0;
		for(i=0; i < min(counters.domains, count+skip); i++)
		{
			// Get sorted indices
			j = temparray[counters.domains-i-1][0];

			// Skip this domain if there is a filter on it
			if(excludedomains != NULL)
			{
				if(insetupVarsArray(domains[j].domain))
				{
					skip++;
					continue;
				}
			}

			if(blocked && showblocked && domains[j].blockedcount > 0)
			{
				sprintf(socketsendbuffer,"%i %i %s\n",i,domains[j].blockedcount,domains[j].domain);
				swrite();
			}
			else if(!blocked && showpermitted && (domains[j].count - domains[j].blockedcount) > 0)
			{
				sprintf(socketsendbuffer,"%i %i %s\n",i,(domains[j].count - domains[j].blockedcount),domains[j].domain);
				swrite();
			}
		}
		if(excludedomains != NULL)
			clearSetupVarsArray();
#if defined(DEBUG)
		logg("Sent top lists data to client");
#endif
	}
	else if(command(">top-clients"))
	{
		int i, temparray[counters.clients][2], count=10, num;

		if(sscanf(socketrecvbuffer, ">top-clients %i", &num) > 0)
		{
			// User wants a different number of requests
			count = num;
		}

		for(i=0; i < counters.clients; i++)
		{
			temparray[i][0] = i;
			temparray[i][1] = clients[i].count;
		}

		// Sort temporary array
		qsort(temparray, counters.clients, sizeof(int[2]), (__compar_fn_t)cmpdomains);

		// Get domains which the user doesn't want to see
		char * excludeclients = read_setupVarsconf("API_EXCLUDE_CLIENTS");
		if(excludeclients != NULL)
			getSetupVarsArray(excludeclients);

		int j, skip = 0;
		for(i=0; i < min(counters.clients, count+skip); i++)
		{
			// Get sorted indices
			j = temparray[counters.clients-i-1][0];

			// Skip this client if there is a filter on it
			if(excludeclients != NULL)
			{
				if(insetupVarsArray(clients[j].ip) ||
				   insetupVarsArray(clients[j].name))
				{
					skip++;
					continue;
				}
			}

			sprintf(socketsendbuffer,"%i %i %s %s\n",i,clients[j].count,clients[j].ip,clients[j].name);
			swrite();
		}
		if(excludeclients != NULL)
			clearSetupVarsArray();
#if defined(DEBUG)
		logg("Sent top clients data to client");
#endif
	}
	else if(command(">forward-dest"))
	{
		int i, temparray[counters.forwarded][2];
		for(i=0; i < counters.forwarded; i++)
		{
			temparray[i][0] = i;
			temparray[i][1] = forwarded[i].count;
		}

		// Sort temporary array
		qsort(temparray, counters.forwarded, sizeof(int[2]), (__compar_fn_t)cmpdomains);

		int j;
		for(i=0; i < min(counters.forwarded, 10); i++)
		{
			// Get sorted indices
			j = temparray[counters.forwarded-i-1][0];
			sprintf(socketsendbuffer,"%i %i %s %s\n",i,forwarded[j].count,forwarded[j].ip,forwarded[j].name);
			swrite();
		}
#if defined(DEBUG)
		logg("Sent forwarded destinations data to client");
#endif
	}
	else if(command(">querytypes"))
	{
		sprintf(socketsendbuffer,"A (IPv4): %i\nAAAA (IPv6): %i\nPTR: %i\nSRV: %i\n",counters.IPv4,counters.IPv6,counters.PTR,counters.SRV);
		swrite();
#if defined(DEBUG)
		logg("Sent query type data to client");
#endif
	}
	else if(command(">getallqueries"))
	{
		// Do we want a more specific version of this command (domain/client/time interval filtered)?
		int from = 0, until = 0;
		if(command(">getallqueries-time"))
		{
			// Get from to until boundaries
			sscanf(socketrecvbuffer, ">getallqueries-time %i %i",&from, &until);
#if defined(DEBUG)
			logg_int("Showing only limited time interval starting at ",from);
#endif
		}

		char *domainname;
		bool filterdomainname = false;
		if(command(">getallqueries-domain"))
		{
			domainname = calloc(128, sizeof(char));
			// Get domain name we want to see only (limit length to 127 chars)
			sscanf(socketrecvbuffer, ">getallqueries-domain %127s", domainname);
#if defined(DEBUG)
			logg_str("Showing only queries with domain ", domainname);
			filterdomainname = true;
#endif
		}

		char *clientname;
		bool filterclientname = false;
		if(command(">getallqueries-client"))
		{
			clientname = calloc(128, sizeof(char));
			// Get client name we want to see only (limit length to 127 chars)
			sscanf(socketrecvbuffer, ">getallqueries-client %127s", clientname);
#if defined(DEBUG)
			logg_str("Showing only queries with client ", clientname);
			filterclientname = true;
#endif
		}

		// Get potentially existing filtering flags
		char * filter = read_setupVarsconf("API_QUERY_LOG_SHOW");
		bool showpermitted = true, showblocked = true;
		if(filter != NULL)
		{
			if((strcmp(filter, "permittedonly")) == 0)
			{
				showblocked = false;
			}
			else if((strcmp(filter, "blockedonly")) == 0)
			{
				showpermitted = false;
			}
			else if((strcmp(filter, "nothing")) == 0)
			{
				showpermitted = false;
				showblocked = false;
			}
		}
		clearSetupVarsArray();

		// Get privacy mode flag
		char * privacy = read_setupVarsconf("API_PRIVACY_MODE");
		bool privacymode = false;
		if(privacy != NULL)
			if(getSetupVarsBool(privacy))
				privacymode = true;
		clearSetupVarsArray();

#if defined(DEBUG)
		if(showpermitted)
			logg("Showing permitted queries");
		else
			logg("Hiding permitted queries");
		if(showblocked)
			logg("Showing blocked queries");
		else
			logg("Hiding blocked queries");
		if(privacymode)
			logg("Privacy mode enabled");
#endif

		int i;
		for(i=0; i < counters.queries; i++)
		{
			char type[5];
			if(queries[i].type == 1)
			{
				strcpy(type,"IPv4");
			}
			else
			{
				strcpy(type,"IPv6");
			}

			if((queries[i].status == 1 || queries[i].status == 4) && !showblocked)
				continue;
			if((queries[i].status == 2 || queries[i].status == 3) && !showpermitted)
				continue;

			if(from != 0 && until != 0)
			{
				// Skip those entries which so not meet the requested timeframe
				if(from > queries[i].timestamp || queries[i].timestamp > until)
					continue;
			}

			if(filterdomainname)
			{
				// Skip if domain name is not identical with what the user wants to see
				if(strcmp(domains[queries[i].domainID].domain, domainname) != 0)
					continue;
			}

			if(filterclientname)
			{
				// Skip if client name and IP are not identical with what the user wants to see
				if((strcmp(clients[queries[i].clientID].ip, clientname) != 0) &&
				   (strcmp(clients[queries[i].clientID].name, clientname) != 0))
					continue;
			}

			if(!privacymode)
				sprintf(socketsendbuffer,"%i %s %s %s %i\n",queries[i].timestamp,type,domains[queries[i].domainID].domain,clients[queries[i].clientID].ip,queries[i].status);
			else
				sprintf(socketsendbuffer,"%i %s %s hidden %i\n",queries[i].timestamp,type,domains[queries[i].domainID].domain,queries[i].status);
			swrite();
		}

		// Free allocated memory
		if(filterclientname)
			free(clientname);
		if(filterdomainname)
			free(domainname);

#if defined(DEBUG)
		logg("Sent all queries data to client");
#endif
	}
	else if(command(">recentBlocked"))
	{
		int i;
		// Find most recent query with either status 1 (blocked)
		// or status 4 (wildcard blocked)
		for(i = counters.queries - 1; i > 0 ; i--)
		{
			if(queries[i].status == 1 || queries[i].status == 4)
			{
				break;
			}
			printf("%i %i\n", i, queries[i].status);
		}
		sprintf(socketsendbuffer,"%s\n",domains[queries[i].domainID].domain);
		swrite();
	}
	// Test only at the end if we want to quit or kill
	// so things can be processed before
	else if(command(">quit") || command(EOT))
	{
		close(clientsocket);
		clientsocket = 0;
#if defined(DEBUG)
		logg("Clients wants to quit");
#endif
	}
	else if(command(">kill"))
	{
		killed = 1;
	}
	// Send EOM
	if(clientsocket)
		seom();
}

bool command(const char* cmd)
{
	if(strstr(socketrecvbuffer,cmd) != NULL)
		return true;
	else
		return false;
}

void formatNumber(bool raw, int n, char* buffer)
{
	if(raw)
	{
		// Don't change number, echo string
		sprintf(buffer, "%d", n);
	}
	else
	{
		// Insert thousand separator
		if(n < 0) {
			sprintf(buffer, "-");
			n = -n;
		}
		else
		{
			// Empty buffer
			buffer[0] = '\0';
		}

		int a[20] = { 0 };
		int *pa = a;
		while(n > 0) {
			*++pa = n % 1000;
			n /= 1000;
		}
		sprintf(buffer, "%s%d", buffer, *pa);
		while(pa > a + 1) {
			sprintf(buffer, "%s,%03d", buffer, *--pa);
		}
	}
}

/* qsort comparision function (count field) */
int cmpdomains(int *elem1, int *elem2)
{
	if (elem1[1] < elem2[1])
		return -1;
	else if (elem1[1] > elem2[1])
		return 1;
	else
		return 0;
}