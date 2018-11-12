#!/usr/bin/python

from __future__ import print_function
import libvirt
import sched, time
import os
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

CONFIG_FILE = '../vmlist.conf'

global conn
global vmlist
global vmobjlist
global s
global memoryinfolist

# s = sched.scheduler(time.time, time.sleep)

class UpdateGraph(object):
    def __init__(self, ax, conn, vmlist, memoryinfolist, vmobjlist, machine_to_graph):
        self.success = 0
        self.memoryinfolist = memoryinfolist
        self.vmobjlist = vmobjlist
        self.line, = ax.plot([], [], 'k-')
        self.line2, = ax.plot([], [], 'g-')
        self.ax = ax

        # Set up plot parameters
        minutes = 2
        self.ax.set_xlim(0, 60 * minutes) #2 minutes
        self.ax.set_ylim(0, 3000) #2 gb = 3000
        self.ax.grid(True)
        self.xset = []
        self.yset = []
        self.yset_used = []
        self.machine_to_graph = machine_to_graph


    def init(self):
        self.success = 0
        self.line.set_data([], [])
        self.line2.set_data([], [])
        return self.line,

    def __call__(self, x):
        # This way the plot can continuously run and we just keep
        # watching new realizations of the process
        if x == 0:
            return self.init()

        for i, vmobj in enumerate(self.vmobjlist):
            memoryinfolist[i] = vmobj.memoryStats()

            actual = memoryinfolist[i]['actual'] / 1024.0
            unused = memoryinfolist[i]['unused'] / 1024.0
            used  = actual - unused

            if i == self.machine_to_graph:
                print("Ploting {} {}".format(x, actual))
                self.xset.append(x)
                self.yset.append(actual)
                self.yset_used.append(used)
                self.line.set_data(self.xset, self.yset)
                self.line2.set_data(self.xset, self.yset_used)

            print("{}: {} {}"
                .format(vmlist[i], 
                        actual, 
                        unused))

        print('-' * 80)
        
        st = os.popen('virsh nodememstats').read()
        print(st)
        print('-' * 80)

        return self.line, self.line2

def run(sc):
    for i in range(len(vmobjlist)):
        memoryinfolist[i] = vmobjlist[i].memoryStats()
        print("{}: {} {}"
            .format(vmlist[i], 
                    memoryinfolist[i]['actual'] / 1024.0,
                    memoryinfolist[i]['unused'] / 1024.0))
    print('-' * 80)
    
    t = os.popen('virsh nodememstats').read()
    print(t)
    print('-' * 80)

    s.enter(2, 1, run, (sc,))

if __name__ == '__main__':
    conn = libvirt.open('qemu:///system')
    vmlist = open(CONFIG_FILE, 'r').read().strip().split()

    vmobjlist = []   

    for vmname in vmlist:
        vm = conn.lookupByName(vmname)
        if vm:
            vmobjlist.append(vm)
        else:
            print('Unable to locate {}.'.format(vmnane))
            exit(-1)
    
    for vm in vmobjlist:
        vm.setMemoryStatsPeriod(1)   
 
    memoryinfolist = [None] * len(vmobjlist)

    #TODO have a graph for each machine
    fig, ax = plt.subplots()
    ug = UpdateGraph(ax, conn, vmlist, memoryinfolist, vmobjlist, 0)
    anim = FuncAnimation(fig, ug, init_func=ug.init, interval=1000, blit=True)

    plt.show()

    # s.enter(2, 1, run, (s,))
    # s.run()
