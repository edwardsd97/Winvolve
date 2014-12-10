
#define GENES_MAX		(32)
#define MAX_ALPHABET	(64)
#define POP_ROWS_MAX	(32)
#define POP_COLS_MAX	(32)
#define POP_MAX			(POP_ROWS_MAX*POP_COLS_MAX)
#define SPECIES_HIST_X	(8)
#define SPECIES_HIST_Y	(16)

struct evolve_state_s;
struct evolve_parms_s;
struct creature_s;

typedef struct creature_s
{
	evolve_state_s *state;
	int				species;
	int				age;
	char			genes[GENES_MAX];

} creature_t;

typedef struct evolve_parms_s
{
	int		ageDeath;				// Generation from birth that creatures will die
	int		ageMature;				// Generation from birth required to mate
	int		rebirthGenerations;		// Generations after catastrophe that no predation occurs
	float	predationLevel;			// Level of Predation (0.0 to 1.0)
	float	speciesMatch;			// Level of matching genes required to mate (0.0 to 1.0)
	int		speciesNew;				// Minimum non matching creatures to define a new species
	int		genes;					// Number of genomes in a gene
	char	alphabet[MAX_ALPHABET];	// Genome alphabet
	int		popRows;				// Population Rows
	int		popCols;				// Population Columns
	int		envChangeRate;			// Environment changes every X generations

} evolve_parms_t;

typedef struct species_record_s
{
	int				generation;
	creature_t		creature;
	creature_t		environment;

} species_record_t;


typedef struct evolve_state_s
{
	evolve_parms_t	parms;
	int				popSize;	// state->popSize(state->parms.popRows * state->parms.popCols)

	creature_t		creatures[POP_MAX];
	creature_t		environment[POP_COLS_MAX];

	unsigned		generation;
	unsigned		speciesNow;
	unsigned		speciesEver;
	unsigned		extinctions;
	unsigned		massExtinctions;
	float			predation;
	int				river_col;
	int				rebirth;
	int				step;
	int				alphabet_size;
	int				orderTable[POP_MAX];

	species_record_s record[SPECIES_HIST_X][SPECIES_HIST_Y];

	creature_t		creatureLastAlive[2];
	int				creatureLastAliveIdx[2];
	int				population[2];

} evolve_state_t;

bool	evolve_init(evolve_state_t *state, evolve_parms_t *parms = NULL);
void	evolve_simulate(evolve_state_t *state);

void	evolve_parms_default(evolve_parms_t *parms);
void	evolve_asteroid(evolve_state_t *state);
void	evolve_earthquake(evolve_state_t *state);
