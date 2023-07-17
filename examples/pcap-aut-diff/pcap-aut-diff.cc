// pcap-aut-diff - gets a pair of NFAs A1 and A2 and a set of network packets P
// in the pcap format, and tests how many packets from P lie in the symmetric
// difference of the languages of A1 and A2

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

#ifdef __APPLE__
#define ETH_ALEN    6       /* Octets in one ethernet addr   */
#define iphdr ip
#define icmphdr icmp
#endif

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
size_t vlan_packets = 0;
size_t ipv4_packets = 0;
size_t ipv6_packets = 0;
size_t tcp_packets = 0;
size_t udp_packets = 0;
size_t ipip_packets = 0;
size_t esp_packets = 0;
size_t icmp_packets = 0;
size_t gre_packets = 0;
size_t icmp6_packets = 0;
size_t v6_fragment_packets = 0;
size_t ip6_in_ip4_packets = 0;
size_t pim_packets = 0;
size_t other_l3_packets = 0;
size_t other_l4_packets = 0;
size_t incons_packets = 0;
size_t accepted_aut1 = 0;
size_t accepted_aut2 = 0;
size_t accepted_aut1_not_aut2 = 0;
size_t accepted_aut2_not_aut1 = 0;
bool prefix_acceptance = false;
Nfa aut1;
Nfa aut2;



void print_usage(const char* prog_name)
{
	std::cout << "usage: " << prog_name << " [-p] aut1.vtf aut2.vtf packets.pcap\n";
	std::cout << "\n";
	std::cout << "Options:\n";
	std::cout << "  -p    prefix acceptance\n";
}

Nfa load_aut(const std::string& file_name)
{
	Nfa result;
	std::ifstream input(file_name);
	if (input.is_open())
	{
		Parsed parsed = parse_mf(input, true);
		Mata::Nfa::DirectAlphabet alphabet;
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
	if (argc != 4 && argc != 5)
	{
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (std::to_string(argv[1]) == "-p")
	{
		prefix_acceptance = true;
		param_start = 2;

		if (argc != 5)
		{
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}
	else
	{
		if (argc != 4)
		{
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	// LOADING INPUTS
	std::string aut1_file = argv[param_start + 0];
	std::string aut2_file = argv[param_start + 1];
	std::string packets_file = argv[param_start + 2];

	try
	{
		aut1 = load_aut(aut1_file);
		aut2 = load_aut(aut2_file);
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

	TimePoint startTime = std::chrono::high_resolution_clock::now();

	// start packet processing loop, just like live capture
	if (pcap_loop(descr, 0, packetHandler, nullptr) < 0)
	{
		std::cout << "pcap_loop() failed: " << pcap_geterr(descr);
		return EXIT_FAILURE;
	}

	TimePoint finishTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> opTime = finishTime - startTime;

	std::cout << "\n";
	std::cout << "Total packets in " << packets_file << ": " << total_packets << "\n";
	std::cout << "Packets with VLAN: " << vlan_packets << "\n";
	std::cout << "Packets with IPv4: " << ipv4_packets << "\n";
	std::cout << "Packets with IPv6: " << ipv6_packets << "\n";
	std::cout << "Packets with other L3 (not processed): " << other_l3_packets << "\n";
	std::cout << "Packets with TCP: " << tcp_packets << "\n";
	std::cout << "Packets with UDP: " << udp_packets << "\n";
	std::cout << "Packets with IPv4-in-IPv4: " << ipip_packets << "\n";
	std::cout << "Packets with ESP: " << esp_packets << "\n";
	std::cout << "Packets with ICMP: " << icmp_packets << "\n";
	std::cout << "Packets with GRE (not processed): " << gre_packets << "\n";
	std::cout << "Packets with ICMPv6: " << icmp6_packets << "\n";
	std::cout << "Packets with IPv6 fragment: " << v6_fragment_packets << "\n";
	std::cout << "Packets with IPv6-in-IPv4: " << ip6_in_ip4_packets << "\n";
	std::cout << "Packets with PIM (not processed): " << pim_packets << "\n";
	std::cout << "Packets with other L4 (not processed): " << other_l4_packets << "\n";
	std::cout << "Packets with payload: " << payloaded_packets << "\n";
	std::cout << "Accepted in Aut1: " << accepted_aut1 << "\n";
	std::cout << "Accepted in Aut2: " << accepted_aut2 << "\n";
	std::cout << "Accepted in Aut1 but not in Aut2: " << accepted_aut1_not_aut2 << "\n";
	std::cout << "Accepted in Aut2 but not in Aut1: " << accepted_aut2_not_aut1 << "\n";
	std::cout << "Inconsistent packets: " << incons_packets << "\n";
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
		++vlan_packets;

		offset = sizeof(vlan_ethhdr);
		const vlan_ethhdr* vlan_hdr = reinterpret_cast<const vlan_ethhdr*>(packet);
		ether_type = ntohs(vlan_hdr->ether_type);
	}

	// Word pac(packet, packet + pkthdr->len);
	// std::cout << "Packet #" << total_packets-1 << ": ";
	// std::cout << std::to_string(pac) << "\n";

	unsigned l4_proto;

	if (ETHERTYPE_IP == ether_type)
	{
		++ipv4_packets;

		const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
		offset += sizeof(ip);
		l4_proto = ip_hdr->ip_p;
	}
	else if (ETHERTYPE_IPV6 == ether_type)
	{
		++ipv6_packets;

		const ip6_hdr* ip_hdr = reinterpret_cast<const ip6_hdr*>(packet + offset);
		offset += sizeof(ip6_hdr);
		l4_proto = ip_hdr->ip6_nxt;
	}
	else
	{
		++other_l3_packets;
		return Word();
	}

	bool ip_in_ip = false;

	bool processing = true;
	while (processing)
	{
		processing = false;
		if (IPPROTO_TCP == l4_proto)
		{
			++tcp_packets;
			const tcphdr* tcp_hdr = reinterpret_cast<const tcphdr*>(packet + offset);
			size_t tcp_hdr_size = tcp_hdr->th_off * 4;
			offset += tcp_hdr_size;
		}
		else if (IPPROTO_UDP == l4_proto)
		{
			++udp_packets;
			offset += sizeof(udphdr);
		}
		else if (IPPROTO_IPIP == l4_proto)
		{
			++ipip_packets;

			if (ip_in_ip) { assert(false); }

			ip_in_ip = true;

			const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
			offset += sizeof(ip);
			l4_proto = ip_hdr->ip_p;

			processing = true;
		}
		else if (IPPROTO_ESP == l4_proto)
		{
			++esp_packets;
			offset += 8;
		}
		else if (IPPROTO_ICMP == l4_proto)
		{
			++icmp_packets;

			offset += sizeof(icmphdr);
		}
		else if (IPPROTO_GRE == l4_proto)
		{
			++gre_packets;

			return Word();
		}
		else if (IPPROTO_ICMPV6 == l4_proto)
		{
			++icmp6_packets;

			offset += sizeof(icmp6_hdr);
		}
		else if (IPPROTO_FRAGMENT == l4_proto)
		{
			++v6_fragment_packets;

			const ip6_frag* ip_hdr = reinterpret_cast<const ip6_frag*>(packet + offset);
			offset += sizeof(ip6_frag);
			l4_proto = ip_hdr->ip6f_nxt;

			processing = true;
		}
		else if (IPPROTO_IPV6 == l4_proto)
		{
			++ip6_in_ip4_packets;

			const ip6_hdr* ip_hdr = reinterpret_cast<const ip6_hdr*>(packet + offset);
			offset += sizeof(ip6_hdr);
			l4_proto = ip_hdr->ip6_nxt;
		}
		else if (IPPROTO_PIM == l4_proto)
		{
			++pim_packets;

			return Word();
		}
		else
		{
			std::cout << "L4 protocol over IPv4: " << l4_proto << "\n";
			++other_l4_packets;
			// std::cout << std::hex << static_cast<unsigned>(ip_hdr->ip_p) << std::dec << "\n";
			// std::cout << static_cast<unsigned>(ip_hdr->ip_p) << "\n";

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

	// std::cout << std::to_string(payload);

	bool in_aut1;
	bool in_aut2;

	if (prefix_acceptance)
	{
		in_aut1 = is_prfx_in_lang(aut1, payload);
		in_aut2 = is_prfx_in_lang(aut2, payload);
	}
	else
	{
		in_aut1 = is_in_lang(aut1, payload);
		in_aut2 = is_in_lang(aut2, payload);
	}

	if (in_aut1) { ++accepted_aut1; }
	if (in_aut2) { ++accepted_aut2; }

	if (in_aut1 != in_aut2)
	{
		++incons_packets;

		if (in_aut1 && !in_aut2)
		{
			++accepted_aut1_not_aut2;
		}
		else if (in_aut2 && !in_aut1)
		{
			++accepted_aut2_not_aut1;
		}
		else
		{
			assert(false);
		}
	}

	if (total_packets % 10000 == 0)
	{
		std::clog << "#";
		std::clog.flush();
	}
}
