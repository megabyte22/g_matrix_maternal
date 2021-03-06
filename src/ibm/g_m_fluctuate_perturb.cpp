//
// evolving G matrix in fluctuating environments


#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <cassert>
#include <string.h>

// random number generation
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

// various functions, such as unique filename creation
#include "bramauxiliary.h"

//#define NDEBUG
//
// the compilation sign should only be turned on when one wants
// to assess the complete distribution of phenotypes

using namespace std;


///////////////////     PARAMS      ///////////////////

// number of generations
const int NumGen = 50000;

// population size
const int Npop = 3000; 

// number of generations to skip when outputting data
const int skip = 10;

// number of loci
const int n_loci_g = 50;

// mutational variance trait 1, 2
double a1 = 0;
double a2 = 0;

// mutational correlation
double r_mu = 0;

// mutation rate conventional gene loci
double mu = 0;

// mutation rate maternal effects loci
double mu_m = 0;
double sdmu_m = 0;

// initial number of generations without change 
int burnin = 5000;

// strengths of selection
double omega[2][2] = {{0,0},{0,0}};

// strength of correlated selection
double r_omega = 0;

// optima 
double theta1 = 0;
double theta2 = 0;

double ampl1  = 0;
double ampl2  = 0;
double stoch1 = 0;
double stoch2 = 0;
double freq1  = 0;
double freq2  = 0;
double shift = 0;
double int1= 0;
double int2= 0;
double int1ptb= 0;
double int2ptb= 0;

size_t B = 10;

///////////////////     STATS       ///////////////////

// track number of individuals 
size_t Nm = 0;
size_t Nf = 0;
size_t NKids = 0;

// printing the covariance between z1 and z2 for offspring
// to have a gist of what is wrong with this model
double meancov = 0;

// mean fitness
double meanw = 0;

// variables to calculate deltas
double Gtmin1[2][2] = {{0,0},{0,0}};
double max_delta_G[2][2] = {{0,0},{0,0}};
double min_delta_G[2][2] = {{0,0},{0,0}};

double ev1_tmin1 = 0;
double ev2_tmin1 = 0;

double ecc_tmin1 = 0;
double size_tmin1 = 0;

double angleLead_tmin1 = 0;
double angleSecond_tmin1 = 0;

// indicator variable if we are printing stats for this generation
bool do_stats = 0;

// keep track of the current generation number
int generation = 0;

// random seed
unsigned seed = 0;

// gnu scientific library random number generator initialization
// http://www.gnu.org/software/gsl/ 
gsl_rng_type const * T; // gnu scientific library rng type
gsl_rng *r; // gnu scientific rng 

// the individual struct
struct Individual
{
    // the trait alleles are given by z_{ijk}
    // 0<i<=n_loci_g: the number of unlinked gene loci 
    // j: the number of independent traits (2 in this case)
    // k: ploidy (in this case diploid)
    double g[2][n_loci_g][2]; 

    double m[2][2][2];

    double phen[2];
    double gen[2];

};

// allocate a population and a population of survivors
typedef Individual Population[Npop];
typedef Individual NewPopulation[Npop*2*20];
Population Males, Females;
NewPopulation NewPop;

double distMales[Npop];
double distFemales[Npop];
double sum_dist_males = 0;
double sum_dist_females = 0;

// generate a unique filename for the output file
string filename("sim_Gmatrix");
string filename_new(create_filename(filename));
ofstream DataFile(filename_new.c_str());  

#ifdef DISTRIBUTION
// generate a filename for the phenotype distribution file
string filename_new2(create_filename("sim_evolving_m_dist"));
ofstream distfile(filename_new2.c_str());
#endif //DISTRIBUTION

// functions to check ram etc
// see http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process 
int parseLine(char* line){
    int i = strlen(line);
    while (*line < '0' || *line > '9') line++;
    line[i-3] = '\0';
    i = atoi(line);
    return i;
}

int getValue(){ //Note: this value is in KB!
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];


    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result;
}

// auxiliary function for reflexive boundaries of the correlation
// coefficients (p. 1858 in Revell 2007)
void boundary(double &r_x)
{
    while(true)
    {
        if (r_x < -1.0)
        {
            r_x = r_x + fabs(r_x + 1.0);
        } 
        else if (r_x > 1.0)
        {
            r_x = r_x - (r_x - 1.0);
        }
        else
        {
            break;
        }
    }
}


// bivariate Gaussian fitness function as in Jones et al 2003 with theta_i 
double v(double const z1, double const z2)
{
    return(
         exp(-.5 * ( 
             (z1 - theta1) * (z1 - theta1) * omega[1][1] 
             - 2 * omega[0][1] * (z1 - theta1) * (z2 - theta2)
             + (z2 - theta2) * (z2 - theta2) * omega[0][0] 
            )/ (omega[0][0] * omega[1][1] - omega[1][0] * omega[1][0])
         ));
}


// initialize simulations from command line arguments
void initArguments(int argc, char *argv[])
{
    a1 = atof(argv[1]);
    a2 = atof(argv[2]);
    r_mu = atof(argv[3]);
    mu = atof(argv[4]);
    mu_m = atof(argv[5]);
    sdmu_m = atof(argv[6]);
    omega[0][0] = atof(argv[7]);
    omega[1][1] = atof(argv[8]);
    r_omega = atof(argv[9]);
    ampl1 = atof(argv[10]);
    ampl2 = atof(argv[11]);
    stoch1 = atof(argv[12]);
    stoch2 = atof(argv[13]);
    freq1 = atof(argv[14]);
    freq2 = atof(argv[15]);
    shift = atof(argv[16]);
    int1 = atof(argv[17]);
    int2 = atof(argv[18]);
    int1ptb = atof(argv[19]);
    int2ptb = atof(argv[20]);

    omega[1][0] = omega[0][1] = r_omega * sqrt(omega[0][0] * omega[1][1]);

    double test_r = 5;
    boundary(test_r);

    assert(test_r >= -1.0 && test_r <= 1.0);
    
    test_r = -5;
    boundary(test_r);
    assert(test_r >= -1.0 && test_r <= 1.0);
}


// mutation according to a continuum of alleles model
// see description in Jones et al 2012 J Evol Biol 
//
// "During the progeny-production phase, we choose
// gametes at random to be affected by mutation. We
// assume a uniform per-locus mutation rate of l. We
// draw a pseudorandom number between 0 and 1 from a
// uniform distribution and assume that a gamete carries a
// new mutation if the random number is less than nloci * mu 
void MutateG(Individual &ind)
{
    // random deviates from bivariate gaussian dist
    double a = 0;
    double b = 0;

    for (size_t locus_i = 0; locus_i < n_loci_g; ++locus_i)
    {
        // mutation in each locus occurs with probability mu
        if (gsl_rng_uniform(r) < mu)
        {
            // generate new allelic increments a,b by drawing them
            // from a bivariate gaussian distribution with std deviations a1, a2
            // and 
            gsl_ran_bivariate_gaussian(r, a1, a2, r_mu, &a, &b);

            // add the new allelic effect to trait one
            ind.g[0][locus_i][0] += a;
            // add the new allelic effect to trait two
            ind.g[1][locus_i][0] += b;

            ind.gen[0] += a;
            ind.gen[1] += b;
        }

        // the other genome copy (diploidy)
        if (gsl_rng_uniform(r) < mu)
        {
            // generate new allelic increments a,b by drawing them
            // from a bivariate gaussian distribution with std deviations a1, a2
            // and 
            gsl_ran_bivariate_gaussian(r, a1, a2, r_mu, &a, &b);

            // add the new allelic effect to trait one
            ind.g[0][locus_i][1] += a;
            // add the new allelic effect to trait two
            ind.g[1][locus_i][1] += b;

            ind.gen[0] += a;
            ind.gen[1] += b;
        }
    }
}

// mutate the maternal effects matrix
double MutateM(double m)
{
    if (gsl_rng_uniform(r) < mu_m)
    {
        m += gsl_ran_gaussian(r, sdmu_m);
    }

    return(m);
}

// write the parameters (typically at the end of the output file)
void WriteParameters()
{
	DataFile << endl
		<< endl 
        << "npop;" << Npop << endl
        << "seed;" << seed << endl
        << "nloci_g;" << n_loci_g << endl
        << "a1;" << a1 << endl 
        << "a2;" << a2 << endl
        << "r_mu;" << r_mu << endl
        << "mu;" << mu << endl
        << "mu_m;" << mu_m << endl
        << "sdmu_m;" << sdmu_m << endl
        << "omega_11;" << omega[0][0] << endl
        << "omega_22;" << omega[1][1] << endl
        << "r_omega;" << r_omega << endl
        << "ampl1;" << ampl1 << endl
        << "ampl2;" << ampl2 << endl
        << "int1;" << int1 << endl
        << "int2;" << int2 << endl
        << "int1ptb;" << int1ptb << endl
        << "int2ptb;" << int2ptb << endl
        << "stoch1;" << stoch1 << endl
        << "stoch2;" << stoch2 << endl
        << "int1;" << int1 << endl
        << "int2;" << int2 << endl
        << "int1ptb;" << int1ptb << endl
        << "int2ptb;" << int2ptb << endl
        << "shift;" << shift << endl
        << "freq1;" << freq1 << endl
        << "freq2;" << freq2 << endl; 
}

// initialize the simulation
// by giving all the individuals 
// genotypic values
//
// and doing some other stuff (e.g., random seed)
void Init()
{
    // get the timestamp (with nanosecs)
    // to initialize the seed
	seed = get_nanoseconds();
    
    // set up the random number generators
    // (from the gnu gsl library)
    gsl_rng_env_setup();
    T = gsl_rng_default;
    r = gsl_rng_alloc(T);
    gsl_rng_set(r, seed);


    sum_dist_males = 0;
    sum_dist_females = 0;

	// initialize the whole population
	for (size_t i = 0; i < Npop/2; ++i)
	{
        // loop through the different characters
        for (size_t trait_i = 0; trait_i < 2; ++trait_i)
        {
            Males[i].phen[trait_i] = gsl_rng_uniform(r) * 0.2;
            Males[i].gen[trait_i] = 0;
            Females[i].phen[trait_i] = gsl_rng_uniform(r) * 0.2;
            Females[i].gen[trait_i] = 0;
                        
            // loop through each of the loci
            for (size_t j = 0; j < n_loci_g; ++j)
            {
                // loop through each of the alleles
                for (size_t k = 0; k < 2; ++k)
                {
                    Males[i].g[trait_i][j][k] = 0;
                    Females[i].g[trait_i][j][k] = 0;
                }
            }

            distMales[i] = sum_dist_males + v(
                    Males[i].phen[0], 
                    Males[i].phen[1]
            );

            sum_dist_males = distMales[i];

            distFemales[i] = sum_dist_females + v(
                    Females[i].phen[0], 
                    Females[i].phen[1]
            );

            sum_dist_females = distFemales[i];

        }
	}

    Nf = Npop/2;
    Nm = Npop/2;
}

// create an offspring
void Create_Kid(size_t const mother, size_t const father, Individual &kid)
{
    // copy mother and father, preventing many array lookups
    Individual Mother = Females[mother];
    Individual Father = Males[father];

    // reset the total genetic values corresponding to each trait to 0
    kid.gen[0] = 0;
    kid.gen[1] = 0;

    // inherit 'normal' gene loci
    // loop through all the loci
    for (size_t i = 0; i < n_loci_g; ++i)
    {
        if (gsl_rng_uniform(r) < 0.5)
        {
            kid.g[0][i][0] = Mother.g[0][i][0];
            kid.gen[0] += kid.g[0][i][0];
            
            kid.g[1][i][0] = Mother.g[1][i][0];
            kid.gen[1] += kid.g[1][i][0];
        }
        else
        {
            kid.g[0][i][0] = Mother.g[0][i][1];
            kid.gen[0] += kid.g[0][i][0];
            
            kid.g[1][i][0] = Mother.g[1][i][1];
            kid.gen[1] += kid.g[1][i][0];
        }
        
        
        if (gsl_rng_uniform(r) < 0.5)
        {
            kid.g[0][i][1] = Father.g[0][i][0];
            kid.gen[0] += kid.g[0][i][1];
            
            kid.g[1][i][1] = Father.g[1][i][0];
            kid.gen[1] += kid.g[1][i][1];
        }
        else
        {
            kid.g[0][i][1] = Father.g[0][i][1];
            kid.gen[0] += kid.g[0][i][1];
            
            kid.g[1][i][1] = Father.g[1][i][1];
            kid.gen[1] += kid.g[1][i][1];
        }
    }

    // inherit maternal effect loci
    for (size_t trait_i = 0; trait_i < 2; ++trait_i)
    {
        for (size_t trait_j = 0; trait_j < 2; ++trait_j)
        {
            kid.m[trait_i][trait_j][0] = MutateM(Mother.m[trait_i][trait_j][gsl_rng_uniform_int(r,2)]);
            kid.m[trait_i][trait_j][1] = MutateM(Father.m[trait_i][trait_j][gsl_rng_uniform_int(r,2)]);
        }
    }

    MutateG(kid);
   
    // add environmental variance to each trait by adding a random number
    // drawn from a normal distribution to each phenotype
    kid.phen[0] = kid.gen[0] + gsl_ran_gaussian(r,1.0) + (kid.m[0][0][0] + kid.m[0][0][1]) * Mother.phen[0] + (kid.m[0][1][0] + kid.m[0][1][1]) * Mother.phen[1];
    kid.phen[1] = kid.gen[1] + gsl_ran_gaussian(r,1.0) + (kid.m[1][0][0] + kid.m[1][0][1]) * Mother.phen[0] + (kid.m[1][1][0] + kid.m[1][1][1]) * Mother.phen[1];
}

bool possM(size_t const x, double const val)
{
    return(val > distMales[x]);
}

bool possF(size_t const x, double const val)
{
    return(val > distFemales[x]);
}

// Survival of juveniles to reproductive adults
void Reproduce_Survive()
{
    // set kids counter to 0 prior to reproduction
    NKids = 0;

    // stats for average fitness
    meanw = 0;

    double w;

    // stats for genetic covariance within offspring
    meancov = 0;

    for (size_t i = 0; i < Nf; ++i)
    {
        // random mating
        size_t father = gsl_rng_uniform_int(r, Nm);

        // produce kids and let them survive
        for (size_t j = 0; j < 2 * B; ++j)
        {
            Individual Kid;

            // create a kid from maternal and paternal genes
            Create_Kid(i, father, Kid);

            // calculate survival
            w = v(Kid.phen[0], Kid.phen[1]);

            //cout << i << " " << father << " " << Kid.phen[0] << " " << Kid.phen[1] << " " << w << endl;


            assert(w >= 0 && w <= 1.0);

            meanw += w;

            // individual survives; add to stack
            if (gsl_rng_uniform(r) < w)
            {
                NewPop[NKids++] = Kid;
                assert(NKids < Npop * 2 * 10);
            }
        }

        // remove dad
        Males[father] = Males[--Nm];

        if (Nm == 0)
        {
            break;
        }
    }

    //cout << meancov / (Npop * 2 * B) << endl;

    meanw /= Nf * 2 * B;

    //cout << NKids << endl;
    
    if (NKids < Npop)
    {
        cout << "extinct " << NKids << endl;
        WriteParameters();
        exit(1);
    }

    size_t random_kid;

    Nm = 0;
    Nf = 0;

    // sample new generation from kids
    for (size_t i = 0; i < Npop; ++i)
    {
        assert(NKids >= 1);

        // get a randomly sampled kid
        random_kid = gsl_rng_uniform_int(r, NKids);

        if (gsl_rng_uniform(r) < 0.5)
        {
            Males[Nm++] = NewPop[random_kid];
        }
        else
        {
            Females[Nf++] = NewPop[random_kid];
        }

        // delete kid (no resampling possible) by copying kid
        // from the end of the stack and reducing Nkids by one
        NewPop[random_kid] = NewPop[--NKids];
    }

    if (generation > 2000)
    {
        // change the environment
        theta1 = int1 + ampl1 * sin(freq1 * (generation + shift)) + stoch1 * gsl_rng_uniform(r);
        theta2 = int2 + ampl2 * sin(freq2 * generation) + stoch2 * gsl_rng_uniform(r);
    }

    // perturb things
    if (generation == NumGen/2)
    {
        int1 = int1ptb;
        int2 = int2ptb;
    }
}


// write down summary statistics
void WriteData(bool output)
{
    // genotypic and phenotypic means
    double meangen[2] = {0,0};
    double meanphen[2] = {0,0};

    double meanm[2][2] = {{0,0},{0,0}};
    double ssm[2][2] = {{0,0},{0,0}};

    // genotypic and phenotypic sums of squares 
    double ssgen[2][2] = {{0,0},{0,0}};
    double ssphen[2][2] = {{0,0},{0,0}};

    // get stats from the population
    for (size_t i = 0; i < Nf; ++i)
    {
        // loop through the different traits
        for (size_t j1 = 0; j1 < 2; ++j1)
        {
            meanphen[j1] += Females[i].phen[j1];
            meangen[j1] += Females[i].gen[j1];

            for (size_t j2 = 0; j2 < 2; ++j2)
            {
                ssgen[j1][j2] += Females[i].gen[j1] * Females[i].gen[j2];
                ssphen[j1][j2] += Females[i].phen[j1] * Females[i].phen[j2];

                meanm[j1][j2] += Females[i].m[j1][j2][0] + Females[i].m[j1][j2][1];
                ssm[j1][j2] += (Females[i].m[j1][j2][0] + Females[i].m[j1][j2][1]) * 
                    (Females[i].m[j1][j2][0] + Females[i].m[j1][j2][1]);
            }
        }
    }


    for (size_t i = 0; i < Nm; ++i)
    {
        // loop through the different traits
        for (size_t j1 = 0; j1 < 2; ++j1)
        {
            meanphen[j1] += Males[i].phen[j1];
            meangen[j1] += Males[i].gen[j1];

            for (size_t j2 = 0; j2 < 2; ++j2)
            {
                ssgen[j1][j2] += Males[i].gen[j1] * Males[i].gen[j2];
                ssphen[j1][j2] += Males[i].phen[j1] * Males[i].phen[j2];

                meanm[j1][j2] += Males[i].m[j1][j2][0] + Males[i].m[j1][j2][1];
                ssm[j1][j2] += (Males[i].m[j1][j2][0] + Males[i].m[j1][j2][1]) * 
                    (Males[i].m[j1][j2][0] + Males[i].m[j1][j2][1]);
            }
        }
    }


    if (output)
    {
        DataFile << generation << ";";
    }

    for (size_t j1 = 0; j1 < 2; ++j1)
    {
        meanphen[j1] /= Nf + Nm;
        meangen[j1] /= Nf + Nm; 

        if (output)
        {
            DataFile << meanphen[j1] << ";";
        }

        for (size_t j2 = 0; j2 < 2; ++j2)
        {
            meanm[j1][j2] /= Nf + Nm;

            if (output)
            {
                DataFile << meanm[j1][j2] << ";";
            }
        }
    }

    double G[2][2];
    double P[2][2];
    double Gm[2][2];

    double delta_G[2][2];

    for (size_t j1 = 0; j1 < 2; ++j1)
    {
        for (size_t j2 = 0; j2 < 2; ++j2)
        {
            G[j1][j2] = ssgen[j1][j2] / (Nf + Nm) 
                    - meangen[j1] * meangen[j2];

            P[j1][j2] = ssphen[j1][j2] / (Nf + Nm)
                    - meanphen[j1] * meanphen[j2];

            Gm[j1][j2] = ssm[j1][j2] / (Nf + Nm)
                    - meanm[j1][j2] * meanm[j1][j2];

            // calculate delta G
            delta_G[j1][j2] = G[j1][j2] - Gtmin1[j1][j2];
            Gtmin1[j1][j2] = G[j1][j2];

            if (delta_G[j1][j2] > max_delta_G[j1][j2])
            {
                max_delta_G[j1][j2] = delta_G[j1][j2];
            }

            if (delta_G[j1][j2] < min_delta_G[j1][j2])
            {
                min_delta_G[j1][j2] = delta_G[j1][j2];
            }

            if (output)
            {
                DataFile << G[j1][j2] << ";" << P[j1][j2] << ";" << Gm[j1][j2] << ";" << delta_G[j1][j2] << ";" << max_delta_G[j1][j2] << ";" << min_delta_G[j1][j2] << ";";
            }
        }
    }


    double trace = G[0][0] + G[1][1];
    double det = G[0][0] * G[1][1] - G[1][0] * G[0][1];

    // calculate eigenvalues
    double ev1 = .5 * (trace + sqrt(trace*trace - 4 * det));
    double ev2 = .5 * (trace - sqrt(trace*trace - 4 * det));

    double delta_ev1 = ev1 - ev1_tmin1;
    double delta_ev2 = ev2 - ev2_tmin1;
    
    ev1_tmin1 = ev1;
    ev2_tmin1 = ev2;

    double evec1[2] = {0,0};
    double evec2[2] = {0,0};

    if (G[0][1] != 0)
    {
         evec1[0] =  
            (ev1 - G[1][1])/
                sqrt((ev1 - G[1][1])*(ev1 - G[1][1])+G[0][1]*G[0][1]);

         evec1[1] =
            G[0][1]/
                sqrt((ev1 - G[1][1])*(ev1 - G[1][1])+G[0][1]*G[0][1]);
       
         evec2[0] = 
            (ev2 - G[1][1])/
                sqrt((ev2 - G[1][1])*(ev2 - G[1][1])+G[0][1]*G[0][1]);

         evec2[1] =
            G[0][1]/
                sqrt((ev2 - G[1][1])*(ev2 - G[1][1])+G[0][1]*G[0][1]);
    }
    else
    {
        evec1[0] = 0;
        evec1[1] = 1;
        evec2[0] = 1;
        evec2[1] = 0;
    }
    
    double angleLead = atan(evec1[1]/evec1[0]) * 180.00/M_PI;
    double angleSecond = atan(evec2[1]/evec2[0]) * 180.00/M_PI;

    double delta_angleLead = angleLead - angleLead_tmin1;
    angleLead_tmin1 = angleLead;

    double delta_angleSecond = angleSecond - angleSecond_tmin1;
    angleSecond_tmin1 = angleSecond;

    double ecc = ev1 < ev2 ? ev1/ev2 : ev2/ev1;

    double delta_ecc = ecc - ecc_tmin1;
    ecc_tmin1 = ecc;

    double delta_size = ev1 + ev2 - size_tmin1;
    size_tmin1 = ev1 + ev2;

    if (output)
    {
    DataFile << trace << ";" 
        << det << ";" 
        << ev1 << ";" 
        << ev2 << ";" 
        << delta_ev1 << ";"
        << delta_ev2 << ";"
        << (ev1+ev2) << ";" 
        << delta_size << ";" 
        << ecc << ";" 
        << delta_ecc << ";" 
        << angleLead << ";"
        << angleSecond << ";"
        << delta_angleLead << ";"
        << delta_angleSecond << ";"
        << meanw << ";" 
        << theta1 << ";" 
        << theta2 << ";" 
        << ((double)getValue() * 1e-06) << ";" 
        << endl;

    }
}

// write the headers of a datafile
void WriteDataHeaders()
{
    DataFile << "generation;";

    for (size_t j1 = 0; j1 < 2; ++j1)
    {
        DataFile << "meanz" << j1 + 1 << ";";

        for (size_t j2 = 0; j2 < 2; ++j2)
        {
            DataFile << "meanm" << j1 + 1 << j2 + 1 << ";";
        }
    }

    for (size_t j1 = 0; j1 < 2; ++j1)
    {
        for (size_t j2 = 0; j2 < 2; ++j2)
        {
            DataFile << "G" << (j1 + 1) << (j2 + 1) << ";"
                        << "P" << (j1 + 1) << (j2 + 1) << ";"
                        << "Gm" << (j1 + 1) << (j2 + 1) << ";"
                        << "delta_G" << (j1 + 1) << (j2 + 1) << ";"
                        << "max_delta_G" << (j1 + 1) << (j2 + 1) << ";"
                        << "min_delta_G" << (j1 + 1) << (j2 + 1) << ";";
        }
    }

    DataFile << "trace;det;ev1;ev2;delta_ev1;delta_ev2;size;delta_size;ecc;delta_ecc;angle_lead;angle_second;delta_engle_lead;delta_angle_second;meanw;theta1;theta2;ram;" << endl;
}


// the guts of the code
int main(int argc, char ** argv)
{
	initArguments(argc, argv);
	WriteDataHeaders();
	Init();

	for (generation = 0; generation <= NumGen; ++generation)
	{
		Reproduce_Survive();

        // output stats every xth generation except for the last 2000 gens
        do_stats = generation % skip == 0;

        WriteData(do_stats || generation > NumGen - 2000);

	}

	WriteParameters();
}
