
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
#define clamp(a,b,c)    ( min( max(a, b), c ) )

float randf()
{
	return float(rand() % 100) / 100.0f;
}

typedef float(*score_func)(creature_t *creatureA, creature_t *creatureB);

char random_letter(evolve_state_t *state, bool lowerOnly = false)
{
	int size = state->alphabet_size;
	if (lowerOnly)
		size = size / 2;
	return state->parms.alphabet[rand() % size];
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

char alphabet_index(evolve_state_t *state, char c)
{
	int i;
	for (i = 0; i < state->alphabet_size; i++)
	{
		if (c == state->parms.alphabet[i])
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
	if (A < 'a')
		A += 'a' - 'A';
	if (B < 'a')
		B += 'a' - 'A';

	if (A == B)
		return 0;

	int bigger = max(A, B);
	int smaller = min(A, B);

	int result1 = bigger - smaller;
	int result2 = 1 + (smaller - state->parms.alphabet[0]) + (state->parms.alphabet[(state->alphabet_size / 2) - 1]) - bigger;

	return min(result1, result2) * 2;
}

float matches_letter(evolve_state_t *state, char A, char B)
{
	float diff = fabs((float)letter_delta(state,A, B));
	float range = (float(state->alphabet_size) / 2.0f);

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

	for (int i = 0; i < state->parms.genes; i++)
	{
		// Prefer those that match my colors the most
		score += matches_letter(state, creatureA->genes[i], creatureB->genes[i]) * (1.0f / (float)(state->parms.genes * 2));

		// Prefer those that stand out against the environment
		score += (1.0f - matches_letter(state, creatureB->genes[i], creature_environment(creatureB)->genes[i])) * (1.0f / (float)(state->parms.genes * 2));
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
		if (creature->genes[gene] > 'Z')
			creature->genes[gene] = creature->genes[gene] - ('a' - 'A');
		else
			creature->genes[gene] = creature->genes[gene] + ('a' - 'A');
	}
	else
	{
		// Change letter but keep case
		int index = alphabet_index(state, creature->genes[gene]);
		index += rand() % 2 ? 1 : -1;
		if (creature->genes[gene] > 'Z')
			index = clamp(index, 0, (state->alphabet_size / 2) - 1);
		else
			index = clamp(index, state->alphabet_size / 2, state->alphabet_size - 1);
		creature->genes[gene] = state->parms.alphabet[index];
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
		state->speciesEver++;
		child->species = state->speciesEver;
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
		state->speciesEver++;
		for (i = 0; i < newSpeciesCnt; i++)
			newSpecies[i]->species = state->speciesEver;
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


int find_best(creature_t *creature, score_func func)
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

	return result;
}

void die( creature_t *creature )
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
			if (score_mate_possible(creature, &state->creatures[i]) > 0.0f )
				break;
		}
		if ( i == state->popSize )
			state->extinctions++;

		memset(creature, 0, sizeof(creature_t));
		creature->state = state;
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
		die(creature);
		return;
	}

	if (state->predation > 0.0f && state->rebirth <= 0)
	{
		// Predation: See if one of us getss eaten
		int eaten = find_best(creature, score_predatory);
		if (eaten >= 0)
		{
			float score = score_predatory(&state->creatures[eaten], &state->creatures[eaten]);
			if (randf() < score * state->predation)
				die(&state->creatures[eaten]);
		}
	}
	else if (state->rebirth < state->parms.rebirthGenerations - 3 && creature->age >= (state->parms.ageDeath * 0.5f))
	{
		// Rebirth: Die of old age with a bit of randomness toward the end
		if ( randf() < (float)(creature->age) / ((float)state->parms.ageDeath))
			die(creature);
	}
}

void procreate(creature_t *creature)
{
	evolve_state_t *state = creature->state;

	// Find best creature to mate
	int mate = find_best(creature, score_mate);
	int free = find_best(creature, score_free);
	if (mate >= 0 && free >= 0)
		breed_creature(state, &state->creatures[free], creature, &state->creatures[mate]);
}

void current_population( evolve_state_t *state )
{
	unsigned i;
	unsigned j;
	int speciesList[POP_MAX];
	state->speciesNow = 0;
	state->population[0] = 0;
	state->population[1] = 0;
	for ( i = 0; i < (unsigned) state->popSize; i++)
	{
		if (state->creatures[i].genes[0] == 0)
			continue;

		if (creature_col(&state->creatures[i]) < state->river_col)
			state->population[0]++;
		else
			state->population[1]++;

		for (j = 0; j < state->speciesNow; j++)
		{
			if (state->creatures[i].species == speciesList[j])
				break;
		}
		if (j == state->speciesNow)
		{
			speciesList[state->speciesNow] = state->creatures[i].species;
			state->speciesNow++;
		}
	}
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
	parms->rebirthGenerations	= 10;
	parms->predationLevel		= 0.9f;
	parms->speciesMatch			= 0.5f;
	parms->speciesNew			= 6;
	parms->popRows				= 22;
	parms->popCols				= 8;
	parms->envChangeRate		= 100;

	strcpy(parms->alphabet, "abcdefghABCDEFGH");
	parms->genes = 8;
}

bool evolve_init(evolve_state_t *state, evolve_parms_t *parms)
{
	memset(state, 0, sizeof(evolve_state_t));

	if (!parms)
		evolve_parms_default(&state->parms);
	else
		state->parms = *parms;

	if (state->parms.genes > GENES_MAX)
		return false;
	if (state->parms.popRows > POP_ROWS_MAX)
		return false;
	if (state->parms.popCols > POP_COLS_MAX)
		return false;
	if (state->parms.ageDeath <= state->parms.ageMature)
		return false;

	state->generation = 1;
	state->predation = state->parms.predationLevel;
	state->alphabet_size = strlen(state->parms.alphabet);
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
		state->massExtinctions++;
		state->speciesEver++;
		state->creatureLastAlive[0].species = state->speciesEver;
		state->speciesEver++;
		state->creatureLastAlive[1].species = state->speciesEver;

		// Initiate a rebirth
		evolve_rebirth(state, true);
		state->step = 0;
	}

	if (state->step >= (state->popSize * 2))
	{
		state->generation++;
		if (state->generation % state->parms.envChangeRate == 0)
		{
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
		die(&state->creatures[state->orderTable[i]]);
	state->rebirth = 0;
}

void evolve_earthquake(evolve_state_t *state)
{
	randomize_order(state);
	for (int i = 0; i < state->popSize; i++)
	{
		if (rand() % 4 == 0)
			die(&state->creatures[state->orderTable[i]]);
	}

	random_environments(state, state->environment, state->parms.popCols);
}
