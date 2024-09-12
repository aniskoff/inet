package inet.examples.inet.simple_routing;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.StandardHost;
import ned.DatarateChannel;
import custom.trafficengineering.simpleobserver.SimpleObserver;
import inet.node.inet.Router;
import inet.visualizer.canvas.integrated.IntegratedCanvasVisualizer;



network SimpleRouting 
{
  
    parameters:
        double per = default(0);
    types:
        channel C extends DatarateChannel
        {
            datarate = 5Mbps;
            delay = 0.1us;
            per = parent.per;
        }
    submodules:
        observer: SimpleObserver {
            parameters:
                @display("p=400,0;i=block/circle");
        }
        client1: StandardHost {
            parameters:
                @display("p=-100,300;i=device/terminal,grey");
        }

        router1: Router {
            parameters:
              @display("p=150,300;i=abstract/router2");
        }

        router2: Router {
            parameters:
                @display("p=250,150;i=abstract/router2");
        }

        router3: Router {
            parameters:
                @display("p=250,500;i=abstract/router2");
        }

        router4: Router {
            parameters:
                @display("p=400,300;i=abstract/router2");
        }


        server1: StandardHost {
          parameters:
              @display("p=600,300;i=device/server,gold");
        }
        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("p=200,0;is=s");
        }

        // configurator: SimpleObserver {
        //     parameters:
        //         @display("p=200,0;is=s");
        // }

        visualizer: IntegratedCanvasVisualizer {
            parameters:
                @display("p=300,0;is=s");
        }

    connections:
        client1.pppg++ <--> C <--> router1.pppg++;
        router1.pppg++ <--> C <--> router2.pppg++;
        router2.pppg++ <--> C <--> router4.pppg++;
        router1.pppg++ <--> C <--> router3.pppg++;
        router3.pppg++ <--> C <--> router4.pppg++;
        router4.pppg++ <--> C <--> server1.pppg++;
        
}