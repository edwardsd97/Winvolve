
#define GENES_SIZE	(9)
#define GENES_LEN	(GENES_SIZE-1)
#define POP_ROWS	(22)
#define POP_COLS	(8)
#define MAX_POP		(POP_ROWS * POP_COLS)

#define AGE_DEATH			(5)
#define AGE_MATURE			(2)
#define REBIRTH_GENERATIONS (10)
#define DEFAULT_PREDATION	(0.9f)
#define SPECIES_MATCH_SCORE (0.5f)
#define NEW_SPECIES_MINIMUM (6)

typedef struct creature_s
{
	int		species;
	int		age;
	char	genes[GENES_SIZE];

} creature_t;

extern creature_t creatures[MAX_POP];
extern creature_t environment[POP_COLS];

extern float predation;
extern int rebirth;
extern unsigned generation;
extern unsigned speciesNow;
extern unsigned speciesEver;
extern unsigned extinctions;
extern unsigned massExtinctions;
extern int river_col;

void	evolve_init();
int		evolve_simulate(int step);
void	evolve_asteroid();
void	evolve_earthquake();
