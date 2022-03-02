from mininet.cli import CLI
from mininet.net import Mininet
from mininet.link import TCLink
from mininet.topo import Topo
from mininet.log import setLogLevel

class StarFish(Topo):
    def __init__(self,**opts):
        Topo.__init__(self,**opts)
        h1=self.addHost('h1')
        h2=self.addHost('h2')
        h3=self.addHost('h3')
        h4=self.addHost('h4')
        h5=self.addHost('h5')
        s=self.addSwitch('s')
        self.addLink(h1,s,bw=2,delay='10ms')
        self.addLink(h2,s,bw=1,delay='10ms')
        self.addLink(h3,s,bw=0.5,delay='10ms')
        self.addLink(h4,s,bw=0.5,delay='10ms')
        self.addLink(h5,s,bw=0.5,delay='10ms')

if __name__=='__main__':
    setLogLevel('info')
    topo=StarFish()
    net=Mininet(topo=topo,link=TCLink,autoSetMacs=True,autoStaticArp=True)
    net.start()
    CLI(net)
    net.stop()
