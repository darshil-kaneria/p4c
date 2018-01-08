#include <core.p4>
#include <ebpf_model.p4>

@ethernetaddress typedef bit<48> EthernetAddress;
@ipv4address typedef bit<32> IPv4Address;
header Ethernet_h {
    EthernetAddress dstAddr;
    EthernetAddress srcAddr;
    bit<16>         etherType;
}

header IPv4_h {
    bit<4>      version;
    bit<4>      ihl;
    bit<8>      diffserv;
    bit<16>     totalLen;
    bit<16>     identification;
    bit<3>      flags;
    bit<13>     fragOffset;
    bit<8>      ttl;
    bit<8>      protocol;
    bit<16>     hdrChecksum;
    IPv4Address srcAddr;
    IPv4Address dstAddr;
}

struct Headers_t {
    Ethernet_h ethernet;
    IPv4_h     ipv4;
}

parser prs(packet_in p, out Headers_t headers) {
    state start {
        p.extract<Ethernet_h>(headers.ethernet);
        transition select(headers.ethernet.etherType) {
            16w0x800: ip;
            default: reject;
        }
    }
    state ip {
        p.extract<IPv4_h>(headers.ipv4);
        transition accept;
    }
}

control pipe(inout Headers_t headers, out bool pass) {
    @name("NoAction") action NoAction_0() {
    }
    @name("Reject") action Reject_0(IPv4Address add) {
        pass = false;
        headers.ipv4.srcAddr[31:0] = add[31:16] ++ add[15:0];
    }
    @name("Check_src_ip") table Check_src_ip {
        key = {
            headers.ipv4.srcAddr: exact @name("headers.ipv4.srcAddr") ;
        }
        actions = {
            Reject_0();
            NoAction_0();
        }
        implementation = hash_table(32w1024);
        const default_action = NoAction_0();
    }
    apply {
        bool hasReturned_0 = false;
        pass = true;
        if (!headers.ipv4.isValid()) {
            pass = false;
            hasReturned_0 = true;
        }
        if (!hasReturned_0) 
            Check_src_ip.apply();
    }
}

ebpfFilter<Headers_t>(prs(), pipe()) main;

