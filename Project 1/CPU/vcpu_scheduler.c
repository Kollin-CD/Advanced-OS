/*
 ============================================================================
 Name        : vcpu_scheduler.c
 Author      : Everardo Villasenor
 Version     : 1.0
 Description : vcpu scheduler
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

// Nanosecond/sec * 100 (to get usage as a %)
static const unsigned long long mySecond = 10000000;

static double USAGE_THRESHOLD = 56.5;

struct domain_stats{
	virDomainPtr domain;
	unsigned long long vcpus_time;
	int pcpu;
};

// Assume Domains used does not change
virDomainPtr *nameList = NULL;
unsigned long long *pcpus_time = NULL; // Need an array to store the pcpu time
double *vcpu_usage = NULL, *pcpu_usage = NULL;
size_t cpumaplen;
int numNames, numActiveDomains;


int vcpuPin(int nhostcpus, struct domain_stats *dom_stats, double period, 
	unsigned long long *vcpuTotalUsage) {

	int mostUsedCpu, leastUsedCpu;
	double mostUsedCpu_usage = 0.0, mostUsedpCPU_usage = 0.0;
	double leastUsedCpu_usage = 100.0, leastUsedpCPU_usage = 100.0;
	double use = 0.0;
	double least_usage = 100.0;
	virDomainPtr leastUsedDomain; 
	unsigned char cpumap;


	for(int i=0; i < nhostcpus; i++) {
		if(vcpuTotalUsage[i] > mostUsedCpu_usage) {
			mostUsedCpu_usage = vcpuTotalUsage[i];
			mostUsedpCPU_usage = pcpu_usage[i];
			mostUsedCpu = i;
		}
		if (vcpuTotalUsage[i] < leastUsedCpu_usage) {
			leastUsedCpu_usage = vcpuTotalUsage[i]; 
			leastUsedpCPU_usage = pcpu_usage[i];
			leastUsedCpu = i;
		}
	}

	printf("Most Used pCPU: %d\n", mostUsedCpu);
	printf("Least Used pCPU: %d\n", leastUsedCpu);

	cpumap = 0x1 << leastUsedCpu;
	// printf("CPUMAP: %d\n", cpumap);
	
	// Steady State:

	if (((mostUsedCpu_usage - leastUsedCpu_usage) < USAGE_THRESHOLD) ||
	((mostUsedpCPU_usage - leastUsedpCPU_usage) < (28.0 / (double) nhostcpus))) {
		printf("Steady State Achieved!\n");
		return 0;
	}

	// Find the best PCPU to pin each VCPU
	for(int i=0; i < numNames; i++) {
		// printf("Domain: %s\n", virDomainGetName(nameList[i]));

		if (mostUsedCpu == dom_stats[i].pcpu) {
			use = vcpu_usage[i];
			if (use < least_usage) {
				least_usage = use;
				leastUsedDomain = nameList[i];
			}
		}
	}

	printf("Pinning vCPU in domain %s to cpu %d\n", virDomainGetName(leastUsedDomain), 
		leastUsedCpu);

	if (virDomainPinVcpu(leastUsedDomain, 0, &cpumap, cpumaplen) != 0) {
		return 1;
	}

	return 0;
}

int getDomainStats(virConnectPtr conn, int nhostcpus, struct domain_stats *dom_stats) {
	int ncpus = 0, n_vcpus = 0;

	virVcpuInfoPtr vcpuinfo;
	// virDomainInfo domainInfo;
	unsigned char *cpumaps;
	virNodeCPUStatsPtr params = NULL;

	for (int i=0; i < numNames; i++) {
		// Number of vCPU used by the domain (should be just 1)
		// if(virDomainGetInfo(nameList[i], &domainInfo) != 0) {
		// 	printf("Failed to get Domain Info: %s\n ", virGetLastErrorMessage());
		// 	return 1;
		// }
		n_vcpus = 1;
		vcpuinfo = calloc(n_vcpus, sizeof(virVcpuInfo));
		// printf("n_vcpus: %d\n", domainInfo.nrVirtCpu);

		// pCPU to vCPU should be 1
		cpumaplen = VIR_CPU_MAPLEN(nhostcpus);
		// printf("cpumaplen: %lu\n", cpumaplen);
		cpumaps = calloc(n_vcpus, cpumaplen);
		if((ncpus = virDomainGetVcpus(nameList[i], vcpuinfo, n_vcpus, cpumaps, cpumaplen))
			< 0){
			printf("Failed to get vCPU info: %s\n", virGetLastErrorMessage());
			return 1;
		}

		// printf("ncpus: %d\n", ncpus);
		// printf("Domain: %s\n", virDomainGetName(nameList[i]));
		// printf("Virtual CPU Number: %d\n", vcpuinfo->number);
		// printf("Virtual State: %d\n", vcpuinfo->state);
		// printf("Virtual CPU Time: %llu\n", vcpuinfo->cpuTime);
		// printf("CPU Number: %d\n", vcpuinfo->cpu);

		// Store it in my structure
		dom_stats[i].domain = nameList[i];
		dom_stats[i].vcpus_time = vcpuinfo->cpuTime;
		dom_stats[i].pcpu = vcpuinfo->cpu;
	}

	for (int i=0; i < nhostcpus; i++) {
		int nparams = 4;
		int cpuNum = i;
		// printf("Cpunum %d\n", cpuNum);
		params = calloc(nparams, sizeof(virNodeCPUStats));
		virNodeGetCPUStats(conn, cpuNum, params, &nparams, 0);
		pcpus_time[cpuNum] = params->value;
		// printf("pCPU Time: %llu (ns), CPU%d\n", pcpus_time[i], i);
	}

	free(vcpuinfo);
	free(cpumaps);
	free(params);

	return 0;
}

int showDomains(virConnectPtr conn){

	int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_RUNNING;

	numActiveDomains = virConnectNumOfDomains(conn);
	if (numActiveDomains == -1) {
		printf("Failed to get number of active domains: %s\n",
			virGetLastErrorMessage());
		return 1;
	}

	// printf("There are %d active domains\n", numActiveDomains);

	numNames = virConnectListAllDomains(conn, &nameList, flags);

	// printf("Number of Domains: %d\n", numNames);

	if (numNames == -1){
		printf("Failed to get a list of all domains: %s\n", virGetLastErrorMessage());
		return 1;
	}

	return 0;
}

int main(int argc, char **argv) {
	// Handle the input
	if(argc != 2) {
		printf("ERROR: Argument is time interval in seconds.");
		return 1;
	}

	double period = atof(argv[1]);
	// printf("Take action every %lf (s)\n", period);

	// Handle the rest
	int nhostcpus, vcpus, n_domains;
	virConnectPtr conn;
	virNodeInfo nodeinfo;
	unsigned long long *prev_vcpuTime, *curr_vcpuTime, *prev_pcpuTime, *curr_pcpuTime, 
	*vcpuTotalUsage;
	int *prev_pcpu, *curr_pcpu;
	struct domain_stats *curr_dom_stats;
	// unsigned char *cpumaps;

	// Connect to the Hypervisor
	conn = virConnectOpen("qemu:///system");

	if (conn == NULL) {
		printf("Failed to open hypervisor connection: %s\n", virGetLastErrorMessage());
		return 1;
	}

	// Obtain the maximum number of virtual CPUs per-guest
	vcpus = virConnectGetMaxVcpus(conn, NULL);
	if (vcpus < 0) {
		printf("Failed to get max vcpus: %s\n", virGetLastErrorMessage());
		return 1;
	}
	// printf("Maximum support virtual CPUs: %d\n", vcpus);
	
	if(virNodeGetInfo(conn, &nodeinfo) < 0) {
		printf("virNodeGetInfo failed: %s\n", virGetLastErrorMessage());
		return 1;
	}
	nhostcpus = nodeinfo.cpus;
	cpumaplen = VIR_CPU_MAPLEN(nhostcpus);

	// printf("Number of Host CPUs: %u\n", nhostcpus);
	// printf("Map Length %ld\n", cpumaplen);

	pcpus_time = malloc(nhostcpus);
	// unsigned char map = 1;
	// int temp = 1;

	// Gather initial data
	if (showDomains(conn) != 0) {
		virConnectClose(conn);
		return 1;
	}

	USAGE_THRESHOLD = (USAGE_THRESHOLD * (double) nhostcpus)/numNames;
	// printf("Thresh: %lf\n", USAGE_THRESHOLD);

	// Gather initial statistics
	curr_dom_stats = calloc(numActiveDomains, sizeof(struct domain_stats));
	getDomainStats(conn, nhostcpus, curr_dom_stats);

	// Initially pin the vCPU to a pCPU

	// Sleep to sample
	sleep(period);

	prev_vcpuTime = malloc(numActiveDomains);
	prev_pcpuTime = malloc(nhostcpus);
	prev_pcpu = malloc(numActiveDomains);

	vcpu_usage = malloc(numActiveDomains);
	pcpu_usage = malloc(nhostcpus);
	curr_vcpuTime = malloc(numActiveDomains);
	vcpuTotalUsage = malloc(nhostcpus);
	curr_pcpuTime = malloc(nhostcpus);
	curr_pcpu = malloc(numActiveDomains);

	while(numActiveDomains > 0) {
		n_domains = numActiveDomains;
		if (n_domains != numActiveDomains) {
			printf("Number of domains changed!\n");
		}

		if (showDomains(conn) != 0) {
		virConnectClose(conn);
		return 1;
		}

		// Get the previous statistics
		for (int j=0; j < nhostcpus; j++) {
			prev_pcpuTime[j] = pcpus_time[j];
			vcpuTotalUsage[j] = 0.0;
			// printf("Previous Stored pCPU Time: %llu (ns) on CPU%d\n", prev_pcpuTime[j], j);
		}

		for (int i=0; i < numNames; i++) {
			prev_vcpuTime[i] = curr_dom_stats[i].vcpus_time;
			prev_pcpu[i] = curr_dom_stats[i].pcpu;
			// printf("Previous Domain: %s\n", virDomainGetName(nameList[i]));
			// printf("Previous Stored vCPU Time: %llu (ns)\n", prev_vcpuTime[i]);
			// printf("Previous Stored pCPU Time: %llu (ns)\n", prev_pcpuTime[i]);
			// printf("Previous Physical CPU: %d\n", prev_pcpu[i]);
			// printf("\n");
		}

		getDomainStats(conn, nhostcpus, curr_dom_stats);

		// Get the current statistics
		for (int j=0; j < nhostcpus; j++) {
			curr_pcpuTime[j] = pcpus_time[j];
			// printf("Current Stored pCPU Time: %llu (ns) on CPU%d\n", curr_pcpuTime[j], j);
		}

		for (int i=0; i < numNames; i++) {
			curr_vcpuTime[i] = curr_dom_stats[i].vcpus_time;
			curr_pcpu[i] = curr_dom_stats[i].pcpu;
			
			// printf("Current Domain: %s\n", virDomainGetName(nameList[i]));
			// printf("Current Stored vCPU Time: %llu (ns)\n", curr_vcpuTime[i]);
			// printf("Current Stored pCPU Time: %llu (ns)\n", curr_pcpuTime[i]);
			// printf("Current Physical CPU: %d\n", curr_pcpu[i]);
			
			// printf("\n");

			// Calculate the % here
			unsigned long long diff_vcpuTime = curr_vcpuTime[i] - prev_vcpuTime[i]; // nanoseconds
			vcpu_usage[i] = diff_vcpuTime/(mySecond*period); // period is in seconds

			// printf("Usage by Domain %s VCPU: %.2lf on PCPU: %d\n", virDomainGetName(nameList[i]), 
			//	vcpu_usage[i], curr_pcpu[i]);

			vcpuTotalUsage[curr_pcpu[i]] += vcpu_usage[i];
			// printf("Total Usage on CPU%d by VCPUs: %llu\n", curr_pcpu[i], 
			//	vcpuTotalUsage[curr_pcpu[i]]);

		}

		for (int j=0; j < nhostcpus; j++) {
			unsigned long long diff_pcpuTime = curr_pcpuTime[j] - prev_pcpuTime[j]; // nanoseconds
			if (curr_pcpuTime[j] < prev_pcpuTime[j]) {
				printf("Wrong pcpu times!\n");
				return 1;
			}
			pcpu_usage[j] = diff_pcpuTime/(mySecond*period); // period is in seconds
			// printf("Usage by CPU%d: %.2lf\n", j, pcpu_usage[j]);
		}

		int ret = vcpuPin(nhostcpus, curr_dom_stats, period, vcpuTotalUsage);

		if (ret != 0) {
			printf("Pinning Failed\n");
		}

		// printf("Sleep\n");
		sleep(period);
		printf("\n");
		// printf("Wake\n");
	}
	
	free(curr_dom_stats);
	free(nameList);
	virConnectClose(conn);
	return 0;
}
