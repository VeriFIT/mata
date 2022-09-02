// prob-lo-mizer.cc - accepts a deterministic FA 'A' and a PCAP file and
// constructs a probabilistic automaton obtained by assigning transitions in 'A'
// probabilities respecting choices of 'A' on the input from the PCAP file

#include <mata/util.hh>
#include <mata/nfa.hh>

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
u_int16_t tcp_port = 0;
Nfa aut;

std::unordered_map<State, size_t> state_occur_cnt;
std::unordered_map<Trans, size_t> trans_occur_cnt;
std::unordered_map<State, size_t> state_accept_cnt;

void print_usage(const char* prog_name)
{
	std::cout << "usage: " << prog_name << " (--tcp PORT) <aut.mf> <input.pcap>\n";
	std::cout << "\n";
	std::cout << "Accepts a deterministic FA in aut.mf and a PCAP file in\n";
	std::cout << "input.pcap and constructs a probabilistic automaton obtained\n";
	std::cout << "by assigning transitions in aut.mf probabilities respecting\n";
	std::cout << "choices of aut.mf on the input from the PCAP file\n";
	std::cout << "\n";
	std::cout << "Options:\n";
	std::cout << "  --tcp PORT  Consider *only* TCP packets *only* on PORT (any from src or dst)\n";
	std::cout << "\n";
	std::cout << "Parameters:\n";
	std::cout << "  aut.mf     Deterministic FA with the structure to be labelled\n";
	std::cout << "  input.pcap  Input sample\n";
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
	if (argc != 3 && argc != 5)
	{
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (std::to_string(argv[1]) == "--tcp")
	{
		param_start = 3;

		if (argc != 5)
		{
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}

		std::istringstream stream(argv[2]);
		try
		{
			stream >> tcp_port;
		}
		catch (...)
		{
			std::cerr << "Invalid number as PORT provided!\n";
			return EXIT_FAILURE;
		}
	}
	else
	{
		if (argc != 3)
		{
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	// LOADING INPUTS
	std::string aut_file = argv[param_start + 0];
	std::string packets_file = argv[param_start + 1];

	try
	{
		aut = load_aut(aut_file);
	}
	catch (const std::exception& ex)
	{
		std::cerr << "Error loading automata: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}

	if (!is_deterministic(aut))
	{
		std::cerr << "The provided automaton is nondeterministic!\n";
		return EXIT_FAILURE;
	}

	// size_t max_state = 0;
	// for (const auto& st : aut.initialstates) { max_state = std::max({max_state, st}); }
	// for (const auto& st : aut.finalstates) { max_state = std::max({max_state, st}); }
	// for (const auto& trans : aut) { max_state = std::max({max_state, trans.src, trans.tgt}); }

	Mata::Nfa::CharAlphabet alphabet;
	if (!is_complete(aut, alphabet))
	{
		// ++max_state;
		make_complete(&aut, alphabet, -1);
	}

	// std::clog << "# States = " << max_state + 1 << "\n";

	// state_occur_cnt = new size_t[max_state+1];
	// for (size_t i = 0; i <= max_state; ++i)
	// {
	// 	state_occur_cnt[i] = 0;
	// }

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

	// set precision of output
	std::cout.precision(std::numeric_limits<double>::max_digits10);

	std::cout << "@DPA\n";
	std::cout << "%Initial " << *aut.initialstates.begin() << ":1.0\n";
	std::cout << "%Final ";
	for (const auto& st_acc : state_accept_cnt)
	{
		double prod = (static_cast<double>(st_acc.second) / state_occur_cnt[st_acc.first]);
		std::cout << st_acc.first << ":" << prod << " ";
	}
	std::cout << "\n";

	for (const auto& trans_acc : trans_occur_cnt)
	{
		double prob = (static_cast<double>(trans_acc.second) /
			state_occur_cnt[trans_acc.first.src]);
		const Trans& trans = trans_acc.first;
		std::cout << trans.src << " ";
		std::cout << trans.symb << ":" << prob << " ";
		std::cout << trans.tgt << "\n";
	}

	std::clog << "\n";
	std::clog << "Total packets in " << packets_file << ": " << total_packets << "\n";
	std::clog << "Packets with payload: " << payloaded_packets << "\n";
	std::clog << "Time: " <<
		std::chrono::duration_cast<std::chrono::nanoseconds>(opTime).count() * 1e-9
		<< "\n";

	return EXIT_SUCCESS;
}


/**
 * @brief  Retrieves payload from a packet
 *
 * @param[in]  tcp_port  If non-0, specifies a TCP port that will only be
 *                       considered (src or dst)
 */
Word get_payload(
	const pcap_pkthdr*   pkthdr,
	const u_char*        packet,
	u_int16_t            tcp_port)
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

	if (IPPROTO_TCP == l4_proto)
	{
		const tcphdr* tcp_hdr = reinterpret_cast<const tcphdr*>(packet + offset);
		if ((tcp_port != 0) &&
			(ntohs(tcp_hdr->th_sport) != tcp_port) &&
			(ntohs(tcp_hdr->th_dport) != tcp_port))
		{
			return Word();
		}

		size_t tcp_hdr_size = tcp_hdr->th_off * 4;
		offset += tcp_hdr_size;
	}
	else
	{
		return Word();
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

	Word payload = get_payload(pkthdr, packet, tcp_port);
	if (payload.empty()) { return; }

	++payloaded_packets;

	// std::clog << std::to_string(payload) << "\n";
	// for (auto i: payload)
	// {
	// 	std::clog << static_cast<char>(i);
	// }
  //
	// std::clog << "\n";

	StateSet cur = aut.initialstates;
	for (Symbol sym : payload)
	{
		if (cur.size() != 1) { std::abort(); }

		State src = *cur.begin();

		// SAFELY NEGLECTED
		// assert(state_occur_cnt[st] < SIZE_MAX);

		++state_occur_cnt[src];

		cur = aut.post(cur, sym);
		if (cur.size() != 1) { std::abort(); }
		State tgt = *cur.begin();

		Trans trans(src, sym, tgt);
		++trans_occur_cnt[trans];
	}

	State st = *cur.begin();
	++state_accept_cnt[st];

	if (total_packets % 10000 == 0)
	{
		std::clog << "#";
		std::clog.flush();
	}
}
