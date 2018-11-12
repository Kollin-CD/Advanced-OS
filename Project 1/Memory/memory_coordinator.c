/*
============================================================================
Name        : memory_coordinator.c
Author      : Everardo Villasenor
Version     : 1.0
Description : memory coordinator
============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <unistd.h>

// static const unsigned long long MEM_RATIO = 4; // now unused. // 25%
// static const unsigned long long MEM_LOWER_RATIO = 5; // 20%
// static const unsigned long long MEM_UPPER_RATIO = 4; // 25%
static const unsigned long long MEM_MIN_VALUE = 256 * 1024; // Minimum amount of memory for domains to have so it does not disconnect randomly. 

static const double waste_factor = 0.6;
static const double starve_factor = 0.3;

virDomainPtr *nameList = NULL;
int numNames;
virDomainMemoryStatStruct mem_stats[4][10];

/*
char *tagToMeaning(int tag)
{
    char *meaning;

    switch (tag) {
    case VIR_DOMAIN_MEMORY_STAT_SWAP_IN:
        meaning = "SWAP IN";
        break;
    case VIR_DOMAIN_MEMORY_STAT_SWAP_OUT:
        meaning = "SWAP OUT";
        break;
    case VIR_DOMAIN_MEMORY_STAT_MAJOR_FAULT:
        meaning = "MAJOR FAULT";
        break;
    case VIR_DOMAIN_MEMORY_STAT_MINOR_FAULT:
        meaning = "MINOR FAULT";
        break;
    case VIR_DOMAIN_MEMORY_STAT_UNUSED:
        meaning = "UNUSED";
        break;
    case VIR_DOMAIN_MEMORY_STAT_AVAILABLE:
        meaning = "AVAILABLE";
        break;
    case VIR_DOMAIN_MEMORY_STAT_USABLE:
        meaning = "USABLE";
        break;
    case VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON:
        meaning = "CURRENT BALLOON";
        break;
    case VIR_DOMAIN_MEMORY_STAT_RSS:
        meaning = "RSS (Resident Set Size)";
        break;
    case VIR_DOMAIN_MEMORY_STAT_NR:
        meaning = "NR";
        break;
    case VIR_DOMAIN_MEMORY_STAT_LAST_UPDATE:
        meaning = "Time of Last Stat";
        break;
    }

    return meaning;
}

void printDomainStats(virDomainPtr *nameList)
{
    printf("------------------------------------------------\n");
    // printf("%d memory stat types supported by this hypervisor\n",
    //        VIR_DOMAIN_MEMORY_STAT_NR);
    // printf("------------------------------------------------\n");
    for (int i = 0; i < numNames; i++) {
        for (int j = 0; j < 10; j++) {
            if (j==0 || j==5 || j==6 || j==7) {
                printf("%s : %s = %llu MB\n",
                       virDomainGetName(nameList[i]),
                       tagToMeaning(mem_stats[i][j].tag),
                       mem_stats[i][j].val/1024);
            }
        }
    }
}

void printHostMemoryStats(virConnectPtr conn)
{
    int nparams = 0;
    virNodeMemoryStatsPtr stats = malloc(sizeof(virNodeMemoryStats));

    if (virNodeGetMemoryStats(conn,
                  VIR_NODE_MEMORY_STATS_ALL_CELLS,
                  NULL,
                  &nparams,
                  0) == 0 && nparams != 0) {
        stats = malloc(sizeof(virNodeMemoryStats) * nparams);
        memset(stats, 0, sizeof(virNodeMemoryStats) * nparams);
        virNodeGetMemoryStats(conn,
                      VIR_NODE_MEMORY_STATS_ALL_CELLS,
                      stats,
                      &nparams,
                      0);
    }
    printf("Hypervisor memory:\n");
    for (int i = 0; i < nparams; i++) {

        printf("%8s : %lld MB\n",
               stats[i].field,
               stats[i].value/1024);
    }
}
*/

int showDomains(virConnectPtr conn) { // Start of the loop.

    int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_RUNNING;

    // printf("There are %d active domains\n", numActiveDomains);

    numNames = virConnectListAllDomains(conn, &nameList, flags);

    if (numNames == -1){ // Error Checking.
        printf("Failed to get a list of all domains: %s\n", virGetLastErrorMessage());
        return 1;
    }

    // virDomainMemoryStatStruct mem_stats[numNames][VIR_DOMAIN_MEMORY_STAT_NR];
    unsigned long long total_given = 0;
    unsigned long long domain_memory = 0;
    unsigned long long unused_memory = 0;
    unsigned long long avail_memory = 0;

    for (int i=0; i < numNames; i++) {

        unsigned int flags = VIR_DOMAIN_AFFECT_CURRENT;
        if (virDomainSetMemoryStatsPeriod(nameList[i], 1, flags) == -1) {
            printf("Failed to set period for domain %8s.\n", virDomainGetName(nameList[i]));
        }

        flags = VIR_DOMAIN_MEMORY_STAT_NR;
        if (virDomainMemoryStats(nameList[i], mem_stats[i], flags, 0) == -1) {
            printf("Failed to get stats for domain %8s.", virDomainGetName(nameList[i]));
        }

        unsigned long long instance_given = 0;
        domain_memory = mem_stats[i][0].val;
        unused_memory = mem_stats[i][5].val;
        avail_memory = mem_stats[i][6].val;
        

        // Check if the actual baloon size is wasteful:
        // printf("Domain Memory1 %s: %s: %llu\n", virDomainGetName(nameList[i]), tagToMeaning(mem_stats[i][0].tag), domain_memory);
        // printf("Unused Memory1 %s: %s: %llu\n", virDomainGetName(nameList[i]), tagToMeaning(mem_stats[i][5].tag), unused_memory);
        if ((double) domain_memory * waste_factor < unused_memory) {
            instance_given = ((double) avail_memory * waste_factor);
            // printf("Avail Memory1 %s: %s: %llu\n", virDomainGetName(nameList[i]), tagToMeaning(mem_stats[i][6].tag), avail_memory/1024);
            // printf("instance_given: %llu\n", instance_given/1024);
            total_given += (double) domain_memory - (double) instance_given; // Count total_given for stats.
            if (domain_memory - instance_given < MEM_MIN_VALUE) {
                printf("Can't go below this allocation!\n");
            } else {
                // Remove memory according to our ratio:
                int set_memory_success = virDomainSetMemory(nameList[i], domain_memory - instance_given);
                if (set_memory_success == -1) { // Error Checking.
                    printf("Failed to set memory!\n");
                    return 1;
                }
                // Print statements for clarity.
                printf("Wasteful Domain %s: deflating memory %llu MB to %llu to host.\n",
                    virDomainGetName(nameList[i]),
                    (domain_memory)/1024,
                    (domain_memory - instance_given)/1024);
            }
        }
    }

    unsigned long long total_needed = 0;
    unsigned long long total_taken = 0;

    // Now that memory is free, it can be allocated to the starving domains.
    for (int i=0; i < numNames; i++) {
        domain_memory = mem_stats[i][0].val;
        unused_memory = mem_stats[i][5].val;
        avail_memory = mem_stats[i][6].val;
        // Is it starving?
        // printf("Domain Memory2 %s: %s: %llu\n", virDomainGetName(nameList[i]), tagToMeaning(mem_stats[i][0].tag), domain_memory);
        // printf("Unused Memory2 %s: %s: %llu\n", virDomainGetName(nameList[i]), tagToMeaning(mem_stats[i][5].tag), unused_memory);
        if ((double) domain_memory * starve_factor > unused_memory) {
            // Add need to stats and print.
            total_needed += ((double) domain_memory * starve_factor) + domain_memory;
            // printf("Avail Memory2 %s: %s: %llu\n", virDomainGetName(nameList[i]), tagToMeaning(mem_stats[i][6].tag), avail_memory);
            // printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
            // printf("Max Memory: %lu\n", virDomainGetMaxMemory(nameList[i])/1024);
            // printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
            // Check if there is enough memory.
            if ( ((double) domain_memory + (double) domain_memory * starve_factor) > virDomainGetMaxMemory(nameList[i])) {
                // If there isn't, warn the user.
                printf("Not enough memory! Allocating maximum of: %lu\n", virDomainGetMaxMemory(nameList[i]));
                // Add what can be allocated to stats.
                total_taken += virNodeGetFreeMemory(conn);
                // Then allocate all that's left.
                if (virDomainSetMemory(nameList[i], virDomainGetMaxMemory(nameList[i])) == -1) { 
                    // But check for errors to be safe.
                    printf("Failed to set memory! 1\n");
                    return 1;
                } 
            } else { // If there is enough memory:
                // Add to stats.
                printf("Taking memory from what was removed\n");
                total_taken += ((double) domain_memory * starve_factor);
                // Allocate the memory.
                if(virDomainSetMemory(nameList[i], (double) domain_memory + ((double) domain_memory * starve_factor)) == -1) {
                    // And be sure to check for errors.
                    printf("Failed to set memory! 2\n");
                    return 1;
                }
            }

            printf("Starving domain %s: inflating memory from %llu MB to %0.2f MB.\n", 
                virDomainGetName(nameList[i]),
                domain_memory/1024,
                (domain_memory + (domain_memory * starve_factor))/1024);
        }
    }

    // printf("\nTotal Removed: %llu (MB)\n", total_given/1024);
    // Print stats.
    // printf("Total Needed:\t%llu\n", total_needed/1024);
    // printf("Total Taken: %llu (MB)\n", total_taken/1024);
    
	return 0; // End of the loop.
}

int main(int argc, char **argv) {
  // Handle the input
	if(argc != 2) { // And check for errors.
		printf("ERROR: Argument is time interval in seconds.");
		return 1;
	}

	double period = atof(argv[1]); // Parse.
    double total = period;
	// printf("Take action every %lf (s)\n", period);
  
  	// Open the conection.
	virConnectPtr conn;
	conn = virConnectOpen("qemu:///system");

	if (!conn) { // Check for errors opening connection.
		printf("Failed to get URI for hypervisor connection: %s\n", virGetLastErrorMessage());
		return 1;
	}
  
    while(showDomains(conn) == 0) { // Loop through the stat collection and memory management process.
        virNodeInfo node_info;
        virNodeGetInfo(conn, &node_info);
        // printf("%d cpus.\n", node_info.cpus); // nhostcpus
        printf("Running Coordinator\n");
        // printDomainStats(nameList);
        // printf("------------------------------------------------\n");
        // Print Host Stats:
        // printHostMemoryStats(conn);
        sleep(period); // Sleep according to user input.
        total += period; 
        // printf("Total time: %.2f (s)\n", total);
        // printf("------------------------------------------------\n");
    }
  
    printf("showDomains(conn) != 0, so exiting.\n"); // Message to indicate graceful termination.
    virConnectClose(conn); // Close connections.
    return 0;
}