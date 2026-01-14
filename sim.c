#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <getopt.h>

typedef enum p_type_t {
    NONE = 0,
    URLLC = 1,
    EMBB = 2
} p_type;

/**
 * @brief Structure containing simulation parameters for URLLC and EMBB.
 *
 * This structure holds the necessary parameters for simulating the transmission rates and resource blocks
 * for URLLC (Ultra-Reliable Low Latency Communications) and EMBB (Enhanced Mobile Broadband) in a network.
 * 
 * @param mu_e EMBB transmission rate per second
 * @param mu_u URLLC transmission rate per second
 * @param S Total resource blocks available
 * @param G URLLC reserved resource blocks
 * @param max_q Maximum EMBB queue size
 * @param sn_u Number of URLLC UEs
 * @param sn_e Number of EMBB UEs
 */
typedef struct sim_params_t {
    uint32_t mu_e;  // EMBB transmission rate per second
    uint32_t mu_u;  // URLLC transmission rate per second
    uint32_t S;     // Total resource blocks available
    uint32_t G;     // URLLC reserved resource blocks
    uint32_t max_q; // Maximum EMBB queue size
    uint32_t sn_u;  // Number of URLLC UEs
    uint32_t sn_e;  // Number of EMBB UEs
} sim_params;

/**
 * @brief Structure representing a packet with its type and transmission delay in cycles.
 *
 * This structure is used to represent a packet that can be either URLLC or EMBB. Each packet has a type indicating
 * its category and a number of cycles representing its transmission delay.
 *
 * @param type The type of the packet (URLLC or EMBB).
 * @param cycles The number of cycles the packets has been in treatment.
 */
typedef struct packet_t {
    p_type type;    // The type of the packet (URLLC or EMBB)
    uint8_t cycles; // The number of cycles the packets has been in treatment
} packet;

/**
 * @brief Structure representing an EMBB packet in the queue.
 *
 * This structure is used to represent a EMBB packet in a queue, where each EMBB packet contains a pointer to the next EMBB packet
 * and the time spent in the queue.
 *
 * @param next A pointer to the next EMBB packet in the queue.
 * @param time The time the EMBB packet spent in queue.
 */
typedef struct queue_t {
    struct queue_t *next; // A pointer to the next EMBB packet in the queue
    uint32_t time; // The time the EMBB packet spent in queue
} queue;

/**
 * @brief Structure to hold the results of the simulation, including lost, transmitted, and waiting metrics.
 *
 * This structure contains metrics related to the performance of the network simulation, including the number
 * of URLLC and EMBB packets that were lost, transmitted, and the total waiting time for EMBB packets in the queue.
 *
 * @param urllc_lost Number of URLLC packets that were lost
 * @param embb_lost Number of EMBB packets that were lost
 * @param urllc_transmited Number of URLLC packets that were transmitted
 * @param embb_transmited Number of EMBB packets that were transmitted
 * @param embb_leaving_q Number of EMBB packets leaving the queue
 * @param total_wait Total waiting time for EMBB packets in the queue
 */
typedef struct res_sim_t {
    uint32_t urllc_lost;       // Number of URLLC packets that were lost
    uint32_t embb_lost;        // Number of EMBB packets that were lost
    uint32_t urllc_transmited; // Number of URLLC packets that were transmitted
    uint32_t embb_transmited;  // Number of EMBB packets that were transmitted
    uint32_t embb_leaving_q;   // Number of EMBB packets leaving the queue
    uint32_t total_wait;       // Total waiting time for EMBB packets in the queue
} res_sim;

uint32_t urllc_arrival(uint32_t time, uint32_t sn_u) {
    return sn_u/10 + ((time%10 == 0)?sn_u%10:0);
}

uint32_t embb_arrival(uint32_t time, uint32_t sn_e) {
    return sn_e/10 + ((time%10 == 0)?sn_e%10:0);
}

/**
 * @brief Handles the state transitions and processing of packets at each time step.
 *
 * This function takes the current time, simulation parameters, an array of packet servers,
 * a queue, the current state of the system, computes the new state, and updates the results
 * for the simulation. It processes both URLLC (Ultra-Reliable Low Latency Communications)
 * and eMBB (enhanced Mobile Broadband) packets, updating counters for transmissions, losses,
 * and queue waits accordingly.
 *
 * @param time The current time step of the simulation.
 * @param params The simulation parameters.
 * @param servers An array of packet servers.
 * @param q The queue where packets are stored.
 * @param in_state An array representing the current state of the system [URLLC, eMBB, Queue].
 * @param out_state An array to store the new state of the system after processing.
 * @param res A pointer to the results structure to store the simulation results.
 */
void transition(uint32_t time, const sim_params params, packet servers[], queue *q, const uint32_t in_state[3], uint32_t out_state[3], res_sim *res) {
    uint32_t urllc_a = urllc_arrival(time, params.sn_u); // Number of URLLC packets arriving
    uint32_t embb_a = embb_arrival(time, params.sn_e);   // Number of EMBB packets arriving

    uint32_t urllc_e = in_state[0]; // URLLC packets in treatment
    uint32_t urllc_o = 0;           // URLLC packets treated

    uint32_t embb_e = in_state[1]; // EMBB packets in treatment
    uint32_t embb_o = 0;           // EMBB packets treated
    uint32_t embb_q = in_state[2]; // EMBB packets in queue
    uint32_t embb_q_leave = 0;     // EMBB packets leaving queue
    uint32_t embb_q_time = 0;      // Total wait time of EMBB packets in queue

    for (uint32_t i = 0; i < params.S; i++) {
        switch (servers[i].type)
        {
        // If the server is taking care of an URLLC packet, update the state of treatment (cycles) and empty the server if the treatment is finished
        case URLLC:
            servers[i].cycles += 1;
            if (servers[i].cycles >= 10000 / params.mu_u) {
                servers[i].cycles = 0;
                servers[i].type = NONE;
                urllc_o += 1;
            }
            break;

        // If the server is taking care of an EMBB packet, update the state of treatment (cycles) and empty the server if the treatment is finished 
        case EMBB:
            servers[i].cycles += 1;
            if (servers[i].cycles >= 10000 / params.mu_e) {
                servers[i].cycles = 0;
                servers[i].type = NONE;
                embb_o += 1;
            }
            break;

        // If the server is empty, accept a new packet
        case NONE:
        default:
            if (urllc_e < params.G && urllc_a > 0) { // Prioriize URLLC packets while not going over G
                urllc_a -= 1;
                urllc_e += 1;
                servers[i].cycles = 0;
                servers[i].type = URLLC;
            } else if (embb_q > 0) { // Take an EMBB packet from the queue
                embb_q -= 1;
                embb_e += 1;
                servers[i].cycles = 0;
                servers[i].type = EMBB;
                embb_q_leave += 1;
                queue *old = q->next;
                embb_q_time += old->time;
                q->next = old->next;
                free(old);
            } else if (embb_a > 0) { // If there is no packets in the queue, take arrivals directly
                embb_a -= 1;
                embb_e += 1;
                servers[i].cycles = 0;
                servers[i].type = EMBB;
            }
            break;
        }
    }

    // Update time spent in queue for packets in EMBB queue
    queue *cur = q->next;
    queue *last = q;
    while (cur != NULL)
    {
        cur->time += 1;
        last = cur;
        cur = cur->next;
    }

    // If there are new EMBB packets and the queue is not full, put them in the queue
    cur = last;
    while (embb_a > 0 && embb_q < params.max_q) {
        embb_q += 1;
        embb_a -= 1;
        cur->next = malloc(sizeof(queue));
        cur = cur->next;
        cur->time = 0;
        cur->next = NULL;
    }

    out_state[0] = urllc_e - urllc_o;
    out_state[1] = embb_e - embb_o;
    out_state[2] = embb_q;

    res->urllc_lost = urllc_a;
    res->embb_lost = embb_a;
    res->urllc_transmited = urllc_o;
    res->embb_transmited = embb_o;
    res->embb_leaving_q = embb_q_leave;
    res->total_wait = embb_q_time;
}

/**
 * @brief Runs the main simulation loop, processing each time step and updating the results.
 *
 * This function simulates the network over a specified duration, updating the state of the system
 * and accumulating results for each time step. It initializes the system state, allocates memory
 * for packet servers and the queue, and iterates through each time step, calling the `transition`
 * function to process packets and update the state. The results are accumulated in the `sim_results`
 * structure, which is updated with the number of transmissions, losses, and queue waits for URLLC
 * and eMBB packets.
 *
 * @param sim_duration The total duration of the simulation.
 * @param params The simulation parameters.
 * @param sim_results A pointer to the results structure where the simulation results will be stored.
 */
void simulation(uint32_t sim_duration, const sim_params params, res_sim *sim_results) {
    *sim_results = (const res_sim){0};
    uint32_t state[3] = {0,0,0};
    packet *servers = malloc(params.S*sizeof(packet));
    queue *q = malloc(sizeof(queue));

    for (uint32_t time = 0; time < sim_duration; time++) {
        uint32_t new_state[3] = {0,0,0};
        res_sim res = (const res_sim){0};

        transition(time, params, servers, q, state, new_state, &res);

        // Update state for next iteration
        state[0] = new_state[0];
        state[1] = new_state[1];
        state[2] = new_state[2];

        sim_results->embb_leaving_q += res.embb_leaving_q;
        sim_results->embb_lost += res.embb_lost;
        sim_results->embb_transmited += res.embb_transmited;
        sim_results->urllc_lost += res.urllc_lost;
        sim_results->urllc_transmited += res.urllc_transmited;
        sim_results->total_wait += res.total_wait;
    }
    //uint32_t avg_wait = sim_results->total_wait/sim_results->embb_leaving_q;
}

int main(int argc, char *argv[]) {
    // Set default values:
    uint32_t duration = 5*10000; // 5 seconds (time is considered in tenth of millisecond)
    sim_params params = {
        1000,           // embb transmition rate per second
        5000,           // urllc transmition rate per second
        1000,           // S
        100,            // G
        512,            // max queue size
        500,            // urllc ue number
        3000            // embb ue number
    };

    res_sim res;

    int c;
    while (1) {
        static struct option long_options[] = {
            {"embb_rate", required_argument, 0, 'e'},
            {"urllc_rate", required_argument, 0, 'u'},
            {"servers", required_argument, 0, 's'},
            {"max_queue", required_argument, 0, 'm'},
            {"urllc_ue", required_argument, 0, 'r'},
            {"embb_ue", required_argument, 0, 'b'},
            {"duration", required_argument, 0, 'd'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "e:u:s:m:r:b:d:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 'e':
                params.mu_e = atoi(optarg);
                break;
            case 'u':
                params.mu_u = atoi(optarg);
                break;
            case 's':
                params.S = atoi(optarg);
                break;
            case 'm':
                params.max_q = atoi(optarg);
                break;
            case 'r':
                params.sn_u = atoi(optarg);
                break;
            case 'b':
                params.sn_e = atoi(optarg);
                break;
            case 'd':
                duration = atoi(optarg)*10000;
                break;
            default:
                printf("Usage: %s -e <embb_rate> -u <urllc_rate> -s <servers> -m <max_queue> -r <urllc_ue> -b <embb_ue> -d <duration>\n", argv[0]);
                return 1;
        }
    }

    simulation(duration, params, &res);

    if (res.embb_leaving_q == 0) res.embb_leaving_q = 1;
    printf("q: %d\n", res.embb_leaving_q);
    printf("wait %d\n", res.total_wait);
    printf("Average eMBB wait time: %d\n", res.total_wait/res.embb_leaving_q);
    printf("Total eMBB transmited: %d\n", res.embb_transmited);
    printf("Total eMBB lost: %d\n", res.embb_lost);
    printf("Total URLLC transmited: %d\n", res.urllc_transmited);
    printf("Total URLLC lost: %d\n", res.urllc_lost);

    return 0;
}