package inet.examples.inet.mainexperiment;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.StandardHost;
import ned.DatarateChannel;
import custom.trafficengineering.simpleobserver.SimpleObserver;
import inet.node.inet.Router;
import inet.visualizer.canvas.integrated.IntegratedCanvasVisualizer;
import custom.queueing.source.CSVPacketSource;


network MainExperiment
{
    parameters:
        double per = default(0);
        @display("bgb=913,407");
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
                @display("i=block/circle");
        }

        //----------------->HOSTS<----------------------//
        s11l: StandardHost {
            parameters:
                @display("i=device/terminal,grey;p=207,21");
        }

        s12l: StandardHost {
            parameters:
                @display("i=device/terminal,grey;p=267,21");
        }

        s21l: StandardHost {
            parameters:
                @display("i=device/terminal,grey;p=338,21");
        }

        s22l: StandardHost {
            parameters:
                @display("i=device/terminal,grey;p=401,21");
        }

        s31l: StandardHost {
            parameters:
                @display("i=device/terminal,grey;p=466,21");
        }

        s32l: StandardHost {
            parameters:
                @display("i=device/terminal,grey;p=523,21");
        }

        //----------------->FST HOPS<-------------------//

        n1tl: Router {
            parameters:
                @display("i=abstract/router2;p=262,106");
        }
        n2tl: Router {
            parameters:
                @display("i=abstract/router2;p=363,106");
        }

        n3tl: Router {
            parameters:
                @display("i=abstract/router2;p=485,106");
        }

        //----------------->MID HOPS<-------------------//

        n1a: Router {
            parameters:
                @display("i=abstract/router2;p=253,188");
        }

        n2a: Router {
            parameters:
                @display("i=abstract/router2;p=369,188");
        }

        n3a: Router {
            parameters:
                @display("i=abstract/router2;p=485,178");
        }

        //----------------->LST HOPS<-------------------//

        n1tr: Router {
            parameters:
                @display("i=abstract/router2;p=267,278");
        }

        n3tr: Router {
            parameters:
                @display("i=abstract/router2;p=369,278");
        }

        n2tr: Router {
            parameters:
                @display("i=abstract/router2;p=471,278");
        }

        //----------------->SERVERS<--------------------//






        s11r: StandardHost {
            parameters:
                @display("i=device/server,gold;p=207,365");
        }

        s12r: StandardHost {
            parameters:
                @display("i=device/server,gold;p=267,365");
        }

        s32r: StandardHost {
            parameters:
                @display("i=device/server,gold;p=338,365");
        }

        s31r: StandardHost {
            parameters:
                @display("i=device/server,gold;p=386,365");
        }

        s21r: StandardHost {
            parameters:
                @display("i=device/server,gold;p=466,365");
        }

        s22r: StandardHost {
            parameters:
                @display("i=device/server,gold;p=525,365");
        }

        configurator: Ipv4NetworkConfigurator {
            parameters:
                @display("is=s;p=810,31");
        }


        visualizer: IntegratedCanvasVisualizer {
            parameters:
                @display("is=s;p=743,31");
        }

    connections:

        //-------->HOSTS<->FSTHOPS<--------//
        s11l.pppg++ <--> C <--> n1tl.pppg++;
        s12l.pppg++ <--> C <--> n1tl.pppg++;

        s21l.pppg++ <--> C <--> n2tl.pppg++;
        s22l.pppg++ <--> C <--> n2tl.pppg++;

        s31l.pppg++ <--> C <--> n3tl.pppg++;
        s32l.pppg++ <--> C <--> n3tl.pppg++;


        //------>FSTHOPS<->MIDHOPS<-------//
        n1tl.pppg++ <--> C <--> n1a.pppg++;
        n1tl.pppg++ <--> C <--> n2a.pppg++;

        n2tl.pppg++ <--> C <--> n1a.pppg++;
        n2tl.pppg++ <--> C <--> n2a.pppg++;
        n2tl.pppg++ <--> C <--> n3a.pppg++;

        n3tl.pppg++ <--> C <--> n2a.pppg++;
        n3tl.pppg++ <--> C <--> n3a.pppg++;


        //----->MIDHOPS<->LSTHOPS<--------//
        n1a.pppg++ <--> C <--> n1tr.pppg++;
        n1a.pppg++ <--> C <--> n3tr.pppg++;

        n2a.pppg++ <--> C <--> n1tr.pppg++;
        n2a.pppg++ <--> C <--> n3tr.pppg++;
        n2a.pppg++ <--> C <--> n2tr.pppg++;

        n3a.pppg++ <--> C <--> n3tr.pppg++;
        n3a.pppg++ <--> C <--> n2tr.pppg++;


        //----->LSTHOPS<->SERVERS<--------//
        n1tr.pppg++ <--> C <--> s11r.pppg++;
        n1tr.pppg++ <--> C <--> s12r.pppg++;

        n3tr.pppg++ <--> C <--> s32r.pppg++;
        n3tr.pppg++ <--> C <--> s31r.pppg++;

        n2tr.pppg++ <--> C <--> s21r.pppg++;
        n2tr.pppg++ <--> C <--> s22r.pppg++;

}
