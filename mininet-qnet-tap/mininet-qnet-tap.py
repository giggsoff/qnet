#!/usr/bin/python
from mininet.node import CPULimitedHost
from mininet.link import TCLink
import traceback
from mininet_common import sshd, checkIntf
from mininet.log import setLogLevel, info
from mininet.node import RemoteController, OVSSwitch, Controller
from mininet.net import Mininet
from mininet.link import Intf
from mininet.cli import CLI

from os import path
import sys
from subprocess import call
import yaml
import sys
import pprint
import time
from parse_yaml import Loader

config = yaml.load(open(sys.argv[1]).read()+open(sys.argv[2]).read(), Loader)
host = sys.argv[3]
#pprint.pprint(config)

setLogLevel('info')

host_config=config['hosts'][host]
class MultiSwitch( OVSSwitch ):
    "Custom Switch() subclass that connects to different controllers"
    def start( self, controllers ):
        return OVSSwitch.start( self, [ cmap[ self.name ] ])

to_kill = []
net = Mininet( switch=MultiSwitch, build=False,
	           host=CPULimitedHost, link=TCLink )
try:
    hosts = {}
    for h in host_config['vhosts']:
         host_ip = h['ip']+'/24'
         host_mac = h['mac']
         host_name = h['name']
         hosts[host_name] = net.addHost(host_name,ip=host_ip, mac=host_mac)
    controllers={}
    controllers_to_add = list()
    for c in host_config['controllers']:
        controller_ip = c['ip']
        controller_port = c['port']
        controller_name = c['name']
        print 'Creating controller:', controller_name
        if controller_ip == 'None':
            c0 = Controller( controller_name, port=controller_port)
            controllers_to_add.append(c0)
        else:
            c0 = RemoteController( controller_name, ip=controller_ip, port=controller_port )
        controllers[controller_name]=c0
    devices = host_config['phys']
    for task in sorted(host_config['pre-scripts'], key=lambda x: x['prio']):
        if task['type'] == 'device':
             devices.append(task)
        print "Running pre-script", task['cmdline'].format(**task)
        if isinstance(task['vhost'], dict):
            host = hosts[task['vhost']['name']]
            host.cmd(task['cmdline'].format(**task))
        else:
            call(task['cmdline'].format(**task), shell=True)
        to_kill.append(task)
    time.sleep(1)
    for device in devices:
        info( '*** Checking', device['device'], '\n' )
        #checkIntf(device['device'])
    cmap={}
    for switch_data in host_config['switches']:
         switch_name = switch_data['name']
         switch_controller_name = switch_data['controller']['name']
         cmap[switch_name]=controllers[switch_controller_name]

    for c in controllers_to_add:
        net.addController(c)

    switches = {}
    for  s in host_config['switches']:
        switches[s['name']]=net.addSwitch(s['name'])
          

    for n1,n2,sp,de in host_config['links']:
        if n1['type']=='switch' and n2['type']=='device':
             intfName = n2['device']
             #checkIntf(intfName)
             if isinstance(n2['vhost'], dict):
                 info( '*** Linking', n2['vhost']['name'],n1['name'],intfName, '\n' )
                 net.addLink(n2['vhost']['name'],n1['name'],intfName1=intfName+'lb', bw=sp, delay=de)
                 hosts[n2['vhost']['name']].cmd('brctl addbr brvh&&brctl addif brvh '+intfName+'&&brctl addif brvh '+intfName+'lb'+'&&ifconfig brvh up')
             else:
                 _intf = Intf(intfName, switches[n1['name']])
        elif n2['type']=='switch' and n1['type']=='device':
             intfName = n1['device']
             #checkIntf(intfName)
             if isinstance(n1['vhost'], dict):
                 info( '*** Linking', n1['vhost']['name'],n2['name'],intfName, '\n' )
                 net.addLink(n1['vhost']['name'],n2['name'],intfName1=intfName+'lb', bw=sp, delay=de)
                 hosts[n1['vhost']['name']].cmd('brctl addbr brvh&&brctl addif brvh '+intfName+'&&brctl addif brvh '+intfName+'lb'+'&&ifconfig brvh up')
             else:
                 _intf = Intf(intfName, switches[n2['name']])
        elif n1['type']=='vhost' and n2['type']=='switch':
             net.addLink(n1['name'],n2['name'], bw=sp, delay=de)
             if isinstance(n1['sw2'], dict):
                 net.addLink(n1['name'], n1['sw2']['name'], intfName1=n1['name']+n1['sw2']['name']+'-2', bw=sp, delay=de)
                 hosts[n1['name']].cmd('ifconfig '+n1['name']+n1['sw2']['name']+'-2'+' '+n1['ip2']+' netmask 255.255.255.0')             
        elif n2['type']=='vhost' and n1['type']=='switch':
             net.addLink(n1['name'],n2['name'], bw=sp, delay=de)
             if isinstance(n2['sw2'], dict):
                 net.addLink(n2['sw2']['name'],n2['name'],intfName2=n2['name']+n2['sw2']['name']+'-2', bw=sp, delay=de)
                 hosts[n2['name']].cmd('ifconfig '+n2['name']+n2['sw2']['name']+'-2'+' '+n2['ip2']+' netmask 255.255.255.0')                          
        else:
             info( '*** Linking', n1['name'],n2['name'], '\n' )
             net.addLink(n1['name'],n2['name'], bw=sp, delay=de)
    if 'ssh' in host_config: 
        for instance in host_config['ssh']:
            print instance
            rootns_ip = instance['ip']
            switch_name = instance['switch']['name']
            sshd(network=net, ip=rootns_ip, switch=switches[switch_name], host=switch_name)
    net.build()
    net.start()
    for task in sorted(host_config['post-scripts'], key=lambda x: x['prio']):
        if task['type'] == 'device':
             devices.append(task)
        print "Running post-script", task['cmdline'].format(**task)
        if isinstance(task['vhost'],dict):
            host = hosts[task['vhost']['name']]
            host.cmd(task['cmdline'].format(**task))
        else:
            call(task['cmdline'].format(**task), shell=True)
        to_kill.append(task)
    time.sleep(5)
    CLI(net)
except:
    traceback.print_exc(file=sys.stdout)
finally:
    call(["pkill", "controller"])
    for task in to_kill:
       if 'killcmd' in task:
           print task['killcmd'].format(**task)
           call(task['killcmd'].format(**task), shell=True)
    net.stop()

