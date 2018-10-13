/*
 * Boolean Network Simulator
 * Author:	Caleb Baker
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <queue>
#include <unordered_set>
#include <random>
#include <unordered_map>
#include <string.h>
#include <list>

#define UINT_64 unsigned long long

#define VERBOSE
#define TRUTH_TABLE
#define SHOW_SETUP
// #define SHOW_TIMES
#define STATE_NUMBERS

#ifdef TRUTH_TABLE
	#define SHOW_INITIAL_STATE
#endif


#ifdef SHOW_SETUP
	#define SHOW_INITIAL_STATE
#endif


struct listItem {
	listItem(UINT_64 a) {
		length = a;
		leftToCheck = a << 2;
		beenChecked = true;
	}
	UINT_64 length;
	UINT_64 leftToCheck;
	bool beenChecked;
};


// Big integer to represent state numbers
class bigInt{
	public:

		bigInt() {
			num = (UINT_64*) calloc(scale, sizeof(UINT_64));
		}

//		~bigInt() {
//			free(num);
//		}

		void leftShift(bool newBit) {
			for (unsigned i = scale - 1; i > 0; i--) {
				num[i] <<= 1;
				num[i] |= ((num[i-1] & 0x8000000000000000) >> 63);
			}
			num[0] <<= 1;
			num[0] |= (int) newBit;
		}

		void print() {
			for (int i = scale - 1; i >= 0; i--) {
				printf("%016llx", num[i]);
			}
			printf("\n");
		}

		bool operator ==(const bigInt & obj) const {
			for (unsigned i = 0; i < scale; i++) {
				if (num[i] != obj.num[i]) {
					return false;
				}
			}
			return true;
		}

		UINT_64 *num;
		static unsigned scale;
};

unsigned bigInt::scale;

struct bigIntHasher {
	size_t operator()(const bigInt & obj) const {
		size_t h = (size_t) obj.num[0];
		for (unsigned i = 1; i < bigInt::scale; i++) {
			h ^= obj.num[i];
		}
		return h;
	}
};


// n is the number of nodes in the network
// k is the number of inputs each node has
// s and l determine the gate delay of the nodes
// t is the maximum simulation time.
// scale is n / 64 + 1
unsigned n, k;
double s, l, t;


// Class to represent nodes in the network
class gate {
	public:
		std::vector<gate*> output;	// Gates affected by this one
		gate **input;				// Gates that affect this one
		bool *table;			// Truth table of defining boolean function
		double delay;			// Gate delay
		bool state;				// Current state of the node

		// Apply the nodes boolean function to its inputs
		bool logic() {
			UINT_64 in = 0;
			for (unsigned i = 0; i < k; i++) {
				in <<= 1;
				in |= input[i]->state;
			}
			return table[in];
		}
};


// Class to represent events in the simulation
class Event {
	public:
		double time;		// the time at which the event happens
		gate *changingGate;	// the gate affected by the event
		bool newState;		// the state that the affected gate changes to

		// Constructor
		Event(double t,  gate *g, bool s) {
			time = t;
			changingGate = g;
			newState = s;
		}

		// Comparison operator
		friend bool operator<(const Event &l, const Event &r) {
			return l.time > r.time;
		}
};

// All of the gates
gate *genes;

// Get the state number for the simulation
bigInt stateNumber() {
	bigInt x;
	for (unsigned i = 0; i < n; i++) {
		x.leftShift(genes[i].state);
	}
	return x;
}


// Get a random integer between 0 and max inclusive
unsigned random(unsigned max) {
	max++;
	unsigned randLimit = RAND_MAX - ((unsigned)RAND_MAX + 1) % max;
	unsigned x;
	while ((x = rand()) > randLimit);
	return x % max;
}


int main(int argc, char **argv) {

	// Seed random stuff
	srand(time(0));
	std::random_device rd;
	std::mt19937 gen(rd());

	// Make sure the proper input is given
	printf("\n");
	if (argc < 6) {
		printf("Ussage:\nnetworkSimulator N K S L T\n\n");
		printf("N:\tnumber of nodes in the network.\n");
		printf("K:\tnumber of inputs into each node.\n");
		printf("S:\tminimum gate delay\n");
		printf("L:\tmaximum gate delay\n");
		printf("T:\tmaximum simulation duration\n\n");
		return 1;
	}

	// Parse input
	sscanf(argv[1], "%u", &n);
	sscanf(argv[2], "%u", &k);
	sscanf(argv[3], "%lf", &s);
	sscanf(argv[4], "%lf", &l);
	sscanf(argv[5], "%lf", &t);
	bigInt::scale = n / 64;
	if (n | 0x3f) {	// if (n % 64 != 0) {
		bigInt::scale++;
	}

	//Make sure values make sense and aren't too big
	if (k >= n) {
		printf("The number of inputs to a node must be less than the number of nodes.\n");
		return 2;
	}
	if (k > 64) {
		printf("The number of inputs to a node cannot exceed 64.\n");
		return 2;
	}

	// Random number generators for gate delays
	std::uniform_real_distribution<> uniform(s, l);
	std::normal_distribution<double> normal(s, l);

	// Determine whether a normal or uniform distribution is used for delays
	bool norm = false;
	if (argc > 6 && argv[6][0] == 'n') {
		norm = true;
	}
	else if (s > l) {
		printf("The minimum gate delay cannot exceed the maximum gate delay.\n");
		return 3;
	}

	genes = (gate*) malloc(n * sizeof(gate));

	// Initialize gates
	for (unsigned i = 0; i < n; i++) {
		gate *curr = genes + i;

		// Initialize input and output
		gate **in = (gate**) malloc(k * sizeof(gate*));
		curr->input = in;
		for (unsigned j = 0; j < k; j++) {
			unsigned inGate = random(n - 1);
			while (inGate == i) {
				inGate = random(n - 1);
			}
			in[j] = genes + inGate;
			genes[inGate].output.push_back(curr);

			#ifdef SHOW_SETUP
				printf("Gate %u influenced by gate %u\n", i, inGate);
			#endif
		}

		// Initialize truth table
		UINT_64 tableSize = 1 << k;
		bool *tab = (bool*) malloc(tableSize * sizeof(bool*));
		curr->table = tab;
		#ifdef TRUTH_TABLE
			printf("\nGate %u truth table:\n\t", i);
		#endif
		for (UINT_64 i = 0; i < tableSize; i++) {
			tab[i] = (bool)random(1);
			#ifdef TRUTH_TABLE
				if (tab[i]) {
					printf("1");
				}
				else {
					printf("0");
				}
			#endif
		}
		#ifdef TRUTH_TABLE
			printf("\n");
		#endif

		// Generate random delay
		if (norm) {
			curr->delay = normal(gen);
			while (curr->delay <= 0) {
				curr->delay = normal(gen);
			}
		}
		else {
			curr->delay = uniform(gen);
		}

		#ifdef SHOW_SETUP
			printf("\nGate %u has a delay of %lf\n\n", i, curr->delay);
		#endif

		curr->state = (bool)random(1);
	}

	std::priority_queue<Event> events;

	// Initialize event queue
	for (unsigned i = 0; i < n; i++) {
		gate *curr = genes + i;
		bool newValue = curr->logic();
		if (newValue != curr->state) {
			events.push(Event(curr->delay, curr, newValue));
		}
		#ifdef SHOW_INITIAL_STATE
			printf("Gate %u starting at %u\n", i, (unsigned)curr->state);
		#endif
	}

	std::list<listItem> suspects;
	std::unordered_multimap<bigInt, UINT_64, bigIntHasher> past;
	UINT_64 timeDiscrete = 0;
	bigInt sn = stateNumber();
	past.insert(std::pair<bigInt, UINT_64>(sn, timeDiscrete));

#ifdef STATE_NUMBERS
	printf("0.000000:\t");
	sn.print();
#endif


	double currentTime = 0.0;

	// Simulate!!!!
	while (!events.empty() && currentTime < t) {

		// Get all current events
		std::vector<Event> currentEvents;
		currentEvents.push_back(events.top());
		events.pop();
		currentTime = currentEvents[0].time;
		while (!events.empty() && events.top().time == currentTime) {
			currentEvents.push_back(events.top());
			events.pop();
		}
		
		bool changed = false;

		// Change gates and get a set of gates affected by changes
		std::unordered_set<gate*> affectedGates;
		for (UINT_64 i = 0; i < currentEvents.size(); i++) {
			Event *e = &currentEvents[i];
			gate *curr = e->changingGate;
			if (curr->state != e->newState) {
				changed = true;
				curr->state = e->newState;
				for (unsigned j = 0; j < curr->output.size(); j++) {
					affectedGates.insert(curr->output[j]);
				}
			#ifdef VERBOSE
				printf("Gate %u changes to %u at time %lf\n", (unsigned)(curr - genes), (unsigned)curr->state, currentTime);
			#endif
			}

		}

		// Create new events for affected gates
		for (auto x : affectedGates) {
			bool newValue = x->logic();
			events.push(Event(currentTime + x->delay, x, newValue));
		}

		// Check for cycles
		if (changed) {
			bigInt stateNum = stateNumber();
			#ifdef STATE_NUMBERS
				printf("%lf:\t", currentTime);
				stateNum.print();
			#endif
			timeDiscrete++;
			auto collide = past.equal_range(stateNum);
			for (auto it = collide.first; it != collide.second; ++it) {
				UINT_64 len = timeDiscrete - it->second;
				for (auto i = suspects.begin(); i != suspects.end(); ++i) {
					if (i->length == len) {
						i->leftToCheck--;
						i->beenChecked = true;
						if (i->leftToCheck == 0) {
							printf("Cycle detected of length %llu\n", i->length);
							printf("Run-in of %llu\n", timeDiscrete - 5 * i->length);
							return 0;
						}
					}
				}
				suspects.push_front(listItem(len));
			}
			for (auto i = suspects.begin(); i != suspects.end(); ++i) {
				if (!i->beenChecked) {
					i = suspects.erase(i);
				}
				else {
					i->beenChecked = false;
				}
			}
			past.insert(std::pair<bigInt, UINT_64>(stateNum, timeDiscrete));
		}

		#ifdef SHOW_TIMES
				printf("%lf\n", currentTime);
		#endif

	}

	printf("\n");

	if (events.empty()) {
		printf("The event queue is empty.\n\n");
		printf("Number of events: %llu\n", timeDiscrete);
	}
	printf("bye\n");
	return 0;
}


