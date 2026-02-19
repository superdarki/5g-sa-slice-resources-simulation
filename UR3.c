#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define NB_SIM 50000
#define NB_PROCESS 64

#define START 0
#define END 1250
#define STEP 5

int lambda_u = 500;
double mu = 1e0;
double NbIter = 5e4;
double seuil = 1e-5;

struct res_sim
{
    double loss;
    double wait_avg;
    double wait_max;
    double urllc_tot;
    double urllc_max;
    double embb_tot;
};

// Define the transition function
void transition(double lambda_e, double lambda_u, double mu, int S, double G, int x1, int x2, int x3, double *duree, int etat[3])
{
    int etats[5][3];
    double taux[5];
    int count = 0;

    if (x1 > 0)
    {
        etats[count][0] = x1 - 1;
        etats[count][1] = x2;
        etats[count][2] = x3;
        taux[count] = 2 * mu * x1;
        count++;
    }

    if (x2 > 0)
    {
        if ((x1 + x2 <= S - G) && x3 > 0)
        {
            etats[count][0] = x1;
            etats[count][1] = x2;
            etats[count][2] = x3 - 1;
        }
        else
        {
            etats[count][0] = x1;
            etats[count][1] = x2 - 1;
            etats[count][2] = x3;
        }
        taux[count] = mu * x2;
        count++;
    }

    if (x1 + x2 < S)
    {
        etats[count][0] = x1 + 1;
        etats[count][1] = x2;
        etats[count][2] = x3;
        taux[count] = lambda_u;
        count++;
    }

    if (x1 + x2 < S - G)
    {
        etats[count][0] = x1;
        etats[count][1] = x2 + 1;
        etats[count][2] = x3;
        taux[count] = lambda_e;
    }
    else
    {
        etats[count][0] = x1;
        etats[count][1] = x2;
        etats[count][2] = x3 + 1;
        taux[count] = lambda_e;
    }
    count++;

    double param_expo = 0.0;
    for (int i = 0; i < count; i++)
    {
        param_expo += taux[i];
    }

    *duree = -1.0 / param_expo * log((double)rand() / RAND_MAX);

    double cumulative_sum = 0.0;
    double u = (double)rand() / RAND_MAX;
    int index = 0;
    for (int i = 0; i < count; i++)
    {
        cumulative_sum += taux[i] / param_expo;
        if (u <= cumulative_sum)
        {
            index = i;
            break;
        }
    }

    etat[0] = etats[index][0];
    etat[1] = etats[index][1];
    etat[2] = etats[index][2];
}

void simu(double lambda_e, double lambda_u, double mu, int S, double G, double NbIter, struct res_sim *res)
{
    int e[3] = {0, 0, 0};
    double cumul = 0.0;
    double temps_total = 0.0;
    double horizon = NbIter / (lambda_e + lambda_u);
    double t = 0.0;
    double wait_avg = 0.0;
    double wait_max = 0.0;
    double urllc_tot = 0.0;
    double urllc_max = 0.0;
    double embb_tot = 0.0;

    while (temps_total < horizon)
    {
        t = 0.0;
        int e_new[3] = {0, 0, 0};
        transition(lambda_e, lambda_u, mu, S, G, e[0], e[1], e[2], &t, e_new);
        temps_total += t;
        wait_avg += e[2] * t;
        wait_max = (wait_max > e[2]) ? wait_max : e[2];
        if (e_new[0] > e[0])
        {
            urllc_tot += 1;
        }
        if (e_new[0] > urllc_max)
        {
            urllc_max = e_new[0];
        }
        if (e_new[1] > e[1])
        {
            embb_tot += 1;
        }
        if (e[0] + e[1] == S)
        {
            cumul += t;
        }
        e[0] = e_new[0];
        e[1] = e_new[1];
        e[2] = e_new[2];
    }

    if (e[0] + e[1] == S)
    {
        cumul += t - (temps_total - horizon);
    }

    res->loss = cumul / horizon;
    res->wait_avg = wait_avg / horizon;
    res->wait_max = wait_max;
    res->urllc_tot = urllc_tot;
    res->urllc_max = urllc_max;
    res->embb_tot = embb_tot;
}

void show_progress_bar(int completed, int total)
{
    int bar_width = 50; // Width of the progress bar
    float progress = (float)completed / total;
    int pos = (int)(bar_width * progress);

    printf("[");
    for (int i = 0; i < bar_width; i++)
    {
        if (i < pos)
            printf("=");
        else if (i == pos)
            printf(">");
        else
            printf(" ");
    }
    printf("] %d/%d   \r", completed, total);
    fflush(stdout);
}

int valeur_canaux_garde_1(double lambda_e, double lambda_u, double mu, int S, double NbIter, double seuil, struct res_sim *res_mean)
{
    int G = -1;
    double a = 1.0;

    while (a > seuil)
    {
        G++;
        *res_mean = (struct res_sim){0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

        for (int i = 0; i < NB_SIM; i++)
        {
            struct res_sim res;
            simu(lambda_e, lambda_u, mu, S, G, NbIter, &res);
            res_mean->loss += res.loss;
            res_mean->wait_avg += res.wait_avg;
            res_mean->wait_max += res.wait_max;
            res_mean->urllc_tot += res.urllc_tot;
            res_mean->urllc_max += res.urllc_max;
            res_mean->embb_tot += res.embb_tot;
        }

        res_mean->loss /= (double)NB_SIM;
        res_mean->wait_avg /= (double)NB_SIM;
        res_mean->wait_max /= (double)NB_SIM;
        res_mean->urllc_tot /= (double)NB_SIM;
        res_mean->urllc_max /= (double)NB_SIM;
        res_mean->embb_tot /= (double)NB_SIM;

        a = res_mean->loss;
    }

    return G;
}

// Main function to run the simulation
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("One arg is required!\n");
        return 1;
    }

    srand(time(NULL));

    int S = atoi(argv[1]); // Get value of S from command-line argument

    time_t start_time, end_time;
    double time_spent;

    // Record the start time
    start_time = time(NULL);

    printf("lambda_u: %d\n", lambda_u);
    printf("mu: %.2f\n", mu);
    printf("S: %d\n", S);
    printf("Number of iterations: %.2f\n", NbIter);
    printf("Loss limit: %.5f\n", seuil);

    // Calculate the number of steps
    int num_steps = (END - START) / STEP + 1;

    // Allocate memory for results, but only for STEP-th values
    double *R = mmap(NULL, num_steps * sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    struct res_sim *res = mmap(NULL, num_steps * sizeof(struct res_sim), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int *progress = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (R == MAP_FAILED || progress == MAP_FAILED || res == MAP_FAILED)
    {
        perror("mmap failed");
        return 1;
    }

    *progress = 0; // Initialize progress counter

    show_progress_bar(0, num_steps);

    double horizon[num_steps];

    // Fork processes and do work in parallel
    for (int p = 0; p < NB_PROCESS; p++)
    {
        pid_t pid = fork();

        if (pid == 0)
        { // Child process
            for (int i = START + p * STEP; i <= END; i += NB_PROCESS * STEP)
            {
                int index = (i - START) / STEP; // Calculate the index in the reduced array
                double lambda_e = i * 1.0;
                horizon[index] = NbIter / (lambda_e + lambda_u);

                struct res_sim res_temp = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
                R[index] = valeur_canaux_garde_1(lambda_e, lambda_u, mu, S, NbIter, seuil, &res_temp);
                res[index] = res_temp;

                __sync_fetch_and_add(progress, 1); // Atomically increment progress
                printf("E=%d, G=%f, L=%f, U=%f, T=%f, B=%f, A=%f, M=%f, H=%f,\n", i, R[index], res[index].loss, res[index].urllc_tot, res[index].urllc_max, res[index].embb_tot, res[index].wait_avg, res[index].wait_max, horizon[index]);
            }

            // Child process exits after its work is done
            exit(0);
        }
    }

    int prev_progress = 0;
    while (prev_progress < num_steps)
    {
        usleep(100000); // Sleep for a short time (100ms)
        if (*progress != prev_progress)
        {
            prev_progress = *progress;
            show_progress_bar(prev_progress, num_steps); // Update progress bar
        }
    }

    printf("\n");

    // Parent process waits for all children to finish
    for (int p = 0; p < NB_PROCESS; p++)
    {
        wait(NULL);
    }

    // Record the end time
    end_time = time(NULL);
    time_spent = difftime(end_time, start_time);

    // File name based on the value of S
    char filename[50];
    sprintf(filename, "S(%d).csv", S);

    // Open the CSV file for writing
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        printf("Error opening file!\n");
        return 1;
    }

    // Convert time spent to hours, minutes, and seconds
    int hours = (int)time_spent / 3600;
    int minutes = ((int)time_spent % 3600) / 60;
    int seconds = (int)time_spent % 60;

    // Write the header for the CSV file
    fprintf(file, "E;G;LoadE;PerG;Loss;WaitAvg;WaitMax;URLLC_Tot;URLLC_Max;eMBB_Tot;Horizon;;# %d hrs %d mins %d s\n", hours, minutes, seconds);
    fflush(file);

    for (int i = START; i <= END; i += STEP)
    {
        int index = (i - START) / STEP;
        double G = R[index];                         // Get the value of G from shared memory
        double LoadE = i / (mu * ((double)(S - G))); // Calculate LoadE as E/mu*(S-G)
        double PerG = (G / (double)S) * 100.0;       // Calculate PerG as (G/S)*100
        double horizon = NbIter / (i + lambda_u);    // Calculate Horizon

        // Extract values from the res_sim struct
        double loss = res[index].loss;
        double wait_avg = res[index].wait_avg;
        double wait_max = res[index].wait_max;
        double urllc_tot = res[index].urllc_tot;
        double urllc_max = res[index].urllc_max;
        double embb_tot = res[index].embb_tot;

        // Write all the values to the file
        fprintf(file, "%d;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;\n", i, G, LoadE, PerG, loss, wait_avg, wait_max, urllc_tot, urllc_max, embb_tot, horizon);
        fflush(file); // Ensure the data is written to the file
    }

    printf("Time: %d hrs %d mins %d s\n", hours, minutes, seconds);

    // Close the CSV file
    fclose(file);

    // Clean up shared memory
    munmap(R, num_steps * sizeof(double));
    munmap(res, num_steps * sizeof(struct res_sim));
    munmap(progress, sizeof(int));

    return 0;
}
