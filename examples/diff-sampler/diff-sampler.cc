// diff-sampler - gets a pair of NFAs A1 and A2 and a set of network packets P
// in the pcap format, and tests how many packets from P lie in the symmetric
// difference of the languages of A1 and A2

#include <vata2/util.hh>
#include <vata2/nfa.hh>

#include <chrono>
#include <iostream>
#include <fstream>

// PCAP-related headers
#include <pcap.h>
#include <net/ethernet.h>

using namespace Vata2::Nfa;
using namespace Vata2::Parser;

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

// GLOBAL VARIABLES
size_t total_packets = 0;
size_t incons_packets = 0;
size_t accepted_aut1 = 0;
size_t accepted_aut2 = 0;
Nfa aut1;
Nfa aut2;

// FUNCTION DECLARATIONS
void packetHandler(u_char *userData, const pcap_pkthdr* pkthdr, const u_char* packet);


void print_usage(const char* prog_name)
{
	std::cout << "usage: " << prog_name << " aut1.vtf aut2.vtf packets.pcap\n";
}

Nfa load_aut(const std::string& file_name)
{
	Nfa result;
	std::ifstream input(file_name);
	if (input.is_open())
	{
		ParsedSection parsec = parse_vtf_section(input);
		return construct(parsec);
	}
	else
	{
		throw std::runtime_error("Cannot open file " + file_name);
	}
}

int main(int argc, char** argv)
{
	if (argc != 4)
	{
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	std::string aut1_file = argv[1];
	std::string aut2_file = argv[2];
	std::string packets_file = argv[3];

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

	std::cout << "aut1:\n";
	std::cout << std::to_string(aut1);
	std::cout << "===================================\n";
	std::cout << "aut2:\n";
	std::cout << std::to_string(aut2);
	std::cout << "===================================\n";

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
	if (pcap_loop(descr, 0, packetHandler, nullptr) < 0) {
		std::cout << "pcap_loop() failed: " << pcap_geterr(descr);
		return EXIT_FAILURE;
	}

	TimePoint finishTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> opTime = finishTime - startTime;

	std::cout << "\n";
	std::cout << "Total packets in " << packets_file << ": " << total_packets << "\n";
	std::cout << "Accepted in Aut1: " << accepted_aut1 << "\n";
	std::cout << "Accepted in Aut2: " << accepted_aut2 << "\n";
	std::cout << "Inconsistent packets: " << incons_packets << "\n";
	std::cout << "Time: " <<
		std::chrono::duration_cast<std::chrono::nanoseconds>(opTime).count() * 1e-9
		<< "\n";

	return EXIT_SUCCESS;
}

void packetHandler(
	u_char* /* userData */,
	const pcap_pkthdr* pkthdr,
	const u_char* packet)
{
	assert(nullptr != pkthdr);
	assert(nullptr != packet);

	++total_packets;

	// THIS SHOULD BE FINISHED!!!!!!!!!!
	// const ether_header* eth_hdr = reinterpret_cast<const ether_header*>(packet);
	// assert(nullptr != eth_hdr);

	// ANYWAY, just throw the packet into the automata
	Word payload(packet, packet + pkthdr->len);

	bool in_aut1 = is_in_lang(aut1, payload);
	bool in_aut2 = is_in_lang(aut2, payload);

	if (in_aut1) { ++accepted_aut1; }
	if (in_aut2) { ++accepted_aut2; }

	if (in_aut1 != in_aut2)
	{
		++incons_packets;
	}

	if (total_packets % 1000 == 0)
	{
		std::cout << "#";
		std::cout.flush();
	}
}
