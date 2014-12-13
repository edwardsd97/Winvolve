
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <malloc.h>
#include <string.h>
#include <direct.h>
#include <time.h>
#include <math.h>

#include "evolve.h"

#pragma warning(disable:4996) // This function or variable may be unsafe.

#define max(a,b)		(((a) > (b)) ? (a) : (b))
#define min(a,b)		(((a) < (b)) ? (a) : (b))
#define clamp(a,b,c)    ( min( max((a), (b)), (c) ) )

float randf()
{
	return float(rand() % 100) / 100.0f;
}

typedef float(*score_func)(creature_t *creatureA, creature_t *creatureB);

char random_letter(evolve_state_t *state, bool lowerOnly = false)
{
	int index = rand() % state->alphabet_size;
	char result = state->parms.alphabet[index];
	if (lowerOnly)
		return LOWER(result);
	return rand() % 2 ? LOWER(result) : UPPER(result);
}

void randomize_order(evolve_state_t *state )
{
	for (int i = 0; i < state->popSize; i++)
	{
		int swap = rand() % state->popSize;
		int temp = state->orderTable[i];
		state->orderTable[i] = state->orderTable[swap];
		state->orderTable[swap] = temp;
	}
}

char evolve_alphabet_index(evolve_state_t *state, char c)
{
	int i;
	for (i = 0; i < state->alphabet_size; i++)
	{
		if (LOWER(c) == LOWER(state->parms.alphabet[i]))
			return i;
	}

	return -1;
}

int creature_index(creature_t *creature)
{
	return creature - creature->state->creatures;
}

int creature_row(creature_t *creature)
{
	return creature_index(creature) / creature->state->parms.popCols;
}

int creature_col(creature_t *creature)
{
	return creature_index(creature) % creature->state->parms.popCols;
}

creature_t *creature_environment(creature_t *creature)
{
	return &creature->state->environment[creature_col(creature)];
}

int letter_delta(evolve_state_t *state,char A, char B)
{
	A = LOWER(A);
	B = LOWER(B);

	if (A == B)
		return 0;

	int bigger = max(A, B);
	int smaller = min(A, B);

	int result1 = bigger - smaller;
	int result2 = 1 + (smaller - state->parms.alphabet[0]) + (state->parms.alphabet[state->alphabet_size - 1]) - bigger;

	return min(result1, result2) * 2;
}

float matches_letter(evolve_state_t *state, char A, char B)
{
	float diff = fabs((float)letter_delta(state,A, B));
	float range = float(state->alphabet_size);

	return clamp(1.0f - (diff / range), 0.0f, 1.0f);
}

float matches_case(char A, char B)
{
	if (B >= 'a' && A >= 'a')
		return 1.0f;
	if (B < 'a' && A < 'a')
		return 1.0f;
	return 0.0f;
}

float score_connected(creature_t *creatureA, creature_t *creatureB)
{
	evolve_state_t *state = creatureA->state;

	int rowA = creature_row(creatureA);
	int colA = creature_col(creatureA);

	int rowB = creature_row(creatureB);
	int colB = creature_col(creatureB);

	if ((colA < state->river_col && colB >= state->river_col) ||
		(colB < state->river_col && colA >= state->river_col))
		return -1.0f; // across the river

	return 1.0f;
}

float score_predatory(creature_t *triggerer, creature_t *prey)
{
	evolve_state_t *state = triggerer->state;

	if (!triggerer->genes[0])
		return -1.0f; // triggerer is dead

	if (!prey->genes[0])
		return -1.0f; // prey is dead

	if (creature_col(triggerer) != creature_col(prey))
		return -1.0f;

	float score = 0;
	for (int i = 0; i < state->parms.genes; i++)
	{
		// Prefer those that stand out against the environment
		score += (1.0f - matches_case(prey->genes[i], creature_environment(prey)->genes[i])) * (1.0f / (float)(state->parms.genes * 8));
		score += (1.0f - matches_letter(state, prey->genes[i], creature_environment(prey)->genes[i])) * (1.0f / (float)(state->parms.genes));
	}

	return score;
}

float score_mate_possible(creature_t *creatureA, creature_t *creatureB)
{
	evolve_state_t *state = creatureA->state;

	if (creatureA->species != creatureB->species)
		return -1;

	if (!creatureA->genes[0])
		return -1; // i am dead

	if (!creatureB->genes[0])
		return -1; // creatureB dead

	if (creatureA == creatureB)
		return -1; // cant mate with myself

	float score = 0;

	for (int i = 0; i < state->parms.genes; i++)
	{
		// Those that match my colors the most
		score += matches_letter(state, creatureA->genes[i], creatureB->genes[i]) * (1.0f / (float)(state->parms.genes));
	}

	if (score >= state->parms.speciesMatch)
		return score;

	return -1;
}

float score_mate(creature_t *creatureA, creature_t *creatureB)
{
	evolve_state_t *state = creatureA->state;

	if (!creatureA->genes[0])
		return -1; // i am dead

	if (!creatureB->genes[0])
		return -1; // creatureB dead

	if (creatureB->age < state->parms.ageMature)
		return -1; // creatureB not old enough

	if (creatureA->age < state->parms.ageMature)
		return -1; // i am not old enough

	if (creatureA == creatureB)
		return -1; // cant mate with myself

	if (score_connected(creatureA, creatureB) <= 0.0f)
		return -1;

	if (score_mate_possible(creatureA, creatureB) <= 0.0f)
		return -1;

	float score = 0;
	float perGeneFactor = (1.0f / (float)(state->parms.genes * 2));

	if (state->rebirth > 0)
		perGeneFactor = 1.0f / (float)(state->parms.genes);

	for (int i = 0; i < state->parms.genes; i++)
	{
		// Prefer those that match my colors the most
		score += matches_letter(state, creatureA->genes[i], creatureB->genes[i]) * perGeneFactor;

		if (state->rebirth <= 0)
		{
			// Prefer those that stand out against the environment
			score += (1.0f - matches_letter(state, creatureB->genes[i], creature_environment(creatureB)->genes[i])) * perGeneFactor;
		}
	}

	return score;
}

float score_near(creature_t *creatureA, creature_t *creatureB)
{
	evolve_state_t *state = creatureA->state;

	if (score_connected(creatureA, creatureB) <= 0.0f)
		return -1.0f;

	int rowDelta = abs(creature_row(creatureB) - creature_row(creatureA));
	int colDelta = abs(creature_row(creatureB) - creature_row(creatureA));
	float diff = (float)max(rowDelta, colDelta);
	float range = max((float)state->parms.popRows, (float)state->parms.popCols);
	return clamp(1.0f - (diff / range), 0.0f, 1.0f);
}

float score_free(creature_t *creatureA, creature_t *creatureB)
{
	evolve_state_t *state = creatureA->state;

	if (creatureB->genes[0])
		return -1.0f; // not free

	if (creatureA == creatureB)
		return -1.0f; // same creature

	// free slot
	return score_near(creatureA, creatureB);
}

void mutate(creature_t *creature)
{
	evolve_state_t *state = creature->state;

	int gene = rand() % state->parms.genes;
	if (rand() % 2)
	{
		// Change case but keep letter
		if ( IS_LOWER(creature->genes[gene]) )
			creature->genes[gene] = UPPER(creature->genes[gene]);
		else
			creature->genes[gene] = LOWER(creature->genes[gene]);
	}
	else
	{
		// Change letter but keep case
		int index = evolve_alphabet_index(state, creature->genes[gene]);
		index += rand() % 2 ? 1 : -1;
		if (index < 0)
			index = state->alphabet_size - 1;
		else if (index >= state->alphabet_size)
			index = 0;
		creature->genes[gene] = state->parms.alphabet[index];
	}
}

void record_species(creature_t *creature, int generation, int alive)
{
	int x;
	int y;
	int open = -1;
	int oldest = -1;
	int oldestIdx = -1;

	evolve_state_t *state = creature->state;

	int oldestSpeciesAge = -1;
	int oldestSpeciesDeadOrAlive = -1;
	for (x = 0; x < state->parms.historySpecies; x++)
	{
		for (y = 0; y < state->parms.popCols; y++)
		{
			int speciesAge = state->record[x][y].generation - state->record[x][0].generation + 1;
			if (speciesAge > oldestSpeciesAge)
			{
				oldestSpeciesAge = speciesAge;
				oldestSpeciesDeadOrAlive = x;
			}
		}
	}


	for (x = 0; x < state->parms.historySpecies; x++)
	{
		if (!state->record[x][0].generation && open < 0)
			open = x;

		for (y = 0; y < state->parms.popCols; y++)
		{
			if (!state->record[x][y].generation)
				break;

			// Always keep the oldest species ever in the history
			if (oldestSpeciesDeadOrAlive == x)
				continue;

			if (oldest == -1 || state->record[x][y].generation < oldest)
			{
				oldest = state->record[x][y].generation;
				oldestIdx = x;
			}
		}
			
		if (state->record[x][0].creature.species == creature->species)
		{
			for (y = 0; state->record[x][y].creature.species == creature->species && y < state->parms.popCols; y++)
			{
				if (state->record[x][y].generation == generation)
					return;
			}

			break;
		}
	}

	if (open < 0)
		open = oldestIdx;

	if (open >= 0 && x >= state->parms.historySpecies)
	{
		// Start a new slot
		x = open;
		y = 0;
		memset(state->record[x], 0, sizeof(state->record[x]));
	}

	if (x < state->parms.historySpecies)
	{
		// Shift down records if necessary (except the first one)
		if (y >= state->parms.popCols)
		{
			for (y = 1; y < state->parms.popCols - 1; y++)
				memcpy(&state->record[x][y], &state->record[x][y + 1], sizeof(species_record_t));
			y = state->parms.popCols - 1;
		}

		// Add record
		state->record[x][y].generation = generation;
		memcpy(&(state->record[x][y].creature), creature, sizeof(creature_t));
		memcpy(&(state->record[x][y].environment), &state->environment[creature_col(creature)], sizeof(creature_t));

		state->stats[ES_SPECIES_MAX_AGE] = max(state->stats[ES_SPECIES_MAX_AGE], generation - state->record[x][0].generation + 1);

		for (y = 0; y < state->parms.popCols; y++)
			state->record[x][y].creature.age = alive ? 0 : (state->parms.ageDeath / 2);
	}
}


void breed_creature(evolve_state_t *state,creature_t *child, creature_t *parentA, creature_t *parentB)
{
	int i;

	memset(child, 0, sizeof(creature_t));
	child->state = state;

	if (parentA && !parentB)
	{
		// Clone of A
		strcpy(child->genes, parentA->genes);
		child->species = parentA->species;
	}
	else if (!parentA && parentB)
	{
		// Clone of B
		strcpy(child->genes, parentA->genes);
		child->species = parentB->species;
	}
	else if (parentA && parentB)
	{
		// Child of A and B
		for (i = 0; i < state->parms.genes; i++)
			child->genes[i] = rand() % 2 ? parentA->genes[i] : parentB->genes[i];
		child->species = parentA->species;
	}
	else
	{
		// Completely Random
		for (i = 0; i < state->parms.genes; i++)
			child->genes[i] = random_letter(state);
		child->genes[state->parms.genes] = 0;
		state->stats[ES_SPECIES_EVER]++;
		child->species = state->stats[ES_SPECIES_EVER];
		record_species(child, state->stats[ES_GENERATIONS], 1);
	}

	// 1 Genetic Mutation
	mutate(child);

	// See if this is a new species
	int possible = 0;
	int matched = 0;
	creature_t *newSpecies[POP_MAX];
	creature_t *oldSpecies[POP_MAX];
	int newSpeciesCnt = 0;
	int oldSpeciesCnt = 0;
	bool wasPossible;
	memset(newSpecies, 0, sizeof(newSpecies));
	memset(oldSpecies, 0, sizeof(oldSpecies));
	for (i = 0; i < state->popSize; i++)
	{
		if (child == &state->creatures[i])
			continue;

		wasPossible = false;
		if (score_mate_possible(child, &state->creatures[i]) > 0.0f)
		{
			wasPossible = true;
			possible++;
		}

		if (state->creatures[i].genes[0] && child->species == state->creatures[i].species)
		{
			matched++;
			if (wasPossible)
			{
				newSpecies[newSpeciesCnt] = &state->creatures[i];
				newSpeciesCnt++;
			}
			else
			{
				oldSpecies[oldSpeciesCnt] = &state->creatures[i];
				oldSpeciesCnt++;
			}
		}
	}

	if (newSpeciesCnt >= state->parms.speciesNew && newSpeciesCnt <= oldSpeciesCnt)
	{
		state->stats[ES_SPECIES_EVER]++;
		for (i = 0; i < newSpeciesCnt; i++)
			newSpecies[i]->species = state->stats[ES_SPECIES_EVER];
		record_species(newSpecies[rand() % newSpeciesCnt], state->stats[ES_GENERATIONS], 1);
	}
}

void random_environments(evolve_state_t *state, creature_t *envs, int count)
{
	char rndA = random_letter(state);
	char rndB = random_letter(state);

	int i;
	int j;
	const int envMod = 4;

	// Bleed some creatures over to the other sides of the river
	if (state->river_col > 0 && state->river_col < state->parms.popCols - 1)
	{
		for (i = 0; i < state->parms.popRows; i++)
		{
			if (rand() % 2)
			{
				// Swap
				creature_t temp;
				memcpy(&temp, &state->creatures[i*state->parms.popCols + state->river_col], sizeof(creature_t));
				memcpy(&state->creatures[i*state->parms.popCols + state->river_col], &state->creatures[i*state->parms.popCols + (state->river_col - 1)], sizeof(creature_t));
				memcpy(&state->creatures[i*state->parms.popCols + (state->river_col - 1)], &temp, sizeof(creature_t));
			}
		}
	}

	state->river_col = state->parms.popCols / 2;

	for (j = 0; j < count; j++)
	{
		envs[j].state = state; 
			
		if (j == state->river_col)
		{
			rndA = random_letter(state);
			rndB = random_letter(state);
		}
		for (i = 0; i < state->parms.genes; i++)
		{
			if (i < state->parms.genes / 2)
				envs[j].genes[i] = rndA;
			else
				envs[j].genes[i] = rndB;
		}

		envs[j].genes[state->parms.genes] = 0;
	}
}


int find_best(creature_t *creature, score_func func, float *resultScore = NULL)
{
	evolve_state_t *state = creature->state;

	float bestScore = 0;
	int i;
	int result = -1;

	for (i = 0; i < state->popSize; i++)
	{
		float score = func(creature, &state->creatures[i]);
		if ((score > bestScore) || (score == bestScore && rand() % 2))
		{
			bestScore = score;
			result = i;
		}
	}

	if (resultScore)
		(*resultScore) = bestScore;

	return result;
}

void die( creature_t *creature, int deathReason )
{
	evolve_state_t *state = creature->state;

	int i;
	if (creature->genes[0])
	{
		if (creature_col(creature) >= state->river_col)
		{
			strcpy(state->creatureLastAlive[1].genes, creature->genes);
			state->creatureLastAliveIdx[1] = creature_index(creature);
		}
		else
		{
			strcpy(state->creatureLastAlive[0].genes, creature->genes);
			state->creatureLastAliveIdx[0] = creature_index(creature);
		}

		// See if this is the last of a species
		for ( i = 0; i < state->popSize; i++)
		{
			// If there are still creatures alive of the same species (even if we cannot physically mate with them at this time)
			//  then this is not considered an extinction
			if (creature != &state->creatures[i] && creature->species == state->creatures[i].species && state->creatures[i].genes[0])
				break;
		}
		if (i == state->popSize)
		{
			state->stats[ES_EXTINCTIONS]++;
			record_species(creature, state->stats[ES_GENERATIONS], 0);
		}

		memset(creature, 0, sizeof(creature_t));
		creature->state = state;
		creature->death = deathReason;
	}
}

void survive( creature_t *creature )
{
	evolve_state_t *state = creature->state;

	if (!creature->genes[0])
		return; // dead creature

	// Age
	creature->age++;
	if (creature->age >= state->parms.ageDeath)
	{
		die(creature, 1);
		return;
	}

	if (state->predation > 0.0f && state->rebirth <= 0)
	{
		// Predation: See if one of us gets eaten
		float score;
		int eaten = find_best(creature, score_predatory, &score);
		if (eaten >= 0)
		{
			if (randf() < score * state->predation)
				die(&state->creatures[eaten], 2);
		}
	}
	else if (state->rebirth < state->parms.rebirthGenerations - 3 && creature->age >= (state->parms.ageDeath * 0.5f))
	{
		// Rebirth: Die of old age with a bit of randomness toward the end
		if ( randf() < (float)(creature->age) / ((float)state->parms.ageDeath))
			die(creature, 1);
	}
}

void procreate(creature_t *creature)
{
	evolve_state_t *state = creature->state;

	// Find best creature to mate
	float score;
	int mate = find_best(creature, score_mate, &score);
	int free = find_best(creature, score_free);
	if (mate >= 0 && free >= 0)
	{
		if (randf() < score * state->parms.procreationLevel)
			breed_creature(state, &state->creatures[free], creature, &state->creatures[mate]);
	}
}

void current_population( evolve_state_t *state )
{
	int i;
	int j;
	int speciesList[POP_MAX];
	state->stats[ES_SPECIES_NOW] = 0;
	state->population[0] = 0;
	state->population[1] = 0;

	for ( i = 0; i < state->popSize; i++)
	{
		if (state->creatures[i].genes[0] == 0)
			continue;

		if (creature_col(&state->creatures[i]) < state->river_col)
			state->population[0]++;
		else
			state->population[1]++;

		for (j = 0; j < (int)state->stats[ES_SPECIES_NOW]; j++)
		{
			if (state->creatures[i].species == speciesList[j])
				break;
		}
		if (j == state->stats[ES_SPECIES_NOW])
		{
			speciesList[state->stats[ES_SPECIES_NOW]] = state->creatures[i].species;
			state->stats[ES_SPECIES_NOW]++;
		}
	}

	state->stats[ES_SPECIES_MAX] = max(state->stats[ES_SPECIES_MAX], state->stats[ES_SPECIES_NOW]);
}

void evolve_rebirth(evolve_state_t *state,bool randomizeEnvironment)
{
	int rebirth_age = max(0, state->parms.ageMature - 1);

	current_population( state );

	for (int i = 0; i < state->popSize; i++)
		state->creatures[i].age = rebirth_age;

	if ( randomizeEnvironment )
		random_environments(state, state->environment, state->parms.popCols);

	for (int col = state->river_col; col >= 0 && col > state->river_col - 2; col--)
	{
		if (col >= state->river_col)
		{
			if (state->population[1] < 2)
			{
				int spawnRow = rand() % state->parms.popRows;
				for (int i = 0; i < (2 - state->population[1]); i++)
				{
					int spawnIdx = (spawnRow*state->parms.popCols) + state->river_col;
					mutate(&state->creatureLastAlive[1]);
					int free = find_best(&state->creatures[spawnIdx], score_free);
					if (free >= 0)
					{
						breed_creature(state, &state->creatures[free], &state->creatureLastAlive[1], NULL);
						state->creatures[free].age = rebirth_age;
					}
				}
			}
		}
		else if (state->river_col > 0)
		{
			if (state->population[0] < 2)
			{
				int spawnRow = rand() % state->parms.popRows;
				for (int i = 0; i < (2 - state->population[0]); i++)
				{
					int spawnIdx = (spawnRow*state->parms.popCols) + (state->river_col - 1);
					mutate(&state->creatureLastAlive[0]);
					int free = find_best(&state->creatures[spawnIdx], score_free);
					if (free >= 0)
					{
						breed_creature(state, &state->creatures[free], &state->creatureLastAlive[0], NULL);
						state->creatures[free].age = rebirth_age;
					}
				}
			}
		}
	}

	state->rebirth = state->parms.rebirthGenerations;
}

void evolve_parms_default(evolve_parms_t *parms)
{
	parms->ageDeath				= 5;
	parms->ageMature			= 2;
	parms->rebirthGenerations	= 15;
	parms->predationLevel		= 0.55f;
	parms->procreationLevel		= 1.0f;
	parms->speciesMatch			= 0.5f;
	parms->speciesNew			= 6;
	parms->popRows				= 21;
	parms->popCols				= 8;
	parms->envChangeRate		= 100;
	parms->historySpecies		= 9;

	strcpy(parms->alphabet, "abcdefgh");
	parms->genes = 8;
}

void evolve_parms_update(evolve_state_t *state, evolve_parms_t *parms)
{
	state->parms.ageDeath = parms->ageDeath;
	state->parms.ageMature = parms->ageMature;
	state->parms.rebirthGenerations = parms->rebirthGenerations;
	state->parms.predationLevel = parms->predationLevel;
	state->parms.procreationLevel = parms->procreationLevel;
	state->parms.speciesMatch = parms->speciesMatch;
	state->parms.speciesNew = parms->speciesNew;
	state->parms.envChangeRate = parms->envChangeRate;
	state->parms.historySpecies = parms->historySpecies;

	state->predation = state->parms.predationLevel;

	state->parms.ageDeath = max(state->parms.ageDeath, state->parms.ageMature + 1);
	state->parms.ageMature = max(0, min(state->parms.ageDeath - 1, state->parms.ageMature));
	parms->ageDeath = state->parms.ageDeath;
	parms->ageMature = state->parms.ageMature;

	// cannot be updated without full re-init
	// state->parms->popRows = 22;
	// state->parms->popCols = 8;
	// state->strcpy(parms->alphabet, "abcdefgh");
	// state->parms->genes = 8;
}

bool evolve_init(evolve_state_t *state, evolve_parms_t *parms)
{
	memset(state, 0, sizeof(evolve_state_t));

	if (!parms)
		evolve_parms_default(&state->parms);
	else
		state->parms = *parms;

	evolve_parms_update(state, &state->parms);

	if (state->parms.genes > GENES_MAX)
		return false;
	if (state->parms.popRows > POP_ROWS_MAX)
		return false;
	if (state->parms.popCols > POP_COLS_MAX)
		return false;
	if (state->parms.ageDeath <= state->parms.ageMature)
		return false;

	state->stats[ES_GENERATIONS] = 1;
	state->predation = state->parms.predationLevel;
	state->alphabet_size = strlen(state->parms.alphabet);
	strlwr(state->parms.alphabet);
	state->popSize = state->parms.popRows * state->parms.popCols;

	srand((unsigned)time(NULL));

	random_environments(state, state->environment, state->parms.popCols);

	// Generate a single random creature in each environment and trigger a time of rebirth
	for (int i = 0; i < state->popSize; i++)
	{
		state->creatures[i].state = state;
		state->orderTable[i] = i;
	}

	state->creatureLastAlive[0].state = state;
	state->creatureLastAlive[1].state = state;

	for (int col = state->river_col; col >= 0 && col > state->river_col - 2; col--)
	{
		int spawnRow = rand() % state->parms.popRows;
		int spawnIdx = (spawnRow*state->parms.popCols) + col;
		breed_creature(state, &state->creatures[spawnIdx], NULL, NULL);
		int spouseIdx = find_best(&state->creatures[spawnIdx], score_free);
		if ( spouseIdx >= 0 )
			breed_creature(state,&state->creatures[spouseIdx], &state->creatures[spawnIdx], NULL);
		if (col >= state->river_col)
		{
			strcpy(state->creatureLastAlive[1].genes, state->creatures[spawnIdx].genes);
			state->creatureLastAliveIdx[1] = spawnIdx;
		}
		else
		{
			strcpy(state->creatureLastAlive[0].genes, state->creatures[spawnIdx].genes);
			state->creatureLastAliveIdx[0] = spawnIdx;
		}
	}

	evolve_rebirth( state, false );
	randomize_order(state);

	return true;
}

void evolve_simulate(evolve_state_t *state)
{
	current_population(state);

	if (state->population[0] == 0 && state->population[1] == 0)
	{
		state->stats[ES_EXTINCTIONS_MASS]++;
		state->stats[ES_SPECIES_EVER]++;
		state->creatureLastAlive[0].species = state->stats[ES_SPECIES_EVER];
		state->stats[ES_SPECIES_EVER]++;
		state->creatureLastAlive[1].species = state->stats[ES_SPECIES_EVER];

		// Initiate a rebirth
		evolve_rebirth(state, true);

		for (int i = 0; i < state->popSize; i++)
		{
			if (state->creatures[state->orderTable[i]].genes[0])
				record_species(&state->creatures[state->orderTable[i]], state->stats[ES_GENERATIONS], 1);
		}

		state->step = 0;
	}

	if (state->step >= (state->popSize * 2))
	{
		state->stats[ES_GENERATIONS]++;
		if (state->stats[ES_GENERATIONS] % state->parms.envChangeRate == 0)
		{
			for (int i = 0; i < state->popSize; i++)
			{
				if (state->creatures[state->orderTable[i]].genes[0])
					record_species(&state->creatures[state->orderTable[i]], state->stats[ES_GENERATIONS], 1);
			}

			random_environments(state, state->environment, state->parms.popCols);
			randomize_order(state);
		}

		state->rebirth--;
		state->step = 0;
		return;
	}
	else if (state->step % 2 == 0)
		survive(&state->creatures[state->orderTable[state->step / 2]]);
	else if (state->step % 2 == 1)
		procreate(&state->creatures[state->orderTable[state->step / 2]]);

	if (state->population[0] == 0 && state->population[1] == 0)
	{
		state->step = 0;
		return;
	}
	
	state->step++;
}

void evolve_asteroid(evolve_state_t *state)
{
	randomize_order(state);
	for (int i = 0; i < state->popSize; i++)
	{
		if (state->creatures[state->orderTable[i]].genes[0])
			die(&state->creatures[state->orderTable[i]], 2);
	}
	state->rebirth = 0;
}

void evolve_earthquake(evolve_state_t *state)
{
	randomize_order(state);
	for (int i = 0; i < state->popSize; i++)
	{
		if (rand() % 4 == 0)
			die(&state->creatures[state->orderTable[i]], 2);
	}

	random_environments(state, state->environment, state->parms.popCols);
}
