// pcap-filter.cc - filters packets from a PCAP file that belong (or do not)
// into the language of a provided NFA

#include "mata/utils/util.hh"
#include "mata/nfa/nfa.hh"

#include <chrono>
#include <iostream>
#include <fstream>

// PCAP-related headers
#include <pcap.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

// Ethernet 802.1Q header
// copied from
// https://stackoverflow.com/questions/13166094/build-vlan-header-in-c
struct vlan_ethhdr {
	u_int8_t  ether_dhost[ETH_ALEN];  /* destination eth addr */
	u_int8_t  ether_shost[ETH_ALEN];  /* source ether addr    */
	u_int16_t h_vlan_proto;
	u_int16_t h_vlan_TCI;
	u_int16_t ether_type;
} __attribute__ ((__packed__));

using namespace Mata::Nfa;
using namespace Mata::Parser;

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

// FUNCTION DECLARATIONS
void packetHandler(u_char *userData, const pcap_pkthdr* pkthdr, const u_char* packet);

// GLOBAL VARIABLES
size_t total_packets = 0;
size_t payloaded_packets = 0;
size_t filtered_packets = 0;
bool prefix_acceptance = false;
bool keep_in_language = true;
Nfa aut;
pcap_dumper_t* dumper = nullptr;


void print_usage(const char* prog_name)
{
	std::cout << "usage: " << prog_name << " [-p] <--in|--notin> <aut.mf> <input.pcap> <output.pcap>\n";
	std::cout << "\n";
	std::cout << "Options:\n";
	std::cout << "  --in     keep packets IN the language of aut.mf\n";
	std::cout << "  --notin  keep packets NOT IN the language of aut.mf\n";
	std::cout << "  -p       prefix acceptance\n";
}

Nfa load_aut(const std::string& file_name)
{
	Nfa result;
	std::ifstream input(file_name);
	if (input.is_open())
	{
		Parsed parsed = parse_mf(input);
		Mata::Nfa::CharAlphabet alphabet;
		construct(&result, parsed[0], &alphabet);
	    return result;
	}
	else
	{
		throw std::runtime_error("Cannot open file " + file_name);
	}
}

int main(int argc, char** argv)
{
	// PARSING COMMAND LINE ARGUMENTS
	size_t param_start = 1;
	if (argc != 5 && argc != 6)
	{
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (std::to_string(argv[1]) == "-p")
	{
		prefix_acceptance = true;
		param_start = 2;

		if (argc != 6)
		{
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}
	else
	{
		if (argc != 5)
		{
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (std::to_string(argv[param_start + 0]) == "--in")
	{
		keep_in_language = true;

	}
	else if (std::to_string(argv[param_start + 0]) == "--notin")
	{
		keep_in_language = false;
	}
	else
	{
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	// LOADING INPUTS
	std::string aut_file = argv[param_start + 1];
	std::string packets_file = argv[param_start + 2];
	std::string dump_file = argv[param_start + 3];

	try
	{
		aut = load_aut(aut_file);
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error loading automata: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}

	pcap_t *descr = nullptr;
	char errbuf[PCAP_ERRBUF_SIZE];

	// open capture file for offline processing
	descr = pcap_open_offline(packets_file.c_str(), errbuf);
	if (nullptr == descr)
	{
		std::cout << "pcap_open_offline() failed: " << errbuf << "\n";
		return EXIT_FAILURE;
	}

	dumper = pcap_dump_open(descr, dump_file.c_str());
	if (nullptr == dumper)
	{
		std::cout << "pcap_dump_open() failed: " << pcap_geterr(descr);
		return EXIT_FAILURE;
	}

	TimePoint startTime = std::chrono::high_resolution_clock::now();

	// start packet processing loop, just like live capture
	if (pcap_loop(descr, 0, packetHandler, nullptr) < 0)
	{
		std::cout << "pcap_loop() failed: " << pcap_geterr(descr);
		return EXIT_FAILURE;
	}

	pcap_dump_close(dumper);

	TimePoint finishTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> opTime = finishTime - startTime;

	std::cout << "\n";
	std::cout << "Total packets in " << packets_file << ": " << total_packets << "\n";
	std::cout << "Packets with payload: " << payloaded_packets << "\n";
	std::cout << "Filtered packets: " << filtered_packets << "\n";
	std::cout << "Time: " <<
		std::chrono::duration_cast<std::chrono::nanoseconds>(opTime).count() * 1e-9
		<< "\n";

	return EXIT_SUCCESS;
}


Word get_payload(
	const pcap_pkthdr* pkthdr,
	const u_char* packet)
{
	assert(nullptr != pkthdr);
	assert(nullptr != packet);

	size_t offset = sizeof(ether_header);
	const ether_header* eth_hdr = reinterpret_cast<const ether_header*>(packet);
	uint16_t ether_type = ntohs(eth_hdr->ether_type);
	if (ETHERTYPE_VLAN == ether_type)
	{
		offset = sizeof(vlan_ethhdr);
		const vlan_ethhdr* vlan_hdr = reinterpret_cast<const vlan_ethhdr*>(packet);
		ether_type = ntohs(vlan_hdr->ether_type);
	}

	unsigned l4_proto;

	if (ETHERTYPE_IP == ether_type)
	{
		const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
		offset += sizeof(ip);
		l4_proto = ip_hdr->ip_p;
	}
	else if (ETHERTYPE_IPV6 == ether_type)
	{
		const ip6_hdr* ip_hdr = reinterpret_cast<const ip6_hdr*>(packet + offset);
		offset += sizeof(ip6_hdr);
		l4_proto = ip_hdr->ip6_nxt;
	}
	else
	{
		return Word();
	}

	bool ip_in_ip = false;

	bool processing = true;
	while (processing)
	{
		processing = false;
		if (IPPROTO_TCP == l4_proto)
		{
			const tcphdr* tcp_hdr = reinterpret_cast<const tcphdr*>(packet + offset);
			size_t tcp_hdr_size = tcp_hdr->th_off * 4;
			offset += tcp_hdr_size;
		}
		else if (IPPROTO_UDP == l4_proto)
		{
			offset += sizeof(udphdr);
		}
		else if (IPPROTO_IPIP == l4_proto)
		{
			if (ip_in_ip) { assert(false); }

			ip_in_ip = true;

			const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
			offset += sizeof(ip);
			l4_proto = ip_hdr->ip_p;

			processing = true;
		}
		else if (IPPROTO_ESP == l4_proto)
		{
			offset += 8;
		}
		else if (IPPROTO_ICMP == l4_proto)
		{
			offset += sizeof(icmphdr);
		}
		else if (IPPROTO_GRE == l4_proto)
		{
			return Word();
		}
		else if (IPPROTO_ICMPV6 == l4_proto)
		{
			offset += sizeof(icmp6_hdr);
		}
		else if (IPPROTO_FRAGMENT == l4_proto)
		{
			const ip6_frag* ip_hdr = reinterpret_cast<const ip6_frag*>(packet + offset);
			offset += sizeof(ip6_frag);
			l4_proto = ip_hdr->ip6f_nxt;

			processing = true;
		}
		else if (IPPROTO_IPV6 == l4_proto)
		{
			const ip6_hdr* ip_hdr = reinterpret_cast<const ip6_hdr*>(packet + offset);
			offset += sizeof(ip6_hdr);
			l4_proto = ip_hdr->ip6_nxt;
		}
		else if (IPPROTO_PIM == l4_proto)
		{
			return Word();
		}
		else
		{
			return Word();
		}
	}

	return Word(packet + offset, packet + std::max(static_cast<size_t>(pkthdr->len), offset));
	// return Word(packet + offset, packet + pkthdr->len);
}

void packetHandler(
	u_char* /* userData */,
	const pcap_pkthdr* pkthdr,
	const u_char* packet)
{
	assert(nullptr != pkthdr);
	assert(nullptr != packet);

	++total_packets;

	Word payload = get_payload(pkthdr, packet);
	if (payload.empty())
	{
		return;
	}

	++payloaded_packets;

	bool in_lang = false;
	if (prefix_acceptance)
	{
		in_lang = is_prfx_in_lang(aut, payload);
	}
	else
	{
		in_lang = is_in_lang(aut, payload);
	}

	if ((in_lang && keep_in_language) || (!in_lang && !keep_in_language))
	{
		++filtered_packets;
		pcap_dump((u_char*)dumper, pkthdr, packet);
	}

	if (total_packets % 10000 == 0)
	{
		std::clog << "#";
		std::clog.flush();
	}
}
